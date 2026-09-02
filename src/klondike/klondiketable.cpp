#include "klondiketable.h"

#include "cards/cardcodec.h"

#include <algorithm>
#include <utility>

namespace {
constexpr std::size_t kMaxHistory = 200;
}

void KlondikeTable::deal()
{
    std::vector<Card> deck = makeDeck(1, 4);
    shuffleCards(deck, m_rng);

    for (std::vector<Card>& f : m_foundations)
        f.clear();
    for (std::vector<Card>& t : m_tableau)
        t.clear();
    m_waste.clear();
    m_stock.clear();
    m_held.clear();
    m_history.clear();
    m_score = 0;

    // Columns get 1..7 cards, only the last of each face up.
    std::size_t next = 0;
    for (int col = 0; col < kColumns; ++col) {
        for (int i = 0; i <= col; ++i) {
            Card c = deck[next++];
            c.faceUp = (i == col);
            m_tableau[std::size_t(col)].push_back(c);
        }
    }
    m_stock.assign(deck.begin() + next, deck.end());
    for (Card& c : m_stock)
        c.faceUp = false;
}

std::vector<Card>& KlondikeTable::pileAt(PileKind kind, int index)
{
    switch (kind) {
    case PileKind::Stock: return m_stock;
    case PileKind::Waste: return m_waste;
    case PileKind::Foundation: return m_foundations[std::size_t(index)];
    case PileKind::Tableau: return m_tableau[std::size_t(index)];
    }
    return m_stock;
}

const std::vector<Card>& KlondikeTable::pile(PileKind kind, int index) const
{
    return const_cast<KlondikeTable*>(this)->pileAt(kind, index);
}

bool KlondikeTable::won() const
{
    int total = 0;
    for (const std::vector<Card>& f : m_foundations)
        total += int(f.size());
    return total == kPackSize;
}

void KlondikeTable::setDrawCount(int count)
{
    if (count == 1 || count == 3)
        m_drawCount = count;
}

bool KlondikeTable::canStackOnTableau(const Card& moving, int column) const
{
    const std::vector<Card>& target = m_tableau[std::size_t(column)];
    // An empty column takes a King and nothing else.
    if (target.empty())
        return moving.rank == kKing;
    const Card& top = target.back();
    if (!top.faceUp)
        return false;
    return isRed(top) != isRed(moving) && top.rank == moving.rank + 1;
}

bool KlondikeTable::canPlaceOnFoundation(const Card& moving, int foundation) const
{
    const std::vector<Card>& target = m_foundations[std::size_t(foundation)];
    if (target.empty())
        return moving.rank == kAce;
    return target.back().suit == moving.suit && target.back().rank + 1 == moving.rank;
}

bool KlondikeTable::canLift(PileKind kind, int index, int from) const
{
    const std::vector<Card>& source = pile(kind, index);
    if (from < 0 || from >= int(source.size()))
        return false;
    if (kind == PileKind::Stock)
        return false;
    if (kind != PileKind::Tableau)
        return from == int(source.size()) - 1;
    // A run only moves from a face-up card down, and everything under it in
    // the column is face up by construction.
    return source[std::size_t(from)].faceUp;
}

void KlondikeTable::pushUndo()
{
    m_history.push_back({ m_stock, m_waste, m_foundations, m_tableau, m_score });
    if (m_history.size() > kMaxHistory)
        m_history.pop_front();
}

void KlondikeTable::dropUndo()
{
    if (!m_history.empty())
        m_history.pop_back();
}

std::vector<Card> KlondikeTable::lift(PileKind kind, int index, int from)
{
    if (holding() || !canLift(kind, index, from))
        return {};

    pushUndo();
    std::vector<Card>& source = pileAt(kind, index);
    m_held.assign(source.begin() + from, source.end());
    source.erase(source.begin() + from, source.end());
    m_heldFrom = kind;
    m_heldPile = index;
    return m_held;
}

void KlondikeTable::putBack()
{
    if (!holding())
        return;
    std::vector<Card>& source = pileAt(m_heldFrom, m_heldPile);
    for (const Card& c : m_held)
        source.push_back(c);
    m_held.clear();
    dropUndo();
}

void KlondikeTable::revealSource()
{
    if (m_heldFrom != PileKind::Tableau)
        return;
    std::vector<Card>& source = pileAt(m_heldFrom, m_heldPile);
    if (source.empty() || source.back().faceUp)
        return;
    source.back().faceUp = true;
    m_score += kScoreTurnUp;
}

