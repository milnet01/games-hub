#pragma once

#include "cards/card.h"
#include "gameview.h"

#include <QPointF>
#include <QRectF>

#include <optional>
#include <random>
#include <vector>

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
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

    static constexpr int kRows = 7;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 860, 640 }; }
    QSize minimumSizeHint() const override { return { 600, 460 }; }

private:
    // Where a selectable card lives.
    enum class Source { Pyramid, Waste, Stock };

    struct Slot {
        Card card;
        bool removed = false;
    };

    struct Snapshot {
        std::vector<Slot> pyramid;
        std::vector<Card> stock;
        std::vector<Card> waste;
        int pairs = 0;
        int redeals = 0;
    };

    void buildActions();
    void newGame();
    void undo();
    void pushUndo();

    double cardWidth() const;
    double cardHeight() const { return cardWidth() * 1.4; }
    QRectF pyramidRect(int row, int index) const;
    QRectF stockRect() const;
    QRectF wasteRect() const;

    static int slotIndex(int row, int index) { return row * (row + 1) / 2 + index; }
    // A pyramid card can only be taken once both cards resting on it are gone.
    bool isExposed(int row, int index) const;
    std::optional<int> pyramidAt(QPointF pos) const;

    void select(Source source, int index);
    void clearSelection();
    // Takes the selected card(s) if they add to 13.
    void tryPair(Source source, int index, const Card& card);
    void removeFrom(Source source, int index);
    void dealFromStock();
    void checkEnd();
    void refresh(const QString& message = {});

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;

    std::vector<Slot> m_pyramid;
    std::vector<Card> m_stock;
    std::vector<Card> m_waste;

    bool m_hasSelection = false;
    Source m_selectedSource = Source::Pyramid;
    int m_selectedIndex = -1;

    std::vector<Snapshot> m_history;
    std::mt19937 m_rng { std::random_device {}() };
    int m_pairs = 0;
    int m_redeals = 0;
    bool m_won = false;
    bool m_announced = false;
};
