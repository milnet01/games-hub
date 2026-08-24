#pragma once

#include "canasta/canastaai.h"
#include "canasta/canastaengine.h"
#include "gameview.h"

#include <QFont>
#include <QPointF>
#include <QRectF>

#include <array>
#include <vector>

class QTimer;

// Canasta's table. Presentation and timing only — every rule lives in
// canasta::Engine, and every computer decision in canasta::Ai.
class CanastaView : public GameView
{
    Q_OBJECT

public:
    explicit CanastaView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    void activate() override;
    void deactivate() override;
    // Canasta caches nothing, but it does hold cards in the air whose
    // destinations were worked out from the old geometry, and the switch moves
    // the geometry — so this is a re-layout point rather than a repaint.
    void applyLegibility(bool enabled) override;
    // A game to 5000 is several sittings, so the whole table is kept and put
    // back. Nothing in the air is saved — a restored game shows the position as
    // it settles, which is where the cards were going anyway.
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

    // The width of the smallest card this game draws a FACE on — a melded one.
    // The legibility contract is about that number rather than about
    // cardWidth(): a game holds CardArt::kFaceMinWidth at the smallest scale it
    // actually draws at, not at 1.0
    // (docs/specs/GHUB-0017-legibility-switch.md § 4.4). Public because it is
    // what the UI test asserts; nothing in the game calls it.
    double smallestFaceWidth() const;
    // The same number, under the name every game answers to. Canasta's melds
    // are the smallest scale it draws a card at, which is exactly what the
    // base class asks for.
    double smallestCardWidth() const override { return smallestFaceWidth(); }
    // The one game in the collection that answers this true: applyLegibility
    // lands every card in flight, and a landed card cannot be un-landed.
    bool hasPendingAnimation() const override { return animating(); }

    // True when the table is big enough to lay this card out on its own, with
    // cardWidth()'s floor never having to lift it. Floor and minimum size move
    // together or not at all (§ 4.4): a floored card is one the table has no
    // room for, and it overflows rather than reading better. A test that
    // asserts only the width has therefore seen half the contract.
    bool cardsFitTable() const;

    // Where the table says why a move was refused, and an empty rect when it
    // has nothing to say. The refusals are the most useful sentences this game
    // produces, and until GHUB-0040 they were delivered only to the status bar
    // — which is the one place the player never looks. Public because it is
    // what the UI test asserts; the panel itself is drawn from it.
    QRectF messageRect() const;

    // Where the `index`th finished canasta of a stack of `count` is laid, when
    // the house rule stacks them on the red threes. The index is the order they
    // were COMPLETED in, and the orientation alternates with it -- across,
    // upright, across -- which is how the stack is counted at a glance.
    //
    // Takes the count rather than reading it, so a test can walk a stack of any
    // depth without playing a position into existence. Public for the same
    // reason messageRect() is: it is what the UI test asserts.
    QRectF canastaStackRect(int team, int index, int count) const;
    // The same canasta's WHOLE footprint: the card, the ring drawn round it and
    // the badge under it. That is what has to fit the band, and the card alone
    // does not -- both are drawn wider or taller than the card they belong to.
    QRectF canastaStackExtent(int team, int index, int count) const;
    // The strip of table a team's melds and canastas are laid out in. Public
    // for the same reason messageRect() is: it is what the UI test asserts the
    // stack against, and a stack that leaves the band lands on the centre row.
    QRectF bandFor(int team) const;
    // The ranks of `team`'s finished canastas, oldest first, and empty unless
    // the house rule is on. The ORDER is the view's own: a meld becomes a
    // canasta long after it was laid, so the engine's meld order is not it, and
    // sorting by that would make an earlier canasta stand up when a later one
    // completed.
    std::vector<int> canastaOrder(int team) const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    QSize sizeHint() const override { return { 1000, 740 }; }
    QSize minimumSizeHint() const override;

private:
    // Where a card in the air is heading. Arrivals are suppressed at their
    // destination until they land, so a travelling card is never drawn twice.
    enum class Dest { Nowhere, Hand, Pile, Meld };

