#pragma once

#include "ai.h"
#include "board.h"
#include "gameview.h"

#include <QFutureWatcher>

#include <optional>
#include <vector>

class QActionGroup;

class ReversiView : public GameView
{
    Q_OBJECT

public:
    explicit ReversiView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    void activate() override;
    // Not a QTimer, but the same duty: an answer arriving for a board the hub
    // has left must not place a disc on it.
    void deactivate() override;
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 520, 520 }; }
    QSize minimumSizeHint() const override { return { 280, 280 }; }

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

    QRect boardRect() const;
    std::optional<Move> cellAt(QPointF pos) const;

    struct Snapshot {
        Board board;
        Player toMove;
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
    bool m_finished = false;
};
