#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <random>
#include <utility>
#include <vector>

// Mahjong solitaire: the heap, the tiles and the rule that says which may be
// taken (GHUB-0238). No Qt at all, so the self-test can clear whole heaps
// without a display.
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

// The full set: four of every suit tile, wind and dragon, and one of each
// flower and season.
constexpr int kTiles = 144;
std::vector<Face> fullSet();

// Where a tile lies, in HALF-tile units, so a tile on an upper layer can sit
// across two below it. A tile covers x..x+1 and y..y+1 on its layer z.
struct Slot {
    int x = 0;
    int y = 0;
    int z = 0;
};

// The heap's shape: kTiles slots in drawing order (layer, then row, then
// column), which is also the order a save stores them in.
const std::vector<Slot>& layout();
// The layout's extent in half-tile units, and its number of layers.
int layoutWidth();
int layoutHeight();
int layoutLayers();

using Pair = std::pair<int, int>;

class Board
{
public:
    // A fresh heap that can always be cleared: built backwards from an empty
    // table, so solution() clears it.
    void deal(std::mt19937& rng);

    Face at(int slot) const { return m_faces[std::size_t(slot)]; }
    const std::array<Face, kTiles>& faces() const { return m_faces; }
    static bool inBounds(int slot) { return slot >= 0 && slot < kTiles; }
    bool present(int slot) const { return inBounds(slot) && at(slot) != 0; }
    int remaining() const;
    bool cleared() const { return remaining() == 0; }

    // Nothing lies on it.
    bool uncovered(int slot) const;
    // Its left side or its right side is open.
    bool sideOpen(int slot) const;
    // Free to take: there, uncovered, and with a side open.
    bool isFree(int slot) const { return present(slot) && uncovered(slot) && sideOpen(slot); }

    // Takes two free tiles that match. Refuses anything else and leaves the
    // board as it was.
    bool take(int a, int b);

    // Every pair that could be taken now.
    std::vector<Pair> freePairs() const;
    // One of them, the same one every time for one board, or nothing.
    std::optional<Pair> hint() const;

    // Deals the tiles still in play back onto the places still filled, so the
    // heap can again be cleared. False, with the board unchanged, when no deal
    // of them can be: two tiles left, one lying on the other, for instance.
    bool reshuffle(std::mt19937& rng);

    // One way to clear the heap from where deal() or reshuffle() left it, as
    // the pairs in the order to take them. Stale once anything else is taken.
    const std::vector<Pair>& solution() const { return m_solution; }

    // Takes a whole board, for a save. Refuses one no game could reach: a face
    // that is not one, more of a tile than the set holds, a group with an odd
    // count left (its partner is gone), or a tile lying on an empty place,
    // since what lies on a tile is taken first. The board is left as it was
    // on a refusal.
    bool restore(const std::array<Face, kTiles>& faces);

private:
    // Puts `tiles` on `slots` so they can be taken in pairs, recording the
    // order in m_solution. Leaves the board alone and returns false if it
    // cannot.
    bool fill(const std::vector<int>& slots, const std::vector<Face>& tiles, std::mt19937& rng);

    std::array<Face, kTiles> m_faces {};
    std::vector<Pair> m_solution;
};

} // namespace mahjong
