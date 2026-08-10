#pragma once

#include "gameview.h"
#include "pinballtable.h"

#include <QPointF>

class QTimer;

// Draws PinballTable and feeds it input. All the physics lives in the table.
class PinballView : public GameView
{
    Q_OBJECT

public:
    explicit PinballView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    void activate() override;

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

    // Table units -> widget pixels.
    double scale() const;
    QPointF toScreen(QPointF p) const;

    QList<QAction*> m_actions;
    QTimer* m_timer = nullptr;
    PinballTable m_table;
    bool m_plungerHeld = false;
    bool m_announced = false;
};
