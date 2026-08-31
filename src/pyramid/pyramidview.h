#pragma once

#include "cards/card.h"
#include "gameview.h"
#include "pyramid/pyramidtable.h"

#include <QPointF>
#include <QRectF>

#include <optional>

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

    // The pyramid's shape belongs to the rules; kept under the name the
    // painter already used.
    static constexpr int kRows = PyramidTable::kRows;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 860, 640 }; }
    QSize minimumSizeHint() const override { return { 600, 460 }; }

private:
    // Where a selectable card lives.
    using Source = PyramidTable::Source;
    using Slot = PyramidTable::Slot;

    void buildActions();
    void newGame();
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

    void select(Source source, int index);
    void clearSelection();
    // Takes the selected card(s) if they add to 13. The SELECTION is the view's;
    // whether the take is legal is the table's.
    void tryPair(Source source, int index, const Card& card);
    void dealFromStock();
    void checkEnd();
    void refresh(const QString& message = {});

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;

    // The rules. What is left here is the click, the selection and the
    // drawing.
    PyramidTable m_table;

    bool m_hasSelection = false;
    Source m_selectedSource = Source::Pyramid;
    int m_selectedIndex = -1;

    // Restoring a save clears the table's undo history, so canUndo() alone reads
    // a resumed deal as untouched and saveState() then returns {}, which the hub
    // treats as "delete the stored game". This says the deal is worth keeping.
    bool m_resumed = false;
    bool m_won = false;
    bool m_announced = false;
};
