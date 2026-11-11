#pragma once

#include "gameview.h"
#include "noughtsboard.h"

#include <QTimer>

#include <random>
#include <vector>

class QActionGroup;

// Noughts and Crosses on one board, against the computer at two strengths or
// between two people at one machine (GHUB-0236). You are always the crosses;
// who opens swaps every game, so the computer gets to start too.
class NoughtsView : public GameView
{
    Q_OBJECT

public:
    explicit NoughtsView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    // How to play, as this game plays it (GHUB-0016).
    QString rulesText() const override;
    // No cards on this board, said rather than inherited.
    double smallestCardWidth() const override { return 0.0; }
    void activate() override;
    // The computer's reply is a single-shot; leaving the game invalidates it,
    // and a mark still being drawn is finished.
    void deactivate() override;
    TurnLight turnLight() const override { return m_turn.value(); }
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;
    // A mark or the winning line still being drawn.
    bool hasPendingAnimation() const override;
    // The plate carries the score alone: the sentence it would otherwise
    // repeat is already under the board, in large type, switch or no switch.
    QString captionText() const override { return m_score; }
    // For --shot: both sides played by the Easy computer through the rules
    // core, unanimated and silent, so a picture can show a game under way.
    bool advanceForShot(int turns) override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    // Arrow keys move the cursor, Space or Return plays the cell under it, or
    // starts the next game once this one is over (GHUB-0168's shape).
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override { return { 520, 560 }; }
    QSize minimumSizeHint() const override { return { 280, 320 }; }

    // Protected so a test can click a cell and read the cursor without its
    // own copy of the geometry (the reason ReversiView gives).
    QRect boardRect() const;
    int cursorCell() const { return m_cursor; }

private:
    enum class Opponent { Easy, Hard, TwoPlayers };

    void buildActions();
    void newGame();
    void undo();
    void setOpponent(Opponent opponent);
    // The single point that moves the game on: finishes a game that is over,
    // and hands over to the computer when it is its turn.
    void advance();
    void playComputer();
    // The one place a person's move is applied, for the click and the keys.
    void playAt(int cell);
    // Puts a mark down for whoever is to move and starts drawing it.
    void place(int cell);
    // `loud` is false only for --shot, whose contract is silence.
    void finish(bool loud = true);
    void abandonThinking();
    void refresh();

    bool againstComputer() const { return m_opponent != Opponent::TwoPlayers; }
    noughts::Mark toMove() const { return m_board.toMove(m_starter); }
    bool computerToMove() const
    {
        return !m_finished && againstComputer() && toMove() == noughts::Mark::Nought;
    }

    // The sentence under the board: whose turn, or how the game ended.
    QString boardSentence() const;
    QRectF sentenceRect() const;
    QRectF turnBand(int seat) const;
    QRectF cellRect(int cell) const;
    int cellAt(QPointF pos) const;
    void step();
    void paintMark(QPainter& p, const QRectF& cell, noughts::Mark mark, double drawn) const;

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;
    QActionGroup* m_opponentGroup = nullptr;

    noughts::Board m_board;
    Opponent m_opponent = Opponent::Easy;
    noughts::Mark m_starter = noughts::Mark::Cross;
    // Who opens the next game. Swapped by every new game, so the first game
    // of a session is yours to open.
    noughts::Mark m_nextStarter = noughts::Mark::Cross;
    std::vector<noughts::Board> m_history;
    int m_cursor = 4;
    bool m_finished = false;
    bool m_resumed = false;

    // The computer's pause, guarded as Reversi's is: m_generation is stamped
    // when the reply is posted and bumped by anything that outruns it, and
    // m_thinking says a reply is still owed. Neither alone is enough.
    bool m_thinking = false;
    bool m_paused = false;
    quint64 m_generation = 0;
    std::mt19937 m_rng;

    // This session's results. Not saved: it is the evening's score, and a
    // change of opponent starts it again.
    int m_crossWins = 0;
    int m_noughtWins = 0;
    int m_draws = 0;
    QString m_score;

    // The newest mark drawing itself in, then the winning line drawn through.
    // Both run 0 to 1 and are stepped, never clocked.
    int m_newestCell = -1;
    double m_markDrawn = 1.0;
    double m_lineDrawn = 1.0;
    QTimer* m_timer = nullptr;

    TurnLightState m_turn;
    bool m_turnFades = false;
};
