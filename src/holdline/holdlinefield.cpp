#include "holdlinefield.h"

#include <algorithm>
#include <cmath>

namespace {
// The physics step. Small enough that a shot (the fastest thing on the field)
// moves less than an attacker is tall, so it can never step over one.
constexpr double kStep = 1.0 / 120.0;

// Which blocks a fresh shelter has: a low mound, rounded over the top and
// solid to the ground.
bool blockStartsSolid(int row, int column)
{
    const double half = HoldLineField::kShelterColumns / 2.0;
    const double dx = (column + 0.5 - half) / half;
    const double dy = (HoldLineField::kShelterRows - row - 0.5) / HoldLineField::kShelterRows;
    return dx * dx + dy * dy <= 1.0;
}
}

void HoldLineField::newGame()
{
    m_phase = Phase::Ready;
    m_ending = Ending::None;
    m_score = 0;
    m_lives = kLives;
    m_wave = 1;
    m_marchCount = 0;
    m_cannonX = kWidth / 2.0;
    m_move = 0;
    m_shotLive = false;
    m_pauseLeft = 0.0;
    m_events.clear();

    // Shelters are built once per game, not per wave: what you shot away in
    // the first wave stays shot away, which is what makes them worth guarding.
    const double width = kShelterColumns * kBlock;
    for (int i = 0; i < kShelters; ++i) {
        Shelter& s = m_shelters[std::size_t(i)];
        const double centre = kWidth * (i + 1) / (kShelters + 1);
        s.topLeft = QPointF(centre - width / 2.0, kShelterTop);
        for (int r = 0; r < kShelterRows; ++r)
            for (int c = 0; c < kShelterColumns; ++c)
                s.blocks[std::size_t(r)][std::size_t(c)] = blockStartsSolid(r, c) ? kBlockStrength : 0;
    }
    startWave();
}

void HoldLineField::startWave()
{
    m_attackers.clear();
    const double lowered = std::min(m_wave - 1, kMaxLowerings) * kWaveLowering;
    const double left = (kWidth - (kColumns - 1) * kColumnPitch) / 2.0;
    for (int r = 0; r < kRows; ++r)
        for (int c = 0; c < kColumns; ++c)
            m_attackers.push_back({ QPointF(left + c * kColumnPitch,
                                            kFirstRowY + lowered + r * kRowPitch),
                                    r, true });
    m_marchDirection = 1;
    m_marchClock = 0.0;
    m_bombClock = 0.0;
    m_bombs.clear();
    m_shotLive = false;
}

void HoldLineField::setMove(int direction)
{
    m_move = std::clamp(direction, -1, 1);
    if (m_phase == Phase::Ready && m_move != 0)
        m_phase = Phase::Playing;
}

bool HoldLineField::fire()
{
    if (m_phase == Phase::Ready)
        m_phase = Phase::Playing;
    if (m_phase != Phase::Playing || m_shotLive)
        return false;
    m_shotLive = true;
    m_shot = QPointF(m_cannonX, kCannonY - kCannonHalfHeight - kShotHalfLength);
    record(EventKind::Fired, m_shot);
    return true;
}

void HoldLineField::advance(double seconds)
{
    while (seconds > 1e-9) {
        const double dt = std::min(seconds, kStep);
        step(dt);
        seconds -= dt;
    }
}

std::vector<HoldLineField::Event> HoldLineField::takeEvents()
{
    std::vector<Event> out;
    out.swap(m_events);
    return out;
}

int HoldLineField::attackersLeft() const
{
    return int(std::count_if(m_attackers.begin(), m_attackers.end(),
                             [](const Attacker& a) { return a.alive; }));
}

double HoldLineField::marchInterval() const
{
    // Unhurried at the start of each wave, and quickening as it thins: the
    // last attacker of the first wave moves about eight times as often as the
    // full wave did. Later waves start a little brisker, with a floor.
    const double base = std::max(0.42, 0.85 * std::pow(0.92, m_wave - 1));
    const double share = double(attackersLeft()) / double(kRows * kColumns);
    return std::max(0.06, base * (0.12 + 0.88 * share));
}

QRectF HoldLineField::attackerBox(const Attacker& a)
{
    return { a.centre.x() - kAttackerHalfWidth, a.centre.y() - kAttackerHalfHeight,
             2 * kAttackerHalfWidth, 2 * kAttackerHalfHeight };
}

QRectF HoldLineField::blockRect(const Shelter& s, int row, int column)
{
    return { s.topLeft.x() + column * kBlock, s.topLeft.y() + row * kBlock, kBlock, kBlock };
}

QRectF HoldLineField::cannonBox() const
{
    return { m_cannonX - kCannonHalfWidth, kCannonY - kCannonHalfHeight,
             2 * kCannonHalfWidth, 2 * kCannonHalfHeight };
}

