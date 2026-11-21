#pragma once

#include "dealseed.h"
#include "gameview.h"
#include "runner/lanecourse.h"

#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QPointF>

#include <array>
#include <random>
#include <vector>

class QPainter;
class FramePacer;

// Ruin Runner Lanes (GHUB-0241): Ruin Runner seen from behind, three lanes
// into the screen. Draws LaneCourse in a faked perspective and feeds it
// input; every rule lives in the course. Shares Ruin Runner's table of best
// scores. The painted art is in laneart.*.
class LaneView : public GameView
{
    Q_OBJECT

public:
    explicit LaneView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    QString rulesText() const override;
    double smallestCardWidth() const override { return 0.0; }
    void activate() override;
    // A pause, not a stop: activate() picks the run back up, unless the
    // player had paused it themselves.
    void deactivate() override;
    // --shot: runs `turns` seconds with LaneCourse::pilot() at the keys,
    // synchronously and silently, and leaves no effect in the air.
    bool advanceForShot(int turns) override;

    // Runs `seconds` with nobody at the keys, through the same path the
    // clock takes, so a test can reach the end of a run without waiting.
    void runUnattendedForTest(double seconds);

    const LaneCourse& course() const { return m_course; }

    // What a lane sign says about the next row: nothing in the lane, jump,
    // slide, or go round.
    enum class Sign { None, Jump, Slide, GoRound };
    // The signs for the next row, left lane first; all None when no row is
    // near enough to sign.
    std::array<Sign, LaneCourse::kLanes> signs() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    QSize sizeHint() const override { return { 900, 560 }; }
    QSize minimumSizeHint() const override { return { 480, 320 }; }

private:
    void buildActions();
    void newGame();
    void tick(double dt);
    void runClock();
    void refresh();
    void gameOver();
    void readEvents();
    void releaseKeys();
    void burst(QPointF at, const QColor& colour, int count, double speed, double lift);
    void scatterLeaves();

    // The canvas: a fixed 640 x 360 drawing, scaled to the widget.
    double scale() const;
    QRectF canvasRect() const;
    // A point on the course, in metres (across, up, ahead of the runner),
    // to the canvas. The camera sits behind and above the runner.
    QPointF project(double across, double up, double ahead) const;
    // Canvas units a metre is drawn at, that far ahead.
    static double sizeAt(double ahead);
    // The middle of a lane, in metres across.
    static double laneX(double lane);
    // How much of a thing that far off shows through the haze: 0 to 1.
    static double clearness(double ahead);

    void buildLayers();
    void paintGround(QPainter& p) const;
    void paintPit(QPainter& p, const LaneCourse::Obstacle& o) const;
    void paintWorld(QPainter& p) const;
    void paintObstacle(QPainter& p, const LaneCourse::Obstacle& o) const;
    void paintCoin(QPainter& p, const LaneCourse::Coin& c) const;
    void paintRunner(QPainter& p) const;
    void paintLeaves(QPainter& p) const;
    // Motes drifting in the low sun (GHUB-0262).
    void paintAir(QPainter& p) const;
    void paintEffects(QPainter& p) const;
    void paintSigns(QPainter& p) const;
    static void paintSign(QPainter& p, QPointF centre, double radius, Sign sign, double outline);
    void paintHud(QPainter& p) const;
    void paintOverlay(QPainter& p) const;
    void paintPanel(QPainter& p, const QRectF& r) const;
    QFont canvasFont(double units, bool bold = true) const;
    double outlineWidth() const;

    QList<QAction*> m_actions;
    QAction* m_pause = nullptr;
    // The run moves on by wall time, one frame at a time (GHUB-0258,
    // GHUB-0261).
    FramePacer* m_pacer = nullptr;
    // Set by advanceForShot: the run stays at the moment photographed.
    bool m_heldForShot = false;

    LaneCourse m_course;

    bool m_active = false;
    bool m_slideKey = false;
    bool m_slideButton = false;

    bool m_recorded = false;
    int m_rank = -1;
    QList<int> m_table;

    // Presentation only: none of this changes what happens in the run.
    struct Particle {
        QPointF at;       // canvas units
        QPointF velocity; // units a second
        double life = 0.0;
        double maxLife = 1.0;
        double size = 2.0;
        QColor colour;
    };
    struct Popup {
        QPointF at;
        QString text;
        double age = 0.0;
        QColor colour;
    };
    // A leaf drifting down through the picture.
    struct Leaf {
        QPointF at;
        double angle = 0.0;
        double spin = 0.0;
        double fall = 0.0;
        double sway = 0.0;
        QColor colour;
    };
    std::vector<Particle> m_particles;
    std::vector<Popup> m_popups;
    std::vector<Leaf> m_leaves;
    std::mt19937 m_fx { dealSeed() };

    double m_time = 0.0;
    double m_shake = 0.0;
    double m_flash = 0.0;
    // The camera follows the runner across, part of the way, smoothly.
    double m_cameraX = 0.0;
    // Where the runner's stride was last frame, to raise dust as a foot lands.
    double m_stride = 0.0;
    double m_bannerAge = 10.0;
    // Seconds since the runner last landed, for the squash as it does.
    double m_landAge = 10.0;
    QString m_banner;
    QString m_statusShown;

    // The play is painted into m_scene, at m_scenePixels to a canvas unit,
    // then stretched over the canvas: capped, so a 4K window costs no more
    // to paint than a 1080p one (GHUB-0258).
    QImage m_scene;
    double m_scenePixels = 1.0;
    // Painted once per size: the sky the whole window shows beyond the
    // canvas, and the scene's own sky behind and light and leaves over.
    QPixmap m_backdrop;
    QImage m_sceneSky;
    QImage m_sceneOverlay;
};