bool KlondikeTable::dropOnFoundation(int foundation)
{
    if (m_held.size() != 1 || !canPlaceOnFoundation(m_held.front(), foundation))
        return false;
    m_foundations[std::size_t(foundation)].push_back(m_held.front());
    m_score += kScoreToFoundation;
    m_held.clear();
    revealSource();
    return true;
}

bool KlondikeTable::dropOnTableau(int column)
{
    // A run put straight back where it came from is not a move. Spider refuses
    // this and Klondike did not: the drop scored kScoreTableauMove and kept the
    // undo snapshot lift() had banked, so repeating it inflated the stored top
    // score and pushed real states out of the 200-deep history. Refusing sends
    // the view down its putBack() path, which restores the cards AND drops the
    // snapshot -- the route already there for changing your mind.
    if (m_heldFrom == PileKind::Tableau && m_heldPile == column)
        return false;
    if (m_held.empty() || !canStackOnTableau(m_held.front(), column))
        return false;
    for (const Card& c : m_held)
        m_tableau[std::size_t(column)].push_back(c);
    m_score += kScoreTableauMove;
    m_held.clear();
    revealSource();
    return true;
}

bool KlondikeTable::sendToFoundation(PileKind kind, int index)
{
    // Never FROM a foundation. A lone Ace is legal on any empty foundation, so
    // without this a double-click on one moved it to the next empty pile,
    // scored kScoreToFoundation, and could be repeated for as long as you liked.
    if (kind == PileKind::Foundation)
        return false;

    std::vector<Card>& source = pileAt(kind, index);
    if (source.empty() || !source.back().faceUp)
        return false;

    for (int f = 0; f < kFoundations; ++f) {
        if (!canPlaceOnFoundation(source.back(), f))
            continue;
        pushUndo();
        m_foundations[std::size_t(f)].push_back(source.back());
        source.pop_back();
        m_score += kScoreToFoundation;
        if (kind == PileKind::Tableau && !source.empty() && !source.back().faceUp) {
            source.back().faceUp = true;
            m_score += kScoreTurnUp;
        }
        return true;
    }
    return false;
}

bool KlondikeTable::autoFinishStep()
{
    for (int col = 0; col < kColumns; ++col) {
        if (sendToFoundation(PileKind::Tableau, col))
            return true;
    }
    return sendToFoundation(PileKind::Waste, 0);
}

void KlondikeTable::dealFromStock()
{
    // Nothing to turn and nothing to recycle, so nothing to undo. The snapshot
    // was banked before this was known, so clicking an exhausted stock over an
    // empty waste filled the history with states identical to the one before
    // them and pushed the real ones out.
    if (m_stock.empty() && m_waste.empty())
        return;

    pushUndo();
    if (m_stock.empty()) {
        // Recycle: the waste goes back under the stock, face down, in order.
        std::reverse(m_waste.begin(), m_waste.end());
        for (Card& c : m_waste)
            c.faceUp = false;
        m_stock = m_waste;
        m_waste.clear();
        return;
    }
    for (int i = 0; i < m_drawCount && !m_stock.empty(); ++i) {
        Card c = m_stock.back();
        m_stock.pop_back();
        c.faceUp = true;
        m_waste.push_back(c);
    }
}

void KlondikeTable::undo()
{
    if (m_history.empty())
        return;
    // The snapshot is destroyed on the line below, so take its piles rather
    // than copying them. Copy-assigning here allocates thirteen vectors and
    // then frees the originals a line later, every single undo.
    Snapshot& s = m_history.back();
    m_stock = std::move(s.stock);
    m_waste = std::move(s.waste);
    m_foundations = std::move(s.foundations);
    m_tableau = std::move(s.tableau);
    m_score = s.score;
    m_history.pop_back();
    m_held.clear();
}

bool KlondikeTable::restore(const std::vector<Card>& stock, const std::vector<Card>& waste,
                            const std::array<std::vector<Card>, kFoundations>& foundations,
                            const std::array<std::vector<Card>, kColumns>& tableau, int drawCount,
                            int score)
{
    if ((drawCount != 1 && drawCount != 3) || score < 0)
        return false;

    // Klondike never takes a card out of play, so the whole pack must come
    // back: nothing missing and nothing doubled.
    std::vector<Card> all;
    all.reserve(kPackSize);
    cardcodec::gather(all, stock);
    cardcodec::gather(all, waste);
    cardcodec::gather(all, foundations);
    cardcodec::gather(all, tableau);
    if (!cardcodec::matchesPack(all, 1, 4))
        return false;

    m_stock = stock;
    m_waste = waste;
    m_foundations = foundations;
    m_tableau = tableau;
    m_drawCount = drawCount;
    m_score = score;
    m_history.clear();
    m_held.clear();
    return true;
}
