#include "hubwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>

int main(int argc, char* argv[])
{
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

    HubWindow window;
    window.resize(880, 680);

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
