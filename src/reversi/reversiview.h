#pragma once

#include "ai.h"
#include "board.h"
#include "gameview.h"

#include <QFutureWatcher>
#include <QTimer>

#include <optional>
#include <vector>

class QActionGroup;

class ReversiView : public GameView
{
    Q_OBJECT

public:
    explicit ReversiView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    // No cards on this board. Said out loud because the base now answers
    // -1 for "nobody answered", so a game that simply forgot is no longer
    // indistinguishable from one with nothing to measure.
    double smallestCardWidth() const override { return 0.0; }
    void activate() override;
    // Not a QTimer, but the same duty: an answer arriving for a board the hub
    // has left must not place a disc on it.
    void deactivate() override;
    TurnLight turnLight() const override { return m_turn.value(); }
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 520, 520 }; }
    QSize minimumSizeHint() const override { return { 280, 280 }; }

    // Protected rather than private so a test can click a cell without keeping
    // its own copy of this arithmetic. It had one, and GHUB-0063 reserved room
    // outside the frame for the turn bands: the copy went stale and the checks
    // built on it failed on geometry none of them is about. Same reason
    // HeartsView exposes handCardRect().
    QRect boardRect() const;

private:
    void buildActions();
    void newGame();
    void undo();
    // The single point that moves the game on: refreshes, handles forced
    // passes, and hands over to the engine when it is the computer's turn.
    void advance();
    void playEngineMove();
    void refresh(const QString& message = {});
    void announceResult();

    // The strip outside the frame that carries a seat's turn light: yours
    // below the board, the computer's above it, because that is where you sit
    // (GHUB-0063 § 4.5). boardRect() reserves the room for both.
    QRectF turnBand(int seat) const;
    void stepTurnLight();
    std::optional<Move> cellAt(QPointF pos) const;

    struct Snapshot {
        Board board;
        // Defaulted for the reason draughts' is: a default-constructed snapshot
        // must hold a player rather than whatever was on the stack. Black is
        // the opening side.
        Player toMove = Player::Black;
        std::optional<Move> lastMove;
    };

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;
    QActionGroup* m_levelGroup = nullptr;

    Board m_board;
    Player m_toMove = Player::Black;
    Player m_human = Player::Black;
    Difficulty m_difficulty = Difficulty::Medium;
    std::optional<Move> m_lastMove;
    std::vector<Move> m_hints;
    // A pass leaves the board looking exactly as it did, so the sentence is the
    // only sign it happened. Held here and carried into the next status rather
    // than emitted once, because the message after it used to arrive 320ms
    // later and wipe it.
    QString m_passNotice;
    std::vector<Snapshot> m_history;
    bool m_showHints = true;
    bool m_thinking = false;
    // GHUB-0047. The search runs on a worker and the answer arrives back here;
    // see ChessView for the full reasoning. A search the game has outrun is
    // abandoned by generation rather than awaited, and m_paused stops one that
    // was merely SCHEDULED from setting off after the hub has gone.
    struct SearchResult {
        std::optional<Move> move;
        quint64 generation = 0;
    };
    void startSearch();
    void abandonSearch();
    void engineMoveReady(const SearchResult& result);
    QFutureWatcher<SearchResult>* m_search = nullptr;
    quint64 m_generation = 0;
    bool m_paused = false;
    // Restoring a save clears the undo history, so m_history.empty() alone reads
    // a resumed game as untouched and saveState() then returns {}, which the hub
    // treats as "delete the stored game". This says the position is worth keeping.
    bool m_resumed = false;
    bool m_finished = false;
    // GHUB-0063. m_turnFades is false until activate() has run, so a game
    // opened, restored or photographed shows its light at once instead of
    // fading it in.
    TurnLightState m_turn;
    bool m_turnFades = false;
    QTimer* m_turnTimer = nullptr;
};
