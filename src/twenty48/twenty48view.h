#pragma once

#include "gameview.h"

#include <array>
#include <random>
#include <vector>

// The sliding-tile number game: push everything one way, equal neighbours
// merge, a new tile appears, keep going until nothing can move.
class Twenty48View : public GameView
{
    Q_OBJECT

public:
    explicit Twenty48View(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    void activate() override;
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

    static constexpr int kSize = 4;
    static constexpr int kCells = kSize * kSize;
    static constexpr int kTarget = 2048;

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override { return { 520, 560 }; }
    QSize minimumSizeHint() const override { return { 300, 340 }; }

private:
    enum class Direction { Left, Right, Up, Down };

    void buildActions();
    void newGame();
    void undo();
    // Slides and merges. Returns false when nothing moved, in which case no
    // new tile appears — that is what stops a dead key from wasting a turn.
    bool slide(Direction direction);
    void spawn();
    bool canMove() const;
    void checkEnd();
    void refresh(const QString& message = {});

    int& at(int row, int col) { return m_board[std::size_t(row * kSize + col)]; }
    int at(int row, int col) const { return m_board[std::size_t(row * kSize + col)]; }

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;

    std::array<int, kCells> m_board {};
    std::array<int, kCells> m_previous {};
    int m_score = 0;
    int m_previousScore = 0;
    bool m_canUndo = false;
    bool m_reachedTarget = false;
    bool m_finished = false;

    std::mt19937 m_rng { std::random_device {}() };
};
