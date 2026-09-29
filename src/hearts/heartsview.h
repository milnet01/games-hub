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
    TurnLight turnLight() const override { return m_turn.value(); }

    // The keyboard cursor: the place in your hand, 0 for the leftmost card.
    // It walks every card, dimmed ones included, so the hand can be read one
    // card at a time. Exposed for the tests, as KlondikeView::cursorSpot is.
    int cursorSpot() const { return m_cursor; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
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
    // `reason` is why the last press did nothing, shown until anything else
    // happens -- every change calls refresh(), and that is what clears it.
    void refresh(const QString& reason = {});

    // Keyboard (GHUB-0168). The one press for mouse and keyboard: picks or
    // unpicks a card while passing, plays it during play, and says why when
    // it cannot.
    void pressAtCursor();
    QString refusalFor(const Card& card) const;
    void announceHand();

    // Whose turn § 4.2 lights, and the area its light fills. The area is the
    // seat's own cards grown by half a card's width on every side and clipped
    // to the table, so the light spreads well past them without running off.
    int litSeat() const;
    QRectF turnArea(int seat) const;
    void stepTurnLight();

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
    int m_cursor = 0;
    QString m_reason;
    // GHUB-0063. m_turnFades is false until activate() has run, so a game
    // opened, restored or photographed shows its light at once instead of
    // fading it in. m_timer is the AI's clock and cannot carry the fade: it is
    // single-shot and runs at the computer's pace, not at a frame's.
    TurnLightState m_turn;
    bool m_turnFades = false;
    QTimer* m_turnTimer = nullptr;
};
