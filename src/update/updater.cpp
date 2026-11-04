#include "update/updater.h"

#include "update/updatedialog.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QSettings>
#include <QTemporaryFile>
#include <QTimer>
#include <QWidget>

#include <filesystem>
#include <system_error>

namespace update {

namespace {

// § 4.6 and § 4.8's limits.
constexpr qint64 kListCap = qint64(1024) * 1024;      // the releases list: 1 MiB
constexpr qint64 kSignatureCap = 1024;                // a .sig is 64 bytes
constexpr qint64 kDownloadCap = qint64(200) * 1024 * 1024; // the AppImage: 200 MiB
constexpr int kTimeoutMs = 30 * 1000;

const char* const kReleasesUrl = "https://api.github.com/repos/milnet01/games-hub/releases"; // untranslated: a URL

QString autoKey() { return QStringLiteral("update/auto"); } // untranslated: a settings key
QString lastCheckKey() { return QStringLiteral("update/lastCheck"); } // untranslated: a settings key
QString skippedKey() { return QStringLiteral("update/skipped"); } // untranslated: a settings key

Updater* g_active = nullptr;

class QtNetwork : public Network
{
public:
    explicit QtNetwork(QObject* parent)
        : m_manager(new QNetworkAccessManager(parent))
    {
    }

    std::function<void()> get(const QUrl& url, qint64 cap, QIODevice* sink, Progress progress,
                              Done done) override
    {
        QNetworkRequest request(url);
        // GitHub's API refuses a request with no User-Agent. A generic one:
        // the version, and nothing about the player.
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("GamesHub/" GAMESHUB_VERSION)); // untranslated: an HTTP header
        request.setRawHeader("Accept",                      // untranslated: an HTTP header
                             "application/vnd.github+json"); // untranslated: an HTTP header
        request.setTransferTimeout(kTimeoutMs);
        // Asset downloads redirect from github.com to GitHub's asset host; an
        // https address may redirect only to another https one.
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* reply = m_manager->get(request);

        struct State {
            QByteArray body;
            qint64 received = 0;
            bool failed = false;
            bool reported = false;
        };
        auto state = std::make_shared<State>();

        QObject::connect(reply, &QNetworkReply::readyRead, reply, [reply, state, cap, sink] {
            const QByteArray data = reply->readAll();
            state->received += data.size();
            if (state->received > cap) {
                state->failed = true;
                reply->abort();
                return;
            }
            if (sink != nullptr) {
                if (sink->write(data) != data.size()) {
                    state->failed = true;
                    reply->abort();
                }
            } else {
                state->body += data;
            }
        });
        if (progress)
            QObject::connect(reply, &QNetworkReply::downloadProgress, reply, progress);
        QObject::connect(reply, &QNetworkReply::finished, reply, [reply, state, done] {
            if (state->reported)
                return;
            state->reported = true;
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            Reply r;
            r.ok = !state->failed && reply->error() == QNetworkReply::NoError && status == 200;
            if (r.ok)
                r.body = std::move(state->body);
            reply->deleteLater();
            done(r);
        });
        return [reply = QPointer<QNetworkReply>(reply)] {
            if (reply)
                reply->abort();
        };
    }

private:
    QNetworkAccessManager* m_manager;
};

} // namespace

std::unique_ptr<Network> makeNetwork(QObject* parent)
{
    return std::make_unique<QtNetwork>(parent);
}

Updater::Updater(Install install, QString appImage, std::span<const unsigned char> key,
                 Network& network, QWidget* window, QObject* parent)
    : QObject(parent)
    , m_install(install)
    , m_appImage(std::move(appImage))
    , m_key(key)
    , m_network(network)
    , m_window(window)
{
    g_active = this;

    askConsent = [this] { return askToCheckDaily(m_window); };
    present = [this](const Offer& offer) {
        UpdateDialog dialog(offer, m_install, m_window);
        dialog.exec();
        return dialog.choice();
    };
    tell = [this](const QString& text, const QUrl& page) { tellPlayer(m_window, text, page); };
    clock = [] { return QDateTime::currentDateTimeUtc(); };
    relaunch = [this](const QString& image) { return defaultRelaunch(image); };

    m_checkAction = new QAction(tr("Check for updates…"), this);
    m_checkAction->setObjectName(QStringLiteral("checkForUpdates"));
    m_checkAction->setStatusTip(tr("Ask whether a newer version of Games is out"));
    connect(m_checkAction, &QAction::triggered, this, &Updater::checkNow);
}

Updater::~Updater()
{
    if (g_active == this)
        g_active = nullptr;
    // Aborting finishes the request, and its callback must not then act on an
    // updater halfway through being destroyed.
    m_dying = true;
    if (m_cancel)
        m_cancel();
}

