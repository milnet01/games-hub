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
#include "cards/cardcodec.h"
#include "cards/cardflight.h"
#include "chess/chessart.h"
#include "chess/chessboard.h"
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
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QElapsedTimer>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStatusBar>
#include <QToolBar>

#include <algorithm>
#include <functional>
#include <cstdlib>
#include <random>
#include <cstdio>
#include <cstring>

namespace {

int g_failures = 0;

// Hoisted out of the deactivate() block when GHUB-0065 gave a second caller a
// reason to ask the same question: how many of this widget's timers are still
// running.
int activeTimers(QWidget* w)
{
    int running = 0;
    for (QTimer* timer : w->findChildren<QTimer*>())
        if (timer->isActive())
            ++running;
    return running;
}

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

// Mean milliseconds per frame over `frames` renders. render() is the same
// trick paints() uses to force paintEvent through with no display attached;
// the clock around it is the whole of GHUB-0049. One untimed render first, so
// the figure is not paying for whatever the first paint sets up.
//
// This REPORTS. Nothing built on it may assert a threshold: a frame time is a
// property of the machine, the compositor and the graphics stack, and
// CLAUDE.md's rule from the three red Windows runs is that a test asserts what
// the code does and only reports what the platform supplies.
double msPerFrame(QWidget* w, int frames)
{
    QPixmap canvas(w->size());
    canvas.fill(Qt::black);
    w->render(&canvas);

    QElapsedTimer clock;
    clock.start();
    for (int i = 0; i < frames; ++i)
        w->render(&canvas);
    return double(clock.nsecsElapsed()) / 1.0e6 / frames;
}

// Counts paint events delivered to one widget. render() bypasses the event
// queue entirely, so what this counts is repaints the game asked for ITSELF —
// which is exactly what "a deactivated view does no work" has to mean.
class PaintCounter : public QObject
{
public:
    int count = 0;

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::Paint)
            ++count;
        return false;
    }
};

// Let a view finish whatever it started, up to a deadline. Returns whether it
// actually settled, so a caller can say so rather than reporting a figure taken
// mid-animation as if it were a resting one.
bool settle(GameView* view, int budgetMs)
{
    QDeadlineTimer deadline(budgetMs);
    while (!deadline.hasExpired() && view->hasPendingAnimation())
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    return !view->hasPendingAnimation();
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

// ---- frameCost (GHUB-0049) ----
//
// Until this block existed nothing in the project had ever asked how long
// anything takes, so no painting change could be shown to have helped. It
// prints figures for a human to compare across a change and asserts one
// thing only — see msPerFrame's comment for why a threshold here would be
// a red CI leg waiting for a slow runner.
//
// The three subjects are the ones that say the most: Canasta mid-deal with
// cards in the air, the same table at rest, and the fourteen-tile grid.
//
// Run it alone with `gameshub_uitest --bench`, which is what makes a
// before-and-after pair cheap enough to actually take.
void frameCost()
{
    std::printf("\n      frame cost (ms/frame, this machine — a reading, not a bar)\n");

    const int frames = 60;

    CanastaView canasta;
    canasta.resize(1000, 740);
    canasta.show();
    pump(30);

    // A fresh CanastaView deals, so the flights are already in the air and
    // this is the worst frame the game ever draws: every card moving, the
    // whole table repainted for each one.
    const bool dealing = canasta.hasPendingAnimation();
    const double dealMs = msPerFrame(&canasta, frames);
    std::printf("        canasta, mid-deal%s   %6.2f\n",
                dealing ? "        " : " (SETTLED)", dealMs);

    canasta.activate();
    const bool settled = settle(&canasta, 5000);
    canasta.deactivate();
    const double restMs = msPerFrame(&canasta, frames);
    std::printf("        canasta, at rest%s    %6.2f\n",
                settled ? "         " : " (STILL BUSY)", restMs);

    KlondikeView klondike;
    klondike.resize(1000, 740);
    klondike.show();
    pump(30);
    std::printf("        klondike, full tableau        %6.2f\n",
                msPerFrame(&klondike, frames));

    // The most expensive board in the collection: FreeCell is the only game
    // that puts the whole pack on the table FACE UP, so no card back cache
    // reaches it and it is the honest worst case for drawing faces.
    FreeCellView freecell;
    freecell.resize(1000, 740);
    freecell.show();
    pump(30);
    std::printf("        freecell, all 52 face up      %6.2f\n",
                msPerFrame(&freecell, frames));

    HubWindow hub;
    hub.resize(1000, 740);
    hub.show();
    pump(30);
    std::printf("        hub, fourteen tiles           %6.2f\n",
                msPerFrame(&hub, frames));

    // The one honest assertion, because it is a property of the code
    // rather than of the machine: a view the hub has left asks for no
    // repaints at all. The positive control is what makes it mean
    // anything — without it, a counter that never fires under the
    // offscreen platform would pass the deactivated half for free.
    CanastaView watched;
    watched.resize(1000, 740);
    watched.show();
    PaintCounter counter;
    watched.installEventFilter(&counter);

    watched.activate();
    pump(300);
    const int whileActive = counter.count;

    watched.deactivate();
    pump(50);            // let anything already queued drain
    counter.count = 0;
    pump(300);
    const int whileDeactivated = counter.count;

    std::printf("        repaints in 300ms: active %d, deactivated %d\n", whileActive,
                whileDeactivated);
    check(whileActive > 0, "the repaint counter sees an active game working");
    check(whileDeactivated == 0, "a deactivated game asks for no repaints at all");
}

// ---- fuzzSavedGames (GHUB-0052) ----
//
// restoreState() is the app's entire untrusted input surface: SECURITY.md names
// it as the one thing parsed that the app did not write. Every bound in those
// parsers was put there by a person thinking about it, and until this block
// existed every one of them was invisible to the suite — nothing fed a
// malformed blob to any parser, so the next game, or an edit to an existing
// one, had nothing to fail against.
//
// The seed is the game's own saveState(); the mutants are the four shapes the
// roadmap named — flipped bits, truncations, inflated counts, appended garbage
// — plus an empty blob and pure noise. Two invariants, both taken from
// GameView's own contract:
//
//   refused  -> "the game keeps the fresh one it already dealt", so the
//               position must be byte-identical to the one held before the
//               attempt. A parser that writes as it reads and then bails is
//               what this catches, and it is the defect worth having.
//   accepted -> whatever it now holds must be a position it can save and load
//               again, and must survive being painted.
//
// A parser that returns false has passed. Accepting a mutant is not a failure:
// a flipped bit inside a card's rank is often still a position the rules could
// have produced.
//
// **Run this under AddressSanitizer and UndefinedBehaviorSanitizer or it proves
// very little** — an out-of-bounds read that lands in owned memory returns a
// wrong answer quietly and scores a pass here, where ASan aborts on the spot.
// Configure with -DGAMESHUB_SANITIZE=ON; `gameshub_uitest --fuzz` runs this
// block alone so that build is cheap to exercise.
QByteArray mutateBlob(const QByteArray& seed, std::mt19937& rng, int kind)
{
    QByteArray b = seed;
    const int size = int(b.size());
    // rng() is consumed directly rather than through a distribution: the
    // library decides how a distribution consumes its bits, so an identical
    // seed picks different mutants on MSVC and a CI failure stops being
    // reproducible here. The same reasoning card.cpp's shuffle is built on.
    switch (kind) {
    case 0:   // one flipped bit
        if (size > 0)
            b[int(rng() % unsigned(size))] ^= char(1u << (rng() % 8));
        return b;
    case 1:   // one byte scribbled over
        if (size > 0)
            b[int(rng() % unsigned(size))] = char(rng() & 0xFF);
        return b;
    case 2:   // truncated anywhere, including to nothing
        return b.left(size > 0 ? int(rng() % unsigned(size + 1)) : 0);
    case 3: { // garbage appended
        const int extra = 1 + int(rng() % 32);
        for (int i = 0; i < extra; ++i)
            b.append(char(rng() & 0xFF));
        return b;
    }
    case 4: { // an inflated count: QDataStream is big-endian, so a length field
              // overwritten with a huge value is what a reserve() would trust
        if (size < 4)
            return b;
        const int at = int(rng() % unsigned(size - 3));
        const bool huge = (rng() & 1u) != 0;
        b[at] = char(huge ? 0x7F : 0xFF);
        for (int i = 1; i < 4; ++i)
            b[at + i] = char(0xFF);
        return b;
    }
    case 5:   // nothing at all
        return {};
    default: { // pure noise of a plausible length
        const int len = int(rng() % 128);
        QByteArray noise;
        for (int i = 0; i < len; ++i)
            noise.append(char(rng() & 0xFF));
        return noise;
    }
    }
}

// Plays at the board without knowing which game it is, so that a game whose
// fresh position is "nothing worth keeping" has something to save. Deliberately
// ignorant of every game's rules: anything smarter would be a second copy of
// them, and would go stale the first time one changed.
void nudgeIntoPlay(GameView* view)
{
    // No pump between events: a game applies a click in its own handler, so
    // sendEvent is enough to change the position, and pumping for every one of
    // the several hundred sent here would cost the suite half a minute.
    const int kCols = 9;
    const int kRows = 7;
    const QSize size = view->size();
    const auto at = [&](int c, int r) {
        return QPointF(size.width() * (c + 0.5) / kCols, size.height() * (r + 0.5) / kRows);
    };
    const auto send = [&](QEvent::Type type, QPointF p, Qt::MouseButton button,
                          Qt::MouseButtons held) {
        QMouseEvent e(type, p, view->mapToGlobal(p), button, held, Qt::NoModifier);
        QApplication::sendEvent(view, &e);
    };
    const auto click = [&](QPointF p) {
        send(QEvent::MouseButtonPress, p, Qt::LeftButton, Qt::LeftButton);
        send(QEvent::MouseButtonRelease, p, Qt::LeftButton, Qt::NoButton);
    };
    const auto drag = [&](QPointF a, QPointF b) {
        send(QEvent::MouseButtonPress, a, Qt::LeftButton, Qt::LeftButton);
        for (int step = 1; step <= 4; ++step)
            send(QEvent::MouseMove, a + (b - a) * (step / 4.0), Qt::NoButton, Qt::LeftButton);
        send(QEvent::MouseButtonRelease, b, Qt::LeftButton, Qt::NoButton);
    };
    const auto started = [&] { return !view->saveState().isEmpty(); };

    // Single clicks: a stock pile, a Minesweeper square, a Reversi cell.
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            click(at(c, r));
            if (started())
                return;
        }
    }

    // Keys, before the dearer gestures: Sudoku takes typing, 2048 and Snake
    // take arrows.
    const int keys[] = { Qt::Key_Right, Qt::Key_Up, Qt::Key_Left, Qt::Key_Down, Qt::Key_5 };
    for (int key : keys) {
        QKeyEvent down(QEvent::KeyPress, key, Qt::NoModifier,
                       key == Qt::Key_5 ? QStringLiteral("5") : QString());
        QKeyEvent up(QEvent::KeyRelease, key, Qt::NoModifier);
        QApplication::sendEvent(view, &down);
        QApplication::sendEvent(view, &up);
        if (started())
            return;
    }

    // Select-then-move, which is how Chess, Draughts and the board games take a
    // move: one click picks a piece up and a second puts it down.
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            for (int step = 1; step <= 2; ++step) {
                if (r - step < 0)
                    continue;
                click(at(c, r));
                click(at(c, r - step));
                if (started())
                    return;
                click(at(c, r));
                click(at(std::min(c + step, kCols - 1), r - step));
                if (started())
                    return;
            }
        }
    }

    // Drags, which is how the solitaires move a card or a run.
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            for (int target = 0; target < kCols; ++target) {
                if (target == c)
                    continue;
                drag(at(c, r), at(target, r));
                if (started())
                    return;
            }
        }
    }

    // Double-click, which sends a card home in most solitaires.
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            const QPointF p = at(c, r);
            click(p);
            QMouseEvent twice(QEvent::MouseButtonDblClick, p, view->mapToGlobal(p),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(view, &twice);
            send(QEvent::MouseButtonRelease, p, Qt::LeftButton, Qt::NoButton);
            if (started())
                return;
        }
    }
}

