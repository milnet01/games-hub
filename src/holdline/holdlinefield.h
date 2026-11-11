#pragma once

#include "dealseed.h"

#include <QPointF>
#include <QRectF>

#include <array>
#include <random>
#include <vector>

// Hold the Line: a wave of attackers marches sideways and down, and a cannon
// along the bottom shoots it before it lands. Shelters between the two wear
// away under fire from either side.
//
// The rules only, in fixed field units (kWidth x kHeight, y down), so the
// window's size never changes the game. No widget, no timer, no sound and no
// drawing: the view owns the clock and the look, and reads what happened from
// takeEvents(). That split is what lets gameshub_selftest play it headless.
class HoldLineField
{
public:
    static constexpr double kWidth = 480.0;
    static constexpr double kHeight = 540.0;

    static constexpr int kRows = 5;
    static constexpr int kColumns = 9;
    static constexpr int kLives = 3;

    // The cannon rides along this line, between these limits.
    static constexpr double kCannonY = 500.0;
    static constexpr double kCannonHalfWidth = 22.0;
    static constexpr double kCannonHalfHeight = 12.0;
    static constexpr double kCannonMinX = 30.0;
    static constexpr double kCannonMaxX = kWidth - 30.0;
    static constexpr double kCannonSpeed = 230.0;   // units a second

    static constexpr double kShotSpeed = 520.0;
    static constexpr double kShotHalfLength = 8.0;

    // An attacker's box, and the grid the wave keeps.
    static constexpr double kAttackerHalfWidth = 15.0;
    static constexpr double kAttackerHalfHeight = 11.0;
    static constexpr double kColumnPitch = 40.0;
    static constexpr double kRowPitch = 34.0;
    static constexpr double kMarchStep = 8.0;
    static constexpr double kDropStep = 14.0;
    static constexpr double kEdge = 12.0;
    // The wave's top row starts here, and each later wave a little lower.
    static constexpr double kFirstRowY = 92.0;
    static constexpr double kWaveLowering = 12.0;
    static constexpr int kMaxLowerings = 4;

    // Shelters: blocks in a grid, each worn down a step at a time.
    static constexpr int kShelters = 4;
    static constexpr int kShelterColumns = 16;
    static constexpr int kShelterRows = 10;
    static constexpr double kBlock = 4.0;
    static constexpr double kShelterTop = 410.0;
    static constexpr int kBlockStrength = 3;

    static constexpr int kMaxBombs = 3;
    static constexpr double kBombHalfWidth = 5.0;
    static constexpr double kBombHalfLength = 8.0;

    // Pauses, in seconds, during which nothing marches or fires.
    static constexpr double kRespawnPause = 1.8;
    static constexpr double kWavePause = 2.2;

    // Points for an attacker by its row, top row first.
    static constexpr std::array<int, kRows> kRowPoints { 30, 20, 20, 10, 10 };
    static constexpr int kWaveBonusPer = 50;

    // Where play stands. Ready waits for the first shot or move, so nothing
    // happens before the player is looking.
    enum class Phase { Ready, Playing, Respawning, WaveCleared, Over };
    // Why the game ended.
    enum class Ending { None, OutOfLives, Landed };

    enum class EventKind { Fired, AttackerHit, CannonHit, ShelterHit, BombBurst, Marched,
                           WaveCleared, WaveStarted, GameOver };
    struct Event {
        EventKind kind = EventKind::Fired;
        QPointF at;
        int row = -1;     // the attacker's row, for AttackerHit
        int points = 0;   // what it scored, for AttackerHit and WaveCleared
    };

    struct Attacker {
        QPointF centre;
        int row = 0;
        bool alive = true;
    };
    struct Bomb {
        QPointF centre;
        double speed = 0.0;
    };
    struct Shelter {
        QPointF topLeft;
        // [row][column], 0 = gone, kBlockStrength = untouched.
        std::array<std::array<int, kShelterColumns>, kShelterRows> blocks {};
    };

    HoldLineField() { newGame(); }

    void newGame();

    // -1, 0 or 1: which way the cannon is being driven. Starts a Ready game.
    void setMove(int direction);
    // Fires if no shot of yours is in the air. Returns whether it fired.
    // Starts a Ready game.
    bool fire();

    // Moves the game on by `seconds`, in small fixed steps so a slow frame
    // plays the same as two quick ones.
    void advance(double seconds);

    // What happened since the last call, oldest first.
    std::vector<Event> takeEvents();

    Phase phase() const { return m_phase; }
    Ending ending() const { return m_ending; }
    bool over() const { return m_phase == Phase::Over; }
    int score() const { return m_score; }
    int lives() const { return m_lives; }
    int wave() const { return m_wave; }
    // Counts every sideways or downward step, so a drawing can alternate poses.
    int marchCount() const { return m_marchCount; }
    // Seconds left in a respawn or wave pause; 0 otherwise.
    double pauseLeft() const { return m_pauseLeft; }
    double cannonX() const { return m_cannonX; }
    bool cannonShown() const { return m_phase != Phase::Respawning && m_phase != Phase::Over; }
    bool shotInFlight() const { return m_shotLive; }
    QPointF shot() const { return m_shot; }
    const std::vector<Attacker>& attackers() const { return m_attackers; }
    int attackersLeft() const;
    const std::vector<Bomb>& bombs() const { return m_bombs; }
    const std::array<Shelter, kShelters>& shelters() const { return m_shelters; }
    // Seconds between march steps right now: it shortens as the wave thins.
    double marchInterval() const;

    static QRectF attackerBox(const Attacker& a);
    static QRectF blockRect(const Shelter& s, int row, int column);
    QRectF cannonBox() const;

private:
    void startWave();
    void step(double dt);
    void march();
    void moveShot(double dt);
    void moveBombs(double dt);
    void dropBomb();
    // Wears the block under `p` and its neighbours. Returns whether one was hit.
    bool wearShelterAt(const QPointF& p, int hardHit);
    void attackersCrushShelters();
    void loseLife();
    void end(Ending why);
    void record(EventKind kind, QPointF at, int row = -1, int points = 0);

    std::mt19937 m_rng { dealSeed() };

    Phase m_phase = Phase::Ready;
    Ending m_ending = Ending::None;
    int m_score = 0;
    int m_lives = kLives;
    int m_wave = 1;
    int m_marchCount = 0;

    double m_cannonX = kWidth / 2.0;
    int m_move = 0;

    bool m_shotLive = false;
    QPointF m_shot;

    std::vector<Attacker> m_attackers;
    int m_marchDirection = 1;
    double m_marchClock = 0.0;
    double m_bombClock = 0.0;
    std::vector<Bomb> m_bombs;
    std::array<Shelter, kShelters> m_shelters {};

    double m_pauseLeft = 0.0;
    std::vector<Event> m_events;
};
