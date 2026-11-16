#include "lanecourse.h"

#include "runner/runnercourse.h"

#include <algorithm>
#include <cmath>

namespace {
// The physics step: small enough that at the top pace the course comes less
// than 0.12 metres a step, well under the shallowest obstacle.
constexpr double kStep = 1.0 / 120.0;
// The first row is this far off, so the run opens on a few easy seconds.
constexpr double kFirstRow = 34.0;
// Anything this far behind the runner is gone from view and is dropped.
constexpr double kBehind = 4.0;
}

void LaneCourse::newGame()
{
    m_phase = Phase::Ready;
    m_hearts = kHearts;
    m_hits = 0;
    m_coinsTaken = 0;
    m_speed = kStartSpeed;
    m_distance = 0.0;
    m_runSeconds = 0.0;
    m_nextMilestone = kMilestoneMetres;
    m_lane = 1.0;
    m_target = 1;
    m_height = 0.0;
    m_rise = 0.0;
    m_airborne = false;
    m_slide = false;
    m_jumpQueued = 0.0;
    m_hurtLeft = 0.0;
    m_obstacles.clear();
    m_coins.clear();
    m_events.clear();
    m_rows = 0;

    // A line of coins down the middle lane to run through first.
    for (int i = 0; i < 5; ++i)
        m_coins.push_back({ 1, 6.0 + i * kCoinPitch, kCoinHeight, false });
    m_nextZ = kFirstRow;
    spawnAhead();
}

bool LaneCourse::start()
{
    if (m_phase != Phase::Ready)
        return false;
    m_phase = Phase::Running;
    record(EventKind::Started, 0.0);
    return true;
}

bool LaneCourse::steer(int direction)
{
    if (m_phase == Phase::Ready) {
        start();
        return false;
    }
    if (m_phase != Phase::Running || direction == 0)
        return false;
    const int to = std::clamp(m_target + (direction < 0 ? -1 : 1), 0, kLanes - 1);
    if (to == m_target)
        return false;
    m_target = to;
    record(EventKind::Switched, m_height);
    return true;
}

bool LaneCourse::jump()
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
    m_rise = kJumpSpeed;
    m_jumpQueued = 0.0;
    record(EventKind::Jumped, 0.0);
    return true;
}

void LaneCourse::setSlide(bool held)
{
    m_slide = held && m_phase != Phase::Over;
}

void LaneCourse::advance(double seconds)
{
    while (seconds > 1e-9) {
        const double dt = std::min(seconds, kStep);
        step(dt);
        seconds -= dt;
    }
}

std::vector<LaneCourse::Event> LaneCourse::takeEvents()
{
    std::vector<Event> out;
    out.swap(m_events);
    return out;
}

int LaneCourse::score() const
{
    return metres() + m_coinsTaken * RunnerCourse::kCoinPoints;
}

double LaneCourse::pitDepth(double speed)
{
    // About a third of what one jump carries the runner across at that pace
    // (speed * kAirTime), so the timing has room either side.
    return std::clamp(0.3 * speed * kAirTime + 0.4, 1.6, 3.6);
}

LaneCourse::Input LaneCourse::pilot() const
{
    Input in;
    if (m_phase != Phase::Running)
        return in;
    // The nearest row not yet behind the runner. Obstacles are kept nearest
    // first, so its first obstacle is the first one found.
    const auto next = std::find_if(m_obstacles.begin(), m_obstacles.end(), [](const Obstacle& o) {
        return o.far() > -kRunnerDepth / 2.0;
    });
    if (next == m_obstacles.end())
        return in;
    const int row = next->row;

    // What is in each lane of that row: nothing, something to jump or slide,
    // or a block to go round.
    const Obstacle* inLane[kLanes] = {};
    for (const Obstacle& o : m_obstacles)
        if (o.row == row)
            inLane[o.lane] = &o;
    const auto cost = [&](int lane) {
        if (inLane[lane] == nullptr)
            return 0;
        return inLane[lane]->kind == Kind::Block ? 9 : 1;
    };

    // Go round only before the row arrives; while it is passing, hold on.
    const bool passing = next->z < kRunnerDepth;
    int want = m_target;
    if (!passing) {
        for (int lane = 0; lane < kLanes; ++lane)
            if (cost(lane) < cost(want)
                || (cost(lane) == cost(want) && std::abs(lane - m_target) < std::abs(want - m_target)))
                want = lane;
    }
    in.steer = want > m_target ? 1 : want < m_target ? -1 : 0;

    const Obstacle* mine = inLane[m_target];
    if (mine == nullptr || mine->spent)
        return in;
    switch (mine->kind) {
    case Kind::Root:
    case Kind::Pit: {
        const double ahead = mine->middle();
        in.jump = !m_airborne && ahead > 0.0 && ahead <= jumpLead(m_speed);
        break;
    }
    case Kind::Lintel:
        in.slide = mine->z < m_speed * 0.15 + kRunnerDepth;
        break;
    case Kind::Block:
        break;
    }
    return in;
}

