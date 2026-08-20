#pragma once

#include "gameview.h"
#include "heartsengine.h"

#include <QRectF>

#include <vector>

class QTimer;

class HeartsView : public GameView
{
    Q_OBJECT

public:
    explicit HeartsView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    double smallestCardWidth() const override { return cardWidth(); }
    // Names the suit that was led. That sentence exists nowhere else in this
    // game: the led card is one of up to four in a heap in the middle, and
    // which suit it is decides every legal play you have.
    QString captionText() const override;
    void activate() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 880, 660 }; }
    QSize minimumSizeHint() const override { return { 620, 480 }; }

private:
    void buildActions();
    void newGame();
    void confirmPass();
    // Runs the computer seats until it is the human's turn again, or the trick
    // is full. Driven by a timer so the play is watchable.
    void step();
    void finishTrick();
    void refresh();
    void announceHand();

    double cardWidth() const;
    double cardHeight() const { return cardWidth() * 1.4; }
    QRectF handCardRect(int index) const;
    QRectF trickCardRect(int seat) const;
    QRectF opponentRect(int seat) const;

    QList<QAction*> m_actions;
    QAction* m_passAction = nullptr;

    HeartsEngine m_engine;
    std::vector<Card> m_selected; // cards chosen for passing
    QTimer* m_timer = nullptr;
    bool m_awaitingCollect = false;
    bool m_announced = false;
};
