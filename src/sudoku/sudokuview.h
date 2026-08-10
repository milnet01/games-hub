#pragma once

#include "gameview.h"
#include "sudokugrid.h"

#include <QElapsedTimer>

class QTimer;

class SudokuView : public GameView
{
    Q_OBJECT

public:
    explicit SudokuView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    void activate() override;

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
    QTimer* m_tick = nullptr;
};
