#pragma once

#include "dealseed.h"

#include <random>
#include <vector>

// Ruin Runner seen from behind (GHUB-0241): the runner heads into the screen
// along three lanes, switching lane, jumping and sliding past what comes.
// Scored like the side-on game (RunnerCourse): a point a metre and
// RunnerCourse::kCoinPoints a coin, into the same table.
//
// The rules only, in metres. A lane is a whole number 0..2, left to right;
// `z` is how far ahead of the runner something is, so the course comes
// towards the runner at speed() and z falls. No widget, no timer, no sound
// and no drawing: the view owns the clock and the look, and reads what
// happened from takeEvents().
//
// Every course is fair by construction: obstacles come in rows, every row
// leaves a way through, and rows are spaced so a runner who has just got
// past one has time to cross two lanes and act on the next. pilot() is a
// player that follows those rules exactly, and the self-test runs it for
// minutes on several seeds without a single hit.
class LaneCourse
{
public:
    static constexpr int kLanes = 3;
    // A lane switch takes this long, start to finish.
    static constexpr double kSwitchSeconds = 0.16;

    // The runner, from the feet: standing and sliding heights, and half its
    // width across, in lanes.
    static constexpr double kStandHeight = 1.7;
    static constexpr double kSlideHeight = 0.7;
    static constexpr double kRunnerHalfLane = 0.22;
    // The runner's depth along the path, centred on z = 0.
    static constexpr double kRunnerDepth = 0.5;

    // One jump, always the same height and the same time in the air, so its
    // timing can be learned. The air time is the side-on game's.
    static constexpr double kJumpHeight = 1.25;
    static constexpr double kAirTime = 0.76;
    static constexpr double kGravity = 8.0 * kJumpHeight / (kAirTime * kAirTime);
    static constexpr double kJumpSpeed = kGravity * kAirTime / 2.0;
    // Sliding in the air falls this many times faster.
    static constexpr double kFastFall = 2.6;
    // A jump pressed this soon before landing happens on landing.
    static constexpr double kJumpBuffer = 0.14;

    // The pace, in metres a second: gentle at the start, rising to a cap.
    static constexpr double kStartSpeed = 7.0;
    static constexpr double kMaxSpeed = 14.0;
    static constexpr double kSpeedGain = 0.05;      // per second of running

    static constexpr int kHearts = 3;
    // After a hit nothing can hurt the runner for this long.
    static constexpr double kHurtSeconds = 1.6;
    static constexpr int kMilestoneMetres = 100;

    // Obstacle sizes, in metres. A pit's depth grows with the pace.
    static constexpr double kRootHeight = 0.55;
    static constexpr double kRootDepth = 0.6;
    static constexpr double kBlockHeight = 2.6;
    static constexpr double kBlockDepth = 1.4;
    static constexpr double kLintelDepth = 0.8;
    // The gap under a lintel: room for a sliding runner, not a standing one.
    static constexpr double kLintelClearance = 1.05;
    // Every hit box is this much shorter than the obstacle drawn, front and
    // back, and this many lanes narrower each side, so a near miss is a miss.
    static constexpr double kForgive = 0.12;
    static constexpr double kForgiveLane = 0.08;

    // Each kind first appears this far into a run, so the run teaches them
    // one at a time. Blocks come first: they teach the lanes.
    static constexpr int kRootFromMetres = 60;
    static constexpr int kLintelFromMetres = 150;
    static constexpr int kPitFromMetres = 250;
    // From here a row may fill two lanes.
    static constexpr int kTwoWideFromMetres = 350;

    // How far ahead the course is laid, and so how far the view can see.
    static constexpr double kSightDepth = 80.0;

    // After getting past one row, at least this long before the next reaches
    // the runner: room to cross two lanes and look.
    static constexpr double kRecoverSeconds = 0.9;
    // And up to this much longer, at random.
    static constexpr double kSpreadSeconds = 1.2;

    static constexpr double kCoinPitch = 2.4;
    static constexpr double kCoinHeight = 0.8;

    enum class Phase { Ready, Running, Over };
    // Root: jump. Lintel: slide. Block: change lane. Pit: jump.
    enum class Kind { Root, Lintel, Block, Pit };

