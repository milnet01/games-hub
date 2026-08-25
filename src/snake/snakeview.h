#pragma once

#include "gameview.h"
#include "snake/snakeboard.h"

#include <QPoint>

class QTimer;

// Snake on a walled grid: eat, grow, and do not run into anything.
class SnakeView : public GameView
{
    Q_OBJECT

public:
    explicit SnakeView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    void activate() override;
    // A pause, not a stop: activate() picks the game back up. Without it the
    // snake keeps moving on a board nobody is watching and runs into a wall
    // while you are in another game.
    void deactivate() override;

    // The grid's size belongs to the rules, not to the drawing. Kept here as
    // the names the view already used so the painter reads the same.
    static constexpr int kGridWidth = SnakeBoard::kWidth;
    static constexpr int kGridHeight = SnakeBoard::kHeight;

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override { return { 720, 560 }; }
    QSize minimumSizeHint() const override { return { 360, 300 }; }

private:
    void buildActions();
    void newGame();
    void step();
    void gameOver();
    void refresh();

    QRect boardRect() const;
    double cellSize() const;

    QList<QAction*> m_actions;
    QTimer* m_timer = nullptr;

    // The rules. Everything below is the clock and the presentation: how fast
    // the snake moves, whether it has been set going, and whether the hub has
    // paused it.
    SnakeBoard m_board;
    int m_speedMs = 130;
    bool m_running = false;
    bool m_started = false;
};
