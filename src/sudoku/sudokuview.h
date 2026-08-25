#pragma once

#include "gameview.h"
#include "sudokugrid.h"

#include <QElapsedTimer>
#include <QFont>

class QTimer;

class SudokuView : public GameView
{
    Q_OBJECT

public:
    explicit SudokuView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    void activate() override;
    void deactivate() override;

    // Come back to a puzzle part way through (GHUB-0009). Pause already covers
    // walking away for a minute; this covers closing the app. The grid writes
    // itself; this adds the level, the cursor, the two toggles and the clock,
    // because a time that restarted at zero would be a score you never earned.
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

    // Named for the contract rather than exposing the font: GHUB-0017 §9 makes
    // whichever per-game pass lands responsible for its own layout being
    // checkable, and what a pencil mark owes is to be as large as it can be
    // while nine of them still sit in one cell without touching. The size is
    // the point of the pass; marksFitCell() is what stops the size growing
    // past the layout that has to hold it — Canasta's cardsFitTable() plays
    // exactly this part against its own floor.
    double markPointSize() const;
    bool marksFitCell() const;
    bool marksFitAt(double pointSize) const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override { return { 560, 600 }; }
    QSize minimumSizeHint() const override { return { 340, 380 }; }

private:
    void buildActions();
    void newGame(SudokuGrid::Level level);
    void enter(int digit);
    void refresh(const QString& message = {});
    void checkSolved();

    QRect boardRect() const;
    double cellSize() const;
    QFont markFont() const;

    QList<QAction*> m_actions;
    SudokuGrid m_grid;
    SudokuGrid::Level m_level = SudokuGrid::Level::Easy;

    int m_row = 4;
    int m_col = 4;
    // Pencil mode writes small candidate digits instead of an answer.
    bool m_pencil = false;
    bool m_highlightErrors = true;
    bool m_solved = false;
    bool m_announced = false;

    QElapsedTimer m_clock;
    // Paused: the clock stops and the grid is covered, so a puzzle left on
    // screen does not quietly cost you your time.
    bool m_paused = false;
    // Set while another game is on screen; the clock stops there too.
    bool m_suspended = false;
    qint64 m_elapsedMs = 0; // banked before the current run of the clock
    qint64 elapsedMs() const;
    QAction* m_pauseAction = nullptr;
    QTimer* m_tick = nullptr;
};
