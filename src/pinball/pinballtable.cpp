#include "pinballtable.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kMaxSpeed = 1700.0;
// Physics runs in small fixed slices; one big step per frame lets a fast ball
// tunnel straight through a wall.
constexpr double kSubStep = 1.0 / 480.0;
constexpr double kLaneCentre = 369.0;

double lengthOf(QPointF v) { return std::hypot(v.x(), v.y()); }

double dot(QPointF a, QPointF b) { return a.x() * b.x() + a.y() * b.y(); }

// Closest point to p on the segment ab.
QPointF closestOnSegment(QPointF a, QPointF b, QPointF p)
{
    const QPointF ab = b - a;
    const double denom = dot(ab, ab);
    if (denom < 1e-9)
        return a;
    const double t = std::clamp(dot(p - a, ab) / denom, 0.0, 1.0);
    return a + ab * t;
}

} // namespace

PinballTable::PinballTable()
{
    buildTable();
    newGame();
}

// Height of the dome's underside directly above the launch lane. The ball has
// to reach this to get round the table, so the plunger is calibrated from it.
double PinballTable::minimumLaunchSpeed()
{
    const double dx = kLaneCentre - 200.0;
    const double domeY = kDomeCentreY - std::sqrt(std::max(0.0, kDomeRadius * kDomeRadius - dx * dx));
    // A 6% margin so the weakest launch clears the dome rather than stalling
    // just under it.
    return std::sqrt(2.0 * kGravity * (kBallPark - domeY)) * 1.06;
}

void PinballTable::buildTable()
{
    m_walls.clear();
    m_bumpers.clear();

    auto wall = [this](QPointF a, QPointF b, double bounce = 0.62) {
        m_walls.push_back(Wall { a, b, 4.0, bounce, 0.0, 0, 0.0, false });
    };

    // Outer boundary. The launch lane runs the full height on the right and is
    // walled off from the table all the way down, so a launch that falls short
    // returns to the plunger instead of dropping out of the drain.
    wall({ 14, kDomeCentreY }, { 14, 540 });
    wall({ kLaneX, kDomeCentreY }, { kLaneX, 700 });
    wall({ 386, 120 }, { 386, 700 });

    // Domed top, a fan of short segments. It spans the whole table including
    // the lane, so a launched ball meets its underside and is turned left into
    // the play field — that curve is the ball's way in.
    QPointF previous;
    for (int i = 0; i <= 20; ++i) {
        const double a = M_PI + i * (M_PI / 20.0);
        const QPointF pt(200.0 + std::cos(a) * kDomeRadius,
                         kDomeCentreY + std::sin(a) * kDomeRadius);
        if (i > 0)
            wall(previous, pt, 0.66);
        previous = pt;
    }

    // One-way gate across the mouth of the lane, just under the dome: the ball
    // rides up through it and cannot drop back down the lane afterwards.
    m_walls.push_back(Wall { { kLaneX, 196 }, { 386, 196 }, 4.0, 0.3, 0.0, 0, 0.0, true });

    // Inlane walls funnelling towards the flippers.
    wall({ 14, 540 }, { 104, 636 });
    wall({ kLaneX, 540 }, { 296, 636 });

    // Slingshots: the angled kickers just above each flipper.
    m_walls.push_back(Wall { { 62, 486 }, { 108, 574 }, 5.0, 0.7, 240.0, 50, 0.0, false });
    m_walls.push_back(Wall { { 334, 486 }, { 292, 574 }, 5.0, 0.7, 240.0, 50, 0.0, false });

    m_bumpers.push_back(Bumper { { 142, 292 }, 26.0, 200.0, 100, 0.0 });
    m_bumpers.push_back(Bumper { { 258, 292 }, 26.0, 200.0, 100, 0.0 });
    m_bumpers.push_back(Bumper { { 200, 372 }, 26.0, 200.0, 100, 0.0 });

    m_left = Flipper { { 108, 640 }, 74.0, 0.52, -0.42, 0.52, 0.52, false, true };
    m_right = Flipper { { 292, 640 }, 74.0, 0.52, -0.42, 0.52, 0.52, false, false };
}

void PinballTable::newGame()
{
    m_score = 0;
    m_ballsLeft = 3;
    m_gameOver = false;
    m_ballLost = false;
    resetBall();
}

void PinballTable::resetBall()
{
    m_ball = QPointF(kLaneCentre, kBallPark);
    m_velocity = QPointF(0, 0);
    m_inLane = true;
    m_plunger = 0.0;
}

void PinballTable::setFlipper(bool left, bool pressed)
{
    (left ? m_left : m_right).pressed = pressed;
}

void PinballTable::chargePlunger(double seconds)
{
    if (m_inLane)
        m_plunger = std::min(1.0, m_plunger + seconds * 1.6);
}

void PinballTable::launch()
{
    if (!m_inLane || m_gameOver)
        return;
    m_velocity = QPointF(0, -(minimumLaunchSpeed() + m_plunger * 380.0));
    m_inLane = false;
    m_plunger = 0.0;
}

