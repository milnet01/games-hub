#pragma once

#include "chessai.h"
#include "chessboard.h"
#include "gameview.h"

#include <QFutureWatcher>
#include <QTimer>

#include <optional>
#include <vector>

class ChessView : public GameView
{
    Q_OBJECT

public:
    explicit ChessView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    // No cards on this board. Said out loud because the base now answers
    // -1 for "nobody answered", so a game that simply forgot is no longer
    // indistinguishable from one with nothing to measure.
    double smallestCardWidth() const override { return 0.0; }
    // The state sentence alone. The status line adds material and a win count
    // after it, which are worth a glance but not worth a line of the board.
    QString captionText() const override { return m_caption; }
    void activate() override;
    // Not a QTimer, but the same duty: an answer arriving for a board the hub
    // has left must not move a piece on it.
    void deactivate() override;
    TurnLight turnLight() const override { return m_turn.value(); }
    // A game in progress is kept as the moves that made it, and replayed to
    // restore it — see saveState() for why that beats storing the position.
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

    // Which jingle a finished game earns, or nullptr for none. Public because
    // the result box is modal, and a modal dialog in an offscreen test hangs
    // rather than fails -- so the choice is checked here instead.
    static const char* jingleFor(chess::Result result, chess::Colour human);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 620, 620 }; }
    QSize minimumSizeHint() const override { return { 360, 360 }; }

    // Protected rather than private so a test can click a square without
    // keeping its own copy of this arithmetic. It had one, and GHUB-0063 moved
    // the board: the copy went stale and eight checks failed on geometry that
    // had nothing to do with what they assert. Same reason HeartsView exposes
    // handCardRect().
    QRect boardRect() const;

private:
    void buildActions();
    void newGame();
    void undo();
    // The single point that moves the game on, as in Reversi and Draughts.
    // The single point that moves the game on. The message is what the engine's
    // own reply needs to say; every other caller lets refresh() compose it.
    void advance(const QString& message = {});
    void playEngineMove();

    // GHUB-0047. The engine used to search inside the signal handler that
    // started it, on the thread that also paints and handles input, so the
    // window was genuinely frozen for the duration -- no repaint, no resize,
    // and on some desktops a "not responding" prompt. It now searches on a
    // worker and the answer arrives here.
    //
    // The cancellation is the part that matters, and the roadmap said so: a
    // search still running when the player starts a new game, changes level or
    // leaves the page has to be ABANDONED rather than awaited. It is abandoned
    // by generation: every search carries the number the game held when it set
    // off, and an answer whose number no longer matches is dropped on the
    // floor. The worker itself is left to finish -- it holds only copies, it
    // cannot touch the game, and stopping it would mean teaching the search to
    // poll a flag for no benefit the player can see.
    struct SearchResult {
        chess::Move move;
        bool found = false;
        quint64 generation = 0;
    };
    void startSearch();
    void abandonSearch();
    void engineMoveReady(const SearchResult& result);
    void refresh(const QString& message = {});
    void announceResult();

    // The strip outside the frame that carries a seat's turn light: yours
    // below the board, the computer's above it, because that is where you
    // sit (GHUB-0063 § 4.5). boardRect() reserves the room for both.
    QRectF turnBand(int seat) const;
    void stepTurnLight();
    std::optional<chess::Square> squareAt(QPointF pos) const;
    std::vector<chess::Move> movesFrom(chess::Square s) const;
    // Four moves share a destination when a pawn promotes, so the player has
    // to be asked which piece they want.
    bool choosePromotion(chess::PieceType& out);

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;

    chess::ChessGame m_game;
    chess::Colour m_human = chess::Colour::White;
    chess::Level m_level = chess::Level::Medium;

    std::optional<chess::Square> m_selected;
    std::vector<chess::Move> m_selectedMoves;
    std::optional<chess::Move> m_lastMove;
    bool m_thinking = false;
    QFutureWatcher<SearchResult>* m_search = nullptr;
    // Bumped by anything that makes a search in flight answer the wrong
    // question. Never reset, so a stale answer can never collide with it.
    quint64 m_generation = 0;
    // True between deactivate() and activate(). advance() schedules the search
    // on a short timer, so abandoning what is in flight is not enough on its
    // own: without this, a search SCHEDULED before the hub left would still set
    // off afterwards and play a move on a board nobody is looking at, which is
    // the fault GHUB-0073 recorded for Pinball in another form.
    bool m_paused = false;
    bool m_finished = false;
    QString m_caption;
    // GHUB-0063. m_turnFades is false until activate() has run, so a game
    // opened, restored or photographed shows its light at once instead of
    // fading it in.
    TurnLightState m_turn;
    bool m_turnFades = false;
    QTimer* m_turnTimer = nullptr;
};
