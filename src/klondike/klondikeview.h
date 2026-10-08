#pragma once

#include "cards/card.h"
#include "cards/cardflight.h"
#include "gameview.h"
#include "klondike/klondiketable.h"

#include <QPoint>
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
    // A card in the air is what settle() in the UI test waits on (GHUB-0205).
    bool hasPendingAnimation() const override { return !m_flights.empty(); }

    // Whether a run is still off the table. Exists because no picture and no
    // save answers it: saveState() patches a lifted run back onto its pile, so
    // the save looks complete whether or not the table is. See
    // settleForChange().
    bool holdingARun() const { return m_dragging || m_table.holding(); }

    // The keyboard cursor, as {column, depth}: column 0-6 across the table,
    // depth -1 for the top row (stock, waste, foundations) or a card's index in
    // that tableau column. Exists for the same reason ReversiView's cursorCell
    // does: a clamped index is a property of this code, a gold band is the
    // theme's.
    QPoint cursorSpot() const { return { m_cursorCol, m_cursorDepth }; }
    // Where the cursor is drawn: the card under it and everything below, or a
    // held run over the pile it would land on. paintEvent draws this rect, so
    // a test that clicks inside it clicks what the player sees the cursor on.
    QRectF cursorRect() const;

    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
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
    // A scale applied to every step in one column when its natural fan would
    // run past the bottom. 1.0 whenever it fits, which is the deal and most of
    // play. See the comment on the definition.
    double fanScale(const std::vector<Card>& column) const;
    // The seventh column: six face down and one turned up.
    static constexpr int kLongestDealtColumn = 7;

    double cardWidth() const;

public:
    // The bottom edge of the deepest column, and the height it must stay
    // inside. Exists so a test can ask whether a column BUILT past the dealt
    // length still fits, which no rendered picture answers -- a card drawn
    // under the opaque caption plate looks much like one that is not there.
    // Same pair, same reasoning, as SpiderView's and FreeCellView's.
    double deepestColumnBottom() const;
    double roomForColumns() const;

private:
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

    // Keyboard play (GHUB-0168). The top row has no pile in column 2, so the
    // cursor steps over it there. The pile the cursor stands on, as a Spot;
    // `index` is the top card, or the card at the cursor's depth in a column.
    Spot cursorPile() const;
    // Puts the cursor on a clicked spot, so the mouse and the keyboard never
    // disagree about where you are.
    void moveCursorTo(const Spot& s);
    // Keeps the depth on a face-up card of the current column (the only cards
    // Space can lift), or on the top card, after anything moves the table.
    void clampCursor();
    // Space: deal, lift, or drop what the keyboard is holding onto the pile
    // under the cursor. A drop back onto the pile the run came from puts it
    // back, which is how the mouse says "no" too. A click that is not a drag
    // makes the same press (GHUB-0069): it lifts, and the next click drops.
    void pressAtCursor();
    void dropAtCursor();
    // Where a run held by the keyboard is drawn: over the pile under the
    // cursor, where it would land, and raised off it.
    QRectF heldLandingRect() const;

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

    // A run lifted with Space rather than the mouse. The table holds it, as it
    // does mid-drag; m_drag and m_dragFrom carry the view's copy and its
    // origin exactly as a drag does, so saveState patches either back.
    bool m_keyHolding = false;
    // Opens on the stock, because dealing is the first thing a deal asks for.
    int m_cursorCol = 0;
    int m_cursorDepth = -1;

    // Restoring a save clears the table's undo history, so canUndo() alone reads
    // a resumed deal as untouched and saveState() then returns {}, which the hub
    // treats as "delete the stored game". This says the deal is worth keeping.
    bool m_resumed = false;
    bool m_won = false;
};
