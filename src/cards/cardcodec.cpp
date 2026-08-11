#include "cards/cardcodec.h"

#include <QIODevice>

#include <algorithm>

namespace {

// A canonical order, so two collections of cards can be compared as multisets.
// Suit and rank only, matching Card::operator==: which pack a card came from
// decides the colour of its back and nothing else.
bool byOrder(const Card& a, const Card& b)
{
    return a.suit != b.suit ? a.suit < b.suit : a.rank < b.rank;
}

} // namespace

namespace cardcodec {

void writeCard(QDataStream& out, const Card& c)
{
    out << qint8(int(c.suit)) << qint8(c.rank) << qint8(c.faceUp ? 1 : 0) << qint8(c.deck);
}

bool readCard(QDataStream& in, Card& c)
{
    qint8 suit = 0;
    qint8 rank = 0;
    qint8 faceUp = 0;
    qint8 deck = 0;
    in >> suit >> rank >> faceUp >> deck;
    // Every field is checked before it becomes a Card: a suit outside the four
    // would index past the end of any switch on it, and a rank outside the pack
    // would break the arithmetic every game does on ranks.
    if (in.status() != QDataStream::Ok || suit < 0 || suit > 3 || rank < kJoker || rank > kKing
        || deck < 0 || deck > 7)
        return false;

    c.suit = Suit(suit);
    c.rank = int(rank);
    c.faceUp = faceUp != 0;
    c.deck = int(deck);
    return true;
}

void writePile(QDataStream& out, const std::vector<Card>& pile)
{
    out << qint32(pile.size());
    for (const Card& c : pile)
        writeCard(out, c);
}

bool readPile(QDataStream& in, std::vector<Card>& pile)
{
    qint32 count = 0;
    in >> count;
    if (in.status() != QDataStream::Ok || count < 0 || count > kMaxPileSize)
        return false;

    pile.clear();
    pile.reserve(std::size_t(count));
    for (qint32 i = 0; i < count; ++i) {
        Card c;
        if (!readCard(in, c))
            return false;
        pile.push_back(c);
    }
    return true;
}

void gather(std::vector<Card>& all, const std::vector<Card>& pile)
{
    all.insert(all.end(), pile.begin(), pile.end());
}

bool fitsPack(std::vector<Card> present, int decks, int suitsUsed)
{
    std::vector<Card> pack = makeDeck(decks, suitsUsed);
    if (present.size() > pack.size())
        return false;

    std::sort(present.begin(), present.end(), byOrder);
    std::sort(pack.begin(), pack.end(), byOrder);
    return std::includes(pack.begin(), pack.end(), present.begin(), present.end(), byOrder);
}

bool matchesPack(const std::vector<Card>& present, int decks, int suitsUsed)
{
    // No solitaire here deals jokers, so a whole pack is 52 cards per deck.
    return present.size() == std::size_t(decks) * 52 && fitsPack(present, decks, suitsUsed);
}

} // namespace cardcodec