    struct Flight {
        Card card;
        QPointF from;
        QPointF to;
        double fromAngle = 0.0;
        double toAngle = 0.0;
        double progress = 0.0; // 0 at `from`, 1 at `to`
        double delay = 0.0;    // seconds still to wait before moving
        double speed = 4.5;    // progress per second
        bool faceUp = true;
        bool flips = false;    // turns face up on the way
        Dest dest = Dest::Nowhere;
        int destSeat = -1;
        int destTeam = -1;
        int destRank = 0;
    };

    void buildActions();
    void newGame();
    // Puts the toolbar's rule set onto the game in progress, without dealing.
    void applyRules();
    // Sets each seat's strength, which is not simply the chosen level: the
    // partner can be sharpened on its own.
    void applyLevels();
    void tick();

    // --- geometry ---
    QRectF tableRect() const;
    double naturalCardWidth() const;
    double cardWidth() const;
    double cardHeight() const;
    QPointF stockCentre() const;
    QPointF pileCentre() const;
    QPointF handCentre(int index, int count, bool lifted) const;
    double handAngle(int index, int count) const;
    QPointF opponentCentre(int seat, int index, int count) const;
    // Where a card laid on rank `rank` belongs, whether that rank is still a
    // meld in the row or a canasta that has left it for the stack. One answer,
    // so the animation and the table can never point at different places.
    QPointF meldCardTarget(int team, int rank, int index) const;
    // Keeps m_canastaOrder in step with the melds. Called wherever they can
    // change: refresh, a new game, a restored one.
    void trackCanastas();
    // How far the meld row may run before the canasta stack begins. The stack
    // grows leftward out of the red threes into the room the canastas leaving
    // the row just freed, so the two share one budget rather than overlapping.
    double meldRowWidth(int team) const;
    // Where the card that froze the pile sits, given the point in the stack it
    // belongs at. Classic lays it squarely across; the house rule slides it
    // along its own length into a T.
    QPointF freezeCardCentre(const QPointF& seat) const;
    double stackRingPad() const;
    double opponentAngle(int seat, int index, int count) const;
    // Melds are laid out in fixed slots so a meld does not jump sideways when
    // another one appears next to it. Under the stacking house rule the row
    // re-spaces once per canasta, when the finished one leaves it for the
    // stack -- see meldCardCentre.
    std::vector<int> meldOrder(int team) const;
    QPointF meldCardCentre(int team, int slot, int index) const;
    QPointF seatAnchor(int seat) const;
    // The Lay down button, on the felt between your melds and your hand. Empty
    // when there is nothing to lay down, which is also when it is not drawn.
    QRectF layDownButton() const;

    // --- hit testing ---
    static bool hits(const QPointF& pos, const QPointF& centre, double w, double h, double angle);
    int handIndexAt(const QPointF& pos) const;
    int meldRankAt(const QPointF& pos) const;

    // --- selection ---
    bool isSelected(int index) const;
    std::vector<Card> selectedCards() const;
    void clearSelection();
    // Fans your hand back into order, carrying the selection across. Does
    // nothing unless the Sort toggle is on.
    void sortHand();

    // --- moves ---
    void humanDraw();
    void humanTakePile();
    void humanMeld(int targetRank);
    void humanDiscard();
    void aiHalfTurn();
    void refresh();
    void announce(const QString& text);

