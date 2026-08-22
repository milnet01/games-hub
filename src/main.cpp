#include "donate.h"
#include "donatedialog.h"
#include "hubwindow.h"
#include "legibility.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QPixmap>
#include <QTimer>

#include <cstdio>
#include <string_view>

namespace {

// "1400x620" to a size, and anything else to an invalid one. Deliberately
// strict: a size that was meant and misread is worse than one refused, because
// the picture still gets written and still looks plausible.
QSize parseSize(const QString& text)
{
    const qsizetype cross = text.indexOf(QLatin1Char('x'), 0, Qt::CaseInsensitive);
    if (cross <= 0)
        return {};
    bool wideOk = false;
    bool tallOk = false;
    const int wide = text.left(cross).toInt(&wideOk);
    const int tall = text.mid(cross + 1).toInt(&tallOk);
    if (!wideOk || !tallOk || wide <= 0 || tall <= 0)
        return {};
    return { wide, tall };
}

// Renders one game to a file and returns a process exit status. Offscreen is
// the caller's business (QT_QPA_PLATFORM=offscreen), which is what lets this
// run over SSH, on a machine with no compositor, or on a CI runner.
// Errors go to stderr directly rather than through qWarning, for the reason
// --version goes through argv: this is a command-line tool answering a command
// line, and Qt's logging can be turned off by an environment variable nobody
// here set deliberately. A failure that exits 2 in silence is worse than none.
int takeShot(HubWindow& window, const QString& path, const QString& size, bool legible,
             bool gameNamed, bool gameFound)
{
    // A shot of the wrong thing is the failure mode this whole flag exists to
    // avoid: the picture still gets written, and still looks like an answer.
    // Playing on falls back to the tile grid on purpose; photographing does not.
    if (gameNamed && !gameFound) {
        std::fprintf(stderr, "No such game, so there is nothing to photograph.\n");
        return 2;
    }

    // Restored before returning: a screenshot is a question about the app, and
    // answering it should not change what the player chose.
    const bool wasLegible = Legibility::instance().enabled();
    if (legible)
        Legibility::instance().setEnabled(true);

    const QSize wanted = size.isEmpty() ? window.sizeHint() : parseSize(size);
    if (!wanted.isValid()) {
        std::fprintf(stderr, "--size wants WxH, for example 1400x620. Got \"%s\".\n",
                     qPrintable(size));
        Legibility::instance().setEnabled(wasLegible);
        return 2;
    }

    window.resize(wanted);
    window.show();
    // Twice, and not for luck: the first pass builds and activates the layouts
    // a game was constructed into, and the geometry a view paints from is only
    // right once that has happened.
    for (int i = 0; i < 2; ++i)
        QApplication::processEvents();

    const QPixmap shot = window.grab();
    const bool saved = shot.save(path);
    Legibility::instance().setEnabled(wasLegible);

    if (!saved) {
        std::fprintf(stderr, "Could not write \"%s\".\n", qPrintable(path));
        return 1;
    }
    std::printf("%s %dx%d%s\n", qPrintable(path), shot.width(), shot.height(),
                legible ? " (large play)" : "");
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    // --version is answered here, before Qt starts, and the reason is Windows.
    // qt_add_executable sets WIN32_EXECUTABLE, so the release build is a
    // GUI-subsystem binary; QCommandLineParser would put the version in a
    // message box there rather than on stdout, and a packaging check that
    // asks the artifact its version would hang waiting for a dialog. Going
    // through argv also means --version needs no display and no platform
    // plugin on any system, which is what makes it usable as a smoke test.
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--version" || arg == "-v") {
            std::printf("Games %s\n", GAMESHUB_VERSION);
            return 0;
        }
    }

    QApplication app(argc, argv);
    // QSettings derives its file from these, so best scores land in a stable
    // place across launches.
    app.setOrganizationName(QStringLiteral("GamesHub"));
    app.setApplicationName(QStringLiteral("Games"));
    app.setApplicationDisplayName(QStringLiteral("Games"));
    app.setApplicationVersion(QStringLiteral(GAMESHUB_VERSION));

    // Ties the running window to the .desktop file, so a panel launcher shows
    // this window's icon instead of a generic placeholder. On Wayland this is
    // what sets the app id.
    QGuiApplication::setDesktopFileName(QStringLiteral("gameshub"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("gameshub")));

    // The hub sizes itself: each page's size and position are remembered, and
    // a first run with nothing remembered gets a sensible default.
    HubWindow window;

    // `--game hearts` opens straight into one game, so a launcher or shortcut
    // can point at a single title rather than the menu.
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("A small collection of desktop games."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption gameOption(
        { QStringLiteral("g"), QStringLiteral("game") },
        QStringLiteral("Open a game directly: %1.").arg(window.gameNames().join(QStringLiteral(", "))),
        QStringLiteral("name"));
    parser.addOption(gameOption);

    // Nothing else here can see a game. Twelve legibility passes shipped and
    // were checked by the owner's eye, because a layout question otherwise has
    // to be answered as arithmetic about rectangles — which is how a caption
    // came to be printed on top of the card you had just played (GHUB-0084).
    const QCommandLineOption shotOption(
        QStringLiteral("shot"),
        QStringLiteral("Write a picture of the game to <file> and exit, without playing it."),
        QStringLiteral("file"));
    const QCommandLineOption sizeOption(
        QStringLiteral("size"),
        QStringLiteral("Window size for --shot, as WxH. Defaults to the size the app would open at."),
        QStringLiteral("WxH"));
    const QCommandLineOption legibleOption(
        QStringLiteral("legible"),
        QStringLiteral("Turn large play on for --shot only, leaving the stored setting alone."));
    parser.addOption(shotOption);
    parser.addOption(sizeOption);
    parser.addOption(legibleOption);
    parser.process(app);

    const bool named = parser.isSet(gameOption);
    const bool found = named && window.openGameNamed(parser.value(gameOption));
    if (named && !found) {
        qWarning("No game called \"%s\". Known games: %s",
                 qPrintable(parser.value(gameOption)),
                 qPrintable(window.gameNames().join(QStringLiteral(", "))));
    }

    // Before the launch is counted, for the reason --help and --version are:
    // taking a picture is not playing, and it should not walk anybody towards
    // a prompt they did not earn.
    if (parser.isSet(shotOption)) {
        return takeShot(window, parser.value(shotOption), parser.value(sizeOption),
                        parser.isSet(legibleOption), named, found);
    }

    // Counted here rather than before the parser, so `--help` and `--version`
    // are not launches: neither one plays anything, and both would otherwise
    // walk the counter towards a prompt nobody earned.
    const bool owesDonatePrompt = donate::recordLaunchAndAsk();

    window.show();

    // Never over a game in progress. `--game` goes straight into play, and a
    // prompt landing on someone's turn is a different thing from one at the
    // tile grid. Deferred to the event loop so the window is up and painted
    // first — a dialog over a blank frame reads as a startup error.
    if (owesDonatePrompt && !named) {
        QTimer::singleShot(0, &window, [&window] {
            DonateDialog dialog(true, &window);
            dialog.exec();
        });
    }

    return app.exec();
}
