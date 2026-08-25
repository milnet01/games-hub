#pragma once

#include "cards/card.h"
#include "gameview.h"
#include "spider/spidertable.h"

#include <QPointF>
#include <QRectF>

#include <array>
#include <vector>

class SpiderView : public GameView
{
    Q_OBJECT

public:
    explicit SpiderView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    double smallestCardWidth() const override { return cardWidth(); }
    void activate() override;
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

    // The table's shape belongs to the rules; kept under the name the
    // painter already used.
    static constexpr int kColumns = SpiderTable::kColumns;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 900, 640 }; }
    QSize minimumSizeHint() const override { return { 620, 440 }; }

private:
    void buildActions();
    void newGame();
    void undo();

    double cardWidth() const;
    double cardHeight() const { return cardWidth() * 1.4; }
    double fanStep(const std::vector<Card>& column, int index) const;
    QRectF columnOrigin(int column) const;
    QRectF cardRect(int column, int index) const;
    QRectF stockRect() const;

    int movableRunLength(int column) const { return m_table.movableRunLength(column); }
    void dealRow();
    void checkWin();
    void refresh();

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;

    // The rules. What is left here is the pointer, the drag and the drawing.
    SpiderTable m_table;

    std::vector<Card> m_drag;
    int m_dragFrom = -1;
    int m_dragIndex = -1;
    QPointF m_dragPos;
    QPointF m_dragGrab;
    QPointF m_pressPos;
    bool m_dragging = false;
    bool m_pressValid = false;

    bool m_won = false;
};
