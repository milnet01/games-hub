#include "runnercourse.h"

#include <algorithm>

namespace {
// The physics step: small enough that at the top pace the course moves less
// than three units a step, far less than the thinnest obstacle.
constexpr double kStep = 1.0 / 120.0;
// Coins in a row sit this far apart.
constexpr double kCoinPitch = 26.0;
// A runner whose feet sink this far into a pit has fallen in.
constexpr double kFallDepth = 26.0;
// Obstacles are laid this far past the screen's edge, which reaches the
// first one newGame() places, so a Ready course already holds it.
constexpr double kSpawnMargin = 300.0;
}

void RunnerCourse::newGame()
{
    m_phase = Phase::Ready;
    m_hearts = kHearts;
    m_hits = 0;
    m_coinsTaken = 0;
    m_speed = kStartSpeed;
    m_distance = 0.0;
    m_runSeconds = 0.0;
    m_nextMilestone = kMilestoneMetres;
    m_feetY = kGroundY;
    m_rise = 0.0;
    m_airborne = false;
    m_duck = false;
    m_jumpQueued = 0.0;
    m_hurtLeft = 0.0;
    m_obstacles.clear();
    m_coins.clear();
    m_events.clear();

    // A row of coins to run through first, and the first obstacle a good
    // few seconds off, so the run opens on something easy.
    addCoinRow(kRunnerX + 120.0, kRunnerX + 120.0 + 5.0 * kCoinPitch);
    m_nextX = kWidth + kStartSpeed * 1.5;
    spawnAhead();
}

bool RunnerCourse::start()
{
    if (m_phase != Phase::Ready)
        return false;
    m_phase = Phase::Running;
    record(EventKind::Started, QPointF(kRunnerX, m_feetY));
    return true;
}

bool RunnerCourse::jump()
{
    if (m_phase == Phase::Ready) {
        start();
        return false;
    }
    if (m_phase != Phase::Running)
        return false;
    if (m_airborne) {
        m_jumpQueued = kJumpBuffer;
        return false;
    }
    m_airborne = true;
    m_rise = -kJumpSpeed;
    m_jumpQueued = 0.0;
    record(EventKind::Jumped, QPointF(kRunnerX, m_feetY));
    return true;
}

void RunnerCourse::setDuck(bool held)
{
    m_duck = held && m_phase != Phase::Over;
}

void RunnerCourse::advance(double seconds)
{
    while (seconds > 1e-9) {
        const double dt = std::min(seconds, kStep);
        step(dt);
        seconds -= dt;
    }
}

std::vector<RunnerCourse::Event> RunnerCourse::takeEvents()
{
    std::vector<Event> out;
    out.swap(m_events);
    return out;
}

QRectF RunnerCourse::runnerBox() const
{
    const double h = m_duck ? kDuckHeight : kStandHeight;
    return { kRunnerX - kRunnerHalfWidth, m_feetY - h, 2.0 * kRunnerHalfWidth, h };
}

QRectF RunnerCourse::hitBox(const Obstacle& o)
{
    const double left = o.x + kForgive;
    const double width = o.width - 2.0 * kForgive;
    switch (o.kind) {
    case Kind::Root:
        return { left, kGroundY - kRootHeight + kForgive, width, kRootHeight - kForgive };
    case Kind::Stone:
        return { left, kGroundY - kStoneHeight + kForgive, width, kStoneHeight - kForgive };
    case Kind::Arch:
        // From well above the screen down to the lintel's underside, so it
        // cannot be jumped over.
        return { left, -kHeight, width, kHeight + kGroundY - kArchClearance };
    case Kind::Gap:
        break;
    }
    return {};
}

double RunnerCourse::gapWidth(double speed)
{
    // At most about half of what one jump carries the runner across at that
    // pace (speed * kAirTime), so the timing has room either side.
    return std::clamp(0.28 * speed + 20.0, 64.0, 116.0);
}

