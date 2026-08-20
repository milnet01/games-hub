#pragma once

#include "cards/card.h"
#include "gameview.h"

#include <QPointF>
#include <QRectF>

#include <array>
#include <random>
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

    static constexpr int kColumns = 8;
    static constexpr int kCells = 4;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 900, 640 }; }
    QSize minimumSizeHint() const override { return { 620, 440 }; }

private:
    enum class PileKind { Cell, Foundation, Column };

    struct Spot {
        PileKind kind = PileKind::Column;
        int pile = 0;
        int index = -1;
        bool valid = false;
    };

    struct Snapshot {
        std::array<std::vector<Card>, kColumns> columns;
        std::array<std::vector<Card>, kCells> cells;
        std::array<std::vector<Card>, 4> foundations;
        int moves = 0;
    };

    void buildActions();
    void newGame();
    void undo();
    void pushUndo();

    double cardWidth() const;
    double cardHeight() const { return cardWidth() * 1.4; }
    QRectF pileOrigin(PileKind kind, int pile) const;
    QRectF cardRect(int column, int index) const;
    double fanStep() const { return cardHeight() * 0.27; }
    Spot hitTest(QPointF pos) const;

    std::vector<Card>& pileFor(PileKind kind, int pile);
    const std::vector<Card>& pileFor(PileKind kind, int pile) const;

    // How many cards may move as a unit: one, doubled for every empty column,
    // times one more than the free cells. This is the rule that makes FreeCell
    // a planning game rather than a shuffling one.
    int maxMoveSize(bool toEmptyColumn) const;
    int orderedRunLength(int column) const;
    bool canStack(const Card& moving, int column) const;
    bool canPlaceOnFoundation(const Card& moving, int foundation) const;
    bool sendToFoundation(PileKind from, int pile);
    void checkWin();
    void refresh(const QString& message = {});

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;

    std::array<std::vector<Card>, kColumns> m_columns;
    std::array<std::vector<Card>, kCells> m_cells;
    std::array<std::vector<Card>, 4> m_foundations;

    std::vector<Card> m_drag;
    Spot m_dragFrom;
    QPointF m_dragPos;
    QPointF m_dragGrab;
    QPointF m_pressPos;
    bool m_dragging = false;
    bool m_pressValid = false;

    std::vector<Snapshot> m_history;
    std::mt19937 m_rng { std::random_device {}() };
    int m_moves = 0;
    bool m_won = false;
};
