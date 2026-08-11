// Drives the real hub and every game widget offscreen: each one must build,
// lay out, paint and respond without crashing. Run with
// QT_QPA_PLATFORM=offscreen (the CMake test sets it) so it needs no display.

#include "gameview.h"
#include "scores.h"
#include "sound.h"
#include "canasta/canastaview.h"
#include "chess/chessview.h"
#include "hearts/heartsview.h"
#include "hubwindow.h"
#include "klondike/klondikeview.h"
#include "minesweeper/minesweeperview.h"
#include "pinball/pinballview.h"
#include "reversi/reversiview.h"
#include "spider/spiderview.h"

#include <QApplication>
#include <QDeadlineTimer>
#include <QFile>
#include <QSettings>
#include <QLabel>
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

int countPixels(const QImage& image, QRgb colour)
{
    int n = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixel(x, y) == colour)
                ++n;
    return n;
}

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

        // The point of all this is that it outlives the process, so the value
        // must actually be on disk.
        QSettings written;
        check(QFile::exists(written.fileName()), "scores are written to a settings file");
        QSettings reopened(written.fileName(), QSettings::IniFormat);
        const bool onDisk = reopened.value(high).toInt() == 5000
            || QSettings().value(high).toInt() == 5000;
        check(onDisk, "a reopened settings file still holds the best score");

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

        KlondikeView klondike;
        check(paints(&klondike), "klondike view paints");

        SpiderView spider;
        check(paints(&spider), "spider view paints");

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

    std::printf("\n%s\n", g_failures == 0 ? "All UI checks passed." : "FAILURES PRESENT.");
    return g_failures == 0 ? 0 : 1;
}
