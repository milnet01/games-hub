#pragma once

#include "dealseed.h"
#include "gameview.h"
#include "runner/runnerart.h"
#include "runner/runnercourse.h"

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QPointF>

#include <random>
#include <vector>

class QPainter;
class FramePacer;

// Ruin Runner: run through the jungle ruins, jumping and ducking, for a high
// score. Draws RunnerCourse and feeds it input; every rule lives in the course.
class RunnerView : public GameView
{
    Q_OBJECT

public:
    explicit RunnerView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    // How to play, as the hub's Help window shows it (GHUB-0016).
    QString rulesText() const override;
    // No cards on this course.
    double smallestCardWidth() const override { return 0.0; }
    void activate() override;
    // A pause, not a stop: activate() picks the run back up, unless the
    // player had paused it themselves.
    void deactivate() override;
    // --shot: runs `turns` seconds with RunnerCourse::pilot() at the keys,
    // synchronously and silently, and leaves no effect in the air.
    bool advanceForShot(int turns) override;

    // Runs `seconds` with nobody at the keys, through the same path the
    // clock takes, so a test can reach the end of a run without waiting.
    void runUnattendedForTest(double seconds);

    // The saved table, best first, at most kTableSize long. The view from
    // behind (LaneView) shares it.
    static QList<int> highScores();
    // Enters a finished run's score in the table. Returns where it landed,
    // or -1 when it did not make the table.
    static int recordScore(int score);
    static constexpr int kTableSize = 5;

    const RunnerCourse& course() const { return m_course; }

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

    // Course units -> widget pixels.
    double scale() const;
    QRectF courseRect() const;

    // What the art is painted for at this size (GHUB-0262).
    RunnerArt::Frame artFrame() const;
    // Everything on the course but its words, in scene pixels. Run in bands
    // at once, so it reads members and writes nothing.
    void paintScene(QPainter& q, QPoint shake) const;
    void paintPits(QPainter& p) const;
    void paintCoins(QPainter& p) const;
    void paintRunner(QPainter& p) const;
    void paintAir(QPainter& p) const;
    void paintWarnings(QPainter& p) const;
    void paintEffects(QPainter& p) const;
    void paintPopups(QPainter& p) const;
    void paintHud(QPainter& p) const;
    void paintOverlay(QPainter& p) const;
    void paintPanel(QPainter& p, const QRectF& r) const;
    QFont courseFont(double units, bool bold = true) const;
    double outlineWidth() const;

    QList<QAction*> m_actions;
    QAction* m_pause = nullptr;
    // The run moves on by wall time, one frame at a time (GHUB-0258,
    // GHUB-0261).
    FramePacer* m_pacer = nullptr;
    // Set by advanceForShot: the run stays at the moment photographed.
    bool m_heldForShot = false;

    RunnerCourse m_course;

    bool m_active = false;
    bool m_duckKey = false;
    bool m_duckButton = false;

    // The game over has been recorded in the table.
    bool m_recorded = false;
    // Where this run's score landed in the table, or -1.
    int m_rank = -1;
    // highScores() as last read, so painting never reads the settings.
    QList<int> m_table;

    // Presentation only: none of this changes what happens in the run.
    struct Particle {
        QPointF at;       // course units
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
    std::vector<Particle> m_particles;
    std::vector<Popup> m_popups;
    // The scatter of particles. From dealSeed() like every other random
    // thing here, so --seed pins it too.
    std::mt19937 m_fx { dealSeed() };

    double m_time = 0.0;
    double m_shake = 0.0;
    double m_flash = 0.0;
    // Seconds since a banner went up, and what it says.
    double m_bannerAge = 10.0;
    QString m_banner;
    QString m_statusShown;

    // Seconds since the runner last landed, for the squash as it does.
    double m_landAge = 10.0;
    // The stride last kicked up dust.
    long long m_stride = 0;

    // The art for this size, and the picture the course is painted into,
    // no sharper than RunnerArt's cap however large the window.
    RunnerArt::Layers m_layers;
    QImage m_scene;
};
