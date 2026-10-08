#pragma once

#include "cards/card.h"
#include "cards/cardflight.h"
#include "freecell/freecelltable.h"
#include "gameview.h"

#include <QPoint>
#include <QPointF>
#include <QRectF>

#include <array>
#include <vector>

class QTimer;

// FreeCell: every card face up from the start, four cells to park singles in,
// and almost every deal winnable.
class FreeCellView : public GameView
{
    Q_OBJECT

public:
    explicit FreeCellView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    double smallestCardWidth() const override { return cardWidth(); }
    void activate() override;
    // A game that owns a QTimer overrides this — structural, not observed
    // (GHUB-0046). The flight timer is one.
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

    // Whether a run is off the table, by mouse or by keyboard. Exists because
    // no picture and no save answers it: saveState() patches a lifted run back
    // onto its pile, so the save looks complete whether or not the table is.
    bool holdingARun() const { return m_dragging || m_keyHolding; }

    // The keyboard cursor, as {column, depth}: columns 0-7. Depth -1 is the
    // top row, which lines up with the columns -- the four cells over columns
    // 0-3, the four foundations over 4-7 -- and any other depth is a card's
    // index in that column. Exists for the reason KlondikeView::cursorSpot does.
    QPoint cursorSpot() const { return { m_cursorCol, m_cursorDepth }; }
    // Where the cursor is drawn, as KlondikeView::cursorRect is: paintEvent
    // draws this rect, so a test clicking inside it clicks what the player sees.
    QRectF cursorRect() const;

    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

    // The table's shape belongs to the rules; kept under the names the painter
    // and its tests already used.
    static constexpr int kColumns = FreeCellTable::kColumns;
    static constexpr int kCells = FreeCellTable::kCells;

    // The bottom edge of the deepest column, and the height it must stay
    // inside. Exists so a test can ask whether a BUILT column -- one longer
    // than the deal the budget was sized for -- still fits, which no rendered
    // picture answers: a card drawn under the opaque caption plate looks much
    // like one that is not there. Same reasoning, and same shape, as
    // SpiderView's pair.
    double deepestColumnBottom() const;
    double roomForColumns() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override { return { 900, 640 }; }
    QSize minimumSizeHint() const override { return { 620, 440 }; }

private:
    using PileKind = FreeCellTable::PileKind;

    struct Spot {
        PileKind kind = PileKind::Column;
        int pile = 0;
        int index = -1;
        bool valid = false;
    };

    // Lands any card in flight and puts back a run held by the drag or the
    // keyboard, so a change to the table underneath never strands either.
    void settleForChange();

    void buildActions();
    void newGame();
    void undo();

    // Layout, in card heights. cardWidth() solves its height budget from
    // these, so the three move together and a fan that changes shape cannot
    // leave the budget behind — which is how the deal ended up taller than
    // the space reserved for it (GHUB-0083, GHUB-0086).
    static constexpr double kHeaderGap = 0.22;
    static constexpr double kFanStep = 0.27;
    // Four columns of seven, four of six. All face up.
    static constexpr int kLongestDealtColumn = 7;

    double cardWidth() const;
    double cardHeight() const { return cardWidth() * 1.4; }
    QRectF pileOrigin(PileKind kind, int pile) const;
    QRectF cardRect(int column, int index) const;
    double fanStep() const { return cardHeight() * kFanStep; }
    // kLongestDealtColumn budgets the DEAL. Building descending runs in the
    // tableau is the game, and an eighth card ran under the caption plate or
    // off the bottom edge, where hitTest can never reach it. A column past its
    // budget fans tighter instead.
    double fanStep(int column) const;
    Spot hitTest(QPointF pos) const;

    const std::vector<Card>& pileFor(PileKind kind, int pile) const
    {
        return m_table.pile(kind, pile);
    }

    void checkWin();
    void refresh(const QString& message = {});

    // Drops the held run on `target`. One function for the drag and the
    // keyboard, so the two cannot disagree about a move; the table decides
    // whether it is legal. On a refusal for length, `refusal` says how many
    // cards would fit. The caller decides what a refusal means: the drag puts
    // the run back, the keyboard keeps it in hand.
    bool dropHeldOn(const Spot& target, QString* refusal);

    // Keyboard play (GHUB-0168), the boards' scheme. See KlondikeView.
    Spot cursorPile() const;
    // Puts the cursor on a clicked spot, so the mouse and the keyboard never
    // disagree about where you are.
    void moveCursorTo(const Spot& s);
    void clampCursor();
    void pressAtCursor();
    void dropAtCursor();
    QRectF heldLandingRect() const;

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;

    // The rules. What is left here is the pointer, the drag and the drawing.
    FreeCellTable m_table;

    // GHUB-0065. Cards on their way to a foundation. The destination is
    // captured when the card leaves, so anything moving the layout clears
    // these — see cardflight.h.
    std::vector<cardflight::Flight> m_flights;
    mutable std::vector<char> m_flightConsumed;
    QTimer* m_flightTimer = nullptr;
    void launchToFoundation(const Card& card, QRectF fromRect, int foundation);
    int grownFoundation(const std::array<std::size_t, 4>& before) const;

    std::vector<Card> m_drag;
    Spot m_dragFrom;
    QPointF m_dragPos;
    QPointF m_dragGrab;
    QPointF m_pressPos;
    bool m_dragging = false;
    bool m_pressValid = false;

    // A run lifted with Space. m_drag and m_dragFrom carry it exactly as a
    // drag does, so saveState patches either.
    bool m_keyHolding = false;
    // Opens on the top card of the first column; clamped to it on first use.
    int m_cursorCol = 0;
    int m_cursorDepth = 99;

    // Restoring a save clears the table's undo history, so canUndo() alone reads
    // a resumed deal as untouched and saveState() then returns {}, which the hub
    // treats as "delete the stored game". This says the deal is worth keeping.
    bool m_resumed = false;
    bool m_won = false;
};