void HoldLineField::step(double dt)
{
    switch (m_phase) {
    case Phase::Ready:
    case Phase::Over:
        return;
    case Phase::Respawning:
        m_pauseLeft -= dt;
        if (m_pauseLeft <= 0.0) {
            m_pauseLeft = 0.0;
            m_cannonX = kWidth / 2.0;
            m_phase = Phase::Playing;
        }
        return;
    case Phase::WaveCleared:
        m_cannonX = std::clamp(m_cannonX + m_move * kCannonSpeed * dt, kCannonMinX, kCannonMaxX);
        m_pauseLeft -= dt;
        if (m_pauseLeft <= 0.0) {
            m_pauseLeft = 0.0;
            ++m_wave;
            startWave();
            m_phase = Phase::Playing;
            record(EventKind::WaveStarted, QPointF(kWidth / 2.0, kHeight / 2.0));
        }
        return;
    case Phase::Playing:
        break;
    }

    m_cannonX = std::clamp(m_cannonX + m_move * kCannonSpeed * dt, kCannonMinX, kCannonMaxX);

    m_marchClock += dt;
    if (m_marchClock >= marchInterval()) {
        m_marchClock = 0.0;
        march();
        if (m_phase != Phase::Playing)
            return;
    }

    m_bombClock += dt;
    const double bombEvery = std::max(0.6, 1.5 - 0.1 * (m_wave - 1));
    if (m_bombClock >= bombEvery) {
        m_bombClock = 0.0;
        dropBomb();
    }

    moveShot(dt);
    if (m_phase != Phase::Playing)
        return;
    moveBombs(dt);
}

void HoldLineField::march()
{
    double lowest = 0.0;
    double leftmost = kWidth;
    double rightmost = 0.0;
    for (const Attacker& a : m_attackers) {
        if (!a.alive)
            continue;
        leftmost = std::min(leftmost, a.centre.x() - kAttackerHalfWidth);
        rightmost = std::max(rightmost, a.centre.x() + kAttackerHalfWidth);
    }

    // Down and about at the edge, otherwise one step along.
    const bool atEdge = (m_marchDirection > 0 && rightmost + kMarchStep > kWidth - kEdge)
                        || (m_marchDirection < 0 && leftmost - kMarchStep < kEdge);
    for (Attacker& a : m_attackers) {
        if (atEdge)
            a.centre.ry() += kDropStep;
        else
            a.centre.rx() += m_marchDirection * kMarchStep;
        if (a.alive)
            lowest = std::max(lowest, a.centre.y() + kAttackerHalfHeight);
    }
    if (atEdge)
        m_marchDirection = -m_marchDirection;
    ++m_marchCount;
    record(EventKind::Marched, QPointF());

    attackersCrushShelters();
    if (lowest >= kCannonY - kCannonHalfHeight)
        end(Ending::Landed);
}

void HoldLineField::attackersCrushShelters()
{
    for (const Attacker& a : m_attackers) {
        if (!a.alive)
            continue;
        const QRectF box = attackerBox(a);
        for (Shelter& s : m_shelters)
            for (int r = 0; r < kShelterRows; ++r)
                for (int c = 0; c < kShelterColumns; ++c)
                    if (s.blocks[std::size_t(r)][std::size_t(c)] > 0
                        && box.intersects(blockRect(s, r, c)))
                        s.blocks[std::size_t(r)][std::size_t(c)] = 0;
    }
}

void HoldLineField::moveShot(double dt)
{
    if (!m_shotLive)
        return;
    m_shot.ry() -= kShotSpeed * dt;
    const QPointF tip(m_shot.x(), m_shot.y() - kShotHalfLength);

    if (tip.y() < 40.0) {
        m_shotLive = false;
        return;
    }

    for (Attacker& a : m_attackers) {
        if (a.alive && attackerBox(a).adjusted(-1.5, 0, 1.5, 0).contains(tip)) {
            a.alive = false;
            m_shotLive = false;
            const int points = kRowPoints[std::size_t(a.row)];
            m_score += points;
            record(EventKind::AttackerHit, a.centre, a.row, points);
            if (attackersLeft() == 0) {
                const int bonus = kWaveBonusPer * m_wave;
                m_score += bonus;
                m_bombs.clear();
                m_phase = Phase::WaveCleared;
                m_pauseLeft = kWavePause;
                record(EventKind::WaveCleared, QPointF(kWidth / 2.0, kHeight / 2.0), -1, bonus);
            }
            return;
        }
    }

    // A shot meeting a bomb takes both out of the air.
    for (auto it = m_bombs.begin(); it != m_bombs.end(); ++it) {
        const QRectF box(it->centre.x() - kBombHalfWidth - 2, it->centre.y() - kBombHalfLength,
                         2 * kBombHalfWidth + 4, 2 * kBombHalfLength);
        if (box.contains(tip)) {
            record(EventKind::BombBurst, it->centre);
            m_bombs.erase(it);
            m_shotLive = false;
            return;
        }
    }

    // Your own shot chips a shelter more gently than a bomb blasts it.
    if (wearShelterAt(tip, 1)) {
        m_shotLive = false;
        record(EventKind::ShelterHit, tip);
    }
}

