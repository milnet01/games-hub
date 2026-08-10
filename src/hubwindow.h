#pragma once

#include <QMainWindow>
#include <QList>

#include <functional>

class GameView;
class QStackedWidget;
class QToolBar;
class QLabel;

// The hub: a grid of game tiles, plus one page per game. Games are built the
// first time they are opened, so starting the hub costs nothing but the tiles.
class HubWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit HubWindow(QWidget* parent = nullptr);

    // Opens a game by name (case-insensitive), for `gameshub --game hearts`.
    // Returns false if no game matches, leaving the menu on screen.
    bool openGameNamed(const QString& name);

    QStringList gameNames() const;

private:
    struct Entry {
        QString name;
        QString blurb;
        // Drawn on the tile; each game gets a recognisable miniature rather
        // than a generic placeholder.
        std::function<void(QPainter&, const QRectF&)> paintTile;
        std::function<GameView*()> create;
        GameView* view = nullptr;
        int pageIndex = -1;
    };

    void buildEntries();
    void buildChrome();
    void openGame(int index);
    void showMenu();
    void setGameActions(GameView* view);

    QStackedWidget* m_stack = nullptr;
    QWidget* m_menuPage = nullptr;
    QToolBar* m_toolBar = nullptr;
    QAction* m_backAction = nullptr;
    QAction* m_soundAction = nullptr;
    // Marks where game actions are inserted, keeping the sound toggle last.
    QAction* m_soundSeparator = nullptr;
    QLabel* m_status = nullptr;
    QList<QAction*> m_gameActions;
    QList<Entry> m_entries;
};
