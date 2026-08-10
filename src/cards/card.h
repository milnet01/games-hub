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

struct Card {
    Suit suit = Suit::Spades;
    int rank = kAce;
    bool faceUp = false;

    friend bool operator==(const Card& a, const Card& b)
    {
        return a.suit == b.suit && a.rank == b.rank;
    }
};

inline bool isRed(Suit s) { return s == Suit::Diamonds || s == Suit::Hearts; }
inline bool isRed(const Card& c) { return isRed(c.suit); }

QString rankLabel(int rank);
QString suitSymbol(Suit s);

// A deck of `decks` × 52 cards. `suitsUsed` limits how many distinct suits
// appear, which is how Spider sets its difficulty: one suit repeated is much
// easier than four.
std::vector<Card> makeDeck(int decks = 1, int suitsUsed = 4);

void shuffleCards(std::vector<Card>& cards, std::mt19937& rng);
