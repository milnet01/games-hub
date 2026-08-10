#pragma once

#include "ai.h"
#include "board.h"
#include "gameview.h"

#include <optional>
#include <vector>

class ReversiView : public GameView
{
    Q_OBJECT

public:
    explicit ReversiView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    void activate() override;

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

    Board m_board;
    Player m_toMove = Player::Black;
    Player m_human = Player::Black;
    Difficulty m_difficulty = Difficulty::Medium;
    std::optional<Move> m_lastMove;
    std::vector<Move> m_hints;
    std::vector<Snapshot> m_history;
    bool m_showHints = true;
    bool m_thinking = false;
    bool m_finished = false;
};
