#pragma once

// The self-updater's decisions, with no network, no widgets and no stored
// state (GHUB-0043 § 4.1). Everything that varies -- the running version, the
// clock, the stored update/* values, the platform, the environment, the
// executable's path, the key -- arrives as an argument, which is what lets the
// self-test drive every rule here.

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QUrl>

#include <compare>
#include <optional>
#include <span>

class QIODevice;

namespace update {

// Three decimal numbers and nothing else: what a release tag carries after
// its "v". A pre-release suffix is not a version here, so it is never offered.
struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;

    auto operator<=>(const Version&) const = default;
    QString toString() const;
};

// "1.4.0" -> 1.4.0. Anything else, including "v1.4.0", "1.4" and
// "1.4.0-rc.1", is nullopt rather than a guess.
std::optional<Version> parseVersion(const QString& text);
// "v1.4.0" -> 1.4.0, the same strictness after the "v".
std::optional<Version> parseTag(const QString& tag);

// Which kind of install this is, which decides whether the updater exists at
// all (§ 4.4).
enum class Install {
    Inert,    // no check, no menu entry, no setting
    AppImage, // check, offer, install
    Zip,      // check, offer, open the download page
};

enum class Platform { Linux, Windows, Other };

// § 4.4's table. appImage and appDir are $APPIMAGE and $APPDIR as read, empty
// when unset. A key of any size but 32 makes every install inert.
Install detectInstall(Platform platform, const QString& appImage, const QString& appDir,
                      const QString& executablePath, std::span<const unsigned char> key);

struct Asset {
    QString name;
    QUrl url;
};

struct Release {
    Version version;
    QString title;
    QString notes;
    // The release's page on GitHub, for the download-page button.
    QUrl page;
    QList<Asset> assets;
};

// The usable releases in the GitHub API's list, newest version first: drafts,
// pre-releases and any tag that is not "v" and three numbers are left out.
// nullopt when the reply is not a JSON array, so a garbled reply reads as a
// failure rather than as "no releases".
std::optional<QList<Release>> parseReleases(const QByteArray& json);

// The file an install of this kind downloads for a version, as release.yml
// names it. Empty for Inert.
QString assetNameFor(Install install, const Version& version);

struct Offer {
    Release newest;
    Asset file;
    Asset signature;
    // Every release newer than the running one, newest first (§ 4.6).
    QList<Release> missed;
};

// § 4.6: the newest release, when it is strictly newer than `running`, carries
// this install's file and its .sig, and -- on an automatic check only -- is
// not the version the player skipped.
std::optional<Offer> chooseOffer(const QList<Release>& releases, const Version& running,
                                 Install install, const QString& skipped, bool automatic);

// The missed releases' notes as one Markdown document, newest first, each
// under its version and title.
QString notesMarkdown(const QList<Release>& missed);

// INV-7: an automatic check is due when there has been no attempt, or the last
// one was 24 hours or more before `now`. lastAttempt is update/lastCheck as
// stored. A stamp in the future -- the clock was set back -- counts as due,
// so a wrong clock cannot stop checks for good.
bool dailyCheckDue(const QString& lastAttempt, const QDateTime& now);
// The form update/lastCheck is written in.
QString stampFor(const QDateTime& when);

// Reads `device` to its end and reports whether `signature` is the key's
// Ed25519 signature over exactly those bytes. False on a read error.
bool signatureMatches(QIODevice& device, const QByteArray& signature,
                      std::span<const unsigned char> key);
bool signatureMatches(const QByteArray& bytes, const QByteArray& signature,
                      std::span<const unsigned char> key);

// The download page may be opened only when it is GitHub's (§ 4.7).
bool isGitHubPage(const QUrl& url);

} // namespace update
