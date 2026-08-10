#pragma once

#include "gameview.h"

#include <QPoint>

#include <deque>
#include <random>

class QTimer;

// Snake on a walled grid: eat, grow, and do not run into anything.
class SnakeView : public GameView
{
    Q_OBJECT

public:
    explicit SnakeView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    void activate() override;

    static constexpr int kGridWidth = 24;
    static constexpr int kGridHeight = 18;

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override { return { 720, 560 }; }
    QSize minimumSizeHint() const override { return { 360, 300 }; }

private:
    void buildActions();
    void newGame();
    void step();
    void placeFood();
    void gameOver();
    void refresh();

    QRect boardRect() const;
    double cellSize() const;

    QList<QAction*> m_actions;
    QTimer* m_timer = nullptr;

    std::deque<QPoint> m_snake;
    QPoint m_direction { 1, 0 };
    // Buffered so a quick double-tap round a corner is not swallowed by the
    // one-turn-per-step rule.
    QPoint m_pending { 1, 0 };
    QPoint m_food;

    std::mt19937 m_rng { std::random_device {}() };
    int m_score = 0;
    int m_speedMs = 130;
    bool m_running = false;
    bool m_dead = false;
    bool m_started = false;
};
