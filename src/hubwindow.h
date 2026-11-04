#pragma once

#include <QMainWindow>
#include <QList>
#include <QSize>
#include <QStringList>

#include <functional>

class GameView;
class QStackedWidget;
class QToolBar;
#include <QHash>
#include <QSizePolicy>

class QButtonGroup;
class QGridLayout;
class QLabel;
class QMenu;
class QPushButton;
class QTimer;
class QSettings;

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
    // (908x656 with large play on), so this is a live constraint rather than a
    // comfortable one — raising a minimum past it is the thing to notice.
    static constexpr QSize kFitsBesideYourWork { 960, 1000 };

    explicit HubWindow(QWidget* parent = nullptr);

    // Opens a game by name (case-insensitive), for `gameshub --game hearts`.
    // Returns false if no game matches, leaving the menu on screen.
    bool openGameNamed(const QString& name);

    // Off while the app is answering a question about itself rather than being
    // played -- --shot. Nothing is then written to the settings store: opening
    // a game to photograph it goes through rememberPage(), so even a REFUSED
    // shot used to leave the player's remembered window size overwritten.
    void setRemembering(bool on) { m_remembering = on; }

    QStringList gameNames() const;

    // Adds an entry to the Help menu, under How to play. For main() to hand
    // in what the hub does not build itself -- Check for updates, in the
    // downloads that carry an updater (GHUB-0043).
    void addHelpAction(QAction* action);

private:
    // The kind of game, which the filter row narrows the grid by (GHUB-0167).
    // Declared by each game where it is registered, so the filters can never
    // hold a list of their own that goes stale when a game is added. A game
    // names exactly one, so the filters split the grid rather than overlap.
    enum class Category { Board, Cards, Puzzle, Arcade };

    struct Entry {
        // The game's id: saved/<name> and --game <name>.
        // Fixed and never translated (GHUB-0161 § 4.3).
        QString name;
        // What the tile and the window title show. Translated; with no
        // translation loaded it reads exactly as `name` does.
        QString label;
        QString blurb;
        // Drawn on the tile; each game gets a recognisable miniature rather
        // than a generic placeholder.
        std::function<void(QPainter&, const QRectF&)> paintTile;
        std::function<GameView*()> create;
        Category category = Category::Board;
        GameView* view = nullptr;
        int pageIndex = -1;
    };

    void buildEntries();
    void buildChrome();
    void openGame(int index);
    void showMenu();
    void setGameActions(GameView* view);
    // Shows the open game's rules in the shared window.
    void showRules();
    GameView* currentView() const;
    // One window size and position for the whole app (GHUB-0060), written on
    // the way out of every page and restored once, when the window first opens.
    void rememberPage();
    // Hidden pages stop deciding how small the window can be made. See the
    // definition for what goes wrong without it.
    void onlyTheOpenPageSetsTheFloor();
    void restoreGeometryOnce();
    // Puts the window back to the size the player chose once a page that forced
    // it bigger has gone. `before` is the size from just before the switch.
    void keepChosenSize(const QSize& before);
    // True while the window is only the size it is because the open page's
    // minimum forced it up -- a size nobody chose, so it is never stored.
    bool clampedByPage() const;
    // A game's position, kept between sessions for the games that offer one.
    void storeSave(const Entry& e);
    // Reads back whether a settings write actually landed, and says so once if
    // it did not. QSettings reports that nowhere else.
    void checkSettingsWritable(QSettings& s);
    // Writes one key only when its value has actually moved, and reports
    // whether it wrote. Everything worth keeping goes through here, so the
    // autosave tick costs a comparison rather than a disk write.
    bool writeIfChanged(QSettings& s, const QString& key, const QByteArray& value);
    // Banks the open game where it stands. Runs on the autosave timer, so it
    // only ever writes -- see the definition for why it must not clear.
    void autosaveTick();
    // Marks the games with something stored, on their tiles and their Games
    // menu entries (GHUB-0062). Reads settings for every game, so it runs
    // when the grid or the menu is shown and never from a paint.
    void markGamesInProgress();
    bool inProgress(const Entry& e) const;
    // Shows only the tiles the chosen filter keeps, in their usual order, and
    // says so on the grid when the favourites filter has nothing to show.
    void applyFilter();
    bool passesFilter(const Entry& e) const;
    // A kind's id as hub/filter stores it. Never translated.
    static QString categoryId(Category c);
    // Stars or unstars a game, and keeps the choice between sessions.
    void toggleFavourite(int index);
    // Says on the board, not only in the status bar, that a game came back.
    void showResumeNotice();
    // Sizes it for the large-play switch and centres it over the board.
    void layOutResumeNotice();

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

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
    // Help -> How to play: on while a game with rules is open (GHUB-0016).
    QAction* m_rulesAction = nullptr;
    // Where addHelpAction() puts what it is given: above this separator.
    QMenu* m_helpMenu = nullptr;
    QAction* m_helpSeparator = nullptr;
    QLabel* m_status = nullptr;
    QList<QAction*> m_gameActions;
    QList<Entry> m_entries;
    // Parallel to m_entries: each game's tile and its Games menu entry.
    QList<QPushButton*> m_tiles;
    QList<QAction*> m_menuEntries;
    // The tile grid's layout, re-filled whenever the filter changes.
    QGridLayout* m_grid = nullptr;
    // The filter row above the grid: All, the four kinds, then Favourites.
    QButtonGroup* m_filterGroup = nullptr;
    // Said on the grid itself when Favourites is chosen and nothing is starred.
    QLabel* m_noFavourites = nullptr;
    // The chosen filter's id, as stored under hub/filter: "all", a kind, or
    // "favourites". Never translated -- it is a settings value.
    QString m_filter;
    // Starred games, by id (Entry::name), as stored under hub/favourites.
    QStringList m_favourites;
    // Over the top of the board for a few seconds after a game resumes.
    QLabel* m_resumeNotice = nullptr;
    QTimer* m_resumeNoticeTimer = nullptr;
    // Which page is on screen: a game's name, or empty for the tile menu.
    QString m_page;
    bool m_geometryReady = false;
    // The size the player chose, held while a page's minimum has forced the
    // window larger than it (m_clampedTo). Invalid when nothing is clamped.
    QSize m_chosenSize;
    QSize m_clampedTo;
    // The per-page sizes from before GHUB-0060 are deleted once per run, by
    // the first write -- never by a hub told not to remember.
    bool m_oldSizesForgotten = false;
    bool m_remembering = true;
    // Each page's own size policy, kept so a hidden page can be given Ignored
    // and handed its own back when it comes forward.
    QHash<QWidget*, QSizePolicy> m_pagePolicy;
    // Said once per session: repeating it on every page switch would bury the
    // status line the player is actually reading.
    bool m_settingsTroubleReported = false;
    // What this process has already put on disk, per settings key, so an
    // unchanged value is not rewritten.
    QHash<QString, QByteArray> m_written;
    // Banks the open game while it is being played rather than on the way out,
    // so a crash, a kill or a power cut loses at most this interval -- and so
    // two copies of the app settle by most recent move rather than by whichever
    // happens to exit last.
    QTimer* m_autosave = nullptr;
};
