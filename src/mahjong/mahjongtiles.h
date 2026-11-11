#pragma once

#include <cstdint>
#include <random>
#include <utility>
#include <vector>

// The Mahjong tiles themselves, shared by Mahjong Solitaire (GHUB-0238) and
// the four-player table game (GHUB-0239). No Qt at all, so both rules cores
// can be checked without a display.
namespace mahjong {

// A tile's face. 0 is no tile. The three numbered suits run 1-9 each; then
// the four winds, the three dragons, the four flowers and the four seasons.
using Face = std::uint8_t;

enum class Suit : std::uint8_t { None, Circles, Bamboo, Characters, Wind, Dragon, Flower, Season };

constexpr Face kCirclesFirst = 1;
constexpr Face kBambooFirst = 10;
constexpr Face kCharactersFirst = 19;
constexpr Face kWindFirst = 28;   // east, south, west, north
constexpr Face kDragonFirst = 32; // red, green, white
constexpr Face kFlowerFirst = 35;
constexpr Face kSeasonFirst = 39; // spring, summer, autumn, winter
constexpr Face kLastFace = 42;

Suit suitOf(Face face);
// 1-9 in a numbered suit, 1-4 for a wind, flower or season, 1-3 for a dragon.
int rankOf(Face face);
// Two tiles match when they share a group. Every face is its own group,
// except that any flower matches any flower and any season any season.
int matchGroup(Face face);
inline bool matches(Face a, Face b)
{
    return a != 0 && b != 0 && matchGroup(a) == matchGroup(b);
}

// How many of one face the set holds: four of a suit tile, wind or dragon,
// one of each flower and season, none of anything else.
int copiesOf(Face face);

// The full set: four of every suit tile, wind and dragon, and one of each
// flower and season.
constexpr int kTiles = 144;
std::vector<Face> fullSet();

// An index below `n` from rng() directly, never a std distribution, so a seed
// deals the same tiles on every compiler (see shuffleCards in
// src/cards/card.cpp).
inline int pickIndex(std::mt19937& rng, std::size_t n)
{
    return int(rng() % n);
}

// Fisher-Yates over pickIndex, for the same reason.
template <typename T>
void shuffleInPlace(std::vector<T>& items, std::mt19937& rng)
{
    for (std::size_t i = items.size(); i > 1; --i)
        std::swap(items[i - 1], items[std::size_t(pickIndex(rng, i))]);
}

} // namespace mahjong
