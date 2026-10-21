#pragma once

#include "cards/card.h"
#include "cards/cardflight.h"
#include "gameview.h"
#include "pyramid/pyramidtable.h"

#include <QPoint>
#include <QPointF>
#include <QRectF>

#include <optional>
#include <vector>

class QTimer;

// Pyramid: clear a stack of 28 cards by taking pairs that add up to 13.
// Kings are worth 13 on their own and go alone.
class PyramidView : public GameView
{
    Q_OBJECT

public:
    explicit PyramidView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    double smallestCardWidth() const override { return cardWidth(); }
    void activate() override;
    // The flight timer is a QTimer, so leaving the game stops it (GHUB-0046).
    void deactivate() override;
    // A flight carries a destination captured when the card left, and the
    // caption band moves every rect on the surface, so the switch clears them.
    void applyLegibility(bool enabled) override;
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

    // The pyramid's shape belongs to the rules; kept under the name the
    // painter already used.
    static constexpr int kRows = PyramidTable::kRows;

    // The keyboard cursor: x is the place along a row, y the row. Rows 0-6
    // are the pyramid, where the cursor only stands on a card that can be
    // taken. Row 7 is the stock (x 0) and the waste (x 1). For the tests, as
    // KlondikeView::cursorSpot is.
    QPoint cursorSpot() const { return { m_cursorIndex, m_cursorRow }; }
    // A card picked as the first of a pair, by the mouse or the keyboard:
    // Pyramid has one selection, not a hand. For the tests.
    bool holdingACard() const { return m_hasSelection; }
    // Whether a taken card is still on its way off the table, which no
    // rendered picture can answer -- see KlondikeView::flightsInTheAir.
    int flightsInTheAir() const { return int(m_flights.size()); }
    // A card in the air is what settle() in the UI test waits on (GHUB-0205).
    bool hasPendingAnimation() const override { return !m_flights.empty(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override { return { 860, 640 }; }
    QSize minimumSizeHint() const override { return { 600, 460 }; }

private:
    // Where a selectable card lives.
    using Source = PyramidTable::Source;
    using Slot = PyramidTable::Slot;

    void buildActions();
    // A deal the player asked for: the table, then the cards flying to it
    // (GHUB-0065). The deal made when the view is built is dealTable() alone,
    // because nobody asked for it and the view has no size yet.
    void newGame();
    void dealTable();
    void undo();

    double cardWidth() const;
    double cardHeight() const { return cardWidth() * 1.4; }
    QRectF pyramidRect(int row, int index) const;
    QRectF stockRect() const;
    QRectF wasteRect() const;
    double pileTop() const;

    static int slotIndex(int row, int index) { return PyramidTable::slotIndex(row, index); }
    // A pyramid card can only be taken once both cards resting on it are gone.
    bool isExposed(int row, int index) const { return m_table.isExposed(row, index); }
    std::optional<int> pyramidAt(QPointF pos) const;
    // Where the card a Source and index name is drawn. A pyramid index is a
    // slot, as the table counts them.
    QRectF rectOf(Source source, int index) const;

    void select(Source source, int index);
    void clearSelection();
    // Takes the selected card(s) if they add to 13. The SELECTION is the view's;
    // whether the take is legal is the table's.
    void tryPair(Source source, int index, const Card& card);
    void dealFromStock();
    // A taken card leaves the table visibly: it flies off past the right edge
    // from where it lay. Nothing on the surface receives it, so nothing waits
    // for it to land and no press has to land it first.
    void launchTaken(const std::vector<QRectF>& from, const std::vector<Card>& cards);
    // The opening deal, from the stock to the pyramid a row at a time from
    // the top, as a dealer lays it. A dealt card's destination is its slot,
    // which is how the pyramid hides it until it lands; a taken card's is -1.
    void launchDeal();
    // A draw slides the card from the stock to the waste; a redeal sends one
    // back the other way, carrying the stock's new top card, as Klondike's
    // does. Keyed past the last slot, so the pyramid never matches them.
    static constexpr int kWasteKey = PyramidTable::kPyramidCards;
    static constexpr int kStockKey = kWasteKey + 1;
    void launchDraw(bool redealt);
    void startFlights();
    // Puts every dealt or drawn card in the air down where it is going, so a
    // press never picks up a card the table is still hiding. Taken cards keep
    // flying.
    void landDealtCards();
    void clearFlights();
    void checkEnd();
    void refresh(const QString& message = {});

    // Keyboard (GHUB-0168). A stop is a place the cursor may stand.
    static constexpr int kPileRow = kRows;
    bool isStop(int row, int index) const;
    QPointF stopPos(int row, int index) const;
    QRectF stopRect(int row, int index) const;
    void stepCursor(int dx, int dy);
    void clampCursor();
    void resetCursor();
    void pressAtCursor();

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;

    // The rules. What is left here is the click, the selection and the
    // drawing.
    PyramidTable m_table;

    std::vector<cardflight::Flight> m_flights;
    // Per-repaint scratch for cardflight::suppressAt. Cleared at the top of
    // paintEvent, never read outside it.
    std::vector<char> m_flightConsumed;
    QTimer* m_flightTimer = nullptr;

    bool m_hasSelection = false;
    Source m_selectedSource = Source::Pyramid;
    int m_selectedIndex = -1;

    // Where a fresh deal puts it: the bottom row's first card, where every
    // pairing starts. Owner's call, 2026-09-28.
    int m_cursorRow = kRows - 1;
    int m_cursorIndex = 0;

    // Restoring a save clears the table's undo history, so canUndo() alone reads
    // a resumed deal as untouched and saveState() then returns {}, which the hub
    // treats as "delete the stored game". This says the deal is worth keeping.
    bool m_resumed = false;
    bool m_won = false;
    bool m_announced = false;
};
