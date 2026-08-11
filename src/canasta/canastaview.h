#pragma once

#include "canasta/canastaai.h"
#include "canasta/canastaengine.h"
#include "gameview.h"

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
    // A game to 5000 is several sittings, so the whole table is kept and put
    // back. Nothing in the air is saved — a restored game shows the position as
    // it settles, which is where the cards were going anyway.
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    QSize sizeHint() const override { return { 1000, 740 }; }
    QSize minimumSizeHint() const override { return { 720, 560 }; }

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
    void tick();

    // --- geometry ---
    QRectF tableRect() const;
    double cardWidth() const;
    double cardHeight() const;
    QPointF stockCentre() const;
    QPointF pileCentre() const;
    QRectF bandFor(int team) const;
    QPointF handCentre(int index, int count, bool lifted) const;
    double handAngle(int index, int count) const;
    QPointF opponentCentre(int seat, int index, int count) const;
    double opponentAngle(int seat, int index, int count) const;
    // Melds are laid out in fixed slots so a meld does not jump sideways when
    // another one appears next to it.
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
    void paintOpponents(QPainter& p);
    void paintCentre(QPainter& p);
    void paintCentreStrip(QPainter& p);
    void paintHand(QPainter& p);
    void paintLayDown(QPainter& p);
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

    std::vector<Flight> m_flights;
    // Cleared and refilled every paint, so one flight suppresses exactly one
    // card wherever it is heading.
    mutable std::vector<int> m_consumed;

    // The last card thrown and who threw it, kept on screen until the next one
    // replaces it. Three computer seats play faster than a card can be read.
    Card m_lastThrown;
    int m_lastThrownBy = -1;

    QTimer* m_timer = nullptr;
    double m_pause = 0.0;    // seconds to wait before the next computer move
    double m_celebrate = 0.0; // countdown on the canasta flourish
    int m_canastasShown = 0;
    QString m_message;
    bool m_awaitingContinue = false;
};
