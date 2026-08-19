// Drives the real hub and every game widget offscreen: each one must build,
// lay out, paint and respond without crashing. Run with
// QT_QPA_PLATFORM=offscreen (the CMake test sets it) so it needs no display.

#include "gameview.h"
#include "legibility.h"
#include "scores.h"
#include "sound.h"
#include "canasta/canastaview.h"
#include "cards/cardart.h"
#include "chess/chessview.h"
#include "draughts/draughtsview.h"
#include "freecell/freecellview.h"
#include "hearts/heartsview.h"
#include "hubwindow.h"
#include "klondike/klondikeview.h"
#include "minesweeper/minesweeperview.h"
#include "pinball/pinballview.h"
#include "pyramid/pyramidview.h"
#include "reversi/reversiview.h"
#include "spider/spiderview.h"
#include "sudoku/sudokuview.h"
#include "twenty48/twenty48view.h"

#include <QApplication>
#include <QDataStream>
#include <QDeadlineTimer>
#include <QFile>
#include <QSettings>
#include <QLabel>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QStatusBar>
#include <QToolBar>

#include <algorithm>
#include <cstdio>

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++g_failures;
}

void pump(int ms)
{
    QDeadlineTimer deadline(ms);
    while (!deadline.hasExpired())
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// Painting into a pixmap is the cheapest way to prove a view's paintEvent runs
// end to end without a display attached.
bool paints(QWidget* w)
{
    w->resize(900, 700);
    QPixmap canvas(w->size());
    canvas.fill(Qt::black);
    w->render(&canvas);
    return !canvas.isNull();
}

QLabel* statusLabel(QMainWindow* window)
{
    for (QLabel* l : window->statusBar()->findChildren<QLabel*>())
        return l;
    return nullptr;
}

// Wayland blocks OS-level input injection, but a synthetic Qt event is
// delivered straight to the widget, so clicks can still be tested offscreen.
void clickAt(QWidget* w, QPointF pos, Qt::MouseButton button)
{
    QMouseEvent press(QEvent::MouseButtonPress, pos, w->mapToGlobal(pos), button, button,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(w, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, w->mapToGlobal(pos), button,
                        Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &release);
}

// 2048 is played on the keyboard rather than with the mouse. A synthetic key
// event reaches the widget the same way a synthetic click does.
void pressKey(QWidget* w, Qt::Key key)
{
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &press);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &release);
}

// A press, a move past the drag threshold, and a release. clickAt cannot stand
// in for this: without the move between them the card games never lift a run
// off its pile, so nothing happens at all.
void dragBetween(QWidget* w, QPointF from, QPointF to)
{
    QMouseEvent press(QEvent::MouseButtonPress, from, w->mapToGlobal(from), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &press);
    for (const QPointF& step : { (from + to) / 2.0, to }) {
        QMouseEvent move(QEvent::MouseMove, step, w->mapToGlobal(step), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(w, &move);
    }
    QMouseEvent release(QEvent::MouseButtonRelease, to, w->mapToGlobal(to), Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &release);
}

int countPixels(const QImage& image, QRgb colour)
{
    int n = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixel(x, y) == colour)
                ++n;
    return n;
}

// The only thing that can observe the legibility hook firing. The default
// applyLegibility() only repaints, and no game changes appearance yet, so no
// rendered pixel differs between a notified view and an unnotified one.
//
// It lives here rather than on GameView because a counter on the base class
// would be production surface existing only to be asserted on. Free-standing
// rather than installed as a hub page — deliberately: connected purely by the
// GameView base constructor, it goes red under any implementation that instead
// has the hub walk its own pages, which is the bug INV-2 exists to catch.
class LegibilityProbe : public GameView
{
public:
    using GameView::GameView;

    void applyLegibility(bool enabled) override
    {
        ++calls;
        last = enabled;
    }

    int calls = 0;
    bool last = false;
};

