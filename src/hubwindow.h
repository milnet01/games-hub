#pragma once

#include <QMainWindow>
#include <QList>
#include <QSize>

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
    // The README's opening promise — "sized to sit beside whatever you are
    // actually working on" — as a number, because a promise nothing can judge
    // is worse than no promise. Half of a 1920x1080 desktop across, and its
    // full height less a panel and a title bar: the window has to fit there
    // beside a browser or an editor without being dragged smaller.
    //
    // It binds the MINIMUM size of every page, not just the default. A game
    // that cannot be shrunk below 1200 across keeps the promise on first run
    // and breaks it the moment the player tries to make room, which is exactly
    // when the promise matters. Canasta's legibility pass sits closest to it
    // (900x656 with large play on), so this is a live constraint rather than a
    // comfortable one — raising a minimum past it is the thing to notice.
    static constexpr QSize kFitsBesideYourWork { 960, 1000 };

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
    // Window size and position are kept per page, so the hub and each game come
    // back the size you last left them.
    GameView* currentView() const;
    void rememberPage();
    void applyPageGeometry(const QString& page);
    // A game's position, kept between sessions for the games that offer one.
    void storeSave(const Entry& e) const;

protected:
    void closeEvent(QCloseEvent* event) override;

private:

    QStackedWidget* m_stack = nullptr;
    QWidget* m_menuPage = nullptr;
    // The scroller wrapping m_menuPage, and the widget the STACK actually holds.
    // Anything switching pages names this one — m_menuPage is a grandchild of the
    // stack, and QStackedWidget::setCurrentWidget() on a widget it does not hold
    // silently does nothing, which reads as a dead "All Games" button.
    QWidget* m_menuHost = nullptr;
    QToolBar* m_toolBar = nullptr;
    QAction* m_backAction = nullptr;
    QAction* m_soundAction = nullptr;
    QAction* m_legibilityAction = nullptr;
    // Marks where game actions are inserted, keeping the sound toggle last.
    QAction* m_soundSeparator = nullptr;
    QLabel* m_status = nullptr;
    QList<QAction*> m_gameActions;
    QList<Entry> m_entries;
    // Which page's geometry is currently on screen: a game's name, or empty
    // for the tile menu.
    QString m_page;
    bool m_geometryReady = false;
};
