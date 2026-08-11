#pragma once

#include "cards/card.h"
#include "gameview.h"

#include <QPointF>
#include <QRectF>

#include <array>
#include <random>
#include <vector>

class SpiderView : public GameView
{
    Q_OBJECT

public:
    explicit SpiderView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    void activate() override;
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

    static constexpr int kColumns = 10;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 900, 640 }; }
    QSize minimumSizeHint() const override { return { 620, 440 }; }

private:
    struct Snapshot {
        std::array<std::vector<Card>, kColumns> columns;
        std::vector<Card> stock;
        int completed = 0;
        int moves = 0;
    };

    void buildActions();
    void newGame();
    void undo();
    void pushUndo();

    double cardWidth() const;
    double cardHeight() const { return cardWidth() * 1.4; }
    double fanStep(const std::vector<Card>& column, int index) const;
    QRectF columnOrigin(int column) const;
    QRectF cardRect(int column, int index) const;
    QRectF stockRect() const;

    // Length of the same-suit descending run ending at the bottom of a column,
    // which is exactly what the player is allowed to pick up.
    int movableRunLength(int column) const;
    bool canDrop(const Card& moving, int column) const;
    void dealRow();
    // Removes any complete King-to-Ace same-suit run from a column.
    void harvest(int column);
    void checkWin();
    void refresh();

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;

    std::array<std::vector<Card>, kColumns> m_columns;
    std::vector<Card> m_stock;

    std::vector<Card> m_drag;
    int m_dragFrom = -1;
    int m_dragIndex = -1;
    QPointF m_dragPos;
    QPointF m_dragGrab;
    QPointF m_pressPos;
    bool m_dragging = false;
    bool m_pressValid = false;

    std::vector<Snapshot> m_history;
    std::mt19937 m_rng { std::random_device {}() };
    int m_suits = 1;
    int m_completed = 0;
    int m_moves = 0;
    bool m_won = false;
};
