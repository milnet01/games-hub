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
    // GHUB-0168. Arrow keys move the cursor, Space or Return digs, F flags.
    // Flagging needs a key of its own because the mouse flags with the RIGHT
    // button, and a board you can dig but not flag is not playable. Sudoku is
    // the pattern; the focus policy set in the constructor is what makes any of
    // it reachable.
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override { return { 560, 520 }; }
    QSize minimumSizeHint() const override { return { 300, 280 }; }

    // Where the keyboard cursor is, as (column, row). Protected so a test can
    // read it without a copy of the geometry, the same reason the board games
    // expose boardRect().
    QPoint cursorCell() const { return { m_cursorCol, m_cursorRow }; }

private:
    void buildActions();
    void newGame(int levelIndex);
    void refresh();
    QRect fieldRect() const;
    double cellSize() const;
    bool cellAt(QPointF pos, int& row, int& col) const;
    // What a dig on one cell does -- reveal, or chord an already-open number.
    // Reached by a left click and by Space alike, so the two cannot drift
    // apart. Flagging is one call and needs no such function.
    void digAt(int row, int col);

    QList<QAction*> m_actions;
    QAction* m_pauseAction = nullptr;
    QActionGroup* m_levelGroup = nullptr;
    std::unique_ptr<Minefield> m_field;
    int m_level = 1;
    // The keyboard cursor. Clamped by newGame(), because the levels are
    // different sizes and Expert's board is not Beginner's. Deliberately not
    // saved: the cursor is where you are looking, not part of the position, and
    // adding it to saveState() would break every save in tests/saves/ for
    // nothing a player would notice.
    int m_cursorRow = 0;
    int m_cursorCol = 0;
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