QPointF PinballTable::flipperTip(const Flipper& f) const
{
    const double dx = std::cos(f.angle) * (f.facingRight ? 1.0 : -1.0);
    return f.pivot + QPointF(dx, std::sin(f.angle)) * f.length;
}

void PinballTable::advance(double seconds)
{
    if (m_gameOver)
        return;

    // Flippers move towards their target angle at a fixed rate; the angular
    // speed is what gives the ball its kick.
    for (Flipper* f : { &m_left, &m_right }) {
        f->previousAngle = f->angle;
        const double target = f->pressed ? f->activeAngle : f->restAngle;
        const double rate = 18.0 * seconds;
        if (std::abs(target - f->angle) <= rate)
            f->angle = target;
        else
            f->angle += (target > f->angle) ? rate : -rate;
    }

    double remaining = seconds;
    while (remaining > 0.0 && !m_gameOver) {
        const double dt = std::min(kSubStep, remaining);
        stepPhysics(dt);
        remaining -= dt;
    }

    for (Wall& w : m_walls)
        w.flash = std::max(0.0, w.flash - seconds * 3.0);
    for (Bumper& b : m_bumpers)
        b.flash = std::max(0.0, b.flash - seconds * 3.0);
}

void PinballTable::stepPhysics(double dt)
{
    if (m_inLane)
        return; // held by the plunger; nothing to simulate yet

    m_velocity.setY(m_velocity.y() + kGravity * dt);

    const double speed = lengthOf(m_velocity);
    if (speed > kMaxSpeed)
        m_velocity *= kMaxSpeed / speed;

    m_ball += m_velocity * dt;

    for (Wall& w : m_walls)
        collideWall(w);
    for (Bumper& b : m_bumpers)
        collideBumper(b);
    collideFlipper(m_left, dt);
    collideFlipper(m_right, dt);

    // A launch that did not make it round the dome slides back down the lane.
    // That is a fumbled shot, not a lost ball: re-park it for another go.
    if (m_ball.x() > kLaneX && m_ball.y() > 600 && m_velocity.y() > 0) {
        resetBall();
        return;
    }

    if (m_ball.y() - m_ballRadius > kDrainY)
        loseBall();
}

void PinballTable::loseBall()
{
    --m_ballsLeft;
    m_ballLost = true;
    if (m_ballsLeft <= 0) {
        m_gameOver = true;
        m_best = std::max(m_best, m_score);
    } else {
        resetBall();
    }
}

void PinballTable::collideWall(Wall& w)
{
    const QPointF closest = closestOnSegment(w.a, w.b, m_ball);
    QPointF delta = m_ball - closest;
    double distance = lengthOf(delta);
    const double minimum = m_ballRadius + w.radius;

    if (distance >= minimum)
        return;

    // A one-way gate ignores a ball travelling upward through it.
    if (w.oneWayUp && m_velocity.y() < 0)
        return;

    if (distance < 1e-6) {
        delta = QPointF(0, -1);
        distance = 1.0;
    }

    const QPointF normal = delta / distance;
    m_ball = closest + normal * minimum;

    const double along = dot(m_velocity, normal);
    if (along < 0)
        m_velocity -= normal * (1.0 + w.bounce) * along;

    if (w.kick > 0.0) {
        m_velocity += normal * w.kick;
        w.flash = 1.0;
        m_score += w.score;
    }
}

void PinballTable::collideBumper(Bumper& b)
{
    QPointF delta = m_ball - b.centre;
    double distance = lengthOf(delta);
    const double minimum = m_ballRadius + b.radius;

    if (distance >= minimum)
        return;

    if (distance < 1e-6) {
        delta = QPointF(0, -1);
        distance = 1.0;
    }

    const QPointF normal = delta / distance;
    m_ball = b.centre + normal * minimum;

    const double along = dot(m_velocity, normal);
    if (along < 0)
        m_velocity -= normal * 1.5 * along;
    m_velocity += normal * b.kick;

    b.flash = 1.0;
    m_score += b.score;
}

void PinballTable::collideFlipper(Flipper& f, double dt)
{
    const QPointF tip = flipperTip(f);
    const QPointF closest = closestOnSegment(f.pivot, tip, m_ball);
    QPointF delta = m_ball - closest;
    double distance = lengthOf(delta);
    const double minimum = m_ballRadius + 7.0;

    if (distance >= minimum)
        return;

    if (distance < 1e-6) {
        delta = QPointF(0, -1);
        distance = 1.0;
    }

    const QPointF normal = delta / distance;
    m_ball = closest + normal * minimum;

    // Surface velocity at the contact point: this is what actually launches the
    // ball when the flipper is mid-swing.
    const double angularSpeed = (f.angle - f.previousAngle) / std::max(dt, 1e-6);
    const QPointF arm = closest - f.pivot;
    const double sign = f.facingRight ? 1.0 : -1.0;
    const QPointF surface(-arm.y() * angularSpeed * sign, arm.x() * angularSpeed * sign);

    const QPointF relative = m_velocity - surface;
    const double along = dot(relative, normal);
    if (along < 0)
        m_velocity -= normal * 1.55 * along;

    const double swing = std::abs(angularSpeed);
    if (swing > 0.5)
        m_velocity += normal * std::min(swing * 26.0, 520.0);
}
