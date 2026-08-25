#include "freecelltable.h"

#include "cards/cardcodec.h"

#include <algorithm>

namespace {
// A drag never runs deeper than the pack, and the history is bounded so a long
// game does not keep every position it ever held.
constexpr std::size_t kMaxHistory = 300;
}

void FreeCellTable::deal()
{
    std::vector<Card> deck = makeDeck(1, 4);
    shuffleCards(deck, m_rng);

    for (std::vector<Card>& c : m_columns)
        c.clear();
    for (std::vector<Card>& c : m_cells)
        c.clear();
    for (std::vector<Card>& f : m_foundations)
        f.clear();
    m_history.clear();
    m_moves = 0;

    // All 52 face up across eight columns: four of seven, four of six.
    for (std::size_t i = 0; i < deck.size(); ++i) {
        Card c = deck[i];
        c.faceUp = true;
        m_columns[i % kColumns].push_back(c);
    }
}

std::vector<Card>& FreeCellTable::pileAt(PileKind kind, int index)
{
    switch (kind) {
    case PileKind::Cell: return m_cells[std::size_t(index)];
    case PileKind::Foundation: return m_foundations[std::size_t(index)];
    case PileKind::Column: break;
    }
    return m_columns[std::size_t(index)];
}

const std::vector<Card>& FreeCellTable::pile(PileKind kind, int index) const
{
    return const_cast<FreeCellTable*>(this)->pileAt(kind, index);
}

bool FreeCellTable::won() const
{
    int total = 0;
    for (const std::vector<Card>& f : m_foundations)
        total += int(f.size());
    return total == kPackSize;
}

int FreeCellTable::maxMoveSize(bool toEmptyColumn) const
{
    int freeCells = 0;
    for (const std::vector<Card>& c : m_cells) {
        if (c.empty())
            ++freeCells;
    }
    int emptyColumns = 0;
    for (const std::vector<Card>& c : m_columns) {
        if (c.empty())
            ++emptyColumns;
    }

    // Moving *into* an empty column cannot also use that column as a staging
    // area, so it does not count towards the multiplier.
    if (toEmptyColumn && emptyColumns > 0)
        --emptyColumns;

    return (freeCells + 1) * (1 << std::min(emptyColumns, 10));
}

int FreeCellTable::orderedRunLength(int column) const
{
    const std::vector<Card>& col = m_columns[std::size_t(column)];
    if (col.empty())
        return 0;
    int run = 1;
    for (int i = int(col.size()) - 1; i > 0; --i) {
        const Card& lower = col[std::size_t(i)];
        const Card& upper = col[std::size_t(i - 1)];
        if (upper.rank != lower.rank + 1 || isRed(upper) == isRed(lower))
            break;
        ++run;
    }
    return run;
}

int FreeCellTable::firstMovableIndex(int column) const
{
    return int(m_columns[std::size_t(column)].size()) - orderedRunLength(column);
}

bool FreeCellTable::canStack(const Card& moving, int column) const
{
    const std::vector<Card>& target = m_columns[std::size_t(column)];
    if (target.empty())
        return true;
    const Card& top = target.back();
    return isRed(top) != isRed(moving) && top.rank == moving.rank + 1;
}

bool FreeCellTable::canPlaceOnFoundation(const Card& moving, int foundation) const
{
    const std::vector<Card>& target = m_foundations[std::size_t(foundation)];
    if (target.empty())
        return moving.rank == kAce;
    return target.back().suit == moving.suit && target.back().rank + 1 == moving.rank;
}

void FreeCellTable::pushUndo()
{
    m_history.push_back({ m_columns, m_cells, m_foundations, m_moves });
    if (m_history.size() > kMaxHistory)
        m_history.erase(m_history.begin());
}

void FreeCellTable::dropUndo()
{
    if (!m_history.empty())
        m_history.pop_back();
}

std::vector<Card> FreeCellTable::lift(PileKind kind, int index, int from)
{
    std::vector<Card>& source = pileAt(kind, index);
    if (from < 0 || from >= int(source.size()))
        return {};
    if (kind == PileKind::Column) {
        // Only a run in alternating colours moves together.
        if (from < firstMovableIndex(index))
            return {};
    } else if (from != int(source.size()) - 1) {
        // A cell holds one card, and a foundation only ever gives back its top.
        return {};
    }

    pushUndo();
    std::vector<Card> run(source.begin() + from, source.end());
    source.erase(source.begin() + from, source.end());
    return run;
}

void FreeCellTable::putBack(PileKind kind, int index, const std::vector<Card>& run)
{
    std::vector<Card>& source = pileAt(kind, index);
    for (const Card& c : run)
        source.push_back(c);
    dropUndo();
}

bool FreeCellTable::dropOnCell(const std::vector<Card>& run, int cell)
{
    if (run.size() != 1 || !m_cells[std::size_t(cell)].empty())
        return false;
    m_cells[std::size_t(cell)].push_back(run.front());
    ++m_moves;
    return true;
}

bool FreeCellTable::dropOnFoundation(const std::vector<Card>& run, int foundation)
{
    if (run.size() != 1 || !canPlaceOnFoundation(run.front(), foundation))
        return false;
    m_foundations[std::size_t(foundation)].push_back(run.front());
    ++m_moves;
    return true;
}

bool FreeCellTable::dropOnColumn(const std::vector<Card>& run, int column, int* limit)
{
    if (run.empty() || !canStack(run.front(), column))
        return false;

    const int allowed = maxMoveSize(m_columns[std::size_t(column)].empty());
    if (int(run.size()) > allowed) {
        if (limit != nullptr)
            *limit = allowed;
        return false;
    }

    for (const Card& c : run)
        m_columns[std::size_t(column)].push_back(c);
    ++m_moves;
    return true;
}

bool FreeCellTable::sendToFoundation(PileKind kind, int index)
{
    std::vector<Card>& source = pileAt(kind, index);
    if (source.empty())
        return false;
    for (int f = 0; f < kFoundations; ++f) {
        if (!canPlaceOnFoundation(source.back(), f))
            continue;
        pushUndo();
        m_foundations[std::size_t(f)].push_back(source.back());
        source.pop_back();
        ++m_moves;
        return true;
    }
    return false;
}

void FreeCellTable::undo()
{
    if (m_history.empty())
        return;
    const Snapshot& s = m_history.back();
    m_columns = s.columns;
    m_cells = s.cells;
    m_foundations = s.foundations;
    m_moves = s.moves;
    m_history.pop_back();
}

bool FreeCellTable::restore(const std::array<std::vector<Card>, kColumns>& columns,
                            const std::array<std::vector<Card>, kCells>& cells,
                            const std::array<std::vector<Card>, kFoundations>& foundations,
                            int moves)
{
    if (moves < 0)
        return false;
    for (const std::vector<Card>& c : cells) {
        // A cell holds one card or none. Nothing else is a position the rules
        // could have produced.
        if (c.size() > 1)
            return false;
    }

    // FreeCell never takes a card out of play, so the whole pack must come
    // back: nothing missing and nothing doubled.
    std::vector<Card> all;
    all.reserve(kPackSize);
    cardcodec::gather(all, columns);
    cardcodec::gather(all, cells);
    cardcodec::gather(all, foundations);
    if (!cardcodec::matchesPack(all, 1, 4))
        return false;

    m_columns = columns;
    m_cells = cells;
    m_foundations = foundations;
    m_moves = moves;
    m_history.clear();
    return true;
}