void LaneCourse::step(double dt)
{
    if (m_phase != Phase::Running)
        return;

    m_runSeconds += dt;
    m_speed = std::min(kMaxSpeed, m_speed + kSpeedGain * dt);
    const double dz = m_speed * dt;
    for (Obstacle& o : m_obstacles)
        o.z -= dz;
    for (Coin& c : m_coins)
        c.z -= dz;
    m_nextZ -= dz;
    m_distance += dz;
    std::erase_if(m_obstacles, [](const Obstacle& o) { return o.far() < -kBehind; });
    std::erase_if(m_coins, [](const Coin& c) { return c.z < -kBehind; });
    spawnAhead();

    while (metres() >= m_nextMilestone) {
        record(EventKind::Milestone, m_height, m_nextMilestone);
        m_nextMilestone += kMilestoneMetres;
    }

    m_hurtLeft = std::max(0.0, m_hurtLeft - dt);
    m_jumpQueued = std::max(0.0, m_jumpQueued - dt);

    // Across to the lane the runner is heading for, at a steady rate.
    const double across = dt / kSwitchSeconds;
    m_lane = m_lane < m_target ? std::min(double(m_target), m_lane + across)
                               : std::max(double(m_target), m_lane - across);

    if (m_airborne) {
        m_rise -= kGravity * (m_slide ? kFastFall : 1.0) * dt;
        m_height += m_rise * dt;
        if (m_height <= 0.0) {
            m_height = 0.0;
            m_rise = 0.0;
            m_airborne = false;
            record(EventKind::Landed, 0.0);
        }
    }

    // On the ground over a pit: the runner falls in, costs a heart, and
    // scrambles out with a hop onto the far side, which the spent pit no
    // longer takes away.
    if (!m_airborne && overOpenPit()) {
        for (Obstacle& o : m_obstacles) {
            if (o.kind == Kind::Pit && !o.spent && touches(o)) {
                takeHit(o, EventKind::Fell);
                break;
            }
        }
        if (over())
            return;
        m_airborne = true;
        m_rise = kJumpSpeed * 0.6;
    }

    if (m_jumpQueued > 0.0 && !m_airborne)
        jump();

    if (m_hurtLeft <= 0.0) {
        for (Obstacle& o : m_obstacles) {
            if (o.spent || o.kind == Kind::Pit || !touches(o))
                continue;
            takeHit(o, EventKind::Hit);
            if (over())
                return;
            break;
        }
    }

    const double top = m_height + (m_slide ? kSlideHeight : kStandHeight);
    for (Coin& c : m_coins) {
        if (c.taken || std::abs(c.lane - m_lane) > 0.5 || std::abs(c.z) > 0.6 || c.height < m_height - 0.3
            || c.height > top + 0.3)
            continue;
        c.taken = true;
        ++m_coinsTaken;
        record(EventKind::Coin, c.height, RunnerCourse::kCoinPoints);
    }
}

bool LaneCourse::touches(const Obstacle& o) const
{
    if (std::abs(o.lane - m_lane) >= 0.5 - kForgiveLane + kRunnerHalfLane)
        return false;
    if (o.z + kForgive >= kRunnerDepth / 2.0 || o.far() - kForgive <= -kRunnerDepth / 2.0)
        return false;
    const double top = m_height + (m_slide ? kSlideHeight : kStandHeight);
    switch (o.kind) {
    case Kind::Root:
        return m_height < kRootHeight - kForgive;
    case Kind::Block:
        return m_height < kBlockHeight;
    case Kind::Lintel:
        // Down from well above any jump to the underside, so it cannot be
        // jumped over.
        return top > kLintelClearance + kForgive;
    case Kind::Pit:
        // Only the runner's middle over the hole counts; overOpenPit() asks.
        return std::abs(o.lane - m_lane) < 0.5 && o.z + kForgive < 0.0 && o.far() - kForgive > 0.0;
    }
    return false;
}

bool LaneCourse::overOpenPit() const
{
    return std::any_of(m_obstacles.begin(), m_obstacles.end(), [this](const Obstacle& o) {
        return o.kind == Kind::Pit && !o.spent && touches(o);
    });
}

