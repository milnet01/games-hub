#pragma once

#include "cards/card.h"
#include "freecell/freecelltable.h"
#include "gameview.h"

#include <QPointF>
#include <QRectF>

#include <array>
#include <vector>

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
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

    // The table's shape belongs to the rules; kept under the names the painter
    // and its tests already used.
    static constexpr int kColumns = FreeCellTable::kColumns;
    static constexpr int kCells = FreeCellTable::kCells;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
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
    Spot hitTest(QPointF pos) const;

    const std::vector<Card>& pileFor(PileKind kind, int pile) const
    {
        return m_table.pile(kind, pile);
    }

    void checkWin();
    void refresh(const QString& message = {});

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;

    // The rules. What is left here is the pointer, the drag and the drawing.
    FreeCellTable m_table;

    std::vector<Card> m_drag;
    Spot m_dragFrom;
    QPointF m_dragPos;
    QPointF m_dragGrab;
    QPointF m_pressPos;
    bool m_dragging = false;
    bool m_pressValid = false;

    bool m_won = false;
};