    // --- animation ---
    bool animating() const;
    void addFlight(Flight f);
    // Flights for every card that has just arrived. Both walk the destination
    // as it now stands and match against what was gained, so several cards
    // landing at once each get their own slot rather than stacking on one.
    void flyHandArrivals(int seat, std::vector<Card> gained, const QPointF& from, bool faceUp,
                         double& delay, double stagger = 0.035);
    void flyMeldArrivals(int team, std::vector<std::pair<int, Card>> gains, const QPointF& from,
                         double& delay);
    void flyToPile(const Card& c, const QPointF& from);
    void flyTheDeal();
    // Consumes one suppression slot if a flight is still carrying this card to
    // this destination; returns true when the caller should skip drawing it.
    bool suppressed(Dest dest, int seatOrTeam, int rank, const Card& c) const;

    // --- painting ---
    void paintTable(QPainter& p);
    void paintMelds(QPainter& p);
    void paintCanastaStack(QPainter& p, int team);
    // The plate that names a meld, shared by the row and the stack. `topCentre`
    // is where its top edge is centred.
    void paintMeldBadge(QPainter& p, const canasta::Meld& m, const QPointF& topCentre,
                        bool canasta);
    void paintOpponents(QPainter& p);
    void paintCentre(QPainter& p);
    void paintCentreStrip(QPainter& p);
    void paintHand(QPainter& p);
    void paintLayDown(QPainter& p);
    void paintMessagePanel(QPainter& p);
    QFont messageFont() const;
    void paintDrag(QPainter& p);
    void paintFlights(QPainter& p);
    void paintScores(QPainter& p);
    void paintSummary(QPainter& p);
    void paintCard(QPainter& p, const Card& c, const QPointF& centre, double angle, bool faceUp,
                   double scale = 1.0) const;

    QList<QAction*> m_actions;
    QAction* m_meldAction = nullptr;
    QAction* m_discardAction = nullptr;
    QAction* m_rulesAction = nullptr;

    canasta::Engine m_engine;
    std::array<canasta::Ai, canasta::kSeats> m_ai;
    canasta::Level m_level = canasta::Level::Medium;
    // The house set the owner's family plays by, kept alongside the classic one
    // rather than replacing it.
    canasta::Rules m_house;
    bool m_useHouse = false;
    // How long a game runs. Held apart from the two rule sets because it is a
    // choice about this evening rather than about how Canasta works.
    int m_target = 5000;

    std::vector<int> m_selected; // indices into the human's hand
    int m_hover = -1;
    int m_hoverMeld = -1;
    // A press is only a click once the mouse comes up without having moved;
    // past a few pixels it is a drag, and the picked cards follow the cursor.
    int m_pressIndex = -1;
    QPointF m_pressPos;
    QPointF m_dragPos;
    bool m_dragging = false;
    bool m_overButton = false;
    bool m_showHints = true;
    bool m_sortHand = true;
    // North plays Expert whatever the opponents are set to.
    bool m_sharpPartner = false;

    // Each team's finished canastas in the order they were completed, which is
    // what decides whether one lies across or upright. Kept here rather than in
    // the engine because it is a fact about the picture: nothing about play
    // changes with it. Rebuilt from the melds on restore -- a resumed game
    // shows the position as it settles, which is where the cards were anyway.
    std::array<std::vector<int>, canasta::kTeams> m_canastaOrder;

    std::vector<Flight> m_flights;
    // Cleared and refilled every paint, so one flight suppresses exactly one
    // card wherever it is heading.
    mutable std::vector<int> m_consumed;

    // The last card thrown and who threw it, kept on screen until the next one
    // replaces it. Three computer seats play faster than a card can be read.
    Card m_lastThrown;
    int m_lastThrownBy = -1;

    // The window size from before the legibility switch clamped it larger, so
    // turning the switch off puts the window back. Invalid while the switch is
    // off. See CanastaView::applyLegibility.
    QSize m_normalWindowSize;

    QTimer* m_timer = nullptr;
    double m_pause = 0.0;    // seconds to wait before the next computer move
    double m_celebrate = 0.0; // countdown on the canasta flourish
    int m_canastasShown = 0;
    QString m_message;
    bool m_awaitingContinue = false;
};
