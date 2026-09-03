#pragma once

#include "cards/card.h"
#include "cards/cardflight.h"
#include "gameview.h"
#include "klondike/klondiketable.h"

#include <QPointF>
#include <QRectF>

#include <array>
#include <vector>

class QTimer;

class KlondikeView : public GameView
{
    Q_OBJECT

public:
    explicit KlondikeView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    double smallestCardWidth() const override { return cardWidth(); }
    void activate() override;
    // A game that owns a QTimer overrides this. The flight timer is one
    // (GHUB-0046), and the rule is structural rather than observed: no test
    // can see a board that happens to be still when the hub leaves it.
    void deactivate() override;
    // The caption band comes off the height these views solve their card size
    // from, so the switch moves every rect on the surface. A flight carries a
    // destination captured when the card left (cardflight.h), so it has to be
    // landed rather than left pointing at an address that has moved.
    void applyLegibility(bool enabled) override;

    // Exists so a test can ask what no rendered picture can answer: whether a
    // card is actually in the air, rather than whether two frames differ. Same
    // reasoning as SudokuView::marksFitAt — a check that cannot reach the state
    // it is about ends up asserting something weaker and calling it coverage.
    int flightsInTheAir() const { return int(m_flights.size()); }

    // Whether a run is still off the table. Exists because no picture and no
    // save answers it: saveState() patches a lifted run back onto its pile, so
    // the save looks complete whether or not the table is. See
    // settleForChange().
    bool holdingARun() const { return m_dragging || m_table.holding(); }

    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 820, 620 }; }
    QSize minimumSizeHint() const override { return { 560, 420 }; }

private:
    // Lands any card in flight and returns a run held in mid-drag, so a change
    // to the table underneath never strands either. See the definition.
    void settleForChange();

    using PileKind = KlondikeTable::PileKind;

    struct Spot {
        PileKind kind = PileKind::Stock;
        int pile = 0;   // which foundation / tableau column
        int index = -1; // index of the card within that pile
        bool valid = false;
    };

    void buildActions();
    void newGame();
    void undo();

    // Geometry
    //
    // Layout ratios. cardWidth() solves its height budget from these, so the
    // fan and the budget move together and the deal cannot outgrow the space
    // reserved for it (GHUB-0083, GHUB-0086).
    static constexpr double kGapRatio = 0.14;      // of card width
    static constexpr double kHeaderGap = 1.6;      // of gap()
    static constexpr double kFaceUpStep = 0.28;    // of card height
    static constexpr double kFaceDownStep = 0.13;  // of card height
    // The seventh column: six face down and one turned up.
    static constexpr int kLongestDealtColumn = 7;

    double cardWidth() const;
    double cardHeight() const { return cardWidth() * 1.4; }
    double gap() const { return cardWidth() * kGapRatio; }
    QRectF pileOrigin(PileKind kind, int pile) const;
    QRectF cardRect(PileKind kind, int pile, int index) const;
    double fanStep(const std::vector<Card>& pile, int index) const;
    Spot hitTest(QPointF pos) const;

    const std::vector<Card>& pileFor(PileKind kind, int pile) const
    {
        return m_table.pile(kind, pile);
    }

    void dealFromStock();
    void checkWin();
    void refresh();

    // GHUB-0065. Sends `card` on its way from `fromRect` to foundation
    // `foundation`, which the caller has already moved it to in the model. The
    // destination is captured here, when the card leaves, so anything that
    // moves the layout must clear m_flights -- see cardflight.h.
    void launchToFoundation(const Card& card, QRectF fromRect, int foundation);
    // The foundation whose pile grew, given the sizes before the move. A
    // successful send does not say which one took the card.
    int grownFoundation(const std::array<std::size_t, 4>& before) const;

    std::vector<cardflight::Flight> m_flights;
    // Per-repaint scratch for cardflight::suppressAt. Cleared at the top of
    // paintEvent, never read outside it.
    mutable std::vector<char> m_flightConsumed;
    QTimer* m_flightTimer = nullptr;

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;

    // The rules. What is left here is the pointer, the drag and the drawing.
    KlondikeTable m_table;

    // Cards currently under the cursor, lifted off their pile while dragging.
    // The table holds the authoritative copy and knows where they came from;
    // this is what the view draws.
    std::vector<Card> m_drag;
    Spot m_dragFrom;
    QPointF m_dragPos;
    QPointF m_dragGrab;
    bool m_dragging = false;
    bool m_pressValid = false;
    QPointF m_pressPos;

    // Restoring a save clears the table's undo history, so canUndo() alone reads
    // a resumed deal as untouched and saveState() then returns {}, which the hub
    // treats as "delete the stored game". This says the deal is worth keeping.
    bool m_resumed = false;
    bool m_won = false;
};
