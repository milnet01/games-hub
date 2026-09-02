#pragma once

#include "gameview.h"
#include "twenty48/twenty48board.h"

#include <QColor>

// The tile palette and the ink that reads on it. Declared here rather than kept
// in twenty48view.cpp's anonymous namespace so tests/uitest.cpp can assert the
// contrast the player actually gets.
QColor tileColour(int value);
QColor inkFor(int value);

// The WCAG 2.2 formulae. Exported alongside, because a test that re-derives
// them is checking its own arithmetic and stays green over a wrong inkFor().
// scripts/legibility-check.py holds the only other copy, in Python.
double relativeLuminance(const QColor& c);
double contrastRatio(const QColor& a, const QColor& b);

// The sliding-tile number game: push everything one way, equal neighbours
// merge, a new tile appears, keep going until nothing can move.
class Twenty48View : public GameView
{
    Q_OBJECT

public:
    explicit Twenty48View(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    // No cards on this board. Said out loud because the base now answers
    // -1 for "nobody answered", so a game that simply forgot is no longer
    // indistinguishable from one with nothing to measure.
    double smallestCardWidth() const override { return 0.0; }
    void activate() override;
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

    // The board's shape belongs to the rules. Kept under the names the view
    // and its tests already used.
    static constexpr int kSize = Twenty48Board::kSize;
    static constexpr int kCells = Twenty48Board::kCells;
    static constexpr int kTarget = Twenty48Board::kTarget;

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override { return { 520, 560 }; }
    QSize minimumSizeHint() const override { return { 300, 340 }; }

private:
    using Direction = Twenty48Board::Direction;

    void buildActions();
    void newGame();
    void undo();
    // Slides, and on a push that actually moved something lays a new tile and
    // asks whether the board is now stuck.
    void push(Direction direction);
    void checkEnd();
    void refresh(const QString& message = {});
    // The face a tile's number is drawn in. Solved against the font in use
    // rather than scaled by a tuned ratio while the legibility switch is on —
    // how tall a platform draws a digit is a property of the platform.
    QFont tileFont(int value, double cell) const;

    int at(int row, int col) const { return m_board.at(row, col); }

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;

    // The rules. Everything the view still owns is presentation: whether the
    // "no moves left" box has been shown, and the undo ACTION's enabled state.
    Twenty48Board m_board;
    bool m_finished = false;
};
