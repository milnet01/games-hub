#include "snakeboard.h"

#include <algorithm>
#include <vector>

void SnakeBoard::newGame()
{
    m_snake.clear();
    const int midRow = kHeight / 2;
    // push_BACK, so the head is the rightmost segment and the body trails
    // behind it. The view built this with push_front, which put the head at the
    // LEFT end of a snake heading right -- so its first step drove straight
    // into its own neck and the game ended before it began. Pressing Right or D
    // to start was an instant game over; any other arrow was fine, which is why
    // it survived. Nothing could see it: these rules lived inside the widget
    // and no test could reach them (GHUB-0066).
    for (int i = 0; i < kStartLength; ++i)
        m_snake.push_back(QPoint(kWidth / 2 - i, midRow));

    m_direction = { 1, 0 };
    m_pending = { 1, 0 };
    m_score = 0;
    m_dead = false;
    placeFood();
}

bool SnakeBoard::occupies(QPoint p) const
{
    return std::find(m_snake.begin(), m_snake.end(), p) != m_snake.end();
}

bool SnakeBoard::turn(QPoint direction)
{
    // Exactly one square, along one axis. Anything else is not a direction this
    // game has, and letting one through would move the head two squares at once
    // — straight over a body segment without the collision test seeing it.
    const int steps = std::abs(direction.x()) + std::abs(direction.y());
    if (steps != 1)
        return false;
    // Reversing straight into your own neck is never what the player meant.
    // Harmless at length one, where there is no neck to hit.
    if (direction + m_direction == QPoint(0, 0) && m_snake.size() > 1)
        return false;
    m_pending = direction;
    return true;
}

SnakeBoard::Step SnakeBoard::step()
{
    if (m_dead)
        return Step::Died;

    m_direction = m_pending;
    const QPoint next = m_snake.front() + m_direction;

    // The tail square is about to be vacated, so running into it is legal —
    // except when the snake just ate, and the tail stays put.
    const bool eating = next == m_food;
    const auto tailEnd = eating ? m_snake.end() : std::prev(m_snake.end());
    const bool hitSelf = std::find(m_snake.begin(), tailEnd, next) != tailEnd;

    if (!inBounds(next) || hitSelf) {
        m_dead = true;
        return Step::Died;
    }

    m_snake.push_front(next);
    if (!eating) {
        m_snake.pop_back();
        return Step::Moved;
    }

    m_score += kFoodScore;
    placeFood();
    return Step::Ate;
}

void SnakeBoard::placeFood()
{
    std::vector<QPoint> free;
    free.reserve(std::size_t(kWidth) * kHeight);
    for (int x = 0; x < kWidth; ++x) {
        for (int y = 0; y < kHeight; ++y) {
            if (!occupies(QPoint(x, y)))
                free.emplace_back(x, y);
        }
    }

    // Board full: there is nowhere to put it. The food stays where it was,
    // which is under the snake — harmless, because the next step can only end
    // the game.
    if (free.empty())
        return;
    std::uniform_int_distribution<std::size_t> pick(0, free.size() - 1);
    m_food = free[pick(m_rng)];
}
