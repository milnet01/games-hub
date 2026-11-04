#pragma once

// The self-updater's networked half (GHUB-0043): the first-run question, the
// check, the offer, and on the Linux AppImage the download, the signature
// check, the swap and the relaunch. Built only with GAMESHUB_UPDATER, and
// constructed only by main() for an install updatelogic.h's detectInstall()
// says may update -- every other build and every test but its own makes none.

#include "update/updatelogic.h"

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <functional>
#include <memory>
#include <span>

class QAction;
class QIODevice;
class QProgressDialog;
class QTemporaryFile;
class QWidget;

namespace update {

// The network, as the updater sees it. One real implementation; the UI test
// puts a counting fake in its place, which is what proves INV-1.
class Network
{
public:
    struct Reply {
        bool ok = false;
        QByteArray body; // empty when a sink was given
    };
    using Progress = std::function<void(qint64 received, qint64 total)>;
    using Done = std::function<void(Reply)>;

    virtual ~Network() = default;
    // One HTTPS GET. A reply over `cap` bytes, a status other than 200, or no
    // answer within 30 seconds is a failure. With `sink` the body is written
    // there as it arrives rather than returned. `done` is called exactly once,
    // and may be called before get() returns. The result cancels the request,
    // which then finishes as a failure.
    virtual std::function<void()> get(const QUrl& url, qint64 cap, QIODevice* sink,
                                      Progress progress, Done done)
        = 0;
};

// The real network, through QNetworkAccessManager.
std::unique_ptr<Network> makeNetwork(QObject* parent);

class Updater : public QObject
{
    Q_OBJECT

public:
    enum class Choice { Install, Later, Skip };

    // `appImage` is $APPIMAGE for an AppImage install, otherwise unused.
    // `window` is the hub, which dialogs sit over and which is closed -- the
    // normal way, so the game in progress is saved -- before a relaunch.
    Updater(Install install, QString appImage, std::span<const unsigned char> key,
            Network& network, QWidget* window, QObject* parent = nullptr);
    ~Updater() override;

    // The one updater in this process, or null. Preferences asks it whether
    // to show its switch.
    static Updater* active();

    // Called by main() once the window is up: asks the first-run question if
    // it has not been answered, then makes the day's automatic check if one
    // is due. Nothing reaches the network before this.
    void start();
    // Help -> Check for updates: always checks, skipped version included.
    void checkNow();
    QAction* checkAction() const { return m_checkAction; }

    // update/auto: absent until the first-run question is answered.
    bool automatic() const;
    void setAutomatic(bool on);

    // What the updater shows and does, replaceable so the UI test can run it
    // without a modal dialog or a relaunch. The defaults are the real ones.
    std::function<bool()> askConsent;
    std::function<Choice(const Offer&)> present;
    // A one-sentence message; `page`, when valid, adds a button opening it.
    std::function<void(const QString& text, const QUrl& page)> tell;
    std::function<QDateTime()> clock;
    // Starts the new image once this process has gone, and closes the window.
    // False when the waiter could not start.
    std::function<bool(const QString& appImage)> relaunch;

signals:
    // A check or an install has come to an end, however it ended.
    void finished();

private:
    void askThenCheck();
    void automaticCheckIfDue();
    void runCheck(bool automatic);
    void onReleases(const Network::Reply& reply, bool automatic);
    void openPage(const Offer& offer);
    void install(const Offer& offer);
    void onSignature(const Network::Reply& reply, const Offer& offer);
    void onDownloaded(const Network::Reply& reply, const Offer& offer);
    void failInstall(const QString& text, const Offer& offer);
    bool defaultRelaunch(const QString& appImage);

    Install m_install;
    QString m_appImage;
    std::span<const unsigned char> m_key;
    Network& m_network;
    QPointer<QWidget> m_window;
    QAction* m_checkAction = nullptr;
    bool m_busy = false;
    bool m_dying = false;
    QByteArray m_signature;
    std::unique_ptr<QTemporaryFile> m_download;
    QPointer<QProgressDialog> m_progress;
    std::function<void()> m_cancel;
};

} // namespace update
