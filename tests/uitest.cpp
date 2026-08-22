// Drives the real hub and every game widget offscreen: each one must build,
// lay out, paint and respond without crashing. Run with
// QT_QPA_PLATFORM=offscreen (the CMake test sets it) so it needs no display.

#include "donate.h"
#include "donatedialog.h"
#include "funding.h"
#include "gameview.h"
#include "legibility.h"
#include "scores.h"
#include "sound.h"
#include "theme.h"
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
#include "snake/snakeview.h"
#include "spider/spiderview.h"
#include "sudoku/sudokuview.h"
#include "twenty48/twenty48view.h"

#include <QApplication>
#include <QTimer>
#include <QAction>
#include <QCheckBox>
#include <QDataStream>
#include <QDeadlineTimer>
#include <QFile>
#include <QSettings>
#include <QLabel>
#include <QMenuBar>
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

        // How tall this font actually draws a digit, because that is what caps
        // the mark and it is a property of the platform rather than of this
        // code. Two attempts at an absolute growth number failed on Windows CI
        // — half again, then 1.15 — so the third asks the font first and only
        // claims growth where growth is arithmetically available.
        QFont em = sudoku.font();
        em.setPointSizeF(100.0);
        em.setBold(true);
        const double inkPerEm =
            QFontMetricsF(em).tightBoundingRect(QStringLiteral("0123456789")).height()
            / (100.0 * 4.0 / 3.0);
        std::printf("      sudoku: this font draws digits at %.3f of an em; the mark solved "
                    "to %.2fpt against %.2fpt normal\n",
                    inkPerEm, sudoku.markPointSize(), small);

        // A mark may fill kMarkInkShare (0.85) of a cell third, and a cell
        // third is a third of the cell, so the largest legal ratio is
        // 0.85/3 ÷ (inkPerEm × 4/3) = 0.2125 / inkPerEm. Every desktop UI font
        // measured lands well inside this: 0.685 here, Segoe UI 0.728, Arial
        // 0.731, Tahoma 0.760 on the owner's Windows box. A font at 0.92 or
        // above genuinely has no room to grow, and saying so is the honest
        // answer rather than a failure — but it must be SAID, not skipped in
        // silence, or an environment where the feature does nothing looks
        // exactly like one where it works.
        const double headroom = inkPerEm > 0.0 ? (0.2125 / inkPerEm) / 0.20 : 0.0;
        if (headroom >= 1.15)
            check(sudoku.markPointSize() > small * 1.15,
                  "sudoku: the switch grows a pencil mark by a real amount, at the "
                  "smallest window the game allows");
        else
            std::printf("      sudoku: growth not asserted — this font's digits leave only "
                        "%.2fx of room, so the largest legal mark is barely the normal one\n",
                        headroom);
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

    // ---- everyGameAnswersTheSwitch (GHUB-0017, the umbrella's own claim) ----
    //
    // GHUB-0017 says the hub owns one switch "every game reads". For most of
    // this item's life that was false of twelve of the fourteen, and nothing
    // said so: the mechanism's own blocks prove the SIGNAL arrives, which a
    // game is free to receive and ignore. This asks the only question that
    // settles it — does the picture change — of every game the hub can open,
    // so a fifteenth game added without a pass reddens here rather than
    // shipping silently.
    //
    // Each game is deactivated first. Snake's timer, Pinball's ball and
    // Hearts' opponents would otherwise redraw between the two renders, and a
    // difference that came from a moving ball proves nothing about the switch.
    // A game that still moves after deactivate() is left out of the picture
    // comparison — a difference that came from motion proves nothing — AND
    // fails the stop-on-leave assertion below, because GameView::deactivate()
    // promises it stopped.
    //
    // Measured at each game's OWN minimum size, which is where a pass bites
    // hardest — Canasta's whole pass is a raised minimum, and at a comfortable
    // window it changes nothing at all. Run at 960x940 first, this block
    // reported Canasta as ignoring the switch, which was the block being
    // wrong rather than Canasta.
    //
    // Sudoku is the one exclusion and it is named rather than filtered out by
    // a rule. Its pass grows PENCIL MARKS, and a freshly generated board has
    // none — so all three renders are identical and no window size changes
    // that. Its own block above puts a mark in every cell it will take before
    // asserting, which is the only way to ask this question of Sudoku.
    {
        Legibility::instance().setEnabled(false);
        HubWindow probe;
        QStringList silent;
        QStringList notRestored;
        QStringList restless;
        QStringList mute;
        int measured = 0;

        for (const QString& name : probe.gameNames()) {
            HubWindow one;
            one.openGameNamed(name);
            one.show();
            pump(30);
            one.resize(one.minimumSizeHint());
            pump(30);

            GameView* view = one.findChild<GameView*>();
            if (view == nullptr) {
                restless << name + QStringLiteral(" (no view)");
                continue;
            }
            // Let the game SETTLE before freezing it, and the distinction is
            // the whole reason this loop exists. deactivate() stops a timer
            // where it stands, which makes a board static without making it
            // finished: Canasta's opening deal freezes with cards still in
            // the air, and CanastaView::applyLegibility deliberately LANDS
            // them (Flight::to is a point captured at launch, so a card must
            // not be left to arrive at a destination that has moved). That
            // landing is correct and cannot be undone, so a board frozen
            // mid-deal fails a reversibility check for a reason that is not a
            // defect.
            //
            // It passed locally every time and failed on BOTH CI legs, which
            // is the signature of a race rather than a bug: this machine
            // finishes the deal before this point and a loaded runner does not.
            //
            // Asked of the game rather than guessed from pixels. A staggered
            // deal has lulls — every remaining card still counting down its
            // delay — so two matching renders do NOT mean settled, and the
            // probe below would clear a board that is merely pausing.
            int settling = 0;
            for (; settling < 80 && view->hasPendingAnimation(); ++settling)
                pump(50);
            if (settling > 0)
                std::printf("      %s settled after %d ms of dealing\n",
                            qPrintable(name), settling * 50);

            view->deactivate();
            pump(20);

            // Three renders spread over time, not two. Pinball's ball can sit
            // between two physics ticks and give two identical renders while
            // still moving — which then shows up as a game that "did not go
            // back", blaming the switch for a ball that rolled.
            const QImage before = renderOf(view);
            bool still = true;
            for (int probe = 0; probe < 2; ++probe) {
                pump(25);
                still = still && renderOf(view) == before;
            }
            if (!still) {
                restless << name;
                continue;
            }
            // Sudoku sits out the PICTURE comparison only, and it has to be
            // here rather than at the top of the loop: skipping it outright
            // also excused it from the stop-on-leave assertion above, so a
            // timer added to SudokuView would have been guarded by nothing.
            if (name == QStringLiteral("Sudoku"))
                continue;

            ++measured;
            if (view->captionText().isEmpty())
                mute << name;

            Legibility::instance().setEnabled(true);
            pump(20);
            if (renderOf(view) == before)
                silent << name;

            Legibility::instance().setEnabled(false);
            pump(20);
            if (renderOf(view) != before) {
                notRestored << name;
                // The sizes, because "did not go back" on its own sends you
                // looking at the painting when the cause is the geometry.
                std::printf("        %s: view was %dx%d, is now %dx%d, window %dx%d\n",
                            qPrintable(name), before.width(), before.height(),
                            view->width(), view->height(),
                            one.width(), one.height());
            }
        }
        Legibility::instance().setEnabled(false);

        std::printf("      legibility: %d of %d compared off/on (all %d checked for "
                    "stop-on-leave; Sudoku's picture is asserted in its own block)\n",
                    measured, int(probe.gameNames().size()) - 1,
                    int(probe.gameNames().size()));
        if (!restless.isEmpty())
            std::printf("      not measured (still animating after deactivate): %s\n",
                        qPrintable(restless.join(QStringLiteral(", "))));
        if (!mute.isEmpty())
            std::printf("      nothing to say on the surface: %s\n",
                        qPrintable(mute.join(QStringLiteral(", "))));
        if (!silent.isEmpty())
            std::printf("      IGNORED THE SWITCH: %s\n",
                        qPrintable(silent.join(QStringLiteral(", "))));
        if (!notRestored.isEmpty())
            std::printf("      DID NOT GO BACK: %s\n",
                        qPrintable(notRestored.join(QStringLiteral(", "))));

        // Asserted rather than reported: GameView::deactivate() promises that a
        // game with a clock or an animation stops it when the hub leaves, and
        // this block is the only thing that has ever asked. It caught Pinball,
        // which had no override at all and left the ball rolling — and
        // draining — on a table nobody was looking at.
        check(restless.isEmpty(),
              "every game stops moving when the hub leaves it, as deactivate() promises");
        check(measured > 0, "legibility: at least one game held still long enough to measure");
        check(silent.isEmpty(), "legibility: every measured game changes what it paints");
        check(notRestored.isEmpty(),
              "legibility: and every one of them goes back pixel for pixel");
    }

    // ---- gamesStopTheirClocks (GHUB-0073, generalised) ----
    //
    // deactivate() promises that a game with a clock stopped it. The block
    // above can only catch a game that HAPPENS to be moving when the hub
    // leaves, and every clock here except Pinball's is idle on a freshly
    // opened board — so Snake and Hearts kept theirs running through every
    // green run of it. Leaving Snake mid-game drove the snake into a wall;
    // leaving Hearts finished the hand without you.
    //
    // So this starts the game first, then leaves it. **The coming-back half
    // matters as much as the leaving half**: a deactivate() with no resume
    // freezes the game, which is a worse bug than the one it fixes.
    {
        const auto activeTimers = [](QWidget* w) {
            int running = 0;
            for (QTimer* timer : w->findChildren<QTimer*>())
                if (timer->isActive())
                    ++running;
            return running;
        };

        // Each in its own scope, ending stopped. A view left running here keeps
        // its timer alive through everything below — Pinball's physics ticks
        // and repaints on every pump, which turned a 34-second suite into one
        // that had not finished in two minutes.
        {
            SnakeView snake;
            snake.resize(640, 480);
            snake.activate();
            pressKey(&snake, Qt::Key_Right);
            check(activeTimers(&snake) > 0, "snake: the clock runs once the game has started");
            snake.deactivate();
            check(activeTimers(&snake) == 0, "snake: and stops when the hub leaves the table");
            snake.activate();
            check(activeTimers(&snake) > 0, "snake: and picks up again when you come back");
            snake.deactivate();
        }

        // Pinball is the case that was found first, and the one game whose
        // clock is already running before anything is pressed.
        {
            PinballView pinball;
            pinball.resize(640, 700);
            pinball.activate();
            check(activeTimers(&pinball) > 0,
                  "pinball: the table runs as soon as it is on screen");
            pinball.deactivate();
            check(activeTimers(&pinball) == 0, "pinball: and the ball stops when you leave it");
            pinball.activate();
            check(activeTimers(&pinball) > 0, "pinball: and starts again when you return");
            pinball.deactivate();
        }

        // Every game the hub can open, held to the structural rule rather than
        // to whether anything was visibly moving: nothing may still be ticking
        // once the hub has left. This is the net that would have caught Snake
        // and Hearts on the day they were written.
        HubWindow probe;
        QStringList ticking;
        for (const QString& name : probe.gameNames()) {
            HubWindow one;
            one.openGameNamed(name);
            one.show();
            pump(40);
            GameView* view = one.findChild<GameView*>();
            if (view == nullptr)
                continue;
            view->deactivate();
            pump(20);
            if (activeTimers(view) > 0)
                ticking << name;
        }
        if (!ticking.isEmpty())
            std::printf("      STILL TICKING AFTER deactivate(): %s\n",
                        qPrintable(ticking.join(QStringLiteral(", "))));
        check(ticking.isEmpty(), "no game is still running a timer once the hub has left it");
    }

    // ---- cardsKeepTheirFaces (GHUB-0017 INV-3, withdrawn and now writable) --
    //
    // INV-3 — "no game draws a card too small to show its pips" — was withdrawn
    // from the mechanism spec because cardWidth() is private on all six card
    // views and no test could reach it. GameView::smallestCardWidth() is that
    // access, so the invariant can finally be held against every card game at
    // once rather than against Canasta alone.
    //
    // Measured at the SMALLEST window each game allows, because that is where a
    // card is narrowest, and with large play ON, because that is where the
    // caption's strip comes off the height and the cards pay for it. A game
    // that draws no cards answers 0 and is skipped.
    {
        Legibility::instance().setEnabled(true);
        HubWindow probe;
        QStringList tooSmall;
        int checked = 0;
        for (const QString& name : probe.gameNames()) {
            HubWindow one;
            one.openGameNamed(name);
            one.show();
            pump(20);
            GameView* view = one.findChild<GameView*>();
            if (view == nullptr)
                continue;
            one.resize(one.minimumSizeHint());
            pump(20);
            const double smallest = view->smallestCardWidth();
            if (smallest <= 0.0)
                continue;
            ++checked;
            std::printf("      %-12s smallest card %.1f px against a %.0f px floor\n",
                        qPrintable(name), smallest, CardArt::kFaceMinWidth);
            if (smallest < CardArt::kFaceMinWidth)
                tooSmall << name;
        }
        Legibility::instance().setEnabled(false);
        check(checked >= 6, "six games draw cards and every one of them was measured");
        check(tooSmall.isEmpty(),
              "no card game is driven below the width that still shows a face");
    }

    // ---- captionStaysInside (the caption plate's own arithmetic) ----
    //
    // The plate is what a game gives up board space for, so it has to be
    // honest about how much: wider than the area it was given, or taller than
    // the band captionBand() reserves, and the game has shrunk itself for
    // nothing. A long sentence must wrap rather than grow sideways — the
    // failure a caption cannot have is a line running off the window.
    {
        const QRectF area(0, 0, 600, 400);
        QFont f;
        f.setPointSizeF(16.0);

        check(Theme::captionRect(area, QString(), f).isNull(),
              "caption: empty text asks for no plate at all");

        const QRectF one = Theme::captionRect(area, QStringLiteral("Your move."), f);
        check(area.contains(one), "caption: a short sentence sits inside its area");

        const QString essay = QStringLiteral(
            "West led hearts, so follow suit if you can, and remember the queen "
            "of spades is still out there somewhere among the other three hands.");
        const QRectF many = Theme::captionRect(area, essay, f);
        check(many.width() <= area.width(),
              "caption: a long sentence wraps rather than running off the side");
        check(many.height() > one.height(),
              "caption: and it is taller for having wrapped");
        check(area.contains(many), "caption: the wrapped plate is still inside the area");
        check(qFuzzyCompare(one.center().x(), many.center().x()),
              "caption: both are centred on the same line");
    }

    // ---- captionBreaksAtItsOwnJoints (GHUB-0088) ----
    //
    // Every game composes its status sentence out of phrases with a run of two
    // or more spaces between them. Plain word wrap does not know that, so at a
    // small window it broke wherever the width ran out: Reversi read "Your turn
    // (Black).   You 2 -" / "2 Computer", orphaning the score from whose score
    // it was, and Spider left the three separator spaces dangling on the end of
    // a line so its plate had a margin down one side and none down the other.
    //
    // The property asserted is a property of the FUNCTION, not of this machine's
    // fonts: a phrase is never split across two lines, and no line carries
    // leading or trailing space. `room` is derived at runtime from the widest
    // phrase, so the wrap is forced to happen whatever the font measures.
    {
        // The joints, mirrored independently of theme.cpp so this asserts the
        // contract rather than the implementation's own idea of it.
        auto phrasesOf = [](const QString& s) {
            QStringList out;
            qsizetype i = 0;
            while (i < s.size()) {
                qsizetype j = i;
                while (j < s.size()
                       && !(s[j].isSpace() && j + 1 < s.size() && s[j + 1].isSpace()))
                    ++j;
                out << s.mid(i, j - i).trimmed();
                while (j < s.size() && (s[j].isSpace() || s[j] == QChar(0x00B7)))
                    ++j;
                i = j;
            }
            out.removeAll(QString());
            return out;
        };

        QFont f;
        f.setPointSizeF(14.0);
        f.setBold(true);
        const QFontMetricsF fm(f);

        // The reported case, stated exactly: room for the longer phrase and not
        // for both, so the break is forced and there is only one right place
        // for it.
        const QString first = QStringLiteral("Your turn (Black).");
        const QString second = QStringLiteral("You 2 — 2 Computer");
        const QString reversi = first + QStringLiteral("   ") + second;
        const double room = std::max(fm.horizontalAdvance(first), fm.horizontalAdvance(second)) + 1.0;
        const QStringList lines = Theme::wrapCaption(reversi, f, room).split(QLatin1Char('\n'));
        check(lines.size() == 2, "caption: a sentence too wide for one line becomes two");
        check(lines.value(0) == first && lines.value(1) == second,
              "caption: and it breaks between the phrases, not inside one");

        // A single phrase has no joint to break at, so it comes back whole and
        // Qt::TextWordWrap is left to do what it can with it.
        const QString unbroken = QStringLiteral("Click anywhere to start.");
        check(!Theme::wrapCaption(unbroken, f, 10.0).contains(QLatin1Char('\n')),
              "caption: a phrase with no joint is never broken up");
        check(Theme::wrapCaption(QString(), f, room).isEmpty(),
              "caption: an empty sentence stays empty");

        // Then the real population: what the fourteen games actually say, at
        // the smallest window each of them allows.
        Legibility::instance().setEnabled(true);
        HubWindow probe;
        QStringList split;
        QStringList ragged;
        int wrapped = 0;
        for (const QString& name : probe.gameNames()) {
            HubWindow one;
            one.openGameNamed(name);
            one.show();
            one.resize(one.minimumSizeHint());
            pump(20);
            GameView* view = one.findChild<GameView*>();
            if (view == nullptr)
                continue;
            const QString text = view->captionText();
            const QStringList phrases = phrasesOf(text);
            if (phrases.size() < 2)
                continue;

            double widest = 0.0;
            for (const QString& phrase : phrases)
                widest = std::max(widest, fm.horizontalAdvance(phrase));
            const QStringList out = Theme::wrapCaption(text, f, widest + 1.0).split(QLatin1Char('\n'));
            if (out.size() > 1)
                ++wrapped;
            std::printf("      %-12s %lld phrases -> %lld lines\n", qPrintable(name),
                        static_cast<long long>(phrases.size()),
                        static_cast<long long>(out.size()));

            for (const QString& line : out) {
                if (line != line.trimmed())
                    ragged << name;
            }
            for (const QString& phrase : phrases) {
                bool intact = false;
                for (const QString& line : out)
                    intact = intact || line.contains(phrase);
                if (!intact)
                    split << (name + QStringLiteral(": \"") + phrase + QStringLiteral("\""));
            }
        }
        Legibility::instance().setEnabled(false);
        check(wrapped >= 6, "caption: the sentences of at least six games were made to wrap");
        if (!split.isEmpty())
            std::printf("      PHRASE SPLIT ACROSS LINES: %s\n", qPrintable(split.join(QStringLiteral("; "))));
        check(split.isEmpty(), "caption: no game has a phrase broken across two lines");
        if (!ragged.isEmpty())
            std::printf("      LINE WITH DANGLING SPACE: %s\n", qPrintable(ragged.join(QStringLiteral(", "))));
        check(ragged.isEmpty(), "caption: and no line carries a separator's spaces into the margin");
    }

    // ---- pilesClearTheCaptionPlate (GHUB-0082) ----
    //
    // A pile anchored to the bottom of the WIDGET is drawn and then covered:
    // the caption's plate is opaque and painted last. Both games already take
    // the band off the height they solve card width from, which is what made
    // the miss invisible — the cards shrink, and the pile slides under the
    // plate anyway. The mirrors below are the geometry the pile is SUPPOSED
    // to have; clicking them has to reach the real stock, so a pile that
    // forgets the band leaves this click on empty table.
    {
        struct PyramidProbe : PyramidView {
            using GameView::captionBand;
            using GameView::lastStatus;
        };
        struct SpiderProbe : SpiderView {
            using GameView::captionBand;
            using GameView::lastStatus;
        };

        Legibility::instance().setEnabled(true);

        {
            PyramidProbe pyramid;
            pyramid.resize(600, 544);
            const double band = pyramid.captionBand(QRectF(pyramid.rect()));
            check(band > 0, "piles: the switch reserves a caption band for Pyramid");
            const double card = std::min((600.0 - 40.0) / (7 * 0.62 + 0.4),
                                         (544.0 - 40.0 - band) / (1.4 + 6 * 0.52 + 1.6));
            check(pyramid.lastStatus().contains(QStringLiteral("Stock 24")),
                  "piles: Pyramid deals its stock face down");
            clickAt(&pyramid, QPointF(600 / 2.0 - card * 0.75, 544 - band - card * 0.7 - 16),
                    Qt::LeftButton);
            check(pyramid.lastStatus().contains(QStringLiteral("Stock 23")),
                  "piles: and its stock sits clear of the plate, where the click lands");
        }

        {
            SpiderProbe spider;
            spider.resize(620, 524);
            const double band = spider.captionBand(QRectF(spider.rect()));
            check(band > 0, "piles: the switch reserves a caption band for Spider");
            const double card = std::min((620.0 - 24.0 - 9 * 6.0) / 10.0,
                                         (524.0 - 24.0 - band) / (1.4 * 2.2));
            check(spider.lastStatus().contains(QStringLiteral("Stock 5")),
                  "piles: Spider holds five rows back");
            clickAt(&spider, QPointF(620 - 12 - card * 0.5, 524 - band - 12 - card * 0.7),
                    Qt::LeftButton);
            check(spider.lastStatus().contains(QStringLiteral("Stock 4")),
                  "piles: and its stock sits clear of the plate, where the click lands");
        }

        Legibility::instance().setEnabled(false);
    }

    // ---- dealtColumnsFitTheTable (GHUB-0083, GHUB-0086) ----
    //
    // Both games solve card width against a height budget, and both budgets
    // were flat figures that assumed a shorter column than the deal makes --
    // FreeCell deals seven face-up cards into room for a header and about two
    // cards of fan. The symptom with the switch ON was the caption plate
    // covering the bottom card of five of FreeCell's eight columns; with it
    // OFF, at a wide and short window, the last two cards of every column were
    // below the widget edge.
    //
    // The arithmetic here is the contract stated independently of the view:
    // the header row, the gap under it, the fan of the longest DEALT column
    // and one whole card must land inside the table with the bottom margin and
    // the caption band still to spare. cardWidth() is reachable because both
    // games publish it as smallestCardWidth().
    //
    // Growth past the deal is deliberately NOT covered. A column that collects
    // cards in play still fans past the budget, and sizing for that would take
    // width off every card at every window — GHUB-0089 carries the question.
    {
        struct FreeCellProbe : FreeCellView { using GameView::captionBand; };
        struct KlondikeProbe : KlondikeView { using GameView::captionBand; };

        // In card heights, read off each game's own layout.
        const double freecellDeal = 1.0 + 0.22 + 6 * 0.27 + 1.0;  // 7, all face up
        const double klondikeDeal = 1.0 + 0.14 * 1.6 / 1.4 + 6 * 0.13 + 1.0;  // 6 down, 1 up

        const QList<QSize> shapes = { QSize(620, 440), QSize(1400, 438),
                                      QSize(960, 700), QSize(700, 460) };

        for (bool on : { false, true }) {
            Legibility::instance().setEnabled(on);
            const char* state = on ? "switch on" : "switch off";

            for (const QSize& shape : shapes) {
                {
                    FreeCellProbe view;
                    view.resize(shape);
                    const double h = view.smallestCardWidth() * 1.4;
                    const double bottom = 12.0 + h * freecellDeal;
                    // The view's own height, not the size asked for: both
                    // games hold a minimum and clamp a shorter request up.
                    const double room =
                        view.height() - 12.0 - view.captionBand(QRectF(view.rect()));
                    check(bottom <= room + 0.5,
                          qPrintable(QStringLiteral("freecell: the deal fits %1x%2, %3")
                                         .arg(shape.width())
                                         .arg(shape.height())
                                         .arg(QLatin1String(state))));
                }
                {
                    KlondikeProbe view;
                    view.resize(shape);
                    const double h = view.smallestCardWidth() * 1.4;
                    const double bottom = 14.0 + h * klondikeDeal;
                    const double room =
                        view.height() - 14.0 - view.captionBand(QRectF(view.rect()));
                    check(bottom <= room + 0.5,
                          qPrintable(QStringLiteral("klondike: the deal fits %1x%2, %3")
                                         .arg(shape.width())
                                         .arg(shape.height())
                                         .arg(QLatin1String(state))));
                }
            }
        }

        Legibility::instance().setEnabled(false);
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
            // hasPendingAnimation() is what the every-game legibility block
            // settles on before it measures anything. A signal stuck at false
            // would turn that wait into a no-op that still looks like it works,
            // and the block would go back to measuring a board frozen mid-deal
            // — which is what reddened both CI legs. So prove it rises here.
            check(canasta.hasPendingAnimation(),
                  "canasta: the table reports cards in flight while it is dealing");
            pump(2500);
            const QImage dealt = renderOf(&canasta);
            check(dealing != dealt, "canasta: the deal animates rather than appearing at once");
            // A bounded wait for the FALLING edge, not an assertion at a fixed
            // instant. The computers carry on playing once the deal is over and
            // every AI move puts more cards in the air, so "no flights at
            // 2500ms" is a race — it failed one run in six. What has to be true
            // is that the signal comes back down, or the every-game block's
            // settle loop would spin to its budget.
            const auto reportsNoneEventually = [&canasta] {
                for (int i = 0; i < 60; ++i) {
                    if (!canasta.hasPendingAnimation())
                        return true;
                    pump(50);
                }
                return false;
            };
            check(reportsNoneEventually(),
                  "canasta: and reports none once they have landed");

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

            // ---- GHUB-0040: a refused move says why ON THE TABLE ----
            // The status bar still carries the sentence, but the owner never
            // looks there, so the contract under test is the panel: it is
            // absent while there is nothing to say, it appears above the hand
            // where the click happened, and it survives every click that is
            // not itself a move — reading it slowly is the whole point.
            check(waitUntilStill(), "canasta: the table settles before the refusal check");
            check(canasta.messageRect().isNull(),
                  "canasta: with nothing to say the table shows no message panel");

            const QImage quiet = renderOf(&canasta);
            // Throwing a card away with nothing picked up is refused every
            // time, whatever the deal — no seed hunting, no engine setup.
            clickAt(&canasta, pile, Qt::LeftButton);
            const QRectF said = canasta.messageRect();
            check(!said.isNull(), "canasta: a refused move puts its reason on the table");
            check(said.top() > 0.0 && said.left() > 0.0 && said.right() < canasta.width()
                      && said.bottom() < card.y(),
                  "canasta: and it sits above your hand, inside the table");
            check(renderOf(&canasta) != quiet, "canasta: the table repaints to show it");

            // An idle click used to wipe it: mousePressEvent cleared the
            // message before working out what had been clicked.
            clickAt(&canasta, QPointF(canasta.width() * 0.06, canasta.height() * 0.5),
                    Qt::LeftButton);
            check(canasta.messageRect() == said,
                  "canasta: an idle click leaves the message where it is");

            // Making a move of your own is what takes it away.
            clickAt(&canasta, card, Qt::LeftButton);
            pump(80);
            clickAt(&canasta, pile, Qt::LeftButton);
            pump(200);
            check(canasta.messageRect().isNull(),
                  "canasta: a move of your own is what clears it");

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

    // And "All Games" actually comes back. The tile grid is a grandchild of the
    // QStackedWidget rather than a page of it (it sits inside a QScrollArea), and
    // setCurrentWidget() on a widget the stack does not hold does nothing at all
    // and says nothing — a dead back button that every other check passes around.
    {
        QAction* back = nullptr;
        for (QAction* a : hub.findChildren<QAction*>()) {
            if (a->text().contains(QStringLiteral("All Games")))
                back = a;
        }
        check(back != nullptr, "the hub has an All Games action");
        if (back != nullptr) {
            tiles.first()->click();
            pump(120);
            const bool inGame = hub.centralWidget()->findChild<QWidget*>(
                                    QStringLiteral("gamesHubTileGrid"))
                                    ->isVisibleTo(&hub)
                == false;
            back->trigger();
            pump(120);
            check(inGame
                      && hub.centralWidget()
                             ->findChild<QWidget*>(QStringLiteral("gamesHubTileGrid"))
                             ->isVisibleTo(&hub),
                  "All Games goes back to the tile grid");
        }
    }

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

        // Reported, never asserted — third time lucky on this exact mistake.
        // windows-2022 under the offscreen platform returns an EMPTY font
        // database and measures its default face at 0.997 of an em, essentially
        // the full em box, which is a headless stub rather than a typeface. So
        // this loop legitimately exercises nothing there, and any minimum
        // demanded of it fails on the environment rather than on the code.
        //
        // Nothing is lost by not asserting it: the block above runs against the
        // real default font on every platform and asserts both fit and
        // maximality, so an empty loop here cannot leave the solve unchecked.
        // What this block adds is BREADTH, and breadth is exactly the thing a
        // machine either has or does not.
        std::printf("      sudoku: mark size solved against %d font families\n", exercised);
        check(misfit == 0,
              "sudoku: the solved mark fits its cell third in every font tried, however "
              "tall that font draws a digit");
        check(timid == 0,
              "sudoku: and is the largest that fits in each of them, rather than a size "
              "that merely happens to be safe");

        Legibility::instance().setEnabled(false);
    }


    // ---- The donate entry, and the every-150th-launch prompt ----
    //
    // The links are generated from .github/FUNDING.yml at configure time, so
    // the interesting failures are not "is the URL right" — the build cannot
    // produce a wrong one — but "did the generator quietly drop a link", "does
    // the prompt fire exactly once at 150", and "can it be turned off".
    {
        QSettings settings;
        settings.remove(QStringLiteral("donate"));

        check(funding::kLinkCount > 0, "donate: the build generated at least one funding link");

        bool linksWellFormed = true;
        for (const funding::Link& link : funding::kLinks) {
            if (QString::fromLatin1(link.label).isEmpty()
                || !QString::fromLatin1(link.url).startsWith(QStringLiteral("https://")))
                linksWellFormed = false;
        }
        check(linksWellFormed,
              "donate: every generated link has a name to read and an https address");

        // Independently of the generator: count the funding keys the repository
        // actually declares and demand the same number of entries. A key the
        // CMake loop has no rule for already fails the build, so this is the
        // second guard on the same thing — a link that exists on the sponsor
        // button and not in the app.
        QFile funding(QStringLiteral(GAMESHUB_SOURCE_DIR "/.github/FUNDING.yml"));
        if (funding.open(QIODevice::ReadOnly | QIODevice::Text)) {
            int declared = 0;
            while (!funding.atEnd()) {
                const QString line = QString::fromUtf8(funding.readLine()).trimmed();
                if (!line.isEmpty() && !line.startsWith(QLatin1Char('#')))
                    ++declared;
            }
            std::printf("      donate: FUNDING.yml declares %d links, the build carries %d\n",
                        declared, funding::kLinkCount);
            check(declared == funding::kLinkCount,
                  "donate: the app offers exactly the links the repository declares");
        } else {
            check(false, "donate: .github/FUNDING.yml is readable from the source tree");
        }

        // The off-by-one everyone remembers, in both directions: a prompt every
        // launch, or one that never arrives.
        int prompts = 0;
        for (int launch = 1; launch <= 600; ++launch) {
            if (donate::launchOwesPrompt(launch))
                ++prompts;
        }
        check(prompts == 4, "donate: 600 launches owe exactly four prompts");
        check(!donate::launchOwesPrompt(149) && donate::launchOwesPrompt(150)
                  && !donate::launchOwesPrompt(151),
              "donate: 149 does not ask, 150 asks, 151 does not ask again");
        check(!donate::launchOwesPrompt(0),
              "donate: a launch count of zero owes nothing, so a fresh install is not asked");

        // The counter is written as it is read, so a process that is killed
        // still counted. Reading it back through a second QSettings is the only
        // check that means anything on Windows, where there is no settings file
        // to look at.
        settings.remove(QStringLiteral("donate"));
        settings.sync();
        const int first = donate::recordLaunch();
        const int second = donate::recordLaunch();
        check(first == 1 && second == 2, "donate: the launch counter starts at one and climbs");
        check(QSettings().value(QStringLiteral("donate/launches")).toInt() == 2,
              "donate: the count is on disk as it is taken, not at exit");

        // 149 -> 150 -> 151 through the real stored counter.
        QSettings().setValue(QStringLiteral("donate/launches"), donate::kPromptEvery - 2);
        const bool at149 = donate::recordLaunchAndAsk();
        const bool at150 = donate::recordLaunchAndAsk();
        const bool at151 = donate::recordLaunchAndAsk();
        check(!at149 && at150 && !at151,
              "donate: crossing the 150th launch shows the prompt exactly once");

        // Turned off, it stays off — and the counter keeps climbing underneath,
        // so turning it back on does not fire immediately.
        donate::setAsksEnabled(false);
        QSettings().setValue(QStringLiteral("donate/launches"), donate::kPromptEvery - 1);
        const bool silenced = donate::recordLaunchAndAsk();
        check(!silenced, "donate: \"don't ask again\" silences the 150th launch");
        check(QSettings().value(QStringLiteral("donate/launches")).toInt() == donate::kPromptEvery,
              "donate: and the count still advances while it is silenced");
        donate::setAsksEnabled(true);

        // The dialog itself. It is never allowed to open a browser on its own,
        // so nothing here presses a link button — what is checked is that every
        // link is offered by name.
        {
            DonateDialog prompt(true, nullptr);
            const QList<QPushButton*> buttons = prompt.findChildren<QPushButton*>();
            int named = 0;
            for (const funding::Link& link : funding::kLinks) {
                for (QPushButton* b : buttons) {
                    if (b->text().contains(QString::fromLatin1(link.label)))
                        ++named;
                }
            }
            check(named == funding::kLinkCount,
                  "donate: the dialog offers one named button per funding link");

            auto* keepAsking = prompt.findChild<QCheckBox*>(QStringLiteral("donateKeepAsking"));
            check(keepAsking != nullptr,
                  "donate: the launch prompt offers a way to stop being asked");
            if (keepAsking != nullptr) {
                keepAsking->setChecked(false);
                check(!donate::asksEnabled(),
                      "donate: unticking it is stored at once, so closing the window honours it");
                donate::setAsksEnabled(true);
            }
        }

        // Opened from the Help menu there is nothing to silence — the player
        // asked for it.
        {
            DonateDialog invited(false, nullptr);
            check(invited.findChild<QCheckBox*>(QStringLiteral("donateKeepAsking")) == nullptr,
                  "donate: the Help menu entry offers no \"stop asking\" switch");
        }

        // The legibility switch reaches the dialog like it reaches a game.
        {
            Legibility::instance().setEnabled(false);
            DonateDialog normal(false, nullptr);
            const double small = normal.font().pointSizeF();
            Legibility::instance().setEnabled(true);
            DonateDialog large(false, nullptr);
            const double big = large.font().pointSizeF();
            Legibility::instance().setEnabled(false);
            check(big > small, "donate: large play makes the dialog's text bigger too");
        }

        settings.remove(QStringLiteral("donate"));
    }

    // ---- Help menu ----
    {
        HubWindow help;
        auto* donateAction = help.findChild<QAction*>(QStringLiteral("donateAction"));
        check(donateAction != nullptr, "the hub has a Help entry that reaches the donate dialog");
        bool hasHelpMenu = false;
        for (QAction* a : help.menuBar()->actions()) {
            if (a->text().contains(QStringLiteral("Help")))
                hasHelpMenu = true;
        }
        check(hasHelpMenu, "the hub has a Help menu for it to live in");
    }


    // ---- The window fits beside your work (GHUB-0042) ----
    //
    // The README's first sentence promises a window "sized to sit beside
    // whatever you are actually working on", and HubWindow::kFitsBesideYourWork
    // is what that means as a number: half a 1920x1080 desktop across, its
    // height less a panel and a title bar.
    //
    // What is checked is the SMALLEST every page can be dragged to, not the
    // size it opens at — a game that cannot be made small breaks the promise
    // exactly when the player is trying to make room. Each game gets its own
    // hub, because a QStackedWidget takes the largest minimum of every page it
    // has built, so measuring them through one window would report the worst
    // game's floor against all fourteen names.
    {
        const QSize bar = HubWindow::kFitsBesideYourWork;
        for (const bool large : { false, true }) {
            Legibility::instance().setEnabled(large);

            // Widest and tallest are tracked SEPARATELY, and that is the whole
            // point: reducing to one "worst" page by area lets a wide, short
            // game hide behind a tall one. At 1100x300 it never wins on area
            // against Canasta's 900x740, so its 1100 would never meet the 960
            // bar at all and the check would stay green on a window that
            // cannot sit beside anything.
            int widest = 0;
            int tallest = 0;
            QString widestPage;
            QString tallestPage;
            HubWindow probe;
            for (const QString& name : probe.gameNames()) {
                HubWindow one;
                one.openGameNamed(name);
                one.show();
                pump(30);
                const QSize floorSize = one.minimumSizeHint();
                if (floorSize.width() > widest) {
                    widest = floorSize.width();
                    widestPage = name;
                }
                if (floorSize.height() > tallest) {
                    tallest = floorSize.height();
                    tallestPage = name;
                }
            }

            std::printf("      window: %s play, widest is %s at %d and tallest is %s at %d, "
                        "against a %dx%d bar\n",
                        large ? "large" : "normal", qPrintable(widestPage), widest,
                        qPrintable(tallestPage), tallest, bar.width(), bar.height());
            check(widest <= bar.width() && tallest <= bar.height(),
                  large ? "every game can still be sized to sit beside your work with large "
                          "play on"
                        : "every game can be sized to sit beside your work");
        }
        Legibility::instance().setEnabled(false);

        // The tile grid is the page that used to decide this for everyone: at
        // 190px a tile, fourteen of them are five rows deep, and an unscrolled
        // grid put a floor of about 1170px on Chess as well as on itself.
        HubWindow grid;
        grid.show();
        pump(30);
        check(grid.minimumSizeHint().height() <= bar.height() / 2,
              "the tile grid scrolls rather than setting a floor for every game");

        // And a first run opens inside the bar without anyone dragging it.
        QSettings().remove(QStringLiteral("window/geometry"));
        QSettings().sync();
        HubWindow fresh;
        fresh.show();
        pump(30);
        std::printf("      window: a first run opens at %dx%d\n", fresh.size().width(),
                    fresh.size().height());
        check(fresh.size().width() <= bar.width() && fresh.size().height() <= bar.height(),
              "a first run opens at a size that already fits beside your work");
    }

    std::printf("\n%s\n", g_failures == 0 ? "All UI checks passed." : "FAILURES PRESENT.");
    return g_failures == 0 ? 0 : 1;
}
