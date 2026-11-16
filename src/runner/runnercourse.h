#pragma once

#include "dealseed.h"

#include <QPointF>
#include <QRectF>

#include <random>
#include <vector>

// Ruin Runner: a runner dashes left to right through jungle ruins, jumping
// roots, carved stones and pits, and ducking under fallen lintels. The pace
// rises the longer the run lasts. Three hearts; the third hit ends the run.
//
// The rules only, in fixed course units (kWidth x kHeight, y down), so the
// window's size never changes the game. The runner stays at kRunnerX and the
// course scrolls past. No widget, no timer, no sound and no drawing: the view
// owns the clock and the look, and reads what happened from takeEvents().
//
// Every course is fair by construction: obstacles are spaced so a runner who
// has just landed has time to see the next one, and every size is chosen so
// one jump clears it at any speed the run reaches. pilot() is a player that
// follows those rules exactly, and the self-test runs it for minutes on
// several seeds without a single hit.
class RunnerCourse
{
public:
    static constexpr double kWidth = 640.0;
    static constexpr double kHeight = 360.0;
    static constexpr double kGroundY = 300.0;

    // The runner: a box standing on its feet at kRunnerX.
    static constexpr double kRunnerX = 140.0;
    static constexpr double kRunnerHalfWidth = 14.0;
    static constexpr double kStandHeight = 58.0;
    static constexpr double kDuckHeight = 30.0;

    // One jump, always the same height, so its timing can be learned.
    static constexpr double kGravity = 1900.0;      // units a second, squared
    static constexpr double kJumpSpeed = 720.0;     // units a second, upward
    static constexpr double kAirTime = 2.0 * kJumpSpeed / kGravity;
    // Ducking in the air falls this many times faster.
    static constexpr double kFastFall = 2.6;
    // A jump pressed this soon before landing happens on landing.
    static constexpr double kJumpBuffer = 0.14;

    // The pace, in units a second: gentle at the start, rising to a cap.
    static constexpr double kStartSpeed = 170.0;
    static constexpr double kMaxSpeed = 340.0;
    static constexpr double kSpeedGain = 1.4;       // per second of running

    static constexpr int kHearts = 3;
    // After a hit nothing can hurt the runner for this long.
    static constexpr double kHurtSeconds = 1.6;

    static constexpr double kUnitsPerMetre = 30.0;
    static constexpr int kCoinPoints = 5;
    static constexpr double kCoinRadius = 9.0;
    static constexpr int kMilestoneMetres = 100;

    // Obstacle sizes. A gap's width grows with the pace; see gapWidth().
    static constexpr double kRootWidth = 34.0;
    static constexpr double kRootHeight = 22.0;
    static constexpr double kStoneWidth = 40.0;
    static constexpr double kStoneHeight = 40.0;
    static constexpr double kArchWidth = 60.0;
    // The gap under a lintel: room for a ducking runner, not a standing one.
    static constexpr double kArchClearance = 40.0;
    // Every hit box is this much smaller than the obstacle drawn, on each
    // side, so a near miss counts as a miss.
    static constexpr double kForgive = 3.0;

    // Each kind first appears this far into a run, so the run teaches them
    // one at a time.
    static constexpr int kStoneFromMetres = 50;
    static constexpr int kGapFromMetres = 120;
    static constexpr int kArchFromMetres = 200;

    // After landing from one obstacle, at least this long before the next
    // must be acted on.
    static constexpr double kRecoverSeconds = 0.55;
    // And up to this much longer, at random.
    static constexpr double kSpreadSeconds = 1.4;

    enum class Phase { Ready, Running, Over };
    enum class Kind { Root, Stone, Gap, Arch };

    enum class EventKind { Started, Jumped, Landed, Coin, Hit, Fell, Milestone, GameOver };
    struct Event {
        EventKind kind = EventKind::Started;
        QPointF at;
        int value = 0;   // the metres, for Milestone; the points, for Coin
    };