void LaneCourse::takeHit(Obstacle& o, EventKind kind)
{
    o.spent = true;
    ++m_hits;
    m_hearts = std::max(0, m_hearts - 1);
    m_hurtLeft = kHurtSeconds;
    record(kind, m_height + kStandHeight / 2.0);
    if (m_hearts == 0) {
        m_phase = Phase::Over;
        m_slide = false;
        m_jumpQueued = 0.0;
        record(EventKind::GameOver, m_height + kStandHeight / 2.0);
    }
}

LaneCourse::Kind LaneCourse::pickKind(bool allowBlock)
{
    // Drawn from rng() directly rather than a distribution, so one seed lays
    // the same course on every compiler (see shuffleCards).
    std::vector<Kind> open;
    if (allowBlock)
        open.push_back(Kind::Block);
    if (metres() >= kRootFromMetres)
        open.push_back(Kind::Root);
    if (metres() >= kLintelFromMetres)
        open.push_back(Kind::Lintel);
    if (metres() >= kPitFromMetres)
        open.push_back(Kind::Pit);
    if (open.empty())
        return Kind::Root;
    return open[m_rng() % open.size()];
}

void LaneCourse::spawnAhead()
{
    while (m_nextZ < kSightDepth) {
        // The pace when the runner reaches this row, which is a little more
        // than now; the spacing has to suit that pace, not this one.
        const double v = std::min(kMaxSpeed, m_speed + kSpeedGain * std::max(0.0, m_nextZ) / m_speed);
        const double z = m_nextZ;
        const int row = ++m_rows;

        // How many lanes this row fills. A full row is never a block, so
        // every row leaves a way through.
        int count = 1;
        if (metres() >= kTwoWideFromMetres) {
            const unsigned roll = m_rng() % 6;
            count = roll == 0 ? 3 : roll < 3 ? 2 : 1;
        }
        bool taken[kLanes] = {};
        const int first = int(m_rng() % kLanes);
        const Kind fullKind = count == kLanes ? pickKind(false) : Kind::Root;
        double rowFar = z;
        for (int i = 0; i < count; ++i) {
            const int lane = (first + i) % kLanes;
            taken[lane] = true;
            Obstacle o;
            o.kind = count == kLanes ? fullKind : pickKind(true);
            o.lane = lane;
            o.z = z;
            o.row = row;
            switch (o.kind) {
            case Kind::Root: o.depth = kRootDepth; break;
            case Kind::Lintel: o.depth = kLintelDepth; break;
            case Kind::Block: o.depth = kBlockDepth; break;
            case Kind::Pit: o.depth = pitDepth(v); break;
            }
            o.look = int(m_rng() % 4);
            m_obstacles.push_back(o);
            rowFar = std::max(rowFar, o.far());

            // Half the things to jump carry coins along the jump that clears
            // them: a hint at the timing as well as a prize.
            if ((o.kind == Kind::Root || o.kind == Kind::Pit) && m_rng() % 2 == 0) {
                for (int k = -2; k <= 2; ++k) {
                    const double t = k * 0.1;
                    const double h = kJumpHeight - kGravity * t * t / 2.0 + kStandHeight * 0.4;
                    m_coins.push_back({ lane, o.middle() + v * t, h, false });
                }
            }
        }

        // A line of coins leading into a lane this row leaves open: a hint
        // at the way through. Laid in the second before the row, which the
        // spacing keeps clear of the row before.
        if (count < kLanes && m_rng() % 2 == 0) {
            int open = int(m_rng() % kLanes);
            while (taken[open])
                open = (open + 1) % kLanes;
            const double from = z - v * 1.0;
            const int coins = int((v * 1.0 - 1.5) / kCoinPitch) + 1;
            for (int i = 0; i < coins; ++i)
                m_coins.push_back({ open, from + i * kCoinPitch, kCoinHeight, false });
        }

        // Past this row, landed, recovered, and then a while longer at random.
        m_nextZ = rowFar + v * (kAirTime / 2.0 + kRecoverSeconds) + unit() * v * kSpreadSeconds;
    }
}

void LaneCourse::record(EventKind kind, double height, int value)
{
    // A caller that never reads them (a test, a --shot run) must not grow the
    // list for ever; the oldest go first, since only the newest are drawn.
    if (m_events.size() >= 256)
        m_events.erase(m_events.begin());
    m_events.push_back({ kind, m_lane, height, value });
}

double LaneCourse::unit()
{
    return double(m_rng()) / (double(std::mt19937::max()) + 1.0);
}
