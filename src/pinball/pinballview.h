#pragma once

#include "gameview.h"
#include "pinballtable.h"

#include <QPointF>

#include <vector>

class QPainter;
class QTimer;

// Draws PinballTable and feeds it input. All the physics lives in the table.
class PinballView : public GameView
{
    Q_OBJECT

public:
    explicit PinballView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    void activate() override;
    // The base class asks every game with a clock or an animation to stop it
    // here, and Pinball had no override at all: leaving the table for another
    // game left the ball rolling, and it drained while nobody was watching.
    void deactivate() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 460, 760 }; }
    QSize minimumSizeHint() const override { return { 300, 460 }; }

private:
    void buildActions();
    void newGame();
    void tick();
    void refresh();
    void announceGameOver();

    void paintFlipper(QPainter& p, const PinballTable::Flipper& f) const;
    void paintPlayfieldArt(QPainter& p) const;
    void paintLamps(QPainter& p) const;
    void buildLamps();

    // Painted light inserts. They are decoration only — the table's physics
    // knows nothing about them — so they live here rather than in the model.
    struct Lamp {
        QPointF at;      // table units
        QColor colour;
        int group = 0;   // lamps in a group light together in the chase
        double radius = 7.0;
    };

    // Table units -> widget pixels.
    double scale() const;
    QPointF toScreen(QPointF p) const;
    // The strip across the top of the cabinet. Reserved out of the height
    // before the table is scaled, because it is opaque and used to be painted
    // OVER the playfield -- and the legibility switch makes it taller, so the
    // switch that exists to help hid more of the board rather than less.
    // Derived from the widget rather than from scale(), which it constrains.
    double glassHeight() const;

    QList<QAction*> m_actions;
    QTimer* m_timer = nullptr;
    PinballTable m_table;
    std::vector<Lamp> m_lamps;
    double m_animSeconds = 0.0;
    // Rises to 1 whenever the score moves and decays away, driving the
    // everything-flashes moment after a hit.
    double m_celebrate = 0.0;
    int m_lastScore = 0;
    bool m_plungerHeld = false;
    bool m_announced = false;
};
