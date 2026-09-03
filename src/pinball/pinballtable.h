#pragma once

#include <QPointF>

#include <vector>

// The pinball simulation, with no widget attached: geometry, ball, flippers,
// scoring. Keeping it separate is what lets the self-test launch a ball and
// check it actually reaches the play field.
class PinballTable
{
public:
    static constexpr double kWidth = 400.0;
    static constexpr double kHeight = 720.0;
    // Left of this line is the play field; right of it is the launch lane.
    static constexpr double kLaneX = 352.0;
    static constexpr double kDrainY = 706.0;
    static constexpr double kBallPark = 660.0;
    static constexpr double kDomeCentreY = 210.0;
    static constexpr double kDomeRadius = 186.0;
    static constexpr double kGravity = 900.0;

    // A wall is a fat line segment: every collision in the table is a circle
    // against one of these, which keeps the physics to a single routine.
    struct Wall {
        QPointF a;
        QPointF b;
        double radius = 4.0;
        double bounce = 0.62;
        double kick = 0.0; // slingshots push the ball away rather than just stopping it
        int score = 0;
        double flash = 0.0;
        bool oneWayUp = false; // a gate the ball passes upward but cannot fall back through
        // Which way a kicking wall faces. A slingshot's rubber is on its
        // playfield side and its back is plain wood, so the kick fires only
        // where this normal points. Unset on a wall that does not kick.
        QPointF kickFace;
    };

    struct Bumper {
        QPointF centre;
        double radius = 26.0;
        double kick = 200.0;
        int score = 100;
        double flash = 0.0;
    };

    struct Flipper {
        QPointF pivot;
        double length = 74.0;
        double restAngle = 0.52;
        double activeAngle = -0.42;
        double angle = 0.52;
        double previousAngle = 0.52;
        bool pressed = false;
        bool facingRight = true;
    };

    PinballTable();

    void newGame();
    // Advances the simulation by `seconds`, internally in small fixed slices.
    void advance(double seconds);

    void setFlipper(bool left, bool pressed);
    // Places the ball outright, in play. The same idea as Canasta's
    // newGameFromStock: a check builds the exact position it wants to reason
    // about rather than hunting for a launch that happens to produce it.
    void placeBall(QPointF at, QPointF velocity);
    void chargePlunger(double seconds);
    // Fires the plunger. Does nothing unless the ball is waiting in the lane.
    void launch();

    QPointF ball() const { return m_ball; }
    QPointF velocity() const { return m_velocity; }
    double ballRadius() const { return m_ballRadius; }
    bool ballInLane() const { return m_inLane; }
    bool ballInPlayfield() const { return !m_inLane && m_ball.x() < kLaneX; }
    double plunger() const { return m_plunger; }

    int score() const { return m_score; }
    int ballsLeft() const { return m_ballsLeft; }
    bool gameOver() const { return m_gameOver; }
    // True on the frame a ball was lost, so the view can react once.
    bool takeBallLost() { const bool v = m_ballLost; m_ballLost = false; return v; }

    // Hit counts since the last call. The table stays free of any audio
    // dependency; the view drains these each frame and makes the noise.
    int takeBumperHits() { const int v = m_bumperHits; m_bumperHits = 0; return v; }
    int takeSlingHits() { const int v = m_slingHits; m_slingHits = 0; return v; }

    const std::vector<Wall>& walls() const { return m_walls; }
    const std::vector<Bumper>& bumpers() const { return m_bumpers; }
    const Flipper& leftFlipper() const { return m_left; }
    const Flipper& rightFlipper() const { return m_right; }
    QPointF flipperTip(const Flipper& f) const;

    // The launch speed a full-strength plunger delivers, exposed so the view
    // can show a meaningful charge meter.
    static double minimumLaunchSpeed();

private:
    void buildTable();
    void resetBall();
    void stepPhysics(double dt);
    void collideWall(Wall& w);
    void collideBumper(Bumper& b);
    void collideFlipper(Flipper& f, double dt);
    void loseBall();

    std::vector<Wall> m_walls;
    std::vector<Bumper> m_bumpers;
    Flipper m_left;
    Flipper m_right;

    QPointF m_ball;
    QPointF m_velocity;
    double m_ballRadius = 11.0;

    bool m_inLane = true;
    double m_plunger = 0.0;
    int m_ballsLeft = 3;
    int m_score = 0;
    bool m_gameOver = false;
    bool m_ballLost = false;
    int m_bumperHits = 0;
    int m_slingHits = 0;
};
