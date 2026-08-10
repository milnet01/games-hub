#pragma once

#include "chessai.h"
#include "chessboard.h"
#include "gameview.h"

#include <optional>
#include <vector>

class ChessView : public GameView
{
    Q_OBJECT

public:
    explicit ChessView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    void activate() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 620, 620 }; }
    QSize minimumSizeHint() const override { return { 360, 360 }; }

private:
    void buildActions();
    void newGame();
    void undo();
    // The single point that moves the game on, as in Reversi and Draughts.
    void advance();
    void playEngineMove();
    void refresh(const QString& message = {});
    void announceResult();

    QRect boardRect() const;
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
    bool m_finished = false;
};
