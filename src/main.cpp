#include "hubwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>

#include <cstdio>
#include <string_view>

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
    parser.process(app);

    if (parser.isSet(gameOption) && !window.openGameNamed(parser.value(gameOption))) {
        qWarning("No game called \"%s\". Known games: %s",
                 qPrintable(parser.value(gameOption)),
                 qPrintable(window.gameNames().join(QStringLiteral(", "))));
    }

    window.show();

    return app.exec();
}
