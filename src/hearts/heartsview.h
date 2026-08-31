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
    // The TRICK is the smallest face this game draws, at 0.9 of a hand card --
    // reporting the hand card overstated the floor by that factor.
    double smallestCardWidth() const override { return cardWidth() * 0.9; }
    // Names the suit that was led. That sentence exists nowhere else in this
    // game: the led card is one of up to four in a heap in the middle, and
    // which suit it is decides every legal play you have.
    QString captionText() const override;
    void activate() override;

    // The longest game in the hub after Canasta, so it is worth coming back to
    // (GHUB-0007). The engine writes the position; this adds what the VIEW
    // holds that the rules do not -- the cards lifted for a pass that has not
    // been confirmed, and whether a finished trick is still on the table.
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;
    // The computers stop playing when nobody is watching. Without it a hand
    // finishes while you are in another game and you come back to a score.
    void deactivate() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 880, 660 }; }
    QSize minimumSizeHint() const override { return { 620, 480 }; }

    // The strip the caption may use: below the trick, above the hand, and clear
    // of a card lifted for the pass. Reachable from a test with the two rects it
    // has to stay clear of, because a test that mirrored the trick's geometry
    // instead would go stale on exactly the change that reintroduces GHUB-0084.
    QRectF captionArea() const;
    QRectF handCardRect(int index) const;
    QRectF trickCardRect(int seat) const;

    // The room a seat's fanned stack of backs takes, at its fullest. Reachable
    // from a test because a stack that does not fit is invisible to every other
    // check here: it paints happily, off the edge of the window.
    QRectF opponentStackRect(int seat) const;

private:
    void buildActions();
    void newGame();
    void confirmPass();
    // Deal the next hand. Reached from the hand-over box's accept button and
    // from the toolbar, so declining the box no longer ends the match.
    void startNextHand();
    // Runs the computer seats until it is the human's turn again, or the trick
    // is full. Driven by a timer so the play is watchable.
    void step();
    void finishTrick();
    void refresh();
    void announceHand();

    double cardWidth() const;
    double cardHeight() const { return cardWidth() * 1.4; }
    QRectF opponentRect(int seat) const;

    QList<QAction*> m_actions;
    QAction* m_passAction = nullptr;
    QAction* m_nextHandAction = nullptr;

    HeartsEngine m_engine;
    std::vector<Card> m_selected; // cards chosen for passing
    QTimer* m_timer = nullptr;
    bool m_awaitingCollect = false;
    bool m_announced = false;
    // The hand-over box came due while the hub was on another page; activate()
    // raises it when we are back.
    bool m_announcePending = false;
};