    struct Obstacle {
        Kind kind = Kind::Root;
        double x = 0.0;       // left edge
        double width = 0.0;
        bool spent = false;   // already cost a heart, so it can hurt no more
        int look = 0;         // which of a few drawings; no effect on play
        double right() const { return x + width; }
        double centre() const { return x + width / 2.0; }
    };
    struct Coin {
        QPointF at;
        bool taken = false;
    };
    // What a player does this instant: press jump, hold duck.
    struct Input {
        bool jump = false;
        bool duck = false;
    };

    RunnerCourse() { newGame(); }

    void newGame();
    // Starts a Ready run. Returns whether it did.
    bool start();
    // Jumps if the runner is on the ground; in the air, remembers the press
    // for kJumpBuffer. A Ready run starts instead of jumping.
    bool jump();
    // Held: ducks on the ground, falls faster in the air.
    void setDuck(bool held);

    // Moves the run on by `seconds`, in small fixed steps so a slow frame
    // plays the same as two quick ones.
    void advance(double seconds);

    // What happened since the last call, oldest first.
    std::vector<Event> takeEvents();

    Phase phase() const { return m_phase; }
    bool over() const { return m_phase == Phase::Over; }
    int hearts() const { return m_hearts; }
    int metres() const { return int(m_distance / kUnitsPerMetre); }
    int coinsTaken() const { return m_coinsTaken; }
    int score() const { return metres() + m_coinsTaken * kCoinPoints; }
    double speed() const { return m_speed; }
    // Units the course has scrolled; the view's parallax and stride read it.
    double distance() const { return m_distance; }
    double runSeconds() const { return m_runSeconds; }
    int hits() const { return m_hits; }

    double feetY() const { return m_feetY; }
    bool airborne() const { return m_airborne; }
    bool ducking() const { return m_duck; }
    // Seconds of safety left after a hit; 0 when the runner can be hurt.
    double hurtLeft() const { return m_hurtLeft; }

    const std::vector<Obstacle>& obstacles() const { return m_obstacles; }
    const std::vector<Coin>& coins() const { return m_coins; }

    QRectF runnerBox() const;
    // The box that hurts. Empty for a gap, which is a hole rather than a box.
    static QRectF hitBox(const Obstacle& o);
    // A gap's width at a given pace: always one jump wide or less.
    static double gapWidth(double speed);
    // How far ahead of the runner an obstacle's centre is when a jump
    // should start, so the top of the jump comes over its middle.
    static double jumpLead(double speed) { return speed * kJumpSpeed / kGravity; }

    // A player who follows the rules above exactly. The self-test runs it to
    // prove every course is fair, and --shot flies it.
    Input pilot() const;

private:
    void step(double dt);
    void spawnAhead();
    Kind pickKind();
    void addCoinRow(double from, double to);
    void addCoinArc(const Obstacle& o, double speed);
    // The runner's middle is over a gap that has not yet cost a heart.
    bool overOpenGap() const;
    void takeHit(Obstacle& o, EventKind kind);
    void record(EventKind kind, QPointF at, int value = 0);
    double unit();

    std::mt19937 m_rng { dealSeed() };

    Phase m_phase = Phase::Ready;
    int m_hearts = kHearts;
    int m_hits = 0;
    int m_coinsTaken = 0;
    double m_speed = kStartSpeed;
    double m_distance = 0.0;
    double m_runSeconds = 0.0;
    int m_nextMilestone = kMilestoneMetres;

    double m_feetY = kGroundY;
    double m_rise = 0.0;      // vertical speed, units a second, up negative
    bool m_airborne = false;
    bool m_duck = false;
    double m_jumpQueued = 0.0;
    double m_hurtLeft = 0.0;

    std::vector<Obstacle> m_obstacles;
    std::vector<Coin> m_coins;
    // Where the next obstacle's left edge goes; scrolls with the course.
    double m_nextX = 0.0;
    std::vector<Event> m_events;
};