void HoldLineField::dropBomb()
{
    if (int(m_bombs.size()) >= kMaxBombs)
        return;
    // The lowest attacker of a column chosen at random among those still
    // standing. Drawn from rng() directly rather than a distribution, so one
    // seed plays the same game on every compiler (see shuffleCards).
    std::array<int, kColumns> lowestInColumn {};
    lowestInColumn.fill(-1);
    for (int i = 0; i < int(m_attackers.size()); ++i)
        if (m_attackers[std::size_t(i)].alive)
            lowestInColumn[std::size_t(i % kColumns)] = i;
    std::vector<int> candidates;
    for (int i : lowestInColumn)
        if (i >= 0)
            candidates.push_back(i);
    if (candidates.empty())
        return;
    const Attacker& from = m_attackers[std::size_t(candidates[m_rng() % candidates.size()])];
    const double speed = std::min(250.0, 150.0 + 12.0 * (m_wave - 1));
    m_bombs.push_back({ QPointF(from.centre.x(), from.centre.y() + kAttackerHalfHeight + kBombHalfLength),
                        speed });
}

void HoldLineField::moveBombs(double dt)
{
    const QRectF cannon = cannonBox();
    for (auto it = m_bombs.begin(); it != m_bombs.end();) {
        it->centre.ry() += it->speed * dt;
        const QPointF tip(it->centre.x(), it->centre.y() + kBombHalfLength);
        const QRectF box(it->centre.x() - kBombHalfWidth, it->centre.y() - kBombHalfLength,
                         2 * kBombHalfWidth, 2 * kBombHalfLength);
        if (box.intersects(cannon)) {
            record(EventKind::CannonHit, QPointF(m_cannonX, kCannonY));
            loseLife();
            return;
        }
        if (wearShelterAt(tip, 2)) {
            record(EventKind::ShelterHit, tip);
            it = m_bombs.erase(it);
            continue;
        }
        if (tip.y() > kCannonY + kCannonHalfHeight + 14.0) {
            record(EventKind::BombBurst, tip);
            it = m_bombs.erase(it);
            continue;
        }
        ++it;
    }
}

bool HoldLineField::wearShelterAt(const QPointF& p, int hardHit)
{
    for (Shelter& s : m_shelters) {
        const int c = int(std::floor((p.x() - s.topLeft.x()) / kBlock));
        const int r = int(std::floor((p.y() - s.topLeft.y()) / kBlock));
        if (c < 0 || r < 0 || c >= kShelterColumns || r >= kShelterRows)
            continue;
        int& hit = s.blocks[std::size_t(r)][std::size_t(c)];
        if (hit <= 0)
            return false;
        // A round crater rather than a pinprick: the block struck and its four
        // neighbours lose the full blow, the diagonals half of it.
        for (int dr = -1; dr <= 1; ++dr)
            for (int dc = -1; dc <= 1; ++dc) {
                const int rr = r + dr;
                const int cc = c + dc;
                if (rr < 0 || cc < 0 || rr >= kShelterRows || cc >= kShelterColumns)
                    continue;
                const int blow = (dr != 0 && dc != 0) ? hardHit / 2 : hardHit;
                int& b = s.blocks[std::size_t(rr)][std::size_t(cc)];
                b = std::max(0, b - blow);
            }
        // A block left touching nothing on any side crumbles, so a worn
        // shelter has no floating crumbs that look solid and are not.
        for (int rr = std::max(0, r - 2); rr <= std::min(kShelterRows - 1, r + 2); ++rr)
            for (int cc = std::max(0, c - 2); cc <= std::min(kShelterColumns - 1, c + 2); ++cc) {
                int& b = s.blocks[std::size_t(rr)][std::size_t(cc)];
                if (b <= 0)
                    continue;
                const auto solid = [&s](int y, int x) {
                    return y >= 0 && x >= 0 && y < kShelterRows && x < kShelterColumns
                           && s.blocks[std::size_t(y)][std::size_t(x)] > 0;
                };
                if (!solid(rr - 1, cc) && !solid(rr + 1, cc) && !solid(rr, cc - 1) && !solid(rr, cc + 1))
                    b = 0;
            }
        return true;
    }
    return false;
}

void HoldLineField::loseLife()
{
    --m_lives;
    m_bombs.clear();
    m_shotLive = false;
    if (m_lives <= 0) {
        m_lives = 0;
        end(Ending::OutOfLives);
        return;
    }
    m_phase = Phase::Respawning;
    m_pauseLeft = kRespawnPause;
}

void HoldLineField::end(Ending why)
{
    m_phase = Phase::Over;
    m_ending = why;
    m_move = 0;
    m_bombs.clear();
    m_shotLive = false;
    record(EventKind::GameOver, QPointF(m_cannonX, kCannonY));
}

void HoldLineField::record(EventKind kind, QPointF at, int row, int points)
{
    // A caller that never reads them (a test, a --shot run) must not grow the
    // list for ever; the oldest go first, since only the newest are drawn.
    if (m_events.size() >= 256)
        m_events.erase(m_events.begin());
    m_events.push_back({ kind, at, row, points });
}