    enum class EventKind { Started, Jumped, Landed, Switched, Coin, Hit, Fell, Milestone, GameOver };
    struct Event {
        EventKind kind = EventKind::Started;
        double lane = 0.0;   // across the path, where it happened
        double height = 0.0; // above the path
        int value = 0;       // the metres, for Milestone; the points, for Coin
    };

    struct Obstacle {
        Kind kind = Kind::Root;
        int lane = 0;
        double z = 0.0;       // the near face
        double depth = 0.0;
        bool spent = false;   // already cost a heart, so it can hurt no more
        int look = 0;         // which of a few drawings; no effect on play
        int row = 0;          // obstacles laid together share a row
        double far() const { return z + depth; }
        double middle() const { return z + depth / 2.0; }
    };
    struct Coin {
        int lane = 0;
        double z = 0.0;
        double height = kCoinHeight;
        bool taken = false;
    };
    // What a player does this instant: a lane step (-1 left, +1 right),
    // press jump, hold slide.
    struct Input {
        int steer = 0;
        bool jump = false;
        bool slide = false;
    };

    LaneCourse() { newGame(); }

    void newGame();
    // Starts a Ready run. Returns whether it did.
    bool start();
    // Moves the lane the runner heads for one step; a Ready run starts
    // instead. Returns whether the target moved.
    bool steer(int direction);
    // Jumps if on the ground; in the air, remembers the press for
    // kJumpBuffer. A Ready run starts instead of jumping.
    bool jump();
    // Held: slides on the ground, falls faster in the air.
    void setSlide(bool held);

    // Moves the run on by `seconds`, in small fixed steps so a slow frame
    // plays the same as two quick ones.
    void advance(double seconds);

    // What happened since the last call, oldest first.
    std::vector<Event> takeEvents();

    Phase phase() const { return m_phase; }
    bool over() const { return m_phase == Phase::Over; }
    int hearts() const { return m_hearts; }
    int metres() const { return int(m_distance); }
    int coinsTaken() const { return m_coinsTaken; }
    int score() const;
    double speed() const { return m_speed; }
    // Metres the course has come; the view's scenery and stride read it.
    double distance() const { return m_distance; }
    double runSeconds() const { return m_runSeconds; }
    int hits() const { return m_hits; }

    // Where the runner is across the path, in lanes: between two during a
    // switch.
    double lane() const { return m_lane; }
    int targetLane() const { return m_target; }
    double height() const { return m_height; }
    bool airborne() const { return m_airborne; }
    bool sliding() const { return m_slide; }
    // Seconds of safety left after a hit; 0 when the runner can be hurt.
    double hurtLeft() const { return m_hurtLeft; }

    // Kept in the order they were laid, which is nearest first.
    const std::vector<Obstacle>& obstacles() const { return m_obstacles; }
    const std::vector<Coin>& coins() const { return m_coins; }

    // A pit's depth at a given pace: always well short of one jump.
    static double pitDepth(double speed);
    // How far ahead an obstacle's middle is when a jump should start, so the
    // top of the jump comes over it.
    static double jumpLead(double speed) { return speed * kAirTime / 2.0; }

    // A player who follows the rules above exactly. The self-test runs it to
    // prove every course is fair, and --shot flies it.
    Input pilot() const;

private:
    void step(double dt);
    void spawnAhead();
    Kind pickKind(bool allowBlock);
    bool overOpenPit() const;
    bool touches(const Obstacle& o) const;
    void takeHit(Obstacle& o, EventKind kind);
    void record(EventKind kind, double height, int value = 0);
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

    double m_lane = 1.0;
    int m_target = 1;
    double m_height = 0.0;
    double m_rise = 0.0;      // metres a second, up positive
    bool m_airborne = false;
    bool m_slide = false;
    double m_jumpQueued = 0.0;
    double m_hurtLeft = 0.0;

    std::vector<Obstacle> m_obstacles;
    std::vector<Coin> m_coins;
    // Where the next row's near face goes; comes towards the runner.
    double m_nextZ = 0.0;
    int m_rows = 0;
    std::vector<Event> m_events;
};
