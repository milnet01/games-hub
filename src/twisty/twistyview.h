#pragma once

#include "dealseed.h"
#include "gameview.h"
#include "twistyart.h"
#include "twistypuzzle.h"

#include <QElapsedTimer>

#include <deque>
#include <optional>
#include <random>

class QActionGroup;
class QTimer;

// Twisty Cube (GHUB-0253): a twisty puzzle in one of six shapes, drawn in 3D.
// Drag a sticker to turn its layer, drag anywhere else to turn the puzzle
// round. The rules are twisty::Puzzle; this is the drawing, the clock and the
// input.
class TwistyView : public GameView
{
    Q_OBJECT

public:
    explicit TwistyView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    QString rulesText() const override;
    double smallestCardWidth() const override { return 0.0; }
    void activate() override;
    void deactivate() override;
    bool advanceForShot(int turns) override;

    // The scramble, the turns since, the clock and the view.
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;

    const twisty::Puzzle& puzzle() const { return m_puzzle; }
    void setShape(twisty::Shape shape);
    // Turns one layer, shown turning. Every turn the player makes comes
    // through here, after the drag or the key has picked it.
    void playTurn(twisty::Move move) { queueTurn(move); }
    // Turns the layer through `sticker` the way `direction` (screen pixels)
    // points: the one press for a drag and for Shift and an arrow key.
    bool turnFrom(int sticker, const QPointF& direction);
    // The sticker under `point`, or -1.
    int stickerAt(const QPointF& point) const;
    // Where a sticker's centre is drawn, for a check that drags it.
    QPointF stickerCentre(int sticker) const;
    int cursorSticker() const { return m_cursor; }
    bool animating() const { return m_anim.has_value() || m_reveal.has_value(); }
    // Finishes every turn in flight at once.
    void settle();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override { return { 760, 680 }; }
    QSize minimumSizeHint() const override { return { 380, 380 }; }

private:
    struct Anim {
        twisty::Move move;
        double t = 0.0;
        double durationMs = 260.0;
    };

    void buildActions();
    void newScramble(bool animate);
    void queueTurn(twisty::Move move);
    void startNext();
    void onFrame();
    void finishAnim();
    void onSolved();
    void undo();
    void refresh();

    const twisty::Puzzle& shown() const { return m_reveal ? *m_reveal : m_puzzle; }
    QRectF puzzleArea() const;
    twisty::Camera camera() const;
    twisty::Spin spin() const;
    void outlines(std::vector<QPolygonF>& outline, std::vector<bool>& facing) const;
    void moveCursor(const QPointF& direction);
    void keepCursorInSight();
    int centreSticker() const;
    qint64 elapsedMs() const;
    QString timeText(qint64 ms) const;
    QString shapeName(twisty::Shape shape) const;
    QString bestKey() const;
    void paintHud(QPainter& p) const;

    QList<QAction*> m_actions;
    QActionGroup* m_shapeGroup = nullptr;
    QAction* m_pauseAction = nullptr;

    std::mt19937 m_rng { dealSeed() };
    twisty::Puzzle m_puzzle;
    // While a new scramble is being shown turning, the puzzle as it stands
    // part-way through it. The real one is already scrambled.
    std::optional<twisty::Puzzle> m_reveal;
    size_t m_revealNext = 0;

    std::optional<Anim> m_anim;
    std::deque<twisty::Move> m_queue;
    QTimer* m_frame = nullptr;
    QElapsedTimer m_frameClock;
    // The puzzle's spin on a solve, 0 to 1; negative when not celebrating.
    double m_celebrate = -1.0;
    // Confetti in the colours of the puzzle just solved.
    struct Bit {
        QPointF at;
        QPointF velocity; // pixels a second
        double angle = 0.0;
        double spin = 0.0; // degrees a second
        int face = 0;
    };
    std::vector<Bit> m_confetti;
    void throwConfetti();

    double m_yaw = -35.0;
    double m_pitch = 28.0;
    int m_cursor = -1;

    enum class Drag { None, Pending, Turned, Orbit };
    Drag m_drag = Drag::None;
    int m_pressSticker = -1;
    QPointF m_pressPos;
    QPointF m_lastPos;

    QTimer* m_tick = nullptr;
    QElapsedTimer m_clock;
    qint64 m_elapsedMs = 0; // banked before the current run of the clock
    bool m_running = false; // the clock starts on the first turn
    bool m_paused = false;
    bool m_suspended = false;
    bool m_solved = false;
};