Updater* Updater::active()
{
    return g_active;
}

bool Updater::automatic() const
{
    return QSettings().value(autoKey(), false).toBool();
}

void Updater::setAutomatic(bool on)
{
    QSettings().setValue(autoKey(), on);
}

void Updater::start()
{
    if (m_install == Install::Inert)
        return;
    // Deferred to the event loop so the window is up and painted first: a
    // question over a blank frame reads as a startup error. Being modal, it
    // also keeps Help -> Check for updates out of reach until it is answered.
    QTimer::singleShot(0, this, &Updater::askThenCheck);
}

void Updater::askThenCheck()
{
    if (!QSettings().contains(autoKey()))
        setAutomatic(askConsent());
    automaticCheckIfDue();
}

void Updater::automaticCheckIfDue()
{
    if (!automatic())
        return;
    QSettings settings;
    const QDateTime now = clock();
    if (!dailyCheckDue(settings.value(lastCheckKey()).toString(), now))
        return;
    // Written BEFORE the request, so a check that fails still counts and an
    // offline machine does not retry on every launch (INV-7).
    settings.setValue(lastCheckKey(), stampFor(now));
    runCheck(true);
}

void Updater::checkNow()
{
    runCheck(false);
}

void Updater::runCheck(bool automatic)
{
    if (m_install == Install::Inert || m_busy)
        return;
    m_busy = true;
    m_cancel = m_network.get(QUrl(QString::fromLatin1(kReleasesUrl)), kListCap, nullptr, {},
                             [self = QPointer<Updater>(this), automatic](const Network::Reply& reply) {
                                 if (self && !self->m_dying)
                                     self->onReleases(reply, automatic);
                             });
}

void Updater::onReleases(const Network::Reply& reply, bool automatic)
{
    m_cancel = nullptr;
    const std::optional<QList<Release>> releases
        = reply.ok ? parseReleases(reply.body) : std::nullopt;
    if (!releases) {
        m_busy = false;
        // An automatic check that fails says nothing: the player did not ask.
        if (!automatic)
            tell(tr("The update server could not be reached."), {});
        emit finished();
        return;
    }

    // A build whose own version does not parse offers nothing, rather than
    // treating every release as newer than a version it cannot read.
    const std::optional<Version> running = parseVersion(QStringLiteral(GAMESHUB_VERSION));
    const std::optional<Offer> offer
        = running ? chooseOffer(*releases, *running, m_install,
                                QSettings().value(skippedKey()).toString(), automatic)
                  : std::nullopt;
    if (!offer) {
        m_busy = false;
        if (!automatic)
            tell(tr("You have the newest version, %1.").arg(QStringLiteral(GAMESHUB_VERSION)), {});
        emit finished();
        return;
    }

    switch (present(*offer)) {
    case Choice::Install:
        if (m_install == Install::AppImage) {
            install(*offer); // stays busy until the install ends
            return;
        }
        openPage(*offer);
        break;
    case Choice::Skip:
        QSettings().setValue(skippedKey(), offer->newest.version.toString());
        break;
    case Choice::Later:
        break;
    }
    m_busy = false;
    emit finished();
}

void Updater::openPage(const Offer& offer)
{
    if (isGitHubPage(offer.newest.page))
        QDesktopServices::openUrl(offer.newest.page);
}

// § 4.8. Each step stops the install on failure; nothing replaces $APPIMAGE
// until the downloaded bytes have been checked against the compiled-in key.
void Updater::install(const Offer& offer)
{
    m_signature.clear();
    m_cancel = m_network.get(offer.signature.url, kSignatureCap, nullptr, {},
                             [self = QPointer<Updater>(this), offer](const Network::Reply& reply) {
                                 if (self && !self->m_dying)
                                     self->onSignature(reply, offer);
                             });
}

