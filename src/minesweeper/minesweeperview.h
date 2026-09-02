#pragma once

#include "gameview.h"
#include "minefield.h"

#include <QElapsedTimer>

#include <memory>

class QActionGroup;
class QTimer;

class MinesweeperView : public GameView
{
    Q_OBJECT

public:
    explicit MinesweeperView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    // No cards on this board. Said out loud because the base now answers
    // -1 for "nobody answered", so a game that simply forgot is no longer
    // indistinguishable from one with nothing to measure.
    double smallestCardWidth() const override { return 0.0; }
    void activate() override;
    void deactivate() override;
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

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
    QActionGroup* m_levelGroup = nullptr;
    std::unique_ptr<Minefield> m_field;
    int m_level = 1;
    QElapsedTimer m_clock;
    QTimer* m_tick = nullptr;
    bool m_started = false;
    // Paused: the clock stops and the field is covered, so walking away does
    // not cost you a time and does not hand you a free look at the board.
    bool m_paused = false;
    // Set while another game is on screen: the clock must not run on a board
    // nobody is looking at.
    bool m_suspended = false;
    qint64 m_elapsedMs = 0; // time banked before the current run of the clock
    qint64 elapsedMs() const;
    bool m_announced = false;
};
