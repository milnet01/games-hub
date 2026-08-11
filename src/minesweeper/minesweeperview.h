#pragma once

#include "gameview.h"
#include "minefield.h"

#include <QElapsedTimer>

#include <memory>

class QTimer;

class MinesweeperView : public GameView
{
    Q_OBJECT

public:
    explicit MinesweeperView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    void activate() override;

    struct Level {
        const char* name;
        int width;
        int height;
        int mines;
    };

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 560, 520 }; }
    QSize minimumSizeHint() const override { return { 300, 280 }; }

private:
    void buildActions();
    void newGame(int levelIndex);
    void refresh();
    QRect fieldRect() const;
    double cellSize() const;
    bool cellAt(QPointF pos, int& row, int& col) const;

    QList<QAction*> m_actions;
    QAction* m_pauseAction = nullptr;
    std::unique_ptr<Minefield> m_field;
    int m_level = 1;
    QElapsedTimer m_clock;
    QTimer* m_tick = nullptr;
    bool m_started = false;
    // Paused: the clock stops and the field is covered, so walking away does
    // not cost you a time and does not hand you a free look at the board.
    bool m_paused = false;
    qint64 m_elapsedMs = 0; // time banked before the current run of the clock
    qint64 elapsedMs() const;
    bool m_announced = false;
};