RunnerCourse::Input RunnerCourse::pilot() const
{
    Input in;
    if (m_phase != Phase::Running)
        return in;
    const double back = kRunnerX - kRunnerHalfWidth;
    const double front = kRunnerX + kRunnerHalfWidth;
    // The nearest obstacle not yet behind the runner. They are kept in the
    // order they were laid, which is left to right.
    const auto next = std::find_if(m_obstacles.begin(), m_obstacles.end(), [&](const Obstacle& o) {
        return !o.spent && o.right() > back;
    });
    if (next == m_obstacles.end())
        return in;
    if (next->kind == Kind::Arch) {
        in.duck = next->x - front < m_speed * 0.15 + 8.0;
    } else {
        const double ahead = next->centre() - kRunnerX;
        in.jump = !m_airborne && ahead > 0.0 && ahead <= jumpLead(m_speed);
    }
    return in;
}

void RunnerCourse::step(double dt)
{
    if (m_phase != Phase::Running)
        return;

    m_runSeconds += dt;
    m_speed = std::min(kMaxSpeed, m_speed + kSpeedGain * dt);
    const double dx = m_speed * dt;
    for (Obstacle& o : m_obstacles)
        o.x -= dx;
    for (Coin& c : m_coins)
        c.at.rx() -= dx;
    m_nextX -= dx;
    m_distance += dx;
    std::erase_if(m_obstacles, [](const Obstacle& o) { return o.right() < -60.0; });
    std::erase_if(m_coins, [](const Coin& c) { return c.at.x() < -30.0; });
    spawnAhead();

    while (metres() >= m_nextMilestone) {
        record(EventKind::Milestone, QPointF(kRunnerX, m_feetY), m_nextMilestone);
        m_nextMilestone += kMilestoneMetres;
    }

    m_hurtLeft = std::max(0.0, m_hurtLeft - dt);
    m_jumpQueued = std::max(0.0, m_jumpQueued - dt);

    // Off the edge of a pit, the runner starts to fall.
    if (!m_airborne && overOpenGap()) {
        m_airborne = true;
        m_rise = 0.0;
    }
    if (m_airborne) {
        const double before = m_feetY;
        m_rise += kGravity * (m_duck ? kFastFall : 1.0) * dt;
        m_feetY += m_rise * dt;
        if (m_feetY >= kGroundY) {
            // Below the ground with nothing under the runner's middle: in a
            // pit, or landed short against its far wall.
            const bool inPit = before > kGroundY + 0.01 || overOpenGap();
            if (!inPit) {
                m_feetY = kGroundY;
                m_rise = 0.0;
                m_airborne = false;
                record(EventKind::Landed, QPointF(kRunnerX, m_feetY));
            } else if (m_feetY > kGroundY + kFallDepth || !overOpenGap()) {
                // The pit nearest the runner's middle is the one fallen into.
                Obstacle* pit = nullptr;
                for (Obstacle& o : m_obstacles)
                    if (o.kind == Kind::Gap && !o.spent && o.x < kRunnerX + kRunnerHalfWidth
                        && o.right() > kRunnerX - 60.0)
                        pit = &o;
                // Climbs out with a hop, onto ground the spent pit no longer
                // takes away.
                m_feetY = kGroundY;
                m_rise = -kJumpSpeed * 0.7;
                if (pit != nullptr) {
                    takeHit(*pit, EventKind::Fell);
                    if (over())
                        return;
                }
            }
        }
    }

    if (m_jumpQueued > 0.0 && !m_airborne)
        jump();

    const QRectF box = runnerBox();
    if (m_hurtLeft <= 0.0) {
        for (Obstacle& o : m_obstacles) {
            if (o.spent || o.kind == Kind::Gap || !hitBox(o).intersects(box))
                continue;
            takeHit(o, EventKind::Hit);
            if (over())
                return;
            break;
        }
    }

    const QRectF reach = box.adjusted(-kCoinRadius, -kCoinRadius, kCoinRadius, kCoinRadius);
    for (Coin& c : m_coins) {
        if (c.taken || !reach.contains(c.at))
            continue;
        c.taken = true;
        ++m_coinsTaken;
        record(EventKind::Coin, c.at, kCoinPoints);
    }
}

