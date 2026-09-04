#include "spidertable.h"

#include "cards/cardcodec.h"

#include <utility>

namespace {
constexpr std::size_t kMaxHistory = 200;
}

void SpiderTable::deal(int suits)
{
    if (validSuitCount(suits))
        m_suits = suits;

    // Two full packs; the suit count only limits which suits appear.
    std::vector<Card> deck = makeDeck(2, m_suits);
    shuffleCards(deck, m_rng);

    for (std::vector<Card>& c : m_columns)
        c.clear();
    m_stock.clear();
    m_held.clear();
    m_heldFrom = -1;
    m_history.clear();
    m_completed = 0;
    m_moves = 0;

    // The classic deal: 54 cards out, the first four columns getting six.
    std::size_t next = 0;
    for (int col = 0; col < kColumns; ++col) {
        const int count = (col < 4) ? 6 : 5;
        for (int i = 0; i < count; ++i) {
            Card c = deck[next++];
            c.faceUp = (i == count - 1);
            m_columns[std::size_t(col)].push_back(c);
        }
    }
    m_stock.assign(deck.begin() + std::ptrdiff_t(next), deck.end());
    for (Card& c : m_stock)
        c.faceUp = false;
}

int SpiderTable::movableRunLength(int column) const
{
    const std::vector<Card>& col = m_columns[std::size_t(column)];
    if (col.empty())
        return 0;

    int run = 1;
    for (int i = int(col.size()) - 1; i > 0; --i) {
        const Card& lower = col[std::size_t(i)];
        const Card& upper = col[std::size_t(i - 1)];
        if (!upper.faceUp || upper.suit != lower.suit || upper.rank != lower.rank + 1)
            break;
        ++run;
    }
    return run;
}

bool SpiderTable::canDrop(const Card& moving, int column) const
{
    const std::vector<Card>& col = m_columns[std::size_t(column)];
    if (col.empty())
        return true;   // any card may start an empty column
    const Card& top = col.back();
    return top.faceUp && top.rank == moving.rank + 1;
}

bool SpiderTable::canLift(int column, int index) const
{
    if (column < 0 || column >= kColumns)
        return false;
    const std::vector<Card>& col = m_columns[std::size_t(column)];
    if (index < 0 || index >= int(col.size()))
        return false;
    return index >= int(col.size()) - movableRunLength(column);
}

void SpiderTable::pushUndo()
{
    m_history.push_back({ m_columns, m_stock, m_completed, m_moves });
    if (m_history.size() > kMaxHistory)
        m_history.pop_front();
}

void SpiderTable::dropUndo()
{
    if (!m_history.empty())
        m_history.pop_back();
}

std::vector<Card> SpiderTable::lift(int column, int index)
{
    if (holding() || !canLift(column, index))
        return {};

    pushUndo();
    std::vector<Card>& col = m_columns[std::size_t(column)];
    m_held.assign(col.begin() + index, col.end());
    col.erase(col.begin() + index, col.end());
    m_heldFrom = column;
    return m_held;
}

void SpiderTable::putBack()
{
    if (!holding())
        return;
    std::vector<Card>& col = m_columns[std::size_t(m_heldFrom)];
    for (const Card& c : m_held)
        col.push_back(c);
    m_held.clear();
    m_heldFrom = -1;
    dropUndo();
}

SpiderTable::Drop SpiderTable::dropOn(int column)
{
    if (!holding() || column < 0 || column >= kColumns)
        return Drop::Refused;
    if (column == m_heldFrom || !canDrop(m_held.front(), column))
        return Drop::Refused;

    for (const Card& c : m_held)
        m_columns[std::size_t(column)].push_back(c);
    m_held.clear();

    // Turn over whatever the run was covering.
    std::vector<Card>& source = m_columns[std::size_t(m_heldFrom)];
    if (!source.empty() && !source.back().faceUp)
        source.back().faceUp = true;
    m_heldFrom = -1;

    ++m_moves;
    return harvest(column) ? Drop::Completed : Drop::Moved;
}

bool SpiderTable::harvest(int column)
{
    std::vector<Card>& col = m_columns[std::size_t(column)];
    if (int(col.size()) < kRunLength)
        return false;

    // A complete run is King down to Ace, one suit, all face up.
    const int start = int(col.size()) - kRunLength;
    for (int i = 0; i < kRunLength; ++i) {
        const Card& c = col[std::size_t(start) + std::size_t(i)];
        if (!c.faceUp || c.rank != kKing - i || c.suit != col[std::size_t(start)].suit)
            return false;
    }

    col.erase(col.begin() + start, col.end());
    ++m_completed;
    if (!col.empty() && !col.back().faceUp)
        col.back().faceUp = true;
    return true;
}

bool SpiderTable::canDealRow() const
{
    if (m_stock.empty())
        return false;
    for (const std::vector<Card>& col : m_columns) {
        if (col.empty())
            return false;
    }
    return true;
}

bool SpiderTable::dealRow()
{
    if (!canDealRow())
        return false;

    pushUndo();
    for (int col = 0; col < kColumns && !m_stock.empty(); ++col) {
        Card c = m_stock.back();
        m_stock.pop_back();
        c.faceUp = true;
        m_columns[std::size_t(col)].push_back(c);
    }
    for (int col = 0; col < kColumns; ++col)
        harvest(col);
    return true;
}

void SpiderTable::undo()
{
    if (m_history.empty())
        return;
    // The snapshot is destroyed on the line below, so take its piles rather
    // than copying them. Copy-assigning here allocates eleven vectors and then
    // frees the originals a line later, every single undo.
    Snapshot& s = m_history.back();
    m_columns = std::move(s.columns);
    m_stock = std::move(s.stock);
    m_completed = s.completed;
    m_moves = s.moves;
    m_history.pop_back();
    m_held.clear();
    m_heldFrom = -1;
}

bool SpiderTable::restore(const std::array<std::vector<Card>, kColumns>& columns,
                          const std::vector<Card>& stock, int suits, int completed, int moves)
{
    if (!validSuitCount(suits) || completed < 0 || completed > kRunsToWin || moves < 0)
        return false;

    std::vector<Card> all;
    all.reserve(kPackCards);
    cardcodec::gather(all, columns);
    cardcodec::gather(all, stock);
    if (all.size() != std::size_t(kPackCards - kRunLength * completed)
        || !cardcodec::fitsPack(all, 2, suits))
        return false;

    // Once a column turns face up it stays face up to the end of the column.
    // canLift() says as much in as many words -- "everything under it in the
    // column is face up by construction" -- and Spider's movableRunLength()
    // walks from the top assuming the same. Nothing checked it, so a save could
    // bury a face-down card above a face-up one and let a run be lifted through
    // cards nobody has turned over.
    for (const std::vector<Card>& column : columns) {
        bool seenFaceUp = false;
        for (const Card& c : column) {
            if (c.faceUp)
                seenFaceUp = true;
            else if (seenFaceUp)
                return false;
        }
    }

    m_columns = columns;
    m_stock = stock;
    m_suits = suits;
    m_completed = completed;
    m_moves = moves;
    m_history.clear();
    m_held.clear();
    m_heldFrom = -1;
    return true;
}
