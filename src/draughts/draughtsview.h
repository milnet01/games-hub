#pragma once

#include "draughtsboard.h"
#include "gameview.h"

#include <optional>
#include <vector>

class QActionGroup;

class DraughtsView : public GameView
{
    Q_OBJECT

public:
    explicit DraughtsView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    QString captionText() const override { return m_caption; }
    void activate() override;
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return { 560, 560 }; }
    QSize minimumSizeHint() const override { return { 320, 320 }; }

private:
    void buildActions();
    void newGame();
    void undo();
    // The single point that moves the game on, as in Reversi.
    void advance();
    void playEngineMove();
    void refresh(const QString& message = {});
    void announceResult(Side winner);

    QRect boardRect() const;
    std::optional<Square> squareAt(QPointF pos) const;
    // Moves the human can make from the currently selected square.
    std::vector<DraughtsMove> movesFrom(Square s) const;

    struct Snapshot {
        DraughtsBoard board;
        Side toMove;
    };

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;
    QActionGroup* m_levelGroup = nullptr;

    DraughtsBoard m_board;
    Side m_toMove = Side::Red;
    Side m_human = Side::Red;
    DraughtsLevel m_level = DraughtsLevel::Medium;

    std::optional<Square> m_selected;
    std::vector<DraughtsMove> m_selectedMoves;
    std::optional<DraughtsMove> m_lastMove;
    std::vector<Snapshot> m_history;
    bool m_thinking = false;
    bool m_finished = false;
    QString m_caption;
};