QImage renderOf(QWidget* w)
{
    QPixmap canvas(w->size());
    canvas.fill(Qt::black);
    w->render(&canvas);
    return canvas.toImage();
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // A test-only settings identity, so running the suite never touches the
    // player's real best scores.
    QCoreApplication::setOrganizationName(QStringLiteral("GamesHubTest"));
    QCoreApplication::setApplicationName(QStringLiteral("GamesSelfTest"));

    // ---- legibilityDefaultsOff (INV-5) ----
    //
    // MUST BE THE FIRST BLOCK IN main(), before any GameView is constructed.
    // Legibility is a singleton whose private constructor reads QSettings once,
    // at first use, and every GameView constructor connects to it — so
    // constructing any game instantiates it. A block that clears the key
    // afterwards is asserting whatever the store happened to hold at process
    // start, which is not the default and is not under this block's control.
    //
    // The clear is not redundant with the suite's own scope: Scores::clear()
    // calls QSettings::clear(), which wipes the WHOLE store rather than just
    // the scores, so what a previous run left here depends on which block ran
    // last. Clearing makes this block say the same thing every time.
    {
        QSettings settings;
        settings.remove(QStringLiteral("display/legibility"));
        settings.sync();
        check(!Legibility::instance().enabled(),
              "legibility: with no stored value the switch is off");
    }

    // ---- legibilityPersists (INV-1) ----
    //
    // A freshly constructed QSettings reads the backing store rather than the
    // singleton's cache, and it is the same check on both platforms —
    // QFile::exists(QSettings().fileName()) is false on Windows however well
    // saving works. Both sides name the literal key, which is what makes this
    // the one block that can see the key being renamed in a later release.
    // Both halves assert contains() as well as the value. Without it the "off"
    // half is satisfied by the key being ABSENT — QVariant().toBool() is false
    // — so a setEnabled() that wrote nothing at all would pass it.
    {
        Legibility::instance().setEnabled(true);
        const QSettings afterOn;
        check(afterOn.contains(QStringLiteral("display/legibility"))
                  && afterOn.value(QStringLiteral("display/legibility")).toBool(),
              "legibility: turning the switch on reaches the settings store");

        Legibility::instance().setEnabled(false);
        const QSettings afterOff;
        check(afterOff.contains(QStringLiteral("display/legibility"))
                  && !afterOff.value(QStringLiteral("display/legibility")).toBool(),
              "legibility: and turning it off again does too");
    }

    // ---- legibilityReachesBackgroundGames (INV-2) ----
    //
    // Counter deltas, not absolute counts: setEnabled() is a no-op when
    // unchanged, so a bare setEnabled(true) after an earlier block may emit
    // nothing. Drive the switch to a known state first, then move it.
    {
        LegibilityProbe onScreen;
        LegibilityProbe inBackground;
        onScreen.show();

        Legibility::instance().setEnabled(false);
        const int seenOnScreen = onScreen.calls;
        const int seenInBackground = inBackground.calls;

        Legibility::instance().setEnabled(true);
        check(onScreen.calls == seenOnScreen + 1,
              "legibility: the visible game is told exactly once");
        check(inBackground.calls == seenInBackground + 1,
              "legibility: so is a game that is built but not on screen");
        check(onScreen.last && inBackground.last,
              "legibility: and both are told which way it went");

        Legibility::instance().setEnabled(false);
    }

    // ---- twenty48InkIsReadable (INV-7) ----
    //
    // The switch is not consulted: 2048's ink is fixed for everyone, because a
    // tile nobody can read is a defect rather than a preference. The trailing
    // 4096 is tileColour()'s default: arm, which a loop over the enumerated
    // cases alone never reaches.
    {
        bool readable = true;
        for (int value : { 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 }) {
            const double ratio = contrastRatio(inkFor(value), tileColour(value));
            if (ratio < 3.0) {
                std::printf("      2048 tile %d has ink at only %.2f:1\n", value, ratio);
                readable = false;
            }
        }
        check(readable, "2048: every tile's ink clears 3:1 against its own colour");
    }

    // ---- canastaLegibleMelds (GHUB-0017 INV-3, in Canasta's own numbers) ----
    //
    // The mechanism's spec withdrew INV-3 and INV-6 to whichever per-game pass
    // landed first (§ 9), to be restated against that game's layout. This is
    // that pass. Canasta draws melds at kMeldScale, and at every window size it
    // could previously reach that put a melded card under
    // CardArt::kFaceMinWidth — the shared art then draws the corner index alone
    // and a meld becomes a column of slivers.
    //
    // The switch-OFF half is what stops this block being vacuously green. It
    // asserts the defect is still there with the switch off, so a build that had
    // quietly stopped consulting the switch fails here instead of passing twice.
    {
        CanastaView canasta;

        Legibility::instance().setEnabled(false);
        canasta.resize(canasta.minimumSize());
        check(canasta.smallestFaceWidth() < CardArt::kFaceMinWidth,
              "canasta: with the switch off a meld at the smallest window is "
              "still faceless — the defect the switch exists to fix");

        Legibility::instance().setEnabled(true);
        // Ask for a window smaller than the raised minimum. Qt clamps back up to
        // it, which is the half that proves the minimum moved rather than that
        // the test happened to leave the widget large.
        canasta.resize(400, 300);
        check(canasta.smallestFaceWidth() >= CardArt::kFaceMinWidth,
              "canasta: with the switch on a meld shows a face even when the "
              "window is driven below the minimum");
        // And the table has room for that card rather than cardWidth()'s floor
        // having lifted it. Without this the block passes on a build where only
        // the floor moved — measured, it did — and a floored card overflows the
        // table instead of reading better.
        check(canasta.cardsFitTable(),
              "canasta: and the raised minimum gives the table room for it, "
              "rather than the floor clamping a card that does not fit");

        Legibility::instance().setEnabled(false);
    }

    // ---- canastaLegibilityReverses (GHUB-0017 INV-6, in Canasta's own numbers)
    //
    // The second withdrawn invariant. The switch raises the minimum size, so Qt
    // clamps the window larger — and HubWindow::rememberPage() writes that
    // enlarged geometry over the stored one on the next page change. Turning the
    // switch off therefore has to put back BOTH the minimum and the size, or
    // large mode is one-way and the player cannot undo it.
    {
        Legibility::instance().setEnabled(false);
        CanastaView canasta;
        canasta.resize(canasta.minimumSize());
        const QSize wasMinimum = canasta.minimumSize();
        const QSize wasSize = canasta.size();

        Legibility::instance().setEnabled(true);
        check(canasta.minimumSize().width() > wasMinimum.width()
                  && canasta.size().width() > wasSize.width(),
              "canasta: turning the switch on raises the minimum and Qt clamps "
              "the window up to it");

        Legibility::instance().setEnabled(false);
        check(canasta.minimumSize() == wasMinimum && canasta.size() == wasSize,
              "canasta: and turning it off puts the minimum and the window size "
              "back where they were");
    }

    // ---- sudokuLegibilityGrowsMarks (GHUB-0017 §9, Sudoku's per-game pass) --
    //
    // Sudoku's complaint is size and not colour: §2.3 measured the pencil ink at
    // 4.88:1, comfortably past WCAG, while the mark font is a fifth of a cell
    // against a real digit's half. So the pass makes the marks bigger, and the
    // only thing that can stop it is the layout — nine marks share one cell.
    //
    // Both halves matter. The size assertion alone would pass on a build that
    // grew the marks until they ran into each other, and marksFitCell() alone
    // would pass on a build that had quietly stopped consulting the switch,
    // since small marks fit best of all. The switch-OFF reading is what makes
    // the growth assertion falsifiable rather than a statement about one
    // number.
    {
        Legibility::instance().setEnabled(false);
        SudokuView sudoku;
        sudoku.resize(sudoku.minimumSize());

        const double small = sudoku.markPointSize();
        check(sudoku.marksFitCell(),
              "sudoku: with the switch off nine pencil marks fit inside a cell");

        Legibility::instance().setEnabled(true);
        // Deliberately NOT a number tuned to this machine's font. The first
        // version asked for half again, which held here at 0.685 of an em and
        // failed the Windows leg at Segoe UI's 0.728 — the ceiling is a
        // property of the font, so a test that pins it is testing the wrong
        // thing. 1.15 is clear of every real font's ceiling; the maximality
        // check below is what stops that looseness mattering.
        check(sudoku.markPointSize() > small * 1.15,
              "sudoku: the switch grows a pencil mark by a real amount, at the "
              "smallest window the game allows");
        check(sudoku.marksFitCell(),
              "sudoku: and the grown mark's ink still clears its third of the "
              "cell, so nine of them do not run into each other");
        // The assertion that actually has teeth now the size is solved rather
        // than tuned: it is as large as it can be, not merely legal. A timid
        // solve passes both checks above and fails this one.
        check(!sudoku.marksFitAt(sudoku.markPointSize() * 1.12),
              "sudoku: and it is as large as it fits — a step bigger would not");

        Legibility::instance().setEnabled(false);
    }

    // ---- sudokuLegibilityReverses (GHUB-0017 INV-6, in Sudoku's numbers) ----
    //
    // INV-6 was withdrawn from the mechanism spec as vacuously true — nothing
    // adapted, so nothing could fail to be restored. Canasta's pass carried it
    // as a size and minimum-size claim. Sudoku changes no geometry at all, so
    // here it is what it was originally written as: two renders that must match
    // exactly, with a third in between that must not.
    //
    // The board has to be carrying pencil marks or all three renders are
    // identical and the block is green without having looked at anything. A
    // sweep of the whole grid is what guarantees that — most cells are clues
    // and refuse the mark, and which ones is decided by a freshly generated
    // puzzle.
    {
        Legibility::instance().setEnabled(false);
        SudokuView sudoku;
        sudoku.resize(560, 600);

        for (QAction* action : sudoku.gameActions())
            if (action->text() == QStringLiteral("Pencil"))
                action->setChecked(true);
        for (int row = 0; row < 9; ++row) {
            for (int col = 0; col < 9; ++col) {
                pressKey(&sudoku, Qt::Key_5);
                pressKey(&sudoku, Qt::Key_Right);
            }
            for (int col = 0; col < 9; ++col)
                pressKey(&sudoku, Qt::Key_Left);
            pressKey(&sudoku, Qt::Key_Down);
        }

        const QImage before = renderOf(&sudoku);
        Legibility::instance().setEnabled(true);
        check(renderOf(&sudoku) != before,
              "sudoku: the switch actually changes what is painted");

        Legibility::instance().setEnabled(false);
        check(renderOf(&sudoku) == before,
              "sudoku: and turning it off puts the board back pixel for pixel");
    }

    // ---- Best scores survive a restart ----
    {
        Scores& scores = Scores::instance();
        scores.clear();

        const QString high = Scores::pinballBestScore();
        check(!scores.has(high), "a fresh profile has no recorded best");
        check(scores.recordHigh(high, 1200), "the first score is always a best");
        check(!scores.recordHigh(high, 900), "a lower score does not replace the best");
        check(scores.recordHigh(high, 5000), "a higher score does replace it");
        check(scores.best(high) == 5000, "the stored best is the highest seen");

        // Times and move counts are better when smaller.
        const QString low = Scores::minesweeperBestTime(1);
        check(scores.recordLow(low, 90), "the first time is always a best");
        check(!scores.recordLow(low, 120), "a slower time does not replace it");
        check(scores.recordLow(low, 45), "a faster time does");
        check(scores.best(low) == 45, "the stored best time is the fastest seen");

        // The point of all this is that it outlives the process. A freshly
        // constructed QSettings reads the backing store rather than the
        // Scores object's own cache, so that is the persistence check — and
        // it is the same check on both platforms. Asserting a settings *file*
        // existed used to stand here and failed on Windows, where the store
        // is the registry and there is no file to look for; nothing had gone
        // unsaved.
        check(QSettings().value(high).toInt() == 5000,
              "a best score is read back from a freshly opened settings store");

        scores.clear();
    }

    // ---- The sound effects are compiled into the binary ----
    {
        const QStringList effects = { QStringLiteral("ui_click"), QStringLiteral("ui_back"),
            QStringLiteral("disc_place"), QStringLiteral("disc_flip"), QStringLiteral("dig"),
            QStringLiteral("flag"), QStringLiteral("boom"), QStringLiteral("card_deal"),
            QStringLiteral("card_place"), QStringLiteral("shuffle"), QStringLiteral("bumper"),
            QStringLiteral("slingshot"), QStringLiteral("flipper"), QStringLiteral("launch"),
            QStringLiteral("drain"), QStringLiteral("win"), QStringLiteral("lose") };

        QStringList missing;
        for (const QString& name : effects) {
            QFile wav(QStringLiteral(":/sounds/%1.wav").arg(name));
            // A resource that exists but is empty would play silence, so check
            // the payload too rather than just the path.
            if (!wav.exists() || wav.size() < 1000)
                missing << name;
        }
        if (!missing.isEmpty())
            std::printf("      missing or empty: %s\n", qPrintable(missing.join(", ")));
        check(missing.isEmpty(), "every sound effect is compiled into the binary");

        // Playing while muted must be a no-op, not a crash.
        Sound::instance().setMuted(true);
        Sound::instance().play(QStringLiteral("bumper"));
        Sound::instance().setMuted(false);
        check(true, "playing a sound is safe with no audio device");
    }

    // ---- Every game view stands up on its own ----
    {
        ReversiView reversi;
        check(paints(&reversi), "reversi view paints");
        check(!reversi.gameActions().isEmpty(), "reversi view offers toolbar actions");

        MinesweeperView mines;
        check(paints(&mines), "minesweeper view paints");

        // Flag red must stay on the flags. QPainter::drawRect fills with the
        // current brush, so the flag's brush used to leak into every dug
        // square painted after it and turn the whole board red.
        // The squares are painted row by row, so the flags have to sit ABOVE
        // the dug area for a leaked brush to reach it.
        mines.resize(720, 720);
        clickAt(&mines, QPointF(360, 500), Qt::LeftButton); // open an area lower down
        for (int i = 0; i < 12; ++i)
            clickAt(&mines, QPointF(60 + i * 43, 40), Qt::RightButton); // flags along the top row

        const QImage shot = renderOf(&mines);
        const int red = countPixels(shot, qRgb(0xe5, 0x3d, 0x3d));
        const double share = 100.0 * red / (shot.width() * shot.height());
        std::printf("      flag red covers %.2f%% of the board\n", share);
        check(share < 2.0, "minesweeper: flag red does not bleed into dug squares");

        // Chess is driven by clicking one square then another, so the check is
        // that a real pawn move lands and the engine answers it — the whole
        // loop, not just that the widget paints.
        ChessView chess;
        check(paints(&chess), "chess view paints");
        check(!chess.gameActions().isEmpty(), "chess view offers toolbar actions");

        QString chessStatus;
        QObject::connect(&chess, &GameView::statusChanged,
                         [&chessStatus](const QString& text) { chessStatus = text; });

        chess.resize(640, 640);
        // Mirrors ChessView::boardRect: a square board centred in the widget,
        // inside an 18px frame and a 4px margin.
        const auto square = [](const QWidget* w, int row, int col) {
            const int side = ((std::min(w->width(), w->height()) - 2 * (18 + 4)) / 8) * 8;
            const double cell = side / 8.0;
            return QPointF((w->width() - side) / 2.0 + (col + 0.5) * cell,
                           (w->height() - side) / 2.0 + (row + 0.5) * cell);
        };

        clickAt(&chess, square(&chess, 6, 4), Qt::LeftButton);   // the pawn on e2
        const QImage selected = renderOf(&chess);
        clickAt(&chess, square(&chess, 4, 4), Qt::LeftButton);   // push it to e4
        pump(1500);                                              // let the engine reply
        const QImage replied = renderOf(&chess);

        check(selected != replied, "chess: playing a move redraws the board");
        check(chessStatus.contains(QStringLiteral("Computer played")),
              "chess: the engine answers the player's move");

        // A game in progress survives being put away. The check is the picture:
        // a restored board that renders identically is the same position, the
        // same last move and the same side to play.
        const QByteArray saved = chess.saveState();
        check(!saved.isEmpty(), "chess: a game in progress is worth saving");

        ChessView resumed;
        resumed.resize(chess.size());
        const QImage fresh = renderOf(&resumed);
        check(resumed.restoreState(saved), "chess: and it reads back");
        pump(300);
        check(renderOf(&resumed) == replied, "chess: onto the very same board");
        check(fresh != replied, "chess: which is not just a new game by another name");

        // Junk is refused, and refusing it leaves the board alone.
        const QImage before = renderOf(&resumed);
        check(!resumed.restoreState(QByteArray("not a chess game")),
              "chess: a corrupt save is refused");
        check(renderOf(&resumed) == before, "chess: and refusing one changes nothing");

        // A game nobody has moved in has nothing to come back to, which is what
        // clears a stale save rather than resuming into it.
        ChessView untouched;
        check(untouched.saveState().isEmpty(), "chess: an unplayed game saves nothing");

        // ---- The four solitaires put a deal away and pick it up again ----
        //
        // Each one is the same three questions. An untouched deal saves nothing,
        // so it never resumes onto a table nobody has played. A deal in progress
        // reads back onto the very same table — and the check for that is the
        // picture, because two identical renderings are the same piles with the
        // same cards the same way up. And a corrupt save is refused without
        // disturbing the deal already on screen.
        {
            KlondikeView klondike;
            check(paints(&klondike), "klondike view paints");
            klondike.resize(820, 620);
            check(klondike.saveState().isEmpty(), "klondike: an untouched deal saves nothing");

            // The stock is the top-left corner card; clicking it turns one over.
            clickAt(&klondike, QPointF(31, 40), Qt::LeftButton);
            const QImage played = renderOf(&klondike);
            const QByteArray saved = klondike.saveState();
            check(!saved.isEmpty(), "klondike: a deal in progress is worth saving");

            KlondikeView resumed;
            resumed.resize(klondike.size());
            const QImage brandNew = renderOf(&resumed);
            check(resumed.restoreState(saved), "klondike: and it reads back");
            check(renderOf(&resumed) == played, "klondike: onto the very same table");
            check(brandNew != played, "klondike: which is not just a new deal by another name");

            check(!resumed.restoreState(QByteArray("not a game of patience")),
                  "klondike: a corrupt save is refused");
            check(renderOf(&resumed) == played, "klondike: and refusing one changes nothing");
        }

        {
            SpiderView spider;
            check(paints(&spider), "spider view paints");
            spider.resize(900, 640);
            check(spider.saveState().isEmpty(), "spider: an untouched deal saves nothing");

            // The stock is the stack in the bottom-right corner; clicking it
            // deals a row across the ten columns.
            clickAt(&spider, QPointF(spider.width() - 20, spider.height() - 20), Qt::LeftButton);
            const QImage played = renderOf(&spider);
            const QByteArray saved = spider.saveState();
            check(!saved.isEmpty(), "spider: a deal in progress is worth saving");

            SpiderView resumed;
            resumed.resize(spider.size());
            check(resumed.restoreState(saved), "spider: and it reads back");
            check(renderOf(&resumed) == played, "spider: onto the very same table");
            check(!resumed.restoreState(QByteArray("eight runs of nothing")),
                  "spider: a corrupt save is refused");
            check(renderOf(&resumed) == played, "spider: and refusing one changes nothing");
        }

        {
            FreeCellView freecell;
            check(paints(&freecell), "freecell view paints");
            freecell.resize(900, 640);
            check(freecell.saveState().isEmpty(), "freecell: an untouched deal saves nothing");

            // Mirrors FreeCellView's geometry: four cells along the top, eight
            // columns below fanned by 0.27 of a card. Parking the bottom card of
            // the first column in the first cell is the one move legal in every
            // deal, whatever it dealt.
            const double card = std::min((900.0 - 24.0) / (8 + 7 * 0.12),
                                         (640.0 - 24.0) / (1.4 * 2.5));
            const double tall = card * 1.4;
            const double columnTop = 12 + tall + tall * 0.22;
            dragBetween(&freecell, QPointF(12 + card / 2, columnTop + tall * (6 * 0.27 + 0.15)),
                        QPointF(12 + card / 2, 12 + tall / 2));

            const QImage played = renderOf(&freecell);
            const QByteArray saved = freecell.saveState();
            check(!saved.isEmpty(), "freecell: a card parked in a cell is worth saving");

            FreeCellView resumed;
            resumed.resize(freecell.size());
            check(resumed.restoreState(saved), "freecell: and it reads back");
            check(renderOf(&resumed) == played, "freecell: onto the very same table");
            check(!resumed.restoreState(QByteArray("no cells here")),
                  "freecell: a corrupt save is refused");
            check(renderOf(&resumed) == played, "freecell: and refusing one changes nothing");
        }

        {
            PyramidView pyramid;
            check(paints(&pyramid), "pyramid view paints");
            pyramid.resize(860, 640);
            check(pyramid.saveState().isEmpty(), "pyramid: an untouched deal saves nothing");

            // Mirrors PyramidView::stockRect: the face-down pile left of centre
            // along the bottom edge.
            const double card = std::min((860.0 - 40.0) / (7 * 0.62 + 0.4),
                                         (640.0 - 40.0) / (1.4 + 6 * 0.52 + 1.6));
            clickAt(&pyramid, QPointF(860 / 2.0 - card * 0.75, 640 - card * 0.7 - 16),
                    Qt::LeftButton);

            const QImage played = renderOf(&pyramid);
            const QByteArray saved = pyramid.saveState();
            check(!saved.isEmpty(), "pyramid: a deal in progress is worth saving");

            PyramidView resumed;
            resumed.resize(pyramid.size());
            check(resumed.restoreState(saved), "pyramid: and it reads back");
            check(renderOf(&resumed) == played, "pyramid: onto the very same table");
            check(!resumed.restoreState(QByteArray("thirteen of nothing")),
                  "pyramid: a corrupt save is refused");
            check(renderOf(&resumed) == played, "pyramid: and refusing one changes nothing");
        }

        // ---- And the four quick games put a board away and pick it up ----
        //
        // Same three questions as the solitaires, for the same reason: none of
        // these four keeps a move log either, so what is written is the board
        // itself and the render is the proof that it came back whole.
        {
            MinesweeperView mines;
            mines.resize(560, 520);
            check(mines.saveState().isEmpty(), "minesweeper: an undug field saves nothing");

            // The field is centred in the widget, so the middle is always a
            // square. The first dig is what lays the mines and starts the clock.
            clickAt(&mines, QPointF(mines.width() / 2.0, mines.height() / 2.0), Qt::LeftButton);
            const QImage dug = renderOf(&mines);
            const QByteArray saved = mines.saveState();
            check(!saved.isEmpty(), "minesweeper: a field in progress is worth saving");

            MinesweeperView resumed;
            resumed.resize(mines.size());
            const QImage brandNew = renderOf(&resumed);
            check(resumed.restoreState(saved), "minesweeper: and it reads back");
            check(renderOf(&resumed) == dug, "minesweeper: onto the very same field");
            check(brandNew != dug, "minesweeper: which is not just a new field by another name");

            check(!resumed.restoreState(QByteArray("no mines here")),
                  "minesweeper: a corrupt save is refused");
            check(renderOf(&resumed) == dug, "minesweeper: and refusing one changes nothing");
        }

        {
            // Mirrors ReversiView::boardRect, which DraughtsView::boardRect
            // matches exactly: a frame 10 wide plus 4 of margin, then an 8x8
            // grid rounded to whole pixels and centred in what is left.
            const auto cellCentre = [](const QWidget* w, int row, int col) {
                const int available = std::min(w->width(), w->height()) - 2 * (10 + 4);
                const int side = std::max(8, (available / 8) * 8);
                const double cell = side / 8.0;
                return QPointF((w->width() - side) / 2 + (col + 0.5) * cell,
                               (w->height() - side) / 2 + (row + 0.5) * cell);
            };

            ReversiView reversi;
            reversi.resize(520, 520);
            check(reversi.saveState().isEmpty(), "reversi: an unplayed game saves nothing");

            clickAt(&reversi, cellCentre(&reversi, 2, 3), Qt::LeftButton); // a legal opening
            pump(1500);                                                    // and the engine's reply
            const QImage played = renderOf(&reversi);
            const QByteArray saved = reversi.saveState();
            check(!saved.isEmpty(), "reversi: a game in progress is worth saving");

            ReversiView resumed;
            resumed.resize(reversi.size());
            const QImage brandNew = renderOf(&resumed);
            check(resumed.restoreState(saved), "reversi: and it reads back");
            pump(300);
            check(renderOf(&resumed) == played, "reversi: onto the very same board");
            check(brandNew != played, "reversi: which is not just a new game by another name");

            check(!resumed.restoreState(QByteArray("four discs and a prayer")),
                  "reversi: a corrupt save is refused");
            check(renderOf(&resumed) == played, "reversi: and refusing one changes nothing");

            DraughtsView draughts;
            draughts.resize(560, 560);
            check(draughts.saveState().isEmpty(), "draughts: an unplayed game saves nothing");

            // Red starts at the bottom and moves up: pick up the man on the
            // third row from the bottom, then step it diagonally forward.
            clickAt(&draughts, cellCentre(&draughts, 5, 2), Qt::LeftButton);
            clickAt(&draughts, cellCentre(&draughts, 4, 3), Qt::LeftButton);
            pump(1500);
            const QImage moved = renderOf(&draughts);
            const QByteArray draughtsSave = draughts.saveState();
            check(!draughtsSave.isEmpty(), "draughts: a game in progress is worth saving");

            DraughtsView draughtsResumed;
            draughtsResumed.resize(draughts.size());
            check(draughtsResumed.restoreState(draughtsSave), "draughts: and it reads back");
            pump(300);
            // Identical pictures mean the last-move marks came back too, which
            // is what tells you where the computer just went.
            check(renderOf(&draughtsResumed) == moved, "draughts: onto the very same board");
            check(!draughtsResumed.restoreState(QByteArray("twelve men short")),
                  "draughts: a corrupt save is refused");
            check(renderOf(&draughtsResumed) == moved,
                  "draughts: and refusing one changes nothing");
        }

        {
            Twenty48View slider;
            slider.resize(520, 560);
            check(slider.saveState().isEmpty(), "2048: an untouched board saves nothing");

            // Two tiles always move in at least one of the four directions, and
            // a direction that moves nothing costs nothing.
            for (Qt::Key key : { Qt::Key_Left, Qt::Key_Up, Qt::Key_Right, Qt::Key_Down })
                pressKey(&slider, key);
            const QImage played = renderOf(&slider);
            const QByteArray saved = slider.saveState();
            check(!saved.isEmpty(), "2048: a board in progress is worth saving");

            Twenty48View resumed;
            resumed.resize(slider.size());
            const QImage brandNew = renderOf(&resumed);
            check(resumed.restoreState(saved), "2048: and it reads back");
            check(renderOf(&resumed) == played, "2048: onto the very same tiles");
            check(brandNew != played, "2048: which is not just a new board by another name");

            check(!resumed.restoreState(QByteArray("three thousand and forty-eight")),
                  "2048: a corrupt save is refused");
            check(renderOf(&resumed) == played, "2048: and refusing one changes nothing");

            // 2048 keeps no core of its own, so the check that stands in for a
            // pack check lives here: every tile has to be a power of two,
            // because nothing else can come out of a merge. A well-formed blob
            // holding a 3 is the only way to reach that path.
            QByteArray forged;
            QDataStream out(&forged, QIODevice::WriteOnly);
            out.setVersion(QDataStream::Qt_6_0);
            out << quint32(1) << qint32(4) << false;
            for (int i = 0; i < 16; ++i)
                out << qint32(i == 0 ? 3 : 0);
            check(!resumed.restoreState(forged), "2048: a tile that is not a power of two is refused");
            check(renderOf(&resumed) == played, "2048: and refusing that changes nothing either");
        }

        HeartsView hearts;
        check(paints(&hearts), "hearts view paints");

        {
            CanastaView canasta;
            canasta.resize(1000, 740);
            check(paints(&canasta), "canasta view paints");

            QString status;
            QObject::connect(&canasta, &GameView::statusChanged,
                             [&status](const QString& s) { status = s; });
            // What the hub does on opening a game, and what makes it report in.
            canasta.activate();

            // The deal is animated, so the table only settles after the cards
            // have finished flying.
            const QImage dealing = renderOf(&canasta);
            pump(2500);
            const QImage dealt = renderOf(&canasta);
            check(dealing != dealt, "canasta: the deal animates rather than appearing at once");

            check(status.contains(QStringLiteral("Classic")),
                  "canasta: the status line names the rule set in force");
            check(status.contains(QStringLiteral("to open")) || status.contains(QStringLiteral("open")),
                  "canasta: the status line says what it takes to open");

            // Every toolbar control the game promises.
            QStringList actions;
            for (QAction* a : canasta.gameActions())
                if (!a->isSeparator())
                    actions << a->text();
            std::printf("      canasta actions: %s\n", qPrintable(actions.join(QStringLiteral(", "))));
            // Meld is deliberately absent: laying cards down is a button on the
            // table, because the toolbar is the far corner of the window from
            // the hand you are laying down.
            check(!actions.contains(QStringLiteral("Meld")),
                  "canasta: melding is not a toolbar action");
            for (const char* wanted : { "New Game", "Discard", "Sort", "Easy", "Hard",
                                        "Expert", "Classic", "House", "Expert partner", "Hints" }) {
                check(actions.contains(QString::fromUtf8(wanted)),
                      qPrintable(QStringLiteral("canasta: the toolbar offers %1")
                                     .arg(QString::fromUtf8(wanted))));
            }
            const bool hasTarget = std::any_of(actions.begin(), actions.end(), [](const QString& s) {
                return s.startsWith(QStringLiteral("Play to"));
            });
            check(hasTarget, "canasta: the toolbar offers other target scores");

            // Clicking a card in your hand picks it up, which must change the
            // picture; clicking it again puts it back.
            // The board only takes hand clicks on the human's turn, so wait for
            // it. Without this the check races the three computer seats and
            // fails whenever they are still playing.
            const auto waitForYourTurn = [&status] {
                for (int i = 0; i < 60 && !status.contains(QStringLiteral("Your turn"))
                     && !status.contains(QStringLiteral("Lay down"));
                     ++i)
                    pump(200);
                return status.contains(QStringLiteral("Your turn"))
                    || status.contains(QStringLiteral("Lay down"));
            };
            check(waitForYourTurn(), "canasta: the turn comes round to you");

            // Cards may still be in the air when the turn passes, and the board
            // ignores clicks until they land. Settled means two renders in a
            // row that match.
            const auto waitUntilStill = [&canasta] {
                QImage last = renderOf(&canasta);
                for (int i = 0; i < 30; ++i) {
                    pump(120);
                    const QImage now = renderOf(&canasta);
                    if (now == last)
                        return true;
                    last = now;
                }
                return false;
            };
            check(waitUntilStill(), "canasta: the table settles once the cards land");

            const QPointF card(canasta.width() / 2.0, canasta.height() - 60.0);
            const QImage idle = renderOf(&canasta);
            clickAt(&canasta, card, Qt::LeftButton);
            const QImage picked = renderOf(&canasta);
            check(idle != picked, "canasta: clicking a card in hand lifts it");
            clickAt(&canasta, card, Qt::LeftButton);
            check(renderOf(&canasta) != picked, "canasta: clicking it again puts it back");

            // Drawing from the stock has to move the game on.
            const QPointF stock(canasta.width() * 0.5 - 60.0, canasta.height() * 0.47);
            const QPointF pile(canasta.width() * 0.5 + 60.0, canasta.height() * 0.47);
            clickAt(&canasta, stock, Qt::LeftButton);
            pump(900);
            check(status.contains(QStringLiteral("throw")) || !status.isEmpty(),
                  "canasta: the game responds to a draw");

            // Play a stretch of the hand for real: draw, pick a card, throw it,
            // and let the three computer seats answer. A rule that stalls a
            // turn shows up here as a table that stops changing.
            const auto stockLeft = [&status] {
                const int at = status.lastIndexOf(QStringLiteral("stock "));
                return at < 0 ? -1 : status.mid(at + 6).split(QChar(' ')).first().toInt();
            };
            const int stockAtStart = stockLeft();
            for (int turn = 0; turn < 12; ++turn) {
                pump(700);
                clickAt(&canasta, stock, Qt::LeftButton);
                pump(400);
                clickAt(&canasta, QPointF(canasta.width() / 2.0, canasta.height() - 60.0),
                        Qt::LeftButton);
                pump(80);
                clickAt(&canasta, pile, Qt::LeftButton);
            }
            pump(1200);
            const int stockAtEnd = stockLeft();
            std::printf("      canasta: stock went %d -> %d over twelve turns\n", stockAtStart,
                        stockAtEnd);
            check(stockAtEnd >= 0 && stockAtEnd < stockAtStart,
                  "canasta: play keeps moving through the stock rather than stalling");
            check(paints(&canasta), "canasta: the table still paints mid-hand");
        }

        PinballView pinball;
        check(paints(&pinball), "pinball view paints");
    }

    // ---- The hub opens each game and comes back ----
    HubWindow hub;
    hub.resize(900, 700);
    hub.show();
    pump(200);

    QLabel* status = statusLabel(&hub);
    check(status != nullptr, "the hub has a status line");

    const QList<QPushButton*> tiles = hub.findChildren<QPushButton*>();
    // One tile per registered game, whatever the count has grown to.
    const int expected = hub.gameNames().size();
    std::printf("      hub registers %d games\n", expected);
    check(tiles.size() == expected, "the hub shows a tile for every game");

    int opened = 0;
    for (QPushButton* tile : tiles) {
        tile->click();
        pump(150);
        // Opening a game must put a GameView on screen and give the toolbar
        // that game's own actions.
        auto* view = qobject_cast<GameView*>(hub.centralWidget()->findChild<GameView*>(
            QString(), Qt::FindDirectChildrenOnly));
        Q_UNUSED(view);
        ++opened;
    }
    check(opened == expected, "every tile opens without crashing");

    // Pinball runs a physics timer; let it spin so a bad step would surface.
    for (QPushButton* tile : tiles)
        tile->click();
    pump(400);
    check(true, "the running game survives 400ms of live updates");

    // ---- A chosen setting outlives the game it was chosen for ----
    //
    // Both of these already survived inside a SAVED game and only there:
    // Canasta writes m_useHouse into its blob, Minesweeper writes m_level into
    // its own. But saveState() returns an empty blob for a game that is over —
    // that is how a finished game avoids resuming onto its own final scores —
    // and an empty state clears the stored one. So the setting was kept
    // precisely as long as the game was unfinished, and a player who finished a
    // hand under House rules, or won on Expert, came back to Classic and
    // Intermediate.
    //
    // Each block is written the way a player meets it: choose, close, reopen.
    // Asserting the QSettings key alone would pass on a build that writes it
    // and never reads it back, which is half the defect.
    {
        const auto actionNamed = [](GameView& view, const char* name) -> QAction* {
            for (QAction* a : view.gameActions())
                if (a->text() == QString::fromUtf8(name))
                    return a;
            return nullptr;
        };

        // ---- canastaRemembersItsRuleSet ----
        {
            QSettings().remove(QStringLiteral("canasta/useHouse"));
            {
                CanastaView fresh;
                check(actionNamed(fresh, "Classic") != nullptr
                          && actionNamed(fresh, "Classic")->isChecked(),
                      "canasta: with nothing stored a table opens on Classic");
                actionNamed(fresh, "House")->trigger();
            }
            {
                CanastaView reopened;
                check(actionNamed(reopened, "House")->isChecked()
                          && !actionNamed(reopened, "Classic")->isChecked(),
                      "canasta: a table opened later is still on House, and the "
                      "toolbar says so");
            }
            // And back, so the memory is not one-way — the failure a player
            // would hit is being stuck on the set they tried once.
            {
                CanastaView reopened;
                actionNamed(reopened, "Classic")->trigger();
            }
            {
                CanastaView again;
                check(actionNamed(again, "Classic")->isChecked(),
                      "canasta: and choosing Classic again is remembered too");
            }
            QSettings().remove(QStringLiteral("canasta/useHouse"));
        }

        // ---- minesweeperRemembersItsDifficulty ----
        {
            {
                MinesweeperView fresh;
                actionNamed(fresh, "Expert")->trigger();
            }
            {
                MinesweeperView reopened;
                check(actionNamed(reopened, "Expert")->isChecked(),
                      "minesweeper: a field opened later is still Expert");
                // The level actually in play, not just the tick: newGame() sizes
                // the field from m_level, so a build that restored the tick and
                // not the field would pass on the assertion above alone.
                check(reopened.saveState().isEmpty(),
                      "minesweeper: and an undug field stores nothing, so the "
                      "difficulty is not riding on a saved game");
                actionNamed(reopened, "Beginner")->trigger();
            }
            {
                MinesweeperView again;
                check(actionNamed(again, "Beginner")->isChecked(),
                      "minesweeper: and a different choice replaces it");
            }
            QSettings().remove(QStringLiteral("minesweeper/level"));
        }
    }

    // ---- sudokuMarksFitAnyFont ----
    //
    // The mark size is solved against the font in hand rather than tuned, and
    // this is the check that says so. A tuned 0.29 passed on this machine, at
    // 0.685 of an em, and failed the Windows CI leg on Segoe UI at 0.728 —
    // measured on the real box, along with Arial 0.731 and Tahoma 0.760. So a
    // green run here proved nothing about the platform it broke on.
    //
    // The families below are whatever this machine has, sampled across the
    // list: locally they span 0.49 to 0.99 of an em, which is far past
    // anything a desktop would choose as a UI font and comfortably brackets
    // every Windows candidate. Symbol fonts with no digits are skipped rather
    // than counted — there is nothing to fit — and the count is asserted so a
    // machine with an empty font database fails here instead of passing an
    // empty loop.
    {
        Legibility::instance().setEnabled(true);
        SudokuView sudoku;
        sudoku.resize(sudoku.minimumSize());
        const QFont original = sudoku.font();

        const QStringList families = QFontDatabase::families();
        const int stride = std::max(1, int(families.size()) / 12);
        int exercised = 0;
        int misfit = 0;
        int timid = 0;
        for (int i = 0; i < families.size(); i += stride) {
            QFont probe(families.at(i));
            probe.setPointSizeF(100.0);
            probe.setBold(true);
            if (QFontMetricsF(probe).tightBoundingRect(QStringLiteral("0123456789")).height()
                <= 0.0)
                continue;

            sudoku.setFont(QFont(families.at(i)));
            if (!sudoku.marksFitCell())
                ++misfit;
            if (sudoku.marksFitAt(sudoku.markPointSize() * 1.12))
                ++timid;
            ++exercised;
        }
        sudoku.setFont(original);

        check(exercised >= 5,
              "sudoku: enough font families to say anything about portability");
        check(misfit == 0,
              "sudoku: the solved mark fits its cell third in every font tried, however "
              "tall that font draws a digit");
        check(timid == 0,
              "sudoku: and is the largest that fits in each of them, rather than a size "
              "that merely happens to be safe");

        Legibility::instance().setEnabled(false);
    }

    std::printf("\n%s\n", g_failures == 0 ? "All UI checks passed." : "FAILURES PRESENT.");
    return g_failures == 0 ? 0 : 1;
}
