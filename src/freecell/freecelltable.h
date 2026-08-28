#pragma once

#include "cards/card.h"

#include <array>
#include <deque>
#include <random>
#include <vector>

// FreeCell: every card face up from the start, four cells to park singles in,
// and almost every deal winnable.
//
// The rules only — no widget, no drag geometry, no painting. Which pile the
// player dropped on is the view's business; whether the cards may go there is
// this. Extracted so gameshub_selftest can reach the rules (GHUB-0066), which
// it never could while they lived inside the widget.
class FreeCellTable
{
public:
    static constexpr int kColumns = 8;
    static constexpr int kCells = 4;
    static constexpr int kFoundations = 4;
    static constexpr int kPackSize = 52;

    enum class PileKind { Cell, Foundation, Column };

    FreeCellTable() { deal(); }

    void deal();

    const std::array<std::vector<Card>, kColumns>& columns() const { return m_columns; }
    const std::array<std::vector<Card>, kCells>& cells() const { return m_cells; }
    const std::array<std::vector<Card>, kFoundations>& foundations() const
    {
        return m_foundations;
    }
    const std::vector<Card>& pile(PileKind kind, int index) const;
    int moves() const { return m_moves; }
    bool won() const;

    // How many cards may move as a unit: one, doubled for every empty column,
    // times one more than the free cells. This is the rule that makes FreeCell
    // a planning game rather than a shuffling one.
    int maxMoveSize(bool toEmptyColumn) const;
    // The length of the alternating-colour, descending run at the foot of a
    // column. Only a run like that can be picked up together.
    int orderedRunLength(int column) const;
    // The lowest index in a column the player may take hold of.
    int firstMovableIndex(int column) const;
    bool canStack(const Card& moving, int column) const;
    bool canPlaceOnFoundation(const Card& moving, int foundation) const;

    // Picks cards up off a pile, ready to be dropped somewhere. Returns them,
    // or an empty vector if that is not a hold the rules allow.
    //
    // The undo snapshot is taken HERE, before the cards leave the pile, and
    // that is the whole point of doing it in one place. The view used to
    // snapshot at DROP time -- by which point the drag had already erased the
    // cards from their column -- so undoing a completed move restored a table
    // that had never held them and the cards were simply gone. FreeCell's own
    // save then refused to reload, because it demands the whole pack back, so
    // the player lost the game as well as the card (GHUB-0126).
    std::vector<Card> lift(PileKind kind, int index, int from);
    // Puts a lifted run back where it came from, and drops the snapshot lift()
    // banked: nothing happened, so there is nothing to undo.
    void putBack(PileKind kind, int index, const std::vector<Card>& run);

    // Each returns false and moves nothing when the drop is not legal; the run
    // stays in the caller's hands for putBack().
    bool dropOnCell(const std::vector<Card>& run, int cell);
    bool dropOnFoundation(const std::vector<Card>& run, int foundation);
    // `limit` is filled with maxMoveSize when the run is refused for being too
    // long, so the view can say how many cards would fit.
    bool dropOnColumn(const std::vector<Card>& run, int column, int* limit = nullptr);

    // Sends the top card of a pile to whichever foundation will take it.
    bool sendToFoundation(PileKind kind, int index);

    bool canUndo() const { return !m_history.empty(); }
    void undo();
    void forgetHistory() { m_history.clear(); }

    // Adopts a position from a save. FreeCell never takes a card out of play,
    // so the whole pack must come back -- nothing missing and nothing doubled.
    // False leaves this object alone.
    bool restore(const std::array<std::vector<Card>, kColumns>& columns,
                 const std::array<std::vector<Card>, kCells>& cells,
                 const std::array<std::vector<Card>, kFoundations>& foundations, int moves);

private:
    struct Snapshot {
        std::array<std::vector<Card>, kColumns> columns;
        std::array<std::vector<Card>, kCells> cells;
        std::array<std::vector<Card>, kFoundations> foundations;
        int moves = 0;
    };

    std::vector<Card>& pileAt(PileKind kind, int index);
    void pushUndo();
    void dropUndo();

    std::array<std::vector<Card>, kColumns> m_columns;
    std::array<std::vector<Card>, kCells> m_cells;
    std::array<std::vector<Card>, kFoundations> m_foundations;

    std::deque<Snapshot> m_history;
    std::mt19937 m_rng { std::random_device {}() };
    int m_moves = 0;
};
