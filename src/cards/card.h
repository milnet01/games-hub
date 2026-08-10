#pragma once

#include <QString>

#include <random>
#include <vector>

enum class Suit { Clubs, Diamonds, Hearts, Spades };

// Ranks are plain ints 1..13 (Ace low) because every game here does arithmetic
// on them — sequences, comparisons, scoring.
constexpr int kAce = 1;
constexpr int kJack = 11;
constexpr int kQueen = 12;
constexpr int kKing = 13;

// Rank 0 is the joker, which sorts below every real rank and so never collides
// with the arithmetic above. Only Canasta asks for jokers; every other game
// takes the default of none and so never sees one.
constexpr int kJoker = 0;

struct Card {
    Suit suit = Suit::Spades;
    int rank = kAce;
    bool faceUp = false;
    // Which physical pack this card came from, for games dealt from more than
    // one. It only decides the colour of the back, so two packs shuffled
    // together look the way they do on a real table. Deliberately outside
    // operator==: two red kings are the same card whichever pack they are from,
    // and every rule in every game here depends on that.
    int deck = 0;

    friend bool operator==(const Card& a, const Card& b)
    {
        return a.suit == b.suit && a.rank == b.rank;
    }
};

inline bool isRed(Suit s) { return s == Suit::Diamonds || s == Suit::Hearts; }
inline bool isRed(const Card& c) { return isRed(c.suit); }
inline bool isJoker(const Card& c) { return c.rank == kJoker; }

QString rankLabel(int rank);
QString suitSymbol(Suit s);

// A deck of `decks` × 52 cards, plus `jokers` jokers. `suitsUsed` limits how
// many distinct suits appear, which is how Spider sets its difficulty: one suit
// repeated is much easier than four. Jokers are dealt out red, black, red,
// black, so a Canasta pack gets the two of each a real double deck carries.
std::vector<Card> makeDeck(int decks = 1, int suitsUsed = 4, int jokers = 0);

void shuffleCards(std::vector<Card>& cards, std::mt19937& rng);