void RunnerCourse::takeHit(Obstacle& o, EventKind kind)
{
    o.spent = true;
    ++m_hits;
    m_hearts = std::max(0, m_hearts - 1);
    m_hurtLeft = kHurtSeconds;
    record(kind, QPointF(kRunnerX, m_feetY - kStandHeight / 2.0));
    if (m_hearts == 0) {
        m_phase = Phase::Over;
        m_duck = false;
        m_jumpQueued = 0.0;
        record(EventKind::GameOver, QPointF(kRunnerX, m_feetY - kStandHeight / 2.0));
    }
}

bool RunnerCourse::overOpenGap() const
{
    return std::any_of(m_obstacles.begin(), m_obstacles.end(), [](const Obstacle& o) {
        return o.kind == Kind::Gap && !o.spent && kRunnerX > o.x + kForgive
               && kRunnerX < o.right() - kForgive;
    });
}

RunnerCourse::Kind RunnerCourse::pickKind()
{
    // Drawn from rng() directly rather than a distribution, so one seed lays
    // the same course on every compiler (see shuffleCards).
    std::vector<Kind> open { Kind::Root };
    if (metres() >= kStoneFromMetres)
        open.push_back(Kind::Stone);
    if (metres() >= kGapFromMetres)
        open.push_back(Kind::Gap);
    if (metres() >= kArchFromMetres)
        open.push_back(Kind::Arch);
    return open[m_rng() % open.size()];
}

void RunnerCourse::spawnAhead()
{
    while (m_nextX < kWidth + kSpawnMargin) {
        // The pace when the runner reaches it, which is a little more than
        // now; the spacing has to suit that pace, not this one.
        const double ahead = std::max(0.0, m_nextX - kRunnerX);
        const double v = std::min(kMaxSpeed, m_speed + kSpeedGain * ahead / m_speed);

        Obstacle o;
        o.kind = pickKind();
        o.x = m_nextX;
        switch (o.kind) {
        case Kind::Root: o.width = kRootWidth; break;
        case Kind::Stone: o.width = kStoneWidth; break;
        case Kind::Gap: o.width = gapWidth(v); break;
        case Kind::Arch: o.width = kArchWidth; break;
        }
        o.look = int(m_rng() % 4);
        m_obstacles.push_back(o);
        // Half the things to jump carry a line of coins along the jump that
        // clears them: a hint at the timing as well as a prize.
        if (o.kind != Kind::Arch && m_rng() % 2 == 0)
            addCoinArc(o, v);

        // Landed, recovered, and then a while longer at random.
        const double next = o.right() + v * (kAirTime + kRecoverSeconds) + unit() * v * kSpreadSeconds;
        // Coins on the ground between the two, clear of both jumps.
        const double rowFrom = (o.kind == Kind::Arch ? o.right() : o.centre() + jumpLead(v)) + 24.0;
        const double rowTo = next - jumpLead(v) - 24.0;
        if (rowTo - rowFrom > 3.0 * kCoinPitch && m_rng() % 2 == 0)
            addCoinRow(rowFrom, rowTo);
        m_nextX = next;
    }
}

void RunnerCourse::addCoinRow(double from, double to)
{
    const int count = std::min(5, int((to - from) / kCoinPitch) + 1);
    for (int i = 0; i < count; ++i)
        m_coins.push_back({ QPointF(from + i * kCoinPitch, kGroundY - 20.0), false });
}

void RunnerCourse::addCoinArc(const Obstacle& o, double speed)
{
    // Where the runner's middle passes on a jump whose top is over the
    // obstacle's middle, at five moments around that top.
    const double apex = kJumpSpeed * kJumpSpeed / (2.0 * kGravity);
    for (int i = -2; i <= 2; ++i) {
        const double t = i * 0.11;
        const double height = apex - kGravity * t * t / 2.0;
        m_coins.push_back({ QPointF(o.centre() + speed * t, kGroundY - height - kStandHeight / 2.0), false });
    }
}

void RunnerCourse::record(EventKind kind, QPointF at, int value)
{
    // A caller that never reads them (a test, a --shot run) must not grow the
    // list for ever; the oldest go first, since only the newest are drawn.
    if (m_events.size() >= 256)
        m_events.erase(m_events.begin());
    m_events.push_back({ kind, at, value });
}

double RunnerCourse::unit()
{
    return double(m_rng()) / (double(std::mt19937::max()) + 1.0);
}
