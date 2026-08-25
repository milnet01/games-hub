#pragma once

#include "cards/card.h"

#include <array>
#include <random>
#include <vector>

// Spider: two packs, ten columns, and eight King-to-Ace runs to build. Cards
// stack by rank alone, but only a same-suit run moves together — which is what
// makes the four-suit game hard and the one-suit game a puzzle.
//
// The rules only — no widget, no drag geometry, no painting. Extracted so
// gameshub_selftest can reach them (GHUB-0066).
class SpiderTable
{
public:
    static constexpr int kColumns = 10;
    // King down to Ace, one suit.
    static constexpr int kRunLength = 13;
    static constexpr int kRunsToWin = 8;
    static constexpr int kPackCards = 104;   // two packs

    // What a drop did. Completed means it also finished a King-to-Ace run and
    // took it off the table, which the view marks with a sound of its own.
    enum class Drop { Refused, Moved, Completed };

    SpiderTable() { deal(1); }

    // `suits` is 1, 2 or 4: two full packs either way, the suit count only
    // limits which suits appear.
    void deal(int suits);

    const std::array<std::vector<Card>, kColumns>& columns() const { return m_columns; }
    const std::vector<Card>& stock() const { return m_stock; }
    int suits() const { return m_suits; }
    int completed() const { return m_completed; }
    int moves() const { return m_moves; }
    bool won() const { return m_completed >= kRunsToWin; }

    // Length of the same-suit DESCENDING run ending at the foot of a column,
    // which is exactly what the player is allowed to pick up.
    int movableRunLength(int column) const;
    // Spider stacks by rank alone; only MOVING a run needs matching suits.
    bool canDrop(const Card& moving, int column) const;
    bool canLift(int column, int index) const;

    // Picks a run up and HOLDS it, so the table knows which column it came from
    // and can turn over what it uncovered. The undo snapshot is banked HERE,
    // before the cards leave the column.
    //
    // The view used to snapshot at DROP time and then PATCH the snapshot,
    // pushing the held cards back onto its copy of the old column by hand,
    // because the run had already been lifted. That worked, and it is the only
    // one of the three dragging solitaires that did it -- Klondike and FreeCell
    // had the same ordering with no patch and lost the card (GHUB-0126).
    // Banking before the lift makes the patch unnecessary rather than correct.
    std::vector<Card> lift(int column, int index);
    bool holding() const { return !m_held.empty(); }
    const std::vector<Card>& held() const { return m_held; }
    void putBack();

    Drop dropOn(int column);

    // A row may only be dealt with every column occupied: the rule that stops
    // a deal burying an empty column beyond recovery.
    bool canDealRow() const;
    bool dealRow();

    bool canUndo() const { return !m_history.empty(); }
    void undo();
    void forgetHistory() { m_history.clear(); }

    // Adopts a position from a save. Spider takes a finished run off the table
    // for good, so it cannot ask for the whole pack back: what is left is two
    // packs less thirteen for every run completed, and all of it from the pack
    // it was dealt.
    bool restore(const std::array<std::vector<Card>, kColumns>& columns,
                 const std::vector<Card>& stock, int suits, int completed, int moves);

    static bool validSuitCount(int suits) { return suits == 1 || suits == 2 || suits == 4; }

private:
    struct Snapshot {
        std::array<std::vector<Card>, kColumns> columns;
        std::vector<Card> stock;
        int completed = 0;
        int moves = 0;
    };

    void pushUndo();
    void dropUndo();
    // Removes a complete King-to-Ace same-suit run from a column, if there is
    // one. Returns whether it took one.
    bool harvest(int column);

    std::array<std::vector<Card>, kColumns> m_columns;
    std::vector<Card> m_stock;

    std::vector<Card> m_held;
    int m_heldFrom = -1;

    std::vector<Snapshot> m_history;
    std::mt19937 m_rng { std::random_device {}() };
    int m_suits = 1;
    int m_completed = 0;
    int m_moves = 0;
};