void Updater::onSignature(const Network::Reply& reply, const Offer& offer)
{
    m_cancel = nullptr;
    if (!reply.ok || reply.body.size() != 64) {
        failInstall(tr("The update could not be downloaded, so nothing was changed."), offer);
        return;
    }
    m_signature = reply.body;

    // Beside $APPIMAGE, so the final rename is on one filesystem and atomic.
    // A directory the player cannot write to fails here, before any download.
    const QString dir = QFileInfo(m_appImage).absolutePath();
    m_download = std::make_unique<QTemporaryFile>(
        dir + QStringLiteral("/.GamesHub-update-XXXXXX")); // untranslated: a file name
    if (!m_download->open()) {
        m_download.reset();
        failInstall(tr("Games cannot write to the folder it is kept in, so it cannot update "
                       "itself there."),
                    offer);
        return;
    }

    if (m_window) {
        m_progress = new QProgressDialog(tr("Downloading Games %1…")
                                             .arg(offer.newest.version.toString()),
                                         tr("Cancel"), 0, 100, m_window);
        m_progress->setObjectName(QStringLiteral("updateProgress"));
        m_progress->setWindowModality(Qt::WindowModal);
        m_progress->setMinimumDuration(0);
        m_progress->setAutoClose(false);
        m_progress->setAutoReset(false);
        connect(m_progress, &QProgressDialog::canceled, this, [this] {
            if (m_cancel)
                m_cancel();
        });
    }

    QPointer<QProgressDialog> progress = m_progress;
    m_cancel = m_network.get(
        offer.file.url, kDownloadCap, m_download.get(),
        [progress](qint64 received, qint64 total) {
            if (progress && total > 0)
                progress->setValue(int(received * 100 / total));
        },
        [self = QPointer<Updater>(this), offer](const Network::Reply& reply) {
            if (self && !self->m_dying)
                self->onDownloaded(reply, offer);
        });
}

void Updater::onDownloaded(const Network::Reply& reply, const Offer& offer)
{
    m_cancel = nullptr;
    if (m_progress) {
        m_progress->deleteLater();
        m_progress = nullptr;
    }
    if (!reply.ok || !m_download->flush() || !m_download->seek(0)) {
        m_download.reset(); // removes the partial file
        failInstall(tr("The update could not be downloaded, so nothing was changed."), offer);
        return;
    }
    if (!signatureMatches(*m_download, m_signature, m_key)) {
        m_download.reset();
        failInstall(tr("The download could not be verified as this project's own, so it was "
                       "deleted and nothing was changed."),
                    {});
        return;
    }

    m_download->setPermissions(m_download->permissions() | QFileDevice::ExeOwner
                               | QFileDevice::ExeGroup | QFileDevice::ExeOther
                               | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    const QString from = m_download->fileName();
    m_download->close();
    // std::filesystem rather than QFile::rename, which refuses to replace an
    // existing file. On one filesystem this is rename(2): the old image is
    // swapped for the new one in a single step, never half-written.
    std::error_code error;
    std::filesystem::rename(std::filesystem::path(from.toStdString()),
                            std::filesystem::path(m_appImage.toStdString()), error);
    if (error) {
        m_download.reset();
        failInstall(tr("The new version could not be put in place, so nothing was changed."),
                    offer);
        return;
    }
    m_download->setAutoRemove(false); // it is $APPIMAGE now
    m_download.reset();

    m_busy = false;
    emit finished();
    if (!relaunch(m_appImage))
        tell(tr("The new version is in place. Close Games and open it again to use it."), {});
}

void Updater::failInstall(const QString& text, const Offer& offer)
{
    m_busy = false;
    tell(text, offer.newest.page);
    emit finished();
}

// § 4.8 step 4. Not a re-exec: the running image stays mounted until this
// process has gone, and a new one started over it dies -- the trap finbreak
// found in the field. A shell in its own session waits for this PID to
// disappear, then starts the new image without the old one's APPIMAGE,
// APPDIR and ARGV0. Closing the window is the normal exit, which is what
// saves the game in progress (HubWindow::closeEvent).
bool Updater::defaultRelaunch(const QString& appImage)
{
    QProcess waiter;
    waiter.setProgram(QStringLiteral("/bin/sh")); // untranslated: a program
    waiter.setArguments({ QStringLiteral("-c"),   // untranslated: a shell flag
                          QStringLiteral("while kill -0 \"$1\" 2>/dev/null; do sleep 0.2; done; " // untranslated: shell
                                         "exec \"$2\""), // untranslated: shell
                          QStringLiteral("gameshub-relaunch"), // untranslated: the waiter's $0
                          QString::number(QCoreApplication::applicationPid()), appImage });
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("APPIMAGE"));
    env.remove(QStringLiteral("APPDIR"));
    env.remove(QStringLiteral("ARGV0"));
    waiter.setProcessEnvironment(env);
    waiter.setWorkingDirectory(QDir::homePath());
// Unix only: QProcess has no such call on Windows, where nothing relaunches
// (the zip opens the download page instead) but this still has to compile.
#if defined(Q_OS_UNIX) && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    waiter.setUnixProcessParameters(QProcess::UnixProcessFlag::CreateNewSession);
#endif
    if (!waiter.startDetached())
        return false;
    QTimer::singleShot(0, qApp, [window = m_window] {
        // A window that refuses to close keeps this process, and the waiter
        // with it, alive; the new image then starts when the player quits.
        if (!window || window->close())
            QCoreApplication::quit();
    });
    return true;
}

} // namespace update