void fuzzSavedGames(int rounds)
{
    std::printf("\n      saved-game parsers under mutation (GHUB-0052)\n");

    std::mt19937 rng(20260828u);
    HubWindow probe;
    QStringList halfLoaded;
    QStringList inconsistent;
    QStringList unplayable;
    int gamesFuzzed = 0;
    int gamesWithoutASave = 0;
    int totalMutants = 0;
    int totalAccepted = 0;

    for (const QString& name : probe.gameNames()) {
        HubWindow one;
        one.openGameNamed(name);
        one.show();
        pump(40);
        GameView* view = one.findChild<GameView*>();
        if (view == nullptr)
            continue;

        // Freezes any clock or animation for the duration. Minesweeper and
        // Sudoku save elapsedMs(), which is a RUNNING figure — two saves a
        // millisecond apart differ, and the unchanged-on-refusal check below
        // would then fail for reasons that have nothing to do with parsing.
        // deactivate() suspends the clock and is idempotent, so re-calling it
        // after each attempt keeps every comparison stable.
        QByteArray seed = view->saveState();
        if (seed.isEmpty()) {
            // Most games answer "nothing worth keeping" for a position nobody
            // has moved in, so a freshly opened one has no save to mutate. The
            // first run of this block fuzzed three games and skipped eleven,
            // which looked like coverage and was not. Poke at the board until
            // something is worth saving: clicks across the surface for the card
            // games and Minesweeper, arrows and typing for the rest. It is
            // crude on purpose -- no game-specific knowledge to go stale, and
            // any game it cannot start is named in the output rather than
            // passing quietly.
            nudgeIntoPlay(view);
            seed = view->saveState();
        }
        view->deactivate();
        seed = view->saveState();
        if (seed.isEmpty()) {
            ++gamesWithoutASave;
            unplayable << name;
            continue;
        }
        ++gamesFuzzed;

        int accepted = 0;
        int renders = 0;
        for (int round = 0; round < rounds; ++round) {
            const QByteArray mutant = mutateBlob(seed, rng, int(rng() % 7));
            view->deactivate();
            const QByteArray before = view->saveState();

            const bool taken = view->restoreState(mutant);
            ++totalMutants;
            view->deactivate();
            const QByteArray after = view->saveState();

            if (!taken) {
                if (after != before && !halfLoaded.contains(name))
                    halfLoaded << name;
            } else {
                ++accepted;
                // An accepted position must be one the game can hand back and
                // take again. An empty answer is legitimate: the mutant landed
                // on a finished game, which is "nothing worth keeping".
                if (!after.isEmpty() && !view->restoreState(after)
                    && !inconsistent.contains(name))
                    inconsistent << name;
                // Painting is where a position that parses but is not really
                // legal tends to go out of range. Bounded, because a render is
                // far dearer than a parse and this block also runs under ASan.
                if (renders < 25) {
                    ++renders;
                    QPixmap canvas(view->size());
                    canvas.fill(Qt::black);
                    view->render(&canvas);
                }
            }

            // Back to a known position for the next mutant.
            view->deactivate();
            if (!view->restoreState(seed) && !inconsistent.contains(name))
                inconsistent << name;
        }
        totalAccepted += accepted;
        std::printf("      %-12s %4d mutants, %4d refused, %4d taken\n", qPrintable(name),
                    rounds, rounds - accepted, accepted);
    }

    std::printf("      %d games with a save fuzzed, %d with nothing to save, %d mutants\n",
                gamesFuzzed, gamesWithoutASave, totalMutants);
    if (!unplayable.isEmpty())
        std::printf("      NO SAVE TO FUZZ (nudgeIntoPlay could not start them): %s\n",
                    qPrintable(unplayable.join(QStringLiteral(", "))));

    if (!halfLoaded.isEmpty())
        std::printf("      HALF-LOADED ON REFUSAL: %s\n",
                    qPrintable(halfLoaded.join(QStringLiteral(", "))));
    if (!inconsistent.isEmpty())
        std::printf("      WOULD NOT RELOAD ITS OWN SAVE: %s\n",
                    qPrintable(inconsistent.join(QStringLiteral(", "))));

    check(gamesFuzzed > 0, "at least one game offers a save to fuzz");
    check(halfLoaded.isEmpty(), "a refused save leaves the game exactly as it was");
    check(inconsistent.isEmpty(), "an accepted save is one the game can store and load again");
    // Reported rather than asserted: how many mutants a parser accepts is a
    // property of what the format happens to encode, not of this code. A bit
    // flipped inside a card's rank is usually still a legal position.
    std::printf("      %d of %d mutants were accepted as playable positions\n", totalAccepted,
                totalMutants);
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // A test-only settings identity, so running the suite never touches the
    // player's real best scores.
    QCoreApplication::setOrganizationName(QStringLiteral("GamesHubTest"));
    QCoreApplication::setApplicationName(QStringLiteral("GamesSelfTest"));

    // `--bench` runs the frame-cost figures alone. The rest of this suite takes
    // half a minute, and a painting change wants the numbers taken twice — so
    // without this the measurement costs more than the change it is judging,
    // and stops being taken.
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--bench") == 0) {
            frameCost();
            std::printf("\n%s\n", g_failures == 0 ? "Bench complete." : "FAILURES PRESENT.");
            return g_failures == 0 ? 0 : 1;
        }
        // `--fuzz [rounds]` runs the saved-game parsers alone. A sanitizer
        // build is several times slower than this one, and the rest of the
        // suite has nothing to do with parsing — so without this the ASan run
        // costs minutes and stops being taken, which is the state GHUB-0052
        // was filed about.
        if (std::strcmp(argv[i], "--fuzz") == 0) {
            int rounds = 400;
            if (i + 1 < argc) {
                const int asked = std::atoi(argv[i + 1]);
                if (asked > 0)
                    rounds = asked;
            }
            fuzzSavedGames(rounds);
            std::printf("\n%s\n", g_failures == 0 ? "Fuzz complete." : "FAILURES PRESENT.");
            return g_failures == 0 ? 0 : 1;
        }
    }

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

    // ---- canastaStackFitsItsBand (GHUB-0096)
    //
    // The house rule takes a finished canasta out of the meld row, squares it up
    // and lays it on the team's red threes, each one turned ninety degrees from
    // the one below. Two ways that goes wrong, and neither shows in a game that
    // has not made four canastas yet.
    //
    // A canasta is drawn off the end of the band, onto the centre row or under
    // the side seats' fans. What has to fit is not the card but its whole
    // footprint -- the ring is drawn outside the card and the badge is drawn
    // wider than it -- which is what canastaStackExtent is for, and asserting
    // the card alone is how the right-hand end came to hang over the edge.
    //
    // Or the slide is too small and the badges land on one another, at which
    // point the stack cannot be read at all. That one was real: measured on
    // screen at 1.34 cards of slide, a four-canasta stack was unreadable, and
    // the badge is what NAMES a canasta at this size, because the shared card
    // art draws the corner index alone below CardArt::kFaceMinWidth.
    //
    // Walked rather than played: canastaStackRect takes the count, so a stack of
    // any depth is checkable without playing one into existence.
    {
        for (const bool legible : { false, true }) {
            Legibility::instance().setEnabled(legible);
            CanastaView canasta;
            canasta.resize(canasta.minimumSize());
            const QString where = legible ? QStringLiteral("large play")
                                          : QStringLiteral("normal play");
            // The room one canasta reserves for its badge. The layout's own
            // figure, so this asserts what the code does rather than what this
            // machine's fonts happen to measure -- `windows-2022` under the
            // offscreen platform has no font environment at all.
            const double clear = canasta.smallestFaceWidth() * 1.70;

            for (int team = 0; team < canasta::kTeams; ++team) {
                const QRectF band = canasta.bandFor(team);
                // Eight is past anything a hand produces, and that is the point
                // of asking: the far end is where a layout stops fitting.
                int readable = 0;
                for (int count = 1; count <= 8; ++count) {
                    bool spaced = true;
                    QRectF previous;
                    for (int i = 0; i < count; ++i) {
                        const QRectF box = canasta.canastaStackRect(team, i, count);
                        check(band.contains(canasta.canastaStackExtent(team, i, count)),
                              qPrintable(QStringLiteral("canasta: canasta %1 of %2 keeps its ring "
                                                        "and badge inside the band (%3)")
                                             .arg(i + 1).arg(count).arg(where)));
                        // Across, then upright, then across. The alternation is
                        // the whole point of the house rule: it lets the stack
                        // be counted without reading a single index.
                        const bool across = i % 2 == 0;
                        check(across == (box.width() > box.height()),
                              qPrintable(QStringLiteral("canasta: canasta %1 lies %2, alternating "
                                                        "with the one below it (%3)")
                                             .arg(i + 1)
                                             .arg(across ? QStringLiteral("across")
                                                         : QStringLiteral("upright"))
                                             .arg(where)));
                        if (i > 0 && previous.right() - box.right() < clear)
                            spaced = false;
                        previous = box;
                    }
                    if (spaced)
                        readable = count;
                }
                // How deep a stack keeps every badge clear. It moves with the
                // window and with the band's share of it, so it is REPORTED --
                // and what is asserted is the floor the layout promises. Past
                // it the slide narrows on purpose: crowded badges are a far
                // cheaper failure than a canasta drawn off the table, and the
                // containment check above is what holds that line.
                std::printf("      canasta stack: %d canastas keep their badges clear, %s, %s\n",
                            readable, team == 0 ? "your side" : "theirs",
                            qPrintable(where));
                check(readable >= 4,
                      qPrintable(QStringLiteral("canasta: at least four canastas stack with every "
                                                "badge readable (%1)").arg(where)));
            }
        }
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
        // Each in its own scope, ending stopped. A view left running here keeps
        // its timer alive through everything below — Pinball's physics ticks
        // and repaints on every pump, which turned a 34-second suite into one
        // that had not finished in two minutes.
        {
            // Pressing Right to start used to be an instant game over: the
            // snake was laid out head-LEFT while heading right, so its first
            // step drove into its own neck. Found by GHUB-0066, and this very
            // block is why it survived -- it already pressed Right, and only
            // ever looked at the clock.
            //
            // The pump is deliberately under 200ms. gameOver() opens a modal
            // QMessageBox 200ms after a death, and a modal dialog in an
            // offscreen test does not fail, it HANGS. One step at 130ms is all
            // this needs, and it stops well short of the dialog.
            {
                SnakeView first;
                first.resize(640, 480);
                QString said;
                QObject::connect(&first, &GameView::statusChanged,
                                 [&said](const QString& text) { said = text; });
                first.activate();
                pressKey(&first, Qt::Key_Right);
                pump(190);
                check(!said.contains(QStringLiteral("Game over")),
                      "snake: pressing Right to start does not run the snake into itself");
                first.deactivate();
            }

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
        QStringList unanswered;
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
            // -1 is "nobody answered". A game that forgot to override this used
            // to answer the base's 0 and be skipped in silence, which is the
            // whole hole: the six that DO answer already satisfy the floor
            // below, so a fifteenth card game that forgot was caught by
            // nothing and would ship drawing faces too small to read.
            const double smallest = view->smallestCardWidth();
            if (smallest < 0.0) {
                unanswered << name;
                continue;
            }
            if (smallest == 0.0)
                continue;   // said by the game: it draws no cards
            ++checked;
            std::printf("      %-12s smallest card %.1f px against a %.0f px floor\n",
                        qPrintable(name), smallest, CardArt::kFaceMinWidth);
            if (smallest < CardArt::kFaceMinWidth)
                tooSmall << name;
        }
        Legibility::instance().setEnabled(false);
        check(unanswered.isEmpty(),
              "every game says whether it draws cards, so none is skipped in silence");
        if (!unanswered.isEmpty())
            std::printf("      unanswered: %s\n", qPrintable(unanswered.join(QStringLiteral(", "))));
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

    // ---- heartsCaptionClearsTheTable (GHUB-0084, GHUB-0085) ----
    //
    // Hearts reserves no caption band on purpose -- its hand is anchored to the
    // bottom, so a band would come off the cards rather than off empty table --
    // and puts the sentence in the gap between the trick and the hand instead.
    // Two things reach into that gap.
    //
    // cardWidth() is capped at 92, so past a certain width the trick stops
    // moving down the window while the bottom-aligned plate keeps rising to
    // meet it: at 900x600 the plate covered the seat-0 card by 6 pixels and at
    // 1400x620 by 8, and seat 0 is the card YOU played. And a card chosen for
    // the pass lifts out of the hand, which put all three under the plate at
    // every size tested.
    //
    // Asked of the view's own rects rather than of a mirror of its arithmetic:
    // a mirror would keep passing across exactly the change that puts the
    // caption back on the cards.
    {
        struct HeartsProbe : HeartsView {
            using HeartsView::captionArea;
            using HeartsView::handCardRect;
            using HeartsView::trickCardRect;
        };

        Legibility::instance().setEnabled(true);
        // 1900x564 is the widest, shortest shape the hub allows at its floor,
        // and it is where GHUB-0147 was reported: the trick stops moving down
        // once cardWidth() caps, while the hand stays anchored to the bottom, so
        // the gap between them is at its narrowest there.
        const QList<QSize> shapes = { { 620, 480 }, { 880, 660 }, { 900, 600 },
                                      { 1400, 620 }, { 1900, 564 } };
        QStringList covered;
        for (const QSize& shape : shapes) {
            HeartsProbe hearts;
            hearts.resize(shape);
            hearts.show();
            pump(20);

            const QRectF area = hearts.captionArea();
            const QRectF plate = Theme::captionRect(area, hearts.captionText(),
                                                    hearts.captionFont(area));
            check(!plate.isNull(), "hearts: the switch puts a sentence on the table");

            // Asserted FLATLY, with no "where the gap can hold it" clause. That
            // clause is what let GHUB-0147 stand: the overlap was excused at
            // exactly the shapes where it happened. Hearts yields instead now --
            // captionArea() returns an empty area when the plate will not fit,
            // and captionRect() of an empty area is an empty plate, which
            // overlaps nothing. So a plate that IS drawn must clear every card.
            //
            // The heights are read off the platform and reported; only the rule
            // is asserted. A runner with no font environment measures the same
            // sentence several times taller, which is precisely when the gap
            // stops holding it.
            const bool shown = !plate.isNull();
            double worst = 0.0;
            for (int seat = 0; seat < 4; ++seat) {
                const QRectF card = hearts.trickCardRect(seat);
                if (plate.intersects(card)) {
                    covered << QStringLiteral("%1x%2 seat %3")
                                   .arg(shape.width()).arg(shape.height()).arg(seat);
                }
                worst = std::max(worst, card.bottom());
            }
            std::printf("      hearts %4dx%-4d plate %.1f tall in a %.1f gap%s, top %.1f, lowest trick card %.1f\n",
                        shape.width(), shape.height(), plate.height(), area.height(),
                        shown ? "" : " (yielded: too small to hold it)", plate.top(), worst);

            // Choose cards to pass, which lifts them out of the hand. Clicked
            // on the left sliver of each card rather than its centre: the hand
            // is drawn left to right and every card but the last has its middle
            // covered by its neighbour, so a click there lands on the wrong
            // card -- which quietly made this half of the check vacuous at
            // three of the four shapes when it was first written.
            const double resting = hearts.handCardRect(0).top();
            for (int i = 0; i < 3; ++i) {
                const QRectF card = hearts.handCardRect(i);
                clickAt(&hearts, QPointF(card.left() + card.width() * 0.1, card.center().y()),
                        Qt::LeftButton);
            }
            pump(20);

            const QRectF passArea = hearts.captionArea();
            const QRectF passPlate = Theme::captionRect(passArea, hearts.captionText(),
                                                        hearts.captionFont(passArea));
            int lifted = 0;
            for (int i = 0; i < 3; ++i) {
                const QRectF card = hearts.handCardRect(i);
                if (card.top() >= resting - 1.0)
                    continue; // not chosen, so it is not what this is about
                ++lifted;
                if (passPlate.intersects(card)) {
                    covered << QStringLiteral("%1x%2 lifted pass card %3")
                                   .arg(shape.width()).arg(shape.height()).arg(i);
                }
            }
            check(lifted > 0, "hearts: choosing a card to pass lifts it out of the hand");
        }
        Legibility::instance().setEnabled(false);
        if (!covered.isEmpty())
            std::printf("      CAPTION ON TOP OF A CARD: %s\n",
                        qPrintable(covered.join(QStringLiteral(", "))));
        check(covered.isEmpty(), "hearts: the caption never lands on a card, at any window shape");
    }

    // ---- opponentStacksFitTheTable (GHUB-0092) ----
    //
    // Each opponent's hand is a short fanned stack of backs, and the fan runs
    // right and down from the seat's own rect. East's pile is anchored to the
    // right edge, so fanning right put the front card a pixel off the window
    // and cut its border away -- which paints perfectly happily and is visible
    // to nothing here, because a QPainter is glad to draw outside its widget.
    //
    // Found by looking at the first picture --shot ever took, in a game the
    // GHUB-0081 eyeball check had already been over. This is the check that
    // means the next one does not need an eye.
    {
        struct HeartsProbe : HeartsView {
            using HeartsView::opponentStackRect;
        };

        const QList<QSize> shapes = { { 620, 480 }, { 880, 660 }, { 1400, 620 }, { 1000, 900 } };
        QStringList overhanging;
        for (const QSize& shape : shapes) {
            HeartsProbe hearts;
            hearts.resize(shape);
            hearts.show();
            pump(20);

            for (int seat = 1; seat < 4; ++seat) {
                const QRectF stack = hearts.opponentStackRect(seat);
                if (!QRectF(hearts.rect()).contains(stack)) {
                    overhanging << QStringLiteral("%1x%2 seat %3 (%4..%5 of %6)")
                                       .arg(shape.width()).arg(shape.height()).arg(seat)
                                       .arg(stack.left(), 0, 'f', 1)
                                       .arg(stack.right(), 0, 'f', 1)
                                       .arg(hearts.width());
                }
            }
        }
        if (!overhanging.isEmpty())
            std::printf("      STACK OVER THE EDGE: %s\n",
                        qPrintable(overhanging.join(QStringLiteral(", "))));
        check(overhanging.isEmpty(),
              "hearts: every opponent's stack of backs is drawn inside the window");
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

    // ---- Clearing scores clears SCORES, and nothing else ----
    //
    // Every family the app stores lives in one QSettings scope, so a method
    // named for one of them can silently take the rest with it. This one used
    // to: saved games, remembered window sizes, the legibility switch and the
    // Canasta house rules all went with the high scores. Nothing calls it in
    // the app yet, which is the only reason it has not cost anyone a save --
    // GHUB-0068 anticipates the reset button that will reach for the name.
    //
    // The keys below are deliberately ones nothing reads, so the check cannot
    // disturb a later block, while still sitting in the families that matter.
    {
        Scores& scores = Scores::instance();
        const QString aSave = QStringLiteral("saved/NotARealGame");
        const QString aSize = QStringLiteral("window/geometry/NotARealPage");
        const QString aRule = QStringLiteral("canasta/house/notARealRule");

        QSettings store;
        store.setValue(aSave, QByteArray("a deal in progress"));
        store.setValue(aSize, QByteArray("a remembered size"));
        store.setValue(aRule, 4);
        store.sync();

        scores.recordHigh(Scores::pinballBestScore(), 4321);
        scores.recordHigh(QStringLiteral("chess/wins"), 7);
        scores.clear();

        check(!scores.has(Scores::pinballBestScore()), "clearing scores forgets a best");
        // chess and draughts count wins rather than a best_, so a rule written
        // for the one spelling would walk straight past them.
        check(!scores.has(QStringLiteral("chess/wins")), "clearing scores forgets a win tally too");

        QSettings after;
        check(after.value(aSave).toByteArray() == QByteArray("a deal in progress"),
              "clearing scores leaves a saved game standing");
        check(after.value(aSize).toByteArray() == QByteArray("a remembered size"),
              "clearing scores leaves a remembered window size standing");
        check(after.value(aRule).toInt() == 4,
              "clearing scores leaves a house rule standing");

        after.remove(aSave);
        after.remove(aSize);
        after.remove(aRule);
        after.sync();
        scores.clear();
    }

    // ---- A stored best that is not a number reads as no record ----
    //
    // QVariant::toInt() answers 0 for one. In the games where smaller is
    // better -- Minesweeper and Sudoku times, Spider and FreeCell moves, the
    // Hearts total -- a best of 0 is a score nobody can beat, so recordLow
    // refused every future result and the record could never be set again.
    {
        Scores& scores = Scores::instance();
        const QString key = Scores::minesweeperBestTime(0);

        QSettings store;
        store.setValue(key, QStringLiteral("not a time"));
        store.sync();

        check(!scores.has(key), "a best that is not a number reads as no record");
        check(scores.best(key, 999) == 999, "and best() answers the fallback, not zero");
        check(scores.recordLow(key, 42), "so a real time is still recorded over it");
        check(scores.best(key) == 42, "and it becomes the record");
        scores.clear();
    }

    // ---- The legibility button follows the switch it sets ----
    //
    // The button set the switch and nothing listened the other way, so anything
    // moving it without pressing the button left the toolbar reading "Normal",
    // unchecked, beside large play plainly on. --legible does exactly that,
    // before the window exists.
    {
        Legibility::instance().setEnabled(false);
        HubWindow hub;
        hub.resize(900, 700);

        QAction* legibility = nullptr;
        for (QToolBar* bar : hub.findChildren<QToolBar*>())
            for (QAction* a : bar->actions())
                if (a->text().contains(QStringLiteral("Normal"))
                    || a->text().contains(QStringLiteral("Large")))
                    legibility = a;

        check(legibility != nullptr, "the hub has a legibility button");
        if (legibility != nullptr) {
            check(!legibility->isChecked(), "which starts unchecked with the switch off");
            Legibility::instance().setEnabled(true);
            check(legibility->isChecked(), "follows the switch when something else moves it");
            check(legibility->text().contains(QStringLiteral("Large")),
                  "and carries its label across with it");
            Legibility::instance().setEnabled(false);
            check(!legibility->isChecked(), "and follows it back again");
        }
        Legibility::instance().setEnabled(false);
    }

    // ---- Answering a question about the app does not change what was chosen ----
    //
    // --shot photographs a game and exits. It used to leave the player's stored
    // legibility setting and their remembered window size rewritten behind it,
    // which is why CLAUDE.md's shot recipe needed a scratch XDG_CONFIG_HOME.
    // Each absence below is paired with the positive case, so neither can pass
    // by the mechanism simply never running.
    {
        const QString key = QStringLiteral("display/legibility");
        Legibility& legibility = Legibility::instance();
        legibility.setEnabledForSession(false);

        bool heard = false;
        const QMetaObject::Connection listening =
            QObject::connect(&legibility, &Legibility::changed, [&heard](bool) { heard = true; });

        QSettings().remove(key);
        QSettings().sync();
        legibility.setEnabledForSession(true);
        check(legibility.enabled(), "the session setter turns large play on");
        check(heard, "and still tells every built game about it");
        check(!QSettings().contains(key), "but stores nothing");

        legibility.setEnabledForSession(false);
        QSettings().remove(key);
        QSettings().sync();
        legibility.setEnabled(true);
        check(QSettings().contains(key), "while the ordinary setter does store it");
        legibility.setEnabled(false);
        QObject::disconnect(listening);
    }

    {
        // The hub writes the window size when a page is left, and opening a
        // game to photograph it goes through that path -- so even a shot that
        // is about to be refused for a bad --size had already overwritten it.
        const QString sizeKey = QStringLiteral("window/geometry/menu");
        QSettings().remove(sizeKey);
        QSettings().sync();

        {
            HubWindow quiet;
            quiet.setRemembering(false);
            quiet.resize(1000, 700);
            quiet.openGameNamed(QStringLiteral("Hearts"));
            pump(150);
        }
        check(!QSettings().contains(sizeKey),
              "a hub told not to remember writes no window size");

        {
            HubWindow ordinary;
            ordinary.resize(1000, 700);
            ordinary.openGameNamed(QStringLiteral("Hearts"));
            pump(150);
        }
        check(QSettings().contains(sizeKey),
              "while an ordinary hub still remembers where you were");

        QSettings().remove(sizeKey);
        QSettings().sync();
    }

    // ---- A game you have opened does not raise the floor for every other ----
    //
    // A QStackedWidget's minimum is the largest minimum of every page it has
    // BUILT, and each game sets its own. So opening Canasta once put its width
    // under every other game for the rest of the session: the stored size was
    // clamped up on restore, and the next page change wrote the clamped value
    // back. Permanent, and dependent on which games you happened to open.
    //
    // Measured before the fix: Chess asked 360x444 alone and 720x644 once
    // Canasta had been opened. The figures are printed rather than asserted --
    // they are properties of this machine's fonts and metrics -- and what is
    // asserted is the relationship between them, which is not.
    {
        HubWindow hub;
        hub.resize(1000, 700);
        hub.show();
        pump(150);

        hub.openGameNamed(QStringLiteral("Chess"));
        pump(150);
        const QSize alone = hub.minimumSizeHint();

        hub.openGameNamed(QStringLiteral("Canasta"));
        pump(150);
        const QSize wide = hub.minimumSizeHint();

        hub.openGameNamed(QStringLiteral("Chess"));
        pump(150);
        const QSize after = hub.minimumSizeHint();

        std::printf("      hub floor: chess alone %dx%d, canasta %dx%d, chess again %dx%d\n",
                    alone.width(), alone.height(), wide.width(), wide.height(),
                    after.width(), after.height());

        // Canasta really is the wider page, so the check below is not vacuous:
        // without that, a hub whose floor never moved at all would pass it.
        check(wide.width() > alone.width(),
              "hub: canasta really does ask for more room than chess");
        check(after == alone,
              "hub: and going back to chess gives its own floor back");
    }

    // ---- Card art hands the painter back as it found it ----
    //
    // paintSlot and paintHighlight set pen, brush and font on the CALLER's
    // painter and never put them back, and the two paths through paintFace left
    // it in different states: a cached blit touches nothing, while the live
    // fallback -- taken on any non-translate transform, or a card over 4096
    // device pixels -- draws directly and leaks the last font and ink pen. So a
    // caller that worked today broke as soon as a card was rotated or the window
    // grew. Only CanastaView wrapped its own calls; spiderview calls
    // paintHighlight bare inside a paint loop.
    {
        QPixmap canvas(240, 240);
        canvas.fill(Qt::black);
        QPainter p(&canvas);

        const QPen pen(QColor(0x11, 0x22, 0x33), 3.0);
        const QBrush brush(QColor(0x44, 0x55, 0x66));
        QFont font = p.font();
        font.setPointSizeF(17.5);
        font.setBold(true);

        const auto handsItBack = [&](const char* what, const std::function<void()>& draw) {
            p.resetTransform();
            p.setPen(pen);
            p.setBrush(brush);
            p.setFont(font);
            draw();
            check(p.pen() == pen && p.brush() == brush && p.font() == font, what);
        };

        const Card queen { Suit::Hearts, kQueen, true, 0 };
        const QRectF where(20, 20, 90, 126);

        handsItBack("cardart: paintFace leaves the painter as it found it",
                    [&] { CardArt::paintFace(p, where, queen); });
        handsItBack("cardart: paintBack too", [&] { CardArt::paintBack(p, where, 0); });
        handsItBack("cardart: and paintSlot", [&] { CardArt::paintSlot(p, where, QStringLiteral("K")); });
        handsItBack("cardart: and paintHighlight",
                    [&] { CardArt::paintHighlight(p, where, QColor(0x8f, 0xd0, 0xa8)); });
        handsItBack("chessart: paintPiece as well, which the hub tile shares",
                    [&] { ChessArt::paintPiece(p, where, chess::PieceType::Knight,
                                               chess::Colour::White); });

        // And again ROTATED, which is what forces paintFace down its live
        // fallback rather than the cached blit -- the path that actually leaked.
        // Checked separately because the cached path passes either way, so
        // testing only that would prove nothing about the defect.
        handsItBack("cardart: paintFace on a rotated painter, which is the path that leaked",
                    [&] {
                        p.rotate(30);
                        CardArt::paintFace(p, where, queen);
                    });
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
            const QImage beforeTheMove = renderOf(&freecell);
            dragBetween(&freecell, QPointF(12 + card / 2, columnTop + tall * (6 * 0.27 + 0.15)),
                        QPointF(12 + card / 2, 12 + tall / 2));

            const QImage played = renderOf(&freecell);
            check(played != beforeTheMove, "freecell: the card actually moved into the cell");

            // Undo has to put the card BACK. A drag lifts the cards off their
            // pile before the drop decides anything, so a snapshot taken at
            // drop time is a snapshot with the cards already gone -- and
            // undoing it loses them off the table altogether.
            {
                QAction* undoAction = nullptr;
                for (QAction* a : freecell.gameActions()) {
                    if (a->text().contains(QStringLiteral("Undo")))
                        undoAction = a;
                }
                check(undoAction != nullptr && undoAction->isEnabled(),
                      "freecell: the move offers an undo");
                if (undoAction != nullptr)
                    undoAction->trigger();
                check(renderOf(&freecell) == beforeTheMove,
                      "freecell: and undoing it puts the table back exactly as it was");

                if (undoAction != nullptr)
                    undoAction->setEnabled(false);
                // Put the move back, so everything below still sees a table
                // with a card in a cell.
                dragBetween(&freecell,
                            QPointF(12 + card / 2, columnTop + tall * (6 * 0.27 + 0.15)),
                            QPointF(12 + card / 2, 12 + tall / 2));
            }

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

            {
                // Reversi's legal-move dots are the whole affordance -- where
                // you may play is not otherwise marked on the board -- and they
                // ignored the legibility switch, while Draughts had already
                // rejected a faint hint as "the wrong thing to be for the player
                // the switch is for".
                //
                // everyGameAnswersTheSwitch cannot catch this, because Reversi
                // answers the switch with a caption and the picture changed
                // either way. Nor can comparing the two switch states directly:
                // the caption band comes off the height the board is laid out
                // in, so the board MOVES and every pixel differs whether or not
                // the dots did. That version of this check passed with the fix
                // reverted.
                //
                // So the dots are isolated by differencing the same board with
                // them turned on and off, at one geometry. Whatever changes is
                // the dots. The counts are printed and the RELATIONSHIP is what
                // is asserted -- the counts themselves depend on the platform's
                // font, which decides the band and so the board's size.
                ReversiView hints;
                hints.resize(520, 520);
                QAction* hintToggle = nullptr;
                for (QAction* a : hints.gameActions())
                    if (a->text().contains(QStringLiteral("Hints")))
                        hintToggle = a;
                check(hintToggle != nullptr, "reversi: the board offers a Hints switch");

                const auto dotPixels = [&](bool large) {
                    Legibility::instance().setEnabled(large);
                    hintToggle->setChecked(true);
                    const QImage shown = renderOf(&hints);
                    hintToggle->setChecked(false);
                    const QImage hidden = renderOf(&hints);
                    hintToggle->setChecked(true);
                    int differing = 0;
                    for (int y = 0; y < shown.height(); ++y)
                        for (int x = 0; x < shown.width(); ++x)
                            if (shown.pixel(x, y) != hidden.pixel(x, y))
                                ++differing;
                    return differing;
                };

                if (hintToggle != nullptr) {
                    const int plainDots = dotPixels(false);
                    const int largeDots = dotPixels(true);
                    Legibility::instance().setEnabled(false);
                    std::printf("      reversi hint dots: %d pixels normal, %d large\n",
                                plainDots, largeDots);
                    check(plainDots > 0, "reversi: the board marks where you may play");
                    check(largeDots > plainDots,
                          "reversi: and large play makes those marks bigger, not just the caption");
                }
            }

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

            {
                // Two capture chains from one square that finish on the SAME
                // square and take DIFFERENT men. The board used to play the
                // first one it found and drew the destination dot twice in the
                // same place, so nothing said a choice existed and the player
                // could not make it. The position is the one
                // draughtsTwoChainsCanEndOnOneSquare proves is legal.
                Legibility::instance().setEnabled(false);
                QByteArray blob;
                QDataStream out(&blob, QIODevice::WriteOnly);
                out.setVersion(QDataStream::Qt_6_0);
                out << quint32(1) << qint8(0) << qint8(0) << qint8(0) << qint8(0);
                std::array<qint8, 64> board {};
                const auto put = [&board](int row, int col, int piece) {
                    board[std::size_t(row * 8 + col)] = qint8(piece);
                };
                put(5, 2, 1);   // RedMan
                put(4, 1, 3);   // WhiteMan
                put(2, 1, 3);
                put(4, 3, 3);
                put(2, 3, 3);
                for (qint8 c : board)
                    out << c;

                DraughtsView choose;
                choose.resize(560, 560);
                QString said;
                QObject::connect(&choose, &GameView::statusChanged,
                                 [&said](const QString& t) { said = t; });
                check(choose.restoreState(blob), "draughts: the two-route position loads");

                const QByteArray settled = choose.saveState();
                clickAt(&choose, cellCentre(&choose, 5, 2), Qt::LeftButton);
                clickAt(&choose, cellCentre(&choose, 1, 2), Qt::LeftButton);
                check(choose.saveState() == settled,
                      "draughts: clicking a square two chains reach plays neither of them");
                check(said.contains(QStringLiteral("Two ways")),
                      "draughts: it asks which route instead of choosing for you");

                // (3,4) is the first landing square of the RIGHT-hand route.
                // Deliberately not the left one: the board's old arbitrary pick
                // was the left, so choosing that would agree with the bug and
                // the two checks below would pass either way.
                clickAt(&choose, cellCentre(&choose, 3, 4), Qt::LeftButton);
                const QByteArray after = choose.saveState();
                check(after != settled, "draughts: naming the route plays it");

                // And it played THAT route: the two men on the right are still
                // standing, which is the whole point of being asked.
                QDataStream back(after);
                back.setVersion(QDataStream::Qt_6_0);
                quint32 version = 0;
                qint8 toMove = 0;
                qint8 human = 0;
                qint8 level = 0;
                qint8 hasLast = 0;
                back >> version >> toMove >> human >> level >> hasLast;
                if (hasLast == 1) {
                    qint8 r = 0;
                    qint8 c = 0;
                    back >> r >> c;
                    quint8 count = 0;
                    back >> count;
                    for (int i = 0; i < int(count) * 2; ++i)
                        back >> r;
                    back >> count;
                    for (int i = 0; i < int(count) * 2; ++i)
                        back >> r;
                }
                std::array<qint8, 64> now {};
                for (qint8& c : now)
                    back >> c;
                check(now[std::size_t(4 * 8 + 1)] == 3 && now[std::size_t(2 * 8 + 1)] == 3,
                      "draughts: and left standing the men the other route would have taken");
                check(now[std::size_t(4 * 8 + 3)] == 0 && now[std::size_t(2 * 8 + 3)] == 0,
                      "draughts: having taken the two the route you named was for");
            }
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

        // ---- And the two that used to lose everything (GHUB-0007, GHUB-0009) ----
        //
        // Hearts and Sudoku were the last two long games with no save at all:
        // closing the app lost a hand of Hearts and a part-solved puzzle
        // outright. Neither keeps a move log, so both write the position and
        // the render is the proof it came back whole.
        {
            HeartsView table;
            table.resize(880, 660);
            const QImage dealt = renderOf(&table);
            const QByteArray saved = table.saveState();
            check(!saved.isEmpty(), "hearts: a hand in progress is worth saving");

            HeartsView resumed;
            resumed.resize(table.size());
            const QImage brandNew = renderOf(&resumed);
            check(resumed.restoreState(saved), "hearts: and it reads back");
            check(renderOf(&resumed) == dealt, "hearts: onto the very same hand");
            // The deal is random, so a fresh table is a different thirteen
            // cards -- which is what makes the line above mean something.
            check(brandNew != dealt, "hearts: which is not just a new deal by another name");

            check(!resumed.restoreState(QByteArray("thirteen tricks of nothing")),
                  "hearts: a corrupt save is refused");
            check(renderOf(&resumed) == dealt, "hearts: and refusing one changes nothing");

            // The count is what stands in for a pack check here: Hearts takes
            // four cards out of play with every trick collected, so it cannot
            // ask for the whole pack back. A well-formed blob claiming nine
            // tricks played while holding fifty-two cards is the only way to
            // reach that path, and without the count it would restore a full
            // hand into the endgame.
            QByteArray forged(saved);
            {
                QDataStream in(saved);
                in.setVersion(QDataStream::Qt_6_0);
                quint32 version = 0;
                in >> version;
                check(version == 1, "hearts: the save carries the version this check forges against");
            }
            // The trick count is one byte, six from the end of the engine's
            // block: phase, hand, current, leader, lastWinner, tricksPlayed,
            // heartsBroken, then the view's own tail.
            const int tail = 1 /*heartsBroken*/ + 4 + 1 + 1 /*pile length + collect + announced*/;
            forged[forged.size() - tail - 1] = char(9);
            check(!resumed.restoreState(forged),
                  "hearts: a save whose cards do not match its trick count is refused");
            check(renderOf(&resumed) == dealt, "hearts: and refusing that changes nothing either");
        }

        {
            SudokuView puzzle;
            puzzle.resize(560, 600);
            // Something to come back TO: an answer, a pencil mark and the
            // cursor moved off centre. All three are what the save is for.
            pressKey(&puzzle, Qt::Key_Right);
            pressKey(&puzzle, Qt::Key_Down);
            for (int i = 0; i < SudokuGrid::kCells; ++i)
                pressKey(&puzzle, Qt::Key_5);
            const QImage worked = renderOf(&puzzle);
            const QByteArray saved = puzzle.saveState();
            check(!saved.isEmpty(), "sudoku: a puzzle in progress is worth saving");

            SudokuView resumed;
            resumed.resize(puzzle.size());
            const QImage brandNew = renderOf(&resumed);
            check(resumed.restoreState(saved), "sudoku: and it reads back");
            check(renderOf(&resumed) == worked, "sudoku: onto the very same grid");
            check(brandNew != worked, "sudoku: which is not just a new puzzle by another name");

            check(!resumed.restoreState(QByteArray("nine by nine of nothing")),
                  "sudoku: a corrupt save is refused");
            check(renderOf(&resumed) == worked, "sudoku: and refusing one changes nothing");

            // Sudoku has no pack, so what refuses a board the game could not
            // have reached is the solution itself: it must be a completed grid,
            // and every clue must agree with it. A well-formed blob whose
            // solution repeats a digit in a row is the only way to reach that.
            QByteArray forged;
            QDataStream out(&forged, QIODevice::WriteOnly);
            out.setVersion(QDataStream::Qt_6_0);
            out << quint32(1);
            for (int pass = 0; pass < 3; ++pass) {
                for (int i = 0; i < SudokuGrid::kCells; ++i)
                    out << qint8(pass == 0 ? 1 : 0);   // a solution of all ones
            }
            for (int i = 0; i < SudokuGrid::kCells; ++i)
                out << quint16(0);
            out << qint8(0) << qint8(0) << qint8(0) << qint8(0) << qint8(1) << qint8(0)
                << qint8(0) << qint64(0);
            check(!resumed.restoreState(forged),
                  "sudoku: a save whose solution is not a solution is refused");
            check(renderOf(&resumed) == worked, "sudoku: and refusing that changes nothing either");
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

    // A tile paints its own miniature and carries no text, so without an
    // accessible name QAccessibleButton has nothing to report and a screen
    // reader meets fourteen unnamed buttons on the first page of the app.
    // Checked against the registered names rather than a count, so a tile that
    // is named after the wrong game fails too.
    {
        QStringList named;
        for (QPushButton* tile : tiles)
            if (!tile->accessibleName().isEmpty())
                named << tile->accessibleName();
        named.sort();
        QStringList wanted = hub.gameNames();
        wanted.sort();
        check(named == wanted, "every tile carries the accessible name of the game it opens");

        bool described = true;
        for (QPushButton* tile : tiles)
            if (tile->accessibleDescription().isEmpty())
                described = false;
        check(described, "and a description a screen reader can read after it");
    }

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

        // The same, on a font whose size is set in PIXELS. QFont carries either
        // a point size or a pixel size and answers -1 for the other, so adding
        // points to a pixel-sized font used to produce a 2.0pt dialog -- and
        // the large-play branch, the one meant to make things bigger, was the
        // worst affected. Compared in whichever unit the font carries, so this
        // asserts what the code does and borrows nothing from the platform's
        // font metrics.
        {
            const QFont wasAppFont = QApplication::font();
            QFont pixels = wasAppFont;
            pixels.setPixelSize(13);
            QApplication::setFont(pixels);

            const auto sizeOf = [](const QFont& f) {
                return f.pointSizeF() > 0.0 ? f.pointSizeF() : double(f.pixelSize());
            };
            const double appSize = sizeOf(pixels);

            Legibility::instance().setEnabled(false);
            DonateDialog plain(false, nullptr);
            Legibility::instance().setEnabled(true);
            DonateDialog grown(false, nullptr);
            Legibility::instance().setEnabled(false);

            check(sizeOf(plain.font()) >= appSize,
                  "donate: a pixel-sized font is never shrunk by the dialog");
            check(sizeOf(grown.font()) > appSize,
                  "donate: and large play still grows it");

            // The heading grows unconditionally, so on a pixel-sized font it
            // was wrong for every reader rather than only in the large branch.
            auto* heading = plain.findChild<QLabel*>(QStringLiteral("donateHeading"));
            check(heading != nullptr, "donate: the dialog has a heading");
            if (heading != nullptr) {
                check(sizeOf(heading->font()) > appSize,
                      "donate: and the heading is larger than the body, not smaller");
            }

            QApplication::setFont(wasAppFont);
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

    // ---- cardArtKeyDecidesThePicture (GHUB-0048) ----
    //
    // The card art cache rounds a card's size to whole device pixels to build
    // its key. So two rects a fraction of a pixel apart share one entry, and
    // the entry must hold the SAME picture whichever of them is drawn first.
    //
    // The first cut keyed on the rounded size and drew at the exact one, so the
    // picture depended on which rect got there first -- which meant a frame
    // drawn after the cache had been emptied differed from the same frame drawn
    // before. It surfaced as FreeCell failing to go back pixel for pixel across
    // the legibility switch, and only because enough games had run first to
    // fill the cache. That is luck, not a guard. This asks directly, and it was
    // confirmed to go red against that defect before being kept.
    {
        auto draw = [](const Card& c, double w, double h, bool faceUp) {
            QImage img(int(w) + 40, int(h) + 40, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::darkGray);
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::TextAntialiasing, true);
            p.translate(20.0, 20.0);
            if (faceUp)
                CardArt::paintFace(p, QRectF(0, 0, w, h), c);
            else
                CardArt::paintBack(p, QRectF(0, 0, w, h), c.deck);
            return img;
        };
        // Both caches are bounded and drop everything when full, which is how
        // the test gets a cold one to work with. Every junk card is drawn BOTH
        // ways: an earlier cut alternated, which put 150 faces against a
        // 160-entry bound, so the face cache never actually emptied and that
        // half of the check passed without testing anything -- confirmed by
        // reintroducing the defect and watching only the back half go red.
        //
        // Junk heights follow the real card aspect, which is what keeps them
        // off the subject's key: a junk card 62 wide is 86.8 tall and rounds to
        // 87, where the subject rounds to 86.
        auto empty = [&draw] {
            for (int i = 0; i < 400; ++i) {
                // The deck must NOT be derived from i's parity: the width
                // below already is, so `i % 2` gave every width one deck and
                // only 60 distinct back keys against a 64-entry bound -- the
                // back cache then never emptied and that half of this check
                // passed against the very defect it was written for.
                const Card junk { .suit = static_cast<Suit>(i % 4),
                                  .rank = 1 + (i % 13),
                                  .faceUp = true,
                                  .deck = (i / 60) % 2 };
                const double w = 30.0 + (i % 60);
                draw(junk, w, w * CardArt::kAspect, true);
                draw(junk, w, w * CardArt::kAspect, false);
            }
        };

        // 61.6 and 62.4 both round to 62; the height is held equal so the two
        // land on one key rather than two.
        const Card subject { .suit = Suit::Hearts, .rank = 7 };
        for (int pass = 0; pass < 2; ++pass) {
            const bool faceUp = pass == 0;

            empty();
            const QImage alone = draw(subject, 61.6, 86.4, faceUp);

            empty();
            draw(subject, 62.4, 86.4, faceUp);   // fills the shared entry first
            const QImage after = draw(subject, 61.6, 86.4, faceUp);

            check(alone == after,
                  faceUp ? "a card face is the same picture whichever rect filled its cache entry"
                         : "and so is a card back");
        }
    }

    // ---- cardsShowTheirJourney (GHUB-0065) --------------------------------
    //
    // Motion is information: it answers *what just changed and where did it
    // go*, which a redraw of the finished position cannot, because by the time
    // you look the change is over. Three things are checked, and the first is
    // the one no picture could answer — hence GameView's flightsInTheAir().
    {
        // The helper first, on its own, because two of its three rules are the
        // ones Canasta paid real time to learn.
        std::vector<cardflight::Flight> flights;
        cardflight::Flight a;
        a.card = Card { .suit = Suit::Hearts, .rank = 7, .faceUp = true, .deck = 0 };
        a.from = QPointF(0, 0);
        a.to = QPointF(100, 0);
        a.speed = 1.0;
        flights.push_back(a);

        check(cardflight::positionOf(flights.front()) == QPointF(0, 0),
              "a flight starts where the card was");
        check(cardflight::advance(flights, 0.5), "and is still in the air half way");
        const double half = cardflight::positionOf(flights.front()).x();
        check(half > 40.0 && half < 60.0, "eased, so half the time is about half the distance");
        check(!cardflight::advance(flights, 0.6), "it lands and is dropped");
        check(flights.empty(), "leaving nothing behind to draw");

        // A delayed flight must not lose the part of the tick that ran out its
        // wait, or a staggered run gains a frame of lag per card.
        flights.clear();
        cardflight::Flight late = a;
        late.delay = 0.01;
        flights.push_back(late);
        cardflight::advance(flights, 0.51);
        check(cardflight::positionOf(flights.front()).x() > 40.0,
              "a delay spends what is left of the tick on the journey");

        // The trap that cost Canasta: two identical cards arriving together
        // must suppress two destination copies, not one of them twice. Two
        // packs are shuffled together in this project, so this is routine.
        flights.clear();
        flights.push_back(a);
        cardflight::Flight twin = a;
        twin.card.deck = 1;   // deliberately outside operator==
        flights.push_back(twin);
        std::vector<char> consumed;
        consumed.assign(flights.size(), 0);
        check(cardflight::suppressAt(flights, consumed, -1, a.card),
              "a card on its way is suppressed at its destination");
        check(cardflight::suppressAt(flights, consumed, -1, a.card),
              "and an identical second one suppresses the SECOND copy");
        check(!cardflight::suppressAt(flights, consumed, -1, a.card),
              "a third asks for a flight that is not there and is refused");
        consumed.assign(flights.size(), 0);
        check(!cardflight::suppressAt(flights, consumed, 2, a.card),
              "suppression is per destination, not per card");

        // Now the real views. A Klondike or FreeCell deal only sometimes leaves
        // an ace where a double-click can reach it, so this opens fresh games
        // until one takes the move — and says so if none ever does, rather than
        // passing on a game where nothing was ever tried.
        const auto doubleClickEverywhere = [](GameView* view, const auto& airborne) {
            for (int r = 0; r < 8; ++r) {
                for (int c = 0; c < 9; ++c) {
                    const QPointF p(view->width() * (c + 0.5) / 9.0,
                                    view->height() * (r + 0.5) / 8.0);
                    QMouseEvent press(QEvent::MouseButtonPress, p, view->mapToGlobal(p),
                                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QMouseEvent twice(QEvent::MouseButtonDblClick, p, view->mapToGlobal(p),
                                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QMouseEvent release(QEvent::MouseButtonRelease, p, view->mapToGlobal(p),
                                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QApplication::sendEvent(view, &press);
                    QApplication::sendEvent(view, &release);
                    QApplication::sendEvent(view, &twice);
                    QApplication::sendEvent(view, &release);
                    if (airborne())
                        return true;
                }
            }
            return false;
        };

        int klondikeTries = 0;
        bool klondikeFlew = false;
        for (; klondikeTries < 40 && !klondikeFlew; ++klondikeTries) {
            KlondikeView v;
            v.resize(820, 620);
            v.show();
            pump(10);
            klondikeFlew = doubleClickEverywhere(&v, [&v] { return v.flightsInTheAir() > 0; });
            if (!klondikeFlew)
                continue;

            // It must land on its own, and the timer must not outlive it.
            pump(1200);
            check(v.flightsInTheAir() == 0, "klondike: a card sent home lands and stops flying");
            check(activeTimers(&v) == 0, "klondike: and the flight timer stops once it has");
        }
        check(klondikeFlew, "klondike: double-clicking an ace sends it home with a visible journey");

        int freecellTries = 0;
        bool freecellFlew = false;
        for (; freecellTries < 40 && !freecellFlew; ++freecellTries) {
            FreeCellView v;
            v.resize(900, 640);
            v.show();
            pump(10);
            freecellFlew = doubleClickEverywhere(&v, [&v] { return v.flightsInTheAir() > 0; });
            if (!freecellFlew)
                continue;

            // deactivate() must clear the air, not merely pause it: a flight
            // carries a destination captured when the card left, and the hub
            // resizes this page the moment it leaves.
            check(v.flightsInTheAir() > 0, "freecell: a card is in the air to begin with");
            v.deactivate();
            check(v.flightsInTheAir() == 0, "freecell: and leaving the game clears the air");
            check(activeTimers(&v) == 0, "freecell: with no timer left running behind it");
        }
        check(freecellFlew, "freecell: double-clicking an ace sends it home with a visible journey");

        std::printf("      klondike found a home-able ace in %d deals, freecell in %d\n",
                    klondikeTries, freecellTries);

        // Spider is the starkest case in the bullet — thirteen cards leave a
        // column at once — and the only one no amount of poking at a random
        // deal will reach. So the position is BUILT: a one-suit game with King
        // down to Two in the first column and the Ace sitting alone in the
        // second, which is one drag away from a completed run.
        {
            std::vector<Card> deck = makeDeck(2, 1);
            std::array<std::vector<Card>, 14> byRank {};
            for (Card c : deck) {
                c.faceUp = true;
                if (c.rank >= 1 && c.rank <= 13)
                    byRank[std::size_t(c.rank)].push_back(c);
            }

            std::array<std::vector<Card>, 10> columns {};
            bool everyRankPresent = true;
            for (int rank = kKing; rank >= 2; --rank) {
                if (byRank[std::size_t(rank)].empty()) {
                    everyRankPresent = false;
                    break;
                }
                columns[0].push_back(byRank[std::size_t(rank)].back());
                byRank[std::size_t(rank)].pop_back();
            }
            check(everyRankPresent && !byRank[std::size_t(kAce)].empty(),
                  "spider: a two-pack one-suit deck has every rank to build a run from");
            columns[1].push_back(byRank[std::size_t(kAce)].back());
            byRank[std::size_t(kAce)].pop_back();

            // Everything else has to come back too: Spider's own restore
            // refuses a position that is not two whole packs less the runs
            // already taken off.
            std::vector<Card> rest;
            for (std::vector<Card>& bucket : byRank)
                rest.insert(rest.end(), bucket.begin(), bucket.end());
            std::vector<Card> stock;
            for (std::size_t i = 0; i < rest.size(); ++i) {
                if (i < 40)
                    columns[2 + (i % 8)].push_back(rest[i]);
                else
                    stock.push_back(rest[i]);
            }

            QByteArray blob;
            QDataStream out(&blob, QIODevice::WriteOnly);
            out.setVersion(QDataStream::Qt_6_0);
            out << quint32(1) << qint32(1) << qint32(0) << qint32(1);
            for (const std::vector<Card>& col : columns)
                cardcodec::writePile(out, col);
            cardcodec::writePile(out, stock);

            bool harvested = false;
            // The Ace's exact rect is the view's own business, so the drag is
            // swept over the second column's strip until one takes. Same
            // reasoning as nudgeIntoPlay: no copy of the layout to go stale.
            for (int attempt = 0; attempt < 12 && !harvested; ++attempt) {
                SpiderView v;
                v.resize(900, 640);
                v.show();
                pump(10);
                if (!v.restoreState(blob)) {
                    check(false, "spider: the built position is one the rules could have reached");
                    break;
                }
                pump(10);

                const QPointF from(v.width() * 1.5 / 10.0, 24.0 + attempt * 12.0);
                const QPointF to(v.width() * 0.5 / 10.0, v.height() * 0.25);
                QMouseEvent press(QEvent::MouseButtonPress, from, v.mapToGlobal(from),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(&v, &press);
                for (int step = 1; step <= 4; ++step) {
                    const QPointF at = from + (to - from) * (step / 4.0);
                    QMouseEvent move(QEvent::MouseMove, at, v.mapToGlobal(at), Qt::NoButton,
                                     Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(&v, &move);
                }
                QMouseEvent release(QEvent::MouseButtonRelease, to, v.mapToGlobal(to),
                                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(&v, &release);

                if (v.flightsInTheAir() == 0)
                    continue;
                harvested = true;
                check(v.flightsInTheAir() == SpiderTable::kRunLength,
                      "spider: a completed run sends every one of its thirteen cards on its way");
                // Staggered, so it reads as a sequence rather than one smear —
                // which means it must still be going after a tick that would
                // have finished a single unstaggered card.
                pump(200);
                check(v.flightsInTheAir() > 0, "spider: and they leave in order rather than at once");
                pump(3000);
                check(v.flightsInTheAir() == 0, "spider: the whole run arrives and stops flying");
                check(activeTimers(&v) == 0, "spider: with no timer left running behind it");
            }
            check(harvested, "spider: completing a run is visible as thirteen cards leaving");
        }
    }

    // ---- theWindowAnswersWhileTheComputerThinks (GHUB-0047) ---------------
    //
    // Every engine used to search inside the signal handler that started it, on
    // the thread that also paints and handles input, so the window was
    // genuinely frozen for the duration. The measure is RELATIVE and
    // deliberately so: how fast a timer ticks is a property of the machine, and
    // this project has three red Windows legs to show for asserting an
    // environment constant. So an idle view is timed first and the thinking one
    // is held against it. On the old build the busy figure was near zero for
    // the length of the search; a slow runner moves both numbers together.
    {
        // The LONGEST GAP between ticks, not how many there were. Counting
        // ticks was the first attempt and it passed on the unfixed build --
        // 50 against an idle 60 -- because a single 100ms freeze inside a
        // 300ms window still leaves most of the ticks in place. What the
        // player feels is the length of one stretch with no response, so that
        // is what is measured.
        const auto worstGapOver = [](int ms) {
            QElapsedTimer clock;
            clock.start();
            qint64 last = 0;
            qint64 worst = 0;
            QTimer heartbeat;
            heartbeat.setInterval(5);
            QObject::connect(&heartbeat, &QTimer::timeout, [&] {
                worst = std::max(worst, clock.elapsed() - last);
                last = clock.elapsed();
            });
            heartbeat.start();
            while (clock.elapsed() < ms)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
            heartbeat.stop();
            // The tail counts too: a freeze still running when the window
            // closes has produced no tick to measure it with.
            return std::max(worst, clock.elapsed() - last);
        };

        // A real middlegame rather than the opening, so the search has work to
        // do. The moves are played through the engine's own game to guarantee
        // they are legal, then written in the format ChessView saves.
        chess::ChessGame game;
        std::vector<chess::Move> line;
        for (int ply = 0; ply < 21 && !game.isOver(); ++ply) {
            const std::vector<chess::Move> legal = game.legalMoves();
            if (legal.empty())
                break;
            // The middle of the list rather than the front: taking the first
            // every time walks into a shuffling draw that gives the engine
            // almost nothing to think about.
            const chess::Move m = legal[legal.size() / 2];
            line.push_back(m);
            game.play(m);
        }

        QByteArray blob;
        QDataStream out(&blob, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        // Level 2 is Hard, which carries the largest node budget and so the
        // longest freeze this game was ever capable of.
        out << quint32(1) << qint32(2) << qint32(line.size());
        for (const chess::Move& m : line) {
            out << qint8(m.from.row) << qint8(m.from.col) << qint8(m.to.row) << qint8(m.to.col)
                << qint8(m.promotion);
        }

        ChessView chess;
        chess.resize(620, 620);
        chess.show();
        pump(30);
        const qint64 idle = worstGapOver(1200);

        const bool loaded = chess.restoreState(blob);
        check(loaded, "chess: a middlegame position with the computer to move loads");
        const qint64 busy = loaded ? worstGapOver(1200) : 0;

        std::printf("      longest stretch with no response: idle %lldms, "
                    "while the computer thinks %lldms\n",
                    static_cast<long long>(idle), static_cast<long long>(busy));
        // Relative, with slack, because a heartbeat's own jitter belongs to the
        // machine and this project has three red Windows legs from asserting an
        // environment constant. The unfixed build froze for the whole search --
        // a whole multiple of this bound, not a nudge past it.
        check(idle < 60, "chess: an idle window answers promptly to begin with");
        check(busy <= idle * 3 + 40,
              "chess: the window never stops answering for long while the computer thinks");

        // And the answer still arrives — a responsive window that never plays
        // is not the win. WAITED FOR rather than timed: a fixed pump asserts
        // how fast the machine is, and a first attempt at 4000ms duly failed
        // under the sanitizer build, where the same search takes several times
        // longer. The cap is only there so a genuine hang ends the run.
        QElapsedTimer arriving;
        arriving.start();
        while (chess.saveState() == blob && arriving.elapsed() < 60000)
            pump(50);
        std::printf("      the computer's move arrived after %lldms\n",
                    static_cast<long long>(arriving.elapsed()));
        check(chess.saveState() != blob,
              "chess: and the move it was working out is actually played");

        // Abandonment, which the roadmap named as where this kind of change
        // goes wrong. Every route that makes a search in flight answer the
        // wrong question is taken while it is still running, and the last of
        // them destroys the view outright — the one that would be a
        // use-after-free rather than a wrong move. Worth running under
        // -DGAMESHUB_SANITIZE=ON, where a worker still holding view state
        // aborts instead of getting away with it.
        for (int round = 0; round < 6; ++round) {
            ChessView churn;
            churn.resize(620, 620);
            churn.show();
            check(churn.restoreState(blob), "chess: the churn position loads");
            pump(30);   // long enough for the search to have set off

            switch (round % 3) {
            case 0:
                churn.deactivate();     // the hub leaves mid-search
                churn.activate();
                break;
            case 1:
                for (QAction* a : churn.gameActions()) {
                    if (a->isCheckable() && a->text() == QStringLiteral("Easy"))
                        a->trigger();   // a level change mid-search
                }
                break;
            default:
                for (QAction* a : churn.gameActions()) {
                    if (a->text() == QStringLiteral("New Game"))
                        a->trigger();   // a new game mid-search
                }
                break;
            }
            pump(20);
            // ...and then the view goes away with a worker still running.
        }
        check(true, "chess: abandoning a search mid-flight leaves nothing behind");
        pump(2500);   // let every abandoned worker finish into a dead view
    }

    // ---- the engine's stale timer (GHUB-0140) ----
    //
    // advance() hands the turn over with QTimer::singleShot, and nothing in
    // the three engine games can call that shot back once it is in the air.
    // New Game, Undo, a level change and the hub leaving the page all go
    // through abandonSearch(), which bumps a generation and clears m_thinking
    // -- but the timer still fires, and before the fix it went straight into
    // startSearch(). That captures the board AS IT IS NOW together with the
    // CURRENT generation, so engineMoveReady's generation test passed and the
    // engine played a move for the player's own side. The board was then left
    // waiting for a side whose turn had already been taken, which is the
    // deadlock that was reported.
    //
    // Four things are locked, for each of the three games:
    //
    //   INV-1  A move the player makes puts the game on the computer's clock.
    //          That is what schedules the timer, so without it everything
    //          below is asserting about a timer that was never set.
    //   INV-2  Left standing, that move is answered. The answer is TIMED, and
    //          the figure is what sets the wait INV-3 and INV-4 use -- a wait
    //          shorter than a search would let a broken guard through.
    //   INV-3  Undone before the timer fires, the board is still exactly as
    //          Undo left it once that wait has passed: no piece was moved on
    //          the player's behalf.
    //   INV-4  And the game still says it is the player's to move, rather than
    //          sitting on a clock that nobody can run down.
    //
    // The position is private to each view, so the observation is the public
    // surface: the rendered board and the statusChanged line. No wall-clock
    // constant is asserted anywhere here -- the wait is derived from what the
    // engine on THIS machine has just been measured taking, which is the rule
    // the three red Windows legs taught.
    {
        const bool wasLegible = Legibility::instance().enabled();
        // The click points below mirror each view's boardRect with the switch
        // OFF; with it on the board gives up a strip at the bottom and every
        // one of them lands a row out.
        Legibility::instance().setEnabled(false);

        const auto square = [](const QWidget* w, int row, int col) {
            const int side = ((std::min(w->width(), w->height()) - 2 * (18 + 4)) / 8) * 8;
            const double cell = side / 8.0;
            return QPointF((w->width() - side) / 2.0 + (col + 0.5) * cell,
                           (w->height() - side) / 2.0 + (row + 0.5) * cell);
        };
        const auto cellCentre = [](const QWidget* w, int row, int col) {
            const int available = std::min(w->width(), w->height()) - 2 * (10 + 4);
            const int side = std::max(8, (available / 8) * 8);
            const double cell = side / 8.0;
            return QPointF((w->width() - side) / 2 + (col + 0.5) * cell,
                           (w->height() - side) / 2 + (row + 0.5) * cell);
        };

        // Two views per game rather than one: the control keeps its move and
        // the subject takes it back, and the two have to be independent for
        // the timing off the first to mean anything about the second.
        const auto lockStaleTimer = [&](const char* game, GameView* control, GameView* subject,
                                        auto playAMove) {
            const auto say = [game](const char* what) {
                return QStringLiteral("%1: %2")
                    .arg(QString::fromUtf8(game), QString::fromUtf8(what))
                    .toUtf8();
            };

            // INV-2. Waited for rather than timed out: a fixed pump asserts how
            // fast the machine is, and the sanitizer build is several times
            // slower than this one. The cap is only there so a hang ends the
            // run instead of the suite.
            playAMove(control);
            const QImage waiting = renderOf(control);
            QElapsedTimer clock;
            clock.start();
            while (renderOf(control) == waiting && clock.elapsed() < 30000)
                pump(25);
            const qint64 replyMs = clock.elapsed();
            check(renderOf(control) != waiting,
                  say("the engine answers a move that is left standing").constData());

            // INV-1.
            QString status;
            QObject::connect(subject, &GameView::statusChanged,
                             [&status](const QString& text) { status = text; });
            playAMove(subject);
            check(status.contains(QStringLiteral("thinking")),
                  say("the player's move puts the game on the computer's clock").constData());

            QAction* undo = nullptr;
            for (QAction* a : subject->gameActions()) {
                if (a->text() == QStringLiteral("Undo"))
                    undo = a;
            }
            check(undo != nullptr && undo->isEnabled(),
                  say("the move offers an undo").constData());
            if (undo == nullptr)
                return;

            // Taken back before the event loop has been pumped at all, so the
            // search is still only SCHEDULED -- which is the one state
            // abandonSearch() cannot reach into.
            undo->trigger();
            const QString settled = status;
            const QImage board = renderOf(subject);

            // Three times the measured reply and never under a second and a
            // half, so a search set off by the stale timer has had ample room
            // to land its move.
            const int wait = int(std::max<qint64>(replyMs * 3 + 500, 1500));
            pump(wait);

            const QImage later = renderOf(subject);
            int changed = 0;
            for (int y = 0; y < board.height(); ++y)
                for (int x = 0; x < board.width(); ++x)
                    if (board.pixel(x, y) != later.pixel(x, y))
                        ++changed;

            // Printed on every run, pass or fail: the log alone then says which
            // game moved, by how much, and what the game thought it was doing.
            std::printf("      %s: engine reply %lldms, waited %dms after the undo; "
                        "%d pixels changed (expected 0)\n",
                        game, static_cast<long long>(replyMs), wait, changed);
            std::printf("      %s: status was \"%s\", is now \"%s\"\n", game,
                        qUtf8Printable(settled), qUtf8Printable(status));

            check(changed == 0,
                  say("the search scheduled before the undo never moves a piece").constData());
            check(status == settled,
                  say("and the game is still the player's to move").constData());
        };

        {
            ChessView control;
            ChessView subject;
            control.resize(640, 640);
            subject.resize(640, 640);
            lockStaleTimer("chess", &control, &subject, [&](GameView* v) {
                clickAt(v, square(v, 6, 4), Qt::LeftButton);   // the pawn on e2
                clickAt(v, square(v, 4, 4), Qt::LeftButton);   // push it to e4
            });
        }

        {
            ReversiView control;
            ReversiView subject;
            control.resize(520, 520);
            subject.resize(520, 520);
            lockStaleTimer("reversi", &control, &subject, [&](GameView* v) {
                clickAt(v, cellCentre(v, 2, 3), Qt::LeftButton);   // a legal opening
            });
        }

        {
            DraughtsView control;
            DraughtsView subject;
            control.resize(560, 560);
            subject.resize(560, 560);
            lockStaleTimer("draughts", &control, &subject, [&](GameView* v) {
                // Red starts at the bottom and moves up.
                clickAt(v, cellCentre(v, 5, 2), Qt::LeftButton);
                clickAt(v, cellCentre(v, 4, 3), Qt::LeftButton);
            });
        }

        Legibility::instance().setEnabled(wasLegible);
    }

    fuzzSavedGames(150);

    frameCost();

    std::printf("\n%s\n", g_failures == 0 ? "All UI checks passed." : "FAILURES PRESENT.");
    return g_failures == 0 ? 0 : 1;
}
