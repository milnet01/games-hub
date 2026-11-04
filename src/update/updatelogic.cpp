#include "update/updatelogic.h"

#include "update/ed25519.h"

#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

namespace update {

QString Version::toString() const
{
    return QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch);
}

std::optional<Version> parseVersion(const QString& text)
{
    // No leading zeros beyond a lone 0, and no sign: "01.2.3" is not a tag
    // release.yml would make, so it is not one this reads.
    static const QRegularExpression shape(
        QStringLiteral("^(0|[1-9][0-9]{0,5})\\.(0|[1-9][0-9]{0,5})\\.(0|[1-9][0-9]{0,5})$"));
    const QRegularExpressionMatch m = shape.match(text);
    if (!m.hasMatch())
        return std::nullopt;
    return Version { m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt() };
}

std::optional<Version> parseTag(const QString& tag)
{
    if (!tag.startsWith(QLatin1Char('v')))
        return std::nullopt;
    return parseVersion(tag.mid(1));
}

Install detectInstall(Platform platform, const QString& appImage, const QString& appDir,
                      const QString& executablePath, std::span<const unsigned char> key)
{
    if (key.size() != 32)
        return Install::Inert;
    if (platform == Platform::Windows)
        return Install::Zip;
    if (platform != Platform::Linux || appImage.isEmpty() || appDir.isEmpty())
        return Install::Inert;

    const QFileInfo image(appImage);
    if (!image.isFile())
        return Install::Inert;
    // The running program must be the one inside the mounted image. A build
    // directory with $APPIMAGE left set in its shell is not an AppImage, and
    // overwriting the file that variable names would replace something this
    // process did not come from.
    const QString root = QDir::cleanPath(appDir) + QLatin1Char('/');
    const QString self = QDir::cleanPath(executablePath);
    return self.startsWith(root) ? Install::AppImage : Install::Inert;
}

std::optional<QList<Release>> parseReleases(const QByteArray& json)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray())
        return std::nullopt;

    QList<Release> releases;
    for (const QJsonValue value : doc.array()) {
        const QJsonObject o = value.toObject();
        if (o.value(QStringLiteral("draft")).toBool(true)
            || o.value(QStringLiteral("prerelease")).toBool(true))
            continue;
        const std::optional<Version> version
            = parseTag(o.value(QStringLiteral("tag_name")).toString());
        if (!version)
            continue;

        Release r;
        r.version = *version;
        r.title = o.value(QStringLiteral("name")).toString();
        r.notes = o.value(QStringLiteral("body")).toString();
        r.page = QUrl(o.value(QStringLiteral("html_url")).toString());
        for (const QJsonValue a : o.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = a.toObject();
            const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
            if (url.scheme() != QLatin1String("https")) // untranslated: a URL scheme
                continue;
            r.assets.append({ asset.value(QStringLiteral("name")).toString(), url });
        }
        releases.append(r);
    }
    std::stable_sort(releases.begin(), releases.end(),
                     [](const Release& a, const Release& b) { return a.version > b.version; });
    return releases;
}

QString assetNameFor(Install install, const Version& version)
{
    switch (install) {
    case Install::AppImage:
        return QStringLiteral("GamesHub-%1-x86_64.AppImage").arg(version.toString()); // untranslated: a release file name
    case Install::Zip:
        return QStringLiteral("GamesHub-%1-windows-x64.zip").arg(version.toString()); // untranslated: a release file name
    case Install::Inert:
        break;
    }
    return {};
}

std::optional<Offer> chooseOffer(const QList<Release>& releases, const Version& running,
                                 Install install, const QString& skipped, bool automatic)
{
    if (install == Install::Inert || releases.isEmpty())
        return std::nullopt;

    const Release* newest = &releases.first();
    for (const Release& r : releases)
        if (r.version > newest->version)
            newest = &r;
    if (!(newest->version > running))
        return std::nullopt;
    if (automatic && newest->version.toString() == skipped)
        return std::nullopt;

    const QString fileName = assetNameFor(install, newest->version);
    const QString sigName = fileName + QStringLiteral(".sig"); // untranslated: a release file name
    Offer offer;
    offer.newest = *newest;
    bool haveFile = false;
    bool haveSig = false;
    for (const Asset& a : newest->assets) {
        if (a.name == fileName) {
            offer.file = a;
            haveFile = true;
        } else if (a.name == sigName) {
            offer.signature = a;
            haveSig = true;
        }
    }
    // Unsigned is not offered, on either platform: the Windows page-opener
    // installs nothing, but a release missing its .sig is one release.yml did
    // not finish, and pointing a player at it is pointing them at that.
    if (!haveFile || !haveSig)
        return std::nullopt;

    for (const Release& r : releases)
        if (r.version > running)
            offer.missed.append(r);
    std::stable_sort(offer.missed.begin(), offer.missed.end(),
                     [](const Release& a, const Release& b) { return a.version > b.version; });
    return offer;
}

QString notesMarkdown(const QList<Release>& missed)
{
    QString out;
    for (const Release& r : missed) {
        const QString version = r.version.toString();
        // The title release.yml writes already leads with the version
        // ("1.4.0 — Theme"), so it is used whole; a bare one gets it added.
        QString heading = r.title.trimmed();
        if (!heading.contains(version))
            heading = heading.isEmpty() ? version : version + QStringLiteral(" — ") + heading;
        out += QStringLiteral("## ") + heading + QStringLiteral("\n\n") + r.notes.trimmed()
            + QStringLiteral("\n\n");
    }
    return out.trimmed();
}

bool dailyCheckDue(const QString& lastAttempt, const QDateTime& now)
{
    const QDateTime last = QDateTime::fromString(lastAttempt, Qt::ISODate);
    if (!last.isValid() || last > now)
        return true;
    return last.secsTo(now) >= qint64(24) * 60 * 60;
}

QString stampFor(const QDateTime& when)
{
    return when.toUTC().toString(Qt::ISODate);
}

namespace {

std::span<const unsigned char> bytesOf(const QByteArray& a)
{
    return { reinterpret_cast<const unsigned char*>(a.constData()),
             static_cast<std::size_t>(a.size()) };
}

} // namespace

bool signatureMatches(QIODevice& device, const QByteArray& signature,
                      std::span<const unsigned char> key)
{
    Ed25519Check check(key, bytesOf(signature));
    QByteArray chunk(qsizetype(64) * 1024, Qt::Uninitialized);
    for (;;) {
        const qint64 got = device.read(chunk.data(), chunk.size());
        if (got < 0)
            return false;
        if (got == 0)
            break;
        check.addData(reinterpret_cast<const unsigned char*>(chunk.constData()),
                      static_cast<std::size_t>(got));
    }
    return check.verified();
}

bool signatureMatches(const QByteArray& bytes, const QByteArray& signature,
                      std::span<const unsigned char> key)
{
    Ed25519Check check(key, bytesOf(signature));
    check.addData(reinterpret_cast<const unsigned char*>(bytes.constData()),
                  static_cast<std::size_t>(bytes.size()));
    return check.verified();
}

bool isGitHubPage(const QUrl& url)
{
    return url.isValid() && url.scheme() == QLatin1String("https") // untranslated: a URL scheme
        && url.host() == QLatin1String("github.com") && url.userInfo().isEmpty() // untranslated: a host name
        && url.port() == -1;
}

} // namespace update
