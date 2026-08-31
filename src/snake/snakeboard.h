#pragma once

#include <QPoint>

#include <deque>
#include <random>

// Snake on a walled grid: eat, grow, and do not run into anything.
//
// The rules only. No widget, no timer, no sound — the view owns how fast the
// snake moves and what it looks like, and this owns what is legal. That split
// is what lets gameshub_selftest check the rules with no display attached
// (GHUB-0066); before it existed, Snake was the one game in the collection
// that nothing anywhere tested.
class SnakeBoard
{
public:
    static constexpr int kWidth = 24;
    static constexpr int kHeight = 18;
    // Three segments in the middle, heading right.
    static constexpr int kStartLength = 3;
    static constexpr int kFoodScore = 10;

    // What one step did. Died is terminal: stepping again does nothing.
    enum class Step { Moved, Ate, Died };

    SnakeBoard() { newGame(); }

    void newGame();

    // A turn is BUFFERED rather than applied at once, so a quick double-tap
    // round a corner is not swallowed by the one-turn-per-step rule. Refuses a
    // reversal — turning straight into your own neck is never what the player
    // meant — and refuses anything that is not one square along an axis.
    // Returns whether the turn was taken.
    bool turn(QPoint direction);

    Step step();

    const std::deque<QPoint>& snake() const { return m_snake; }
    QPoint head() const { return m_snake.front(); }
    QPoint food() const { return m_food; }
    QPoint direction() const { return m_direction; }
    // The turn `step()` will take. Not the same as direction() until it does.
    QPoint pending() const { return m_turns.empty() ? m_direction : m_turns.front(); }
    int score() const { return m_score; }
    bool dead() const { return m_dead; }
    // The snake fills the grid and there is nowhere left to put food. Reachable
    // in principle and the reason placeFood() may leave the food where it was.
    bool boardFull() const { return int(m_snake.size()) >= kWidth * kHeight; }

    static bool inBounds(QPoint p)
    {
        return p.x() >= 0 && p.y() >= 0 && p.x() < kWidth && p.y() < kHeight;
    }
    bool occupies(QPoint p) const;

private:
    // Picks from the free squares only, so the food never lands under the snake.
    void placeFood();

    std::deque<QPoint> m_snake;
    QPoint m_direction { 1, 0 };
    // Up to two turns waiting, so a corner taken as a quick double-tap is not
    // swallowed. A single slot could not do it: the second tap overwrote the
    // first, so Right-Up-Right dropped the Up and the snake never turned. Each
    // entry is checked against the one BEFORE it rather than against
    // m_direction, which is what makes Right-Up-Left legal -- checking a queued
    // turn against the applied direction refused it as a reversal.
    static constexpr std::size_t kMaxTurns = 2;
    std::deque<QPoint> m_turns;
    QPoint m_food;

    std::mt19937 m_rng { std::random_device {}() };
    int m_score = 0;
    bool m_dead = false;
};
