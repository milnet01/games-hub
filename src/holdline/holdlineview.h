#pragma once

#include "dealseed.h"
#include "gameview.h"
#include "holdline/holdlinefield.h"

#include <QColor>
#include <QPixmap>
#include <QPointF>

#include <random>
#include <vector>

class QPainter;
class QTimer;

// Hold the Line: shoot the marching wave down before it lands. Draws
// HoldLineField and feeds it input; every rule lives in the field.
class HoldLineView : public GameView
{
    Q_OBJECT

public:
    explicit HoldLineView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    // How to play, as the hub's Help window shows it (GHUB-0016).
    QString rulesText() const override;
    // No cards on this field.
    double smallestCardWidth() const override { return 0.0; }
    void activate() override;
    // A pause, not a stop: activate() picks the game back up, unless the
    // player had paused it themselves.
    void deactivate() override;
    // --shot: plays `turns` seconds with the cannon flown by a simple pilot,
    // synchronously and silently, and leaves no explosion in the air.
    bool advanceForShot(int turns) override;

    // The saved table, best first, at most kTableSize long.
    static QList<int> highScores();
    static constexpr int kTableSize = 5;

    const HoldLineField& field() const { return m_field; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    QSize sizeHint() const override { return { 600, 700 }; }
    QSize minimumSizeHint() const override { return { 360, 440 }; }

private:
    void buildActions();
    void newGame();
    void tick();
    void runClock();
    void refresh();
    void gameOver();
    void readEvents();
    void burst(QPointF at, const QColor& colour, int count, double speed);

    // Field units -> widget pixels.
    double scale() const;
    QRectF fieldRect() const;

    void buildBackdrop();
    void paintStars(QPainter& p) const;
    void paintShelters(QPainter& p) const;
    void paintAttacker(QPainter& p, const HoldLineField::Attacker& a) const;
    // `live` is the cannon in play, which flashes as it fires; a life icon is not.
    void paintCannon(QPainter& p, double x, bool live) const;
    void paintShotsAndBombs(QPainter& p) const;
    void paintEffects(QPainter& p) const;
    void paintHud(QPainter& p) const;
    void paintOverlay(QPainter& p) const;
    void paintPanel(QPainter& p, const QRectF& r) const;
    QFont fieldFont(double units, bool bold = true) const;
    double outlineWidth() const;

    QList<QAction*> m_actions;
    QAction* m_pause = nullptr;
    QTimer* m_timer = nullptr;

    HoldLineField m_field;

    bool m_active = false;
    bool m_leftHeld = false;
    bool m_rightHeld = false;
    bool m_fireHeld = false;

    // The game over has been recorded in the table.
    bool m_recorded = false;
    // Where this game's score landed in the table, or -1.
    int m_rank = -1;
    // highScores() as last read, so painting never reads the settings.
    QList<int> m_table;

    // Presentation only: none of this changes what happens in the game.
    struct Particle {
        QPointF at;       // field units
        QPointF velocity; // units a second
        double life = 0.0;
        double maxLife = 1.0;
        double size = 2.0;
        QColor colour;
    };
    struct Ring {
        QPointF at;
        double age = 0.0;
        double maxAge = 0.5;
        double radius = 30.0;
        QColor colour;
    };
    struct Popup {
        QPointF at;
        QString text;
        double age = 0.0;
        QColor colour;
    };
    struct Star {
        QPointF at;      // 0..1 across the widget
        double depth;    // 0 far .. 1 near
        double phase;
    };
    std::vector<Particle> m_particles;
    std::vector<Ring> m_rings;
    std::vector<Popup> m_popups;
    std::vector<Star> m_stars;
    // The scatter of particles and stars. From dealSeed() like every other
    // random thing here, so --seed pins it too.
    std::mt19937 m_fx { dealSeed() };

    double m_time = 0.0;
    double m_shake = 0.0;
    double m_flash = 0.0;
    double m_muzzle = 0.0;
    // Seconds since a wave was cleared or started, for its banner.
    double m_bannerAge = 10.0;
    QString m_banner;
    QString m_statusShown;

    QPixmap m_backdrop;
};
