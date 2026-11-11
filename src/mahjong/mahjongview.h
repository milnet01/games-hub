#pragma once

#include "gameview.h"
#include "mahjongboard.h"

#include <QTimer>

#include <array>
#include <optional>
#include <random>
#include <vector>

// Mahjong solitaire (GHUB-0238): clear a heap of 144 tiles by taking free
// pairs that match. Every deal can be cleared; a hint, an undo and a shuffle
// for when no pair is free.
class MahjongView : public GameView
{
    Q_OBJECT

public:
    explicit MahjongView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    QString rulesText() const override;
    // Tiles, not cards, said rather than inherited.
    double smallestCardWidth() const override { return 0.0; }
    // A taken pair fading out, and the hint's glow, both run on a QTimer.
    void deactivate() override;
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;
    // A taken pair still fading out.
    bool hasPendingAnimation() const override { return !m_fades.empty(); }
    // Under the legibility switch the plate carries the sentence and the
    // counts, and the line under the heap gives way to it, so large play does
    // not cost the tiles their height.
    QString captionText() const override;
    // For --shot: takes `turns` pairs, the hint's choice each time, at once
    // and silently, so a picture can show a heap part-cleared.
    bool advanceForShot(int turns) override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    // Arrow keys move between free tiles; Space or Return picks the one under
    // the cursor; Escape puts a picked tile down; H hints; S shuffles.
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override { return { 1100, 760 }; }
    QSize minimumSizeHint() const override { return { 640, 460 }; }

    // Protected so a test can click a tile and read the cursor without its
    // own copy of the geometry (the reason ReversiView gives).
    QRectF faceRect(int slot) const;
    int cursorSlot() const { return m_cursor; }
    int pickedSlot() const { return m_picked; }
    const mahjong::Board& board() const { return m_board; }

private:
    void buildActions();
    void newGame();
    void undo();
    void showHint();
    void shuffle();
    // The one place a press on a tile is handled, for the click and the keys.
    void pressAt(int slot);
    void take(int a, int b, bool animate = true);
    void refresh();
    void step();
    void settleCursor();
    void stepCursor(int dx, int dy);
    void setDimHeld(bool dim);

    // The sentence under the heap: what to do next, or why a press did nothing.
    QString boardSentence() const;
    double tileWidth() const;
    double depth() const { return tileWidth() * 0.11; }
    QPointF origin() const;
    QRectF sentenceRect() const;
    int slotAt(QPointF pos) const;
    void paintTile(QPainter& p, int slot, mahjong::Face face, double alpha, double lift) const;

    QList<QAction*> m_actions;
    QAction* m_undoAction = nullptr;
    QAction* m_dimAction = nullptr;

    mahjong::Board m_board;
    std::vector<std::array<mahjong::Face, mahjong::kTiles>> m_history;
    std::mt19937 m_rng;

    int m_picked = -1;
    int m_cursor = -1;
    QString m_message;
    QString m_counts;
    bool m_won = false;
    bool m_resumed = false;
    // Held tiles drawn a shade darker, so the free ones stand out. Stored.
    bool m_dimHeld = true;

    // A hinted pair glows for a while. Stepped, never clocked.
    std::optional<mahjong::Pair> m_hint;
    double m_hintLeftMs = 0.0;

    // A taken pair lifts and fades where it lay, 0 to 1.
    struct Fade {
        int slot;
        mahjong::Face face;
        double done;
    };
    std::vector<Fade> m_fades;
    QTimer* m_timer = nullptr;
};
