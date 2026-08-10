#include "card.h"

#include <algorithm>

QString rankLabel(int rank)
{
    switch (rank) {
    case kJoker: return QStringLiteral("★");
    case kAce:   return QStringLiteral("A");
    case kJack:  return QStringLiteral("J");
    case kQueen: return QStringLiteral("Q");
    case kKing:  return QStringLiteral("K");
    default:     return QString::number(rank);
    }
}

QString suitSymbol(Suit s)
{
    switch (s) {
    case Suit::Clubs:    return QStringLiteral("♣");
    case Suit::Diamonds: return QStringLiteral("♦");
    case Suit::Hearts:   return QStringLiteral("♥");
    case Suit::Spades:   return QStringLiteral("♠");
    }
    return {};
}

std::vector<Card> makeDeck(int decks, int suitsUsed, int jokers)
{
    // Spades, then Hearts, then Diamonds, then Clubs, so a one-suit Spider
    // deals the traditional all-spades pack.
    static constexpr Suit kOrder[4] = { Suit::Spades, Suit::Hearts, Suit::Diamonds, Suit::Clubs };
    const int used = std::clamp(suitsUsed, 1, 4);

    std::vector<Card> cards;
    cards.reserve(std::size_t(decks) * 52 + std::size_t(std::max(0, jokers)));
    for (int d = 0; d < decks; ++d)
        for (int s = 0; s < 4; ++s)
            for (int r = kAce; r <= kKing; ++r)
                cards.push_back(Card { kOrder[s % used], r, false, d });

    // A joker has no suit of its own; the suit here only decides whether it is
    // printed red or black, and alternating gives an even split. Two per pack,
    // which is what a real pack carries.
    for (int j = 0; j < jokers; ++j)
        cards.push_back(Card { (j % 2 == 0) ? Suit::Hearts : Suit::Spades, kJoker, false,
                               (j / 2) % std::max(1, decks) });
    return cards;
}

void shuffleCards(std::vector<Card>& cards, std::mt19937& rng)
{
    std::shuffle(cards.begin(), cards.end(), rng);
}
