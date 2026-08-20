#pragma once

#include "cards/card.h"
#include "gameview.h"

#include <QPointF>
#include <QRectF>

#include <array>
#include <random>
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
    enum class PileKind { Stock, Waste, Foundation, Tableau };

    struct Spot {
        PileKind kind = PileKind::Stock;
        int pile = 0;   // which foundation / tableau column
        int index = -1; // index of the card within that pile
        bool valid = false;
    };

    struct Snapshot {
        std::vector<Card> stock;
        std::vector<Card> waste;
        std::array<std::vector<Card>, 4> foundations;
        std::array<std::vector<Card>, 7> tableau;
        int score = 0;
    };

    void buildActions();
    void newGame();
    void undo();
    void pushUndo();

    // Geometry
    double cardWidth() const;
    double cardHeight() const { return cardWidth() * 1.4; }
    double gap() const { return cardWidth() * 0.14; }
    QRectF pileOrigin(PileKind kind, int pile) const;
    QRectF cardRect(PileKind kind, int pile, int index) const;
    double fanStep(const std::vector<Card>& pile, int index) const;
    Spot hitTest(QPointF pos) const;

    std::vector<Card>& pileFor(PileKind kind, int pile);
    const std::vector<Card>& pileFor(PileKind kind, int pile) const;

    // Rules
    bool canStackOnTableau(const Card& moving, int column) const;
    bool canPlaceOnFoundation(const Card& moving, int foundation) const;
    // Sends a card to the first foundation that will take it. Used by
    // double-click and by the auto-finish sweep.
    bool sendToFoundation(PileKind from, int pile);
    bool autoFinishStep();

    void dealFromStock();
    void checkWin();
    void refresh();

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;

    std::vector<Card> m_stock;
    std::vector<Card> m_waste;
    std::array<std::vector<Card>, 4> m_foundations;
    std::array<std::vector<Card>, 7> m_tableau;

    // Cards currently under the cursor, lifted off their pile while dragging.
    std::vector<Card> m_drag;
    Spot m_dragFrom;
    QPointF m_dragPos;
    QPointF m_dragGrab;
    bool m_dragging = false;
    bool m_pressValid = false;
    QPointF m_pressPos;

    std::vector<Snapshot> m_history;
    std::mt19937 m_rng { std::random_device {}() };
    int m_drawCount = 1;
    int m_score = 0;
    bool m_won = false;
};
