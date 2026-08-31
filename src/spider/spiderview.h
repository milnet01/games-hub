#pragma once

#include "cards/card.h"
#include "cards/cardflight.h"
#include "gameview.h"
#include "spider/spidertable.h"

#include <QPointF>
#include <QRectF>

#include <array>
#include <vector>

class QTimer;

class SpiderView : public GameView
{
    Q_OBJECT

public:
    explicit SpiderView(QWidget* parent = nullptr);

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

    // GHUB-0065. A completed run is thirteen cards leaving a column at once,
    // and until this existed the only sign it had happened was a count in the
    // status bar. They fly to the stock corner, staggered, so the run reads as
    // a sequence rather than a disappearance.
    void launchCompletedRun(const std::vector<Card>& run, const std::vector<QRectF>& fromRects);

    std::vector<cardflight::Flight> m_flights;
    QTimer* m_flightTimer = nullptr;

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

    // Restoring a save clears the table's undo history, so canUndo() alone reads
    // a resumed deal as untouched and saveState() then returns {}, which the hub
    // treats as "delete the stored game". This says the deal is worth keeping.
    bool m_resumed = false;
    bool m_won = false;
};
