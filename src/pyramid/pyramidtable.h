#pragma once

#include "dealseed.h"

#include "cards/card.h"

#include <QDataStream>

#include <random>
#include <vector>

// Pyramid: clear a stack of 28 cards by taking pairs that add up to 13. Kings
// are worth 13 on their own and go alone.
//
// The rules only — no widget, no selection, no painting. Which card the player
// has clicked first is the view's business; whether two cards may be taken
// together is this. Extracted so gameshub_selftest can reach the rules at all
// (GHUB-0066): Pyramid was named nowhere in that file.
class PyramidTable
{
public:
    static constexpr int kRows = 7;
    static constexpr int kPyramidCards = kRows * (kRows + 1) / 2;   // 28
    static constexpr int kMaxRedeals = 2;
    // What a pair has to add up to. A King is this on its own.
    static constexpr int kPairTotal = 13;

    // The three places a takeable card can be. Stock is never takeable — it is
    // here because the view names a click target with the same enum.
    enum class Source { Pyramid, Waste, Stock };

    // A taken pyramid card keeps its slot and is simply marked gone, because
    // the slot above it still needs to know both its supports have cleared.
    struct Slot {
        Card card;
        bool removed = false;
    };

    PyramidTable() { deal(); }

    void deal();

    const std::vector<Slot>& pyramid() const { return m_pyramid; }
    const std::vector<Card>& stock() const { return m_stock; }
    const std::vector<Card>& waste() const { return m_waste; }
    int pairs() const { return m_pairs; }
    int redeals() const { return m_redeals; }
    bool redealsLeft() const { return m_redeals < kMaxRedeals; }

    static int slotIndex(int row, int index) { return row * (row + 1) / 2 + index; }

    // A pyramid card can only be taken once both cards resting on it are gone.
    bool isExposed(int row, int index) const;
    bool isExposed(int slot) const;

    // Whether a card at this position can be picked up at all: an exposed
    // pyramid slot, or the top of the waste. Everything below goes through it,
    // so a move the player cannot see is a move the rules refuse.
    bool available(Source source, int index) const;
    Card cardAt(Source source, int index) const;

    // A King is 13 on its own. False if this is not an available King.
    bool takeKing(Source source, int index);
    // Two available cards whose ranks add to kPairTotal. False if either is not
    // available, if they are the same card, or if they do not add up.
    bool takePair(Source first, int firstIndex, Source second, int secondIndex);

    // One card from the stock to the waste. With the stock empty this turns the
    // waste back over instead, if a redeal is left. False when neither is
    // possible, which is when the game can no longer be moved on this way.
    bool drawFromStock();

    bool cleared() const;

    bool canUndo() const { return !m_history.empty(); }
    void undo();
    // Dropped when a game is restored: there is no earlier position to go back
    // to, and offering one would step into a game that was never played.

    // Adopts a position from a save. Pyramid takes matched pairs out of play,
    // so the pack cannot come back whole — what this checks is that nothing
    // was ever outside it, that no card turned up twice, and that the counts
    // are ones the rules could have produced. False leaves this object alone.
    bool restore(const std::vector<Slot>& pyramid, const std::vector<Card>& stock,
                 const std::vector<Card>& waste, int pairs, int redeals);

private:
    struct Snapshot {
        std::vector<Slot> pyramid;
        std::vector<Card> stock;
        std::vector<Card> waste;
        int pairs = 0;
        int redeals = 0;
    };

    void pushUndo();
    void removeFrom(Source source, int index);

    std::vector<Slot> m_pyramid;
    std::vector<Card> m_stock;
    std::vector<Card> m_waste;
    std::vector<Snapshot> m_history;

    std::mt19937 m_rng { dealSeed() };
    int m_pairs = 0;
    int m_redeals = 0;
};
