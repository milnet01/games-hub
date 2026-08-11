#pragma once

#include "cards/card.h"

#include <QDataStream>

#include <array>
#include <cstddef>
#include <vector>

// Reading and writing piles of cards, shared by every game that saves the table
// rather than the moves that made it.
//
// Chess saves its move list and replays it, so the rules re-check every step on
// the way back in (CLAUDE.md § "A game's save is the moves that made it"). A
// solitaire keeps no move log, so its save is the piles themselves — and what
// stands in for that re-check is the pack: the cards that come back must be the
// cards that were dealt. fitsPack() and matchesPack() are that check.
namespace cardcodec {

// Two full packs is 104 cards, so no pile here can be longer than that even in
// the silliest position a game can reach. A length beyond it comes from a
// corrupt blob and must never be trusted into an allocation.
constexpr int kMaxPileSize = 128;

void writeCard(QDataStream& out, const Card& c);
bool readCard(QDataStream& in, Card& c);

void writePile(QDataStream& out, const std::vector<Card>& pile);
bool readPile(QDataStream& in, std::vector<Card>& pile);

// Only the reading half takes a whole array. A save is written a pile at a time
// because a run lifted in mid-drag has to be written back onto the pile it came
// from, and only the view knows which pile that is.
template <std::size_t N>
bool readPiles(QDataStream& in, std::array<std::vector<Card>, N>& piles)
{
    for (std::vector<Card>& pile : piles) {
        if (!readPile(in, pile))
            return false;
    }
    return true;
}

// Adds a pile's cards to `all`, for handing the lot to fitsPack / matchesPack.
void gather(std::vector<Card>& all, const std::vector<Card>& pile);

template <std::size_t N>
void gather(std::vector<Card>& all, const std::array<std::vector<Card>, N>& piles)
{
    for (const std::vector<Card>& pile : piles)
        gather(all, pile);
}

// True when every card in `present` came from the pack makeDeck(decks,
// suitsUsed) builds, none of them appearing more often than that pack holds it.
// This is the check for a game that takes cards out of play — Spider harvests a
// finished run, Pyramid takes pairs off the table — and so cannot ask for the
// whole pack back. Such a game pairs this with a count of its own.
bool fitsPack(std::vector<Card> present, int decks, int suitsUsed);

// True when `present` is that whole pack, exactly. Klondike and FreeCell never
// remove a card from play, so they get the stricter check.
bool matchesPack(const std::vector<Card>& present, int decks, int suitsUsed);

} // namespace cardcodec
