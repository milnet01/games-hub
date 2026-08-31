#include "pyramidtable.h"

#include "cards/cardcodec.h"

#include <algorithm>
#include <utility>

void PyramidTable::deal()
{
    std::vector<Card> deck = makeDeck(1, 4);
    shuffleCards(deck, m_rng);

    m_pyramid.clear();
    m_pyramid.reserve(kPyramidCards);
    for (int i = 0; i < kPyramidCards; ++i) {
        Card c = deck[std::size_t(i)];
        c.faceUp = true;
        m_pyramid.push_back({ c, false });
    }

    m_stock.assign(deck.begin() + kPyramidCards, deck.end());
    for (Card& c : m_stock)
        c.faceUp = false;

    m_waste.clear();
    m_history.clear();
    m_pairs = 0;
    m_redeals = 0;
}

bool PyramidTable::isExposed(int row, int index) const
{
    if (m_pyramid[std::size_t(slotIndex(row, index))].removed)
        return false;
    if (row == kRows - 1)
        return true;
    return m_pyramid[std::size_t(slotIndex(row + 1, index))].removed
        && m_pyramid[std::size_t(slotIndex(row + 1, index + 1))].removed;
}

bool PyramidTable::isExposed(int slot) const
{
    if (slot < 0 || slot >= int(m_pyramid.size()))
        return false;
    // Walk to the row this slot sits in. Seven rows, so a loop is cheaper to
    // read than the inverse triangular number and cannot round the wrong way.
    for (int row = 0; row < kRows; ++row) {
        const int first = slotIndex(row, 0);
        if (slot <= first + row)
            return isExposed(row, slot - first);
    }
    return false;
}

bool PyramidTable::available(Source source, int index) const
{
    switch (source) {
    case Source::Pyramid:
        return isExposed(index);
    case Source::Waste:
        // Only the top of the waste is reachable.
        return !m_waste.empty() && index == int(m_waste.size()) - 1;
    case Source::Stock:
        break;
    }
    return false;
}

Card PyramidTable::cardAt(Source source, int index) const
{
    if (source == Source::Pyramid && index >= 0 && index < int(m_pyramid.size()))
        return m_pyramid[std::size_t(index)].card;
    if (source == Source::Waste && index >= 0 && index < int(m_waste.size()))
        return m_waste[std::size_t(index)];
    return {};
}

void PyramidTable::pushUndo()
{
    m_history.push_back({ m_pyramid, m_stock, m_waste, m_pairs, m_redeals });
}

void PyramidTable::removeFrom(Source source, int index)
{
    if (source == Source::Pyramid)
        m_pyramid[std::size_t(index)].removed = true;
    else if (source == Source::Waste && !m_waste.empty())
        m_waste.pop_back();
}

bool PyramidTable::takeKing(Source source, int index)
{
    if (!available(source, index) || cardAt(source, index).rank != kKing)
        return false;
    pushUndo();
    removeFrom(source, index);
    ++m_pairs;
    return true;
}

bool PyramidTable::takePair(Source first, int firstIndex, Source second, int secondIndex)
{
    if (first == second && firstIndex == secondIndex)
        return false;
    if (!available(first, firstIndex) || !available(second, secondIndex))
        return false;
    if (cardAt(first, firstIndex).rank + cardAt(second, secondIndex).rank != kPairTotal)
        return false;

    pushUndo();
    // Two waste cards can never be a pair -- only the top one is available --
    // so there is no case here where removing one shifts the other's index.
    removeFrom(first, firstIndex);
    removeFrom(second, secondIndex);
    ++m_pairs;
    return true;
}

bool PyramidTable::drawFromStock()
{
    if (m_stock.empty()) {
        if (!redealsLeft() || m_waste.empty())
            return false;
        pushUndo();
        ++m_redeals;
        std::reverse(m_waste.begin(), m_waste.end());
        for (Card& c : m_waste)
            c.faceUp = false;
        m_stock = m_waste;
        m_waste.clear();
        return true;
    }

    pushUndo();
    Card c = m_stock.back();
    m_stock.pop_back();
    c.faceUp = true;
    m_waste.push_back(c);
    return true;
}

bool PyramidTable::cleared() const
{
    return std::all_of(m_pyramid.begin(), m_pyramid.end(),
                       [](const Slot& s) { return s.removed; });
}

void PyramidTable::undo()
{
    if (m_history.empty())
        return;
    // The snapshot is destroyed on the line below, so take its piles rather
    // than copying them. Copy-assigning here allocates three vectors and then
    // frees the originals a line later, every single undo.
    Snapshot& s = m_history.back();
    m_pyramid = std::move(s.pyramid);
    m_stock = std::move(s.stock);
    m_waste = std::move(s.waste);
    m_pairs = s.pairs;
    m_redeals = s.redeals;
    m_history.pop_back();
}

bool PyramidTable::restore(const std::vector<Slot>& pyramid, const std::vector<Card>& stock,
                           const std::vector<Card>& waste, int pairs, int redeals)
{
    if (int(pyramid.size()) != kPyramidCards)
        return false;
    if (pairs < 0 || pairs > 52 || redeals < 0 || redeals > kMaxRedeals)
        return false;

    // Pyramid takes matched pairs out of play, so the pack cannot come back
    // whole. What it can promise is that nothing here was ever outside it, and
    // that no card has turned up twice.
    std::vector<Card> all;
    all.reserve(pyramid.size() + stock.size() + waste.size());
    for (const Slot& s : pyramid)
        all.push_back(s.card);
    cardcodec::gather(all, stock);
    cardcodec::gather(all, waste);
    if (!cardcodec::fitsPack(all, 1, 4))
        return false;
    // fitsPack is a SUBSET test, so a short table passes it -- which is why
    // cardcodec.h asks a game that removes cards to pair it with a count of its
    // own, as Spider and Hearts do. Without it, 28 pyramid slots with an empty
    // stock and waste loses 24 cards in silence, and a table with every slot
    // removed and no pairs reports cleared() at once: a crafted save handing out
    // a win and a best score. A pair takes two cards, but a King goes alone and
    // counts as one all the same, so the exact total is not derivable from
    // `pairs` -- the bound is. Both shapes above are still refused: each is 52
    // cards short of anything this range allows.
    const int live = int(std::count_if(pyramid.begin(), pyramid.end(),
                                       [](const Slot& s) { return !s.removed; }))
        + int(stock.size()) + int(waste.size());
    if (live > 52 - pairs || live < 52 - 2 * pairs)
        return false;

    m_pyramid = pyramid;
    m_stock = stock;
    m_waste = waste;
    m_pairs = pairs;
    m_redeals = redeals;
    m_history.clear();
    return true;
}
