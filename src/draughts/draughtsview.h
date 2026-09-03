#pragma once

#include "draughtsboard.h"
#include "gameview.h"

#include <QFutureWatcher>

#include <optional>
#include <vector>

class QActionGroup;

class DraughtsView : public GameView
{
    Q_OBJECT

public:
    explicit DraughtsView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    // No cards on this board. Said out loud because the base now answers
    // -1 for "nobody answered", so a game that simply forgot is no longer
    // indistinguishable from one with nothing to measure.
    double smallestCardWidth() const override { return 0.0; }
    QString captionText() const override { return m_caption; }
    void activate() override;
    // Not a QTimer, but the same duty: an answer arriving for a board the hub
    // has left must not move a piece on it.
    void deactivate() override;
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
    // The one place a human move is applied. Two click paths reach it now --
    // an unambiguous destination, and a route chosen from several.
    void playMove(const DraughtsMove& m);

    QRect boardRect() const;
    std::optional<Square> squareAt(QPointF pos) const;
    // Moves the human can make from the currently selected square.
    std::vector<DraughtsMove> movesFrom(Square s) const;

    struct Snapshot {
        DraughtsBoard board;
        Side toMove;
        // The gold highlight is painted from m_lastMove. Without it here, undo
        // restored the board and left the highlight on a move that had been
        // taken back -- the surface pointing at something that did not happen,
        // which is worse than pointing at nothing (GHUB-0160).
        std::optional<DraughtsMove> lastMove;
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
    // Two capture chains from one square can finish on the SAME square while
    // taking different pieces -- routine for a king, and English draughts lets
    // you play either. The board used to take the first one it found. These
    // hold the candidates while the player says which, and the step index at
    // which they part company: that square is what is really being chosen.
    std::vector<DraughtsMove> m_choices;
    int m_chooseStep = -1;
    std::optional<DraughtsMove> m_lastMove;
    std::vector<Snapshot> m_history;
    bool m_thinking = false;
    // GHUB-0047. The search runs on a worker and the answer arrives back here;
    // see ChessView for the full reasoning. A search the game has outrun is
    // abandoned by generation rather than awaited, and m_paused stops one that
    // was merely SCHEDULED from setting off after the hub has gone.
    struct SearchResult {
        DraughtsMove move;
        bool found = false;
        quint64 generation = 0;
    };
    void startSearch();
    void abandonSearch();
    void engineMoveReady(const SearchResult& result);
    QFutureWatcher<SearchResult>* m_search = nullptr;
    quint64 m_generation = 0;
    bool m_paused = false;
    // Restoring a save clears the undo history, so m_history.empty() alone reads
    // a resumed game as untouched and saveState() then returns {}, which the hub
    // treats as "delete the stored game". This says the position is worth keeping.
    bool m_resumed = false;
    bool m_finished = false;
    QString m_caption;
};
