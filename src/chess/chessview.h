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
    // The state sentence alone. The status line adds material and a win count
    // after it, which are worth a glance but not worth a line of the board.
    QString captionText() const override { return m_caption; }
    void activate() override;
    // A game in progress is kept as the moves that made it, and replayed to
    // restore it — see saveState() for why that beats storing the position.
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

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
    QString m_caption;
};
