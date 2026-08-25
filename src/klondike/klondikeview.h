#pragma once

#include "cards/card.h"
#include "gameview.h"
#include "klondike/klondiketable.h"

#include <QPointF>
#include <QRectF>

#include <array>
#include <vector>

class KlondikeView : public GameView
{
    Q_OBJECT

public:
    explicit KlondikeView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    double smallestCardWidth() const override { return cardWidth(); }
    void activate() override;
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

    bool m_won = false;
};
