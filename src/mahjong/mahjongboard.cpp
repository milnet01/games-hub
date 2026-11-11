#include "mahjongboard.h"

#include <algorithm>
#include <cstdlib>

namespace mahjong {

Suit suitOf(Face face)
{
    if (face == 0 || face > kLastFace)
        return Suit::None;
    if (face < kBambooFirst)
        return Suit::Circles;
    if (face < kCharactersFirst)
        return Suit::Bamboo;
    if (face < kWindFirst)
        return Suit::Characters;
    if (face < kDragonFirst)
        return Suit::Wind;
    if (face < kFlowerFirst)
        return Suit::Dragon;
    if (face < kSeasonFirst)
        return Suit::Flower;
    return Suit::Season;
}

int rankOf(Face face)
{
    switch (suitOf(face)) {
    case Suit::Circles: return face - kCirclesFirst + 1;
    case Suit::Bamboo: return face - kBambooFirst + 1;
    case Suit::Characters: return face - kCharactersFirst + 1;
    case Suit::Wind: return face - kWindFirst + 1;
    case Suit::Dragon: return face - kDragonFirst + 1;
    case Suit::Flower: return face - kFlowerFirst + 1;
    case Suit::Season: return face - kSeasonFirst + 1;
    case Suit::None: break;
    }
    return 0;
}

int matchGroup(Face face)
{
    switch (suitOf(face)) {
    case Suit::Flower: return kFlowerFirst;
    case Suit::Season: return kSeasonFirst;
    case Suit::None: return 0;
    default: return face;
    }
}

namespace {

// How many of one face the set holds.
int copiesOf(Face face)
{
    const Suit s = suitOf(face);
    if (s == Suit::None)
        return 0;
    return s == Suit::Flower || s == Suit::Season ? 1 : 4;
}

// An index from rng() directly, never a std distribution, so a seed deals the
// same heap on every compiler (see shuffleCards in src/cards/card.cpp).
int pick(std::mt19937& rng, std::size_t n)
{
    return int(rng() % n);
}

template <typename T>
void shuffle(std::vector<T>& items, std::mt19937& rng)
{
    for (std::size_t i = items.size(); i > 1; --i)
        std::swap(items[i - 1], items[std::size_t(pick(rng, i))]);
}

// The heap: four layers, each centred on the one below. The third layer sits
// half a tile down, across two rows of the second, which is what the
// half-tile units are for.
std::vector<Slot> buildLayout()
{
    struct Layer {
        int columns, rows, x, y;
    };
    const Layer layers[] = {
        { 12, 6, 0, 0 },
        { 10, 4, 2, 2 },
        { 8, 3, 4, 3 },
        { 4, 2, 8, 4 },
    };
    std::vector<Slot> slots;
    int z = 0;
    for (const Layer& layer : layers) {
        for (int row = 0; row < layer.rows; ++row)
            for (int col = 0; col < layer.columns; ++col)
                slots.push_back({ layer.x + 2 * col, layer.y + 2 * row, z });
        ++z;
    }
    return slots;
}

// Which slots touch which, worked out once: the tiles lying on each, the
// tiles it lies on, and its neighbours either side on its own layer.
struct Neighbours {
    std::vector<int> above, below, left, right;
};

const std::vector<Neighbours>& neighbours()
{
    static const std::vector<Neighbours> table = [] {
        const std::vector<Slot>& slots = layout();
        std::vector<Neighbours> out(slots.size());
        for (std::size_t i = 0; i < slots.size(); ++i)
            for (std::size_t j = 0; j < slots.size(); ++j) {
                const Slot& a = slots[i];
                const Slot& b = slots[j];
                const bool overlapX = std::abs(a.x - b.x) < 2;
                const bool overlapY = std::abs(a.y - b.y) < 2;
                if (b.z == a.z + 1 && overlapX && overlapY)
                    out[i].above.push_back(int(j));
                else if (b.z == a.z - 1 && overlapX && overlapY)
                    out[i].below.push_back(int(j));
                else if (b.z == a.z && overlapY && b.x == a.x - 2)
                    out[i].left.push_back(int(j));
                else if (b.z == a.z && overlapY && b.x == a.x + 2)
                    out[i].right.push_back(int(j));
            }
        return out;
    }();
    return table;
}

using Occupied = std::array<bool, kTiles>;

bool anyOf(const std::vector<int>& slots, const Occupied& occupied)
{
    return std::any_of(slots.begin(), slots.end(),
                       [&](int s) { return occupied[std::size_t(s)]; });
}

bool freeIn(int slot, const Occupied& occupied)
{
    const Neighbours& n = neighbours()[std::size_t(slot)];
    return occupied[std::size_t(slot)] && !anyOf(n.above, occupied)
        && (!anyOf(n.left, occupied) || !anyOf(n.right, occupied));
}

Occupied occupiedBy(const std::array<Face, kTiles>& faces)
{
    Occupied occupied {};
    for (int i = 0; i < kTiles; ++i)
        occupied[std::size_t(i)] = faces[std::size_t(i)] != 0;
    return occupied;
}

} // namespace

std::vector<Face> fullSet()
{
    std::vector<Face> set;
    set.reserve(kTiles);
    for (Face f = 1; f <= kLastFace; ++f)
        for (int i = 0; i < copiesOf(f); ++i)
            set.push_back(f);
    return set;
}

const std::vector<Slot>& layout()
{
    static const std::vector<Slot> slots = buildLayout();
    return slots;
}

int layoutWidth()
{
    int w = 0;
    for (const Slot& s : layout())
        w = std::max(w, s.x + 2);
    return w;
}

int layoutHeight()
{
    int h = 0;
    for (const Slot& s : layout())
        h = std::max(h, s.y + 2);
    return h;
}

int layoutLayers()
{
    int z = 0;
    for (const Slot& s : layout())
        z = std::max(z, s.z + 1);
    return z;
}

void Board::deal(std::mt19937& rng)
{
    std::vector<int> all(kTiles);
    for (int i = 0; i < kTiles; ++i)
        all[std::size_t(i)] = i;
    m_faces.fill(0);
    // A full heap always fills within a handful of tries; the loop is the
    // retry fill() leaves to its caller.
    while (!fill(all, fullSet(), rng)) { }
}

int Board::remaining() const
{
    return int(std::count_if(m_faces.begin(), m_faces.end(), [](Face f) { return f != 0; }));
}

bool Board::uncovered(int slot) const
{
    return !anyOf(neighbours()[std::size_t(slot)].above, occupiedBy(m_faces));
}

bool Board::sideOpen(int slot) const
{
    const Occupied occupied = occupiedBy(m_faces);
    const Neighbours& n = neighbours()[std::size_t(slot)];
    return !anyOf(n.left, occupied) || !anyOf(n.right, occupied);
}

bool Board::take(int a, int b)
{
    if (a == b || !isFree(a) || !isFree(b) || !matches(at(a), at(b)))
        return false;
    m_faces[std::size_t(a)] = 0;
    m_faces[std::size_t(b)] = 0;
    return true;
}

std::vector<Pair> Board::freePairs() const
{
    std::vector<int> open;
    for (int i = 0; i < kTiles; ++i)
        if (isFree(i))
            open.push_back(i);
    std::vector<Pair> pairs;
    for (std::size_t i = 0; i < open.size(); ++i)
        for (std::size_t j = i + 1; j < open.size(); ++j)
            if (matches(at(open[i]), at(open[j])))
                pairs.emplace_back(open[i], open[j]);
    return pairs;
}

std::optional<Pair> Board::hint() const
{
    const std::vector<Pair> pairs = freePairs();
    if (pairs.empty())
        return std::nullopt;
    return pairs.front();
}

bool Board::reshuffle(std::mt19937& rng)
{
    std::vector<int> slots;
    std::vector<Face> tiles;
    for (int i = 0; i < kTiles; ++i)
        if (present(i)) {
            slots.push_back(i);
            tiles.push_back(at(i));
        }
    // A sparse heap can be impossible to deal, so the tries are bounded.
    for (int attempt = 0; attempt < 200; ++attempt)
        if (fill(slots, tiles, rng))
            return true;
    return false;
}

bool Board::fill(const std::vector<int>& slots, const std::vector<Face>& tiles,
                 std::mt19937& rng)
{
    // The tiles into matching pairs: shuffled within each group, then the
    // pairs shuffled among themselves.
    std::vector<std::vector<Face>> groups(kLastFace + 1);
    for (Face f : tiles)
        groups[std::size_t(matchGroup(f))].push_back(f);
    std::vector<std::pair<Face, Face>> pairs;
    for (std::vector<Face>& group : groups) {
        if (group.size() % 2 != 0)
            return false;
        shuffle(group, rng);
        for (std::size_t i = 0; i < group.size(); i += 2)
            pairs.emplace_back(group[i], group[i + 1]);
    }
    shuffle(pairs, rng);

    // Then play the empty places forward: take any two that are free, and the
    // next pair is what lies there. Taken in the same order, the heap clears.
    Occupied occupied {};
    for (int s : slots)
        occupied[std::size_t(s)] = true;
    std::array<Face, kTiles> faces = m_faces;
    std::vector<Pair> order;
    for (const auto& [first, second] : pairs) {
        std::vector<int> open;
        for (int s : slots)
            if (freeIn(s, occupied))
                open.push_back(s);
        if (open.size() < 2)
            return false;
        const int i = pick(rng, open.size());
        int j = pick(rng, open.size() - 1);
        if (j >= i)
            ++j;
        const int a = open[std::size_t(i)];
        const int b = open[std::size_t(j)];
        faces[std::size_t(a)] = first;
        faces[std::size_t(b)] = second;
        occupied[std::size_t(a)] = false;
        occupied[std::size_t(b)] = false;
        order.emplace_back(a, b);
    }
    m_faces = faces;
    m_solution = std::move(order);
    return true;
}

bool Board::restore(const std::array<Face, kTiles>& faces)
{
    std::array<int, kLastFace + 1> count {};
    for (Face f : faces) {
        if (f > kLastFace)
            return false;
        if (f != 0 && ++count[f] > copiesOf(f))
            return false;
    }
    std::array<int, kLastFace + 1> group {};
    for (Face f = 1; f <= kLastFace; ++f)
        group[std::size_t(matchGroup(f))] += count[f];
    if (std::any_of(group.begin(), group.end(), [](int n) { return n % 2 != 0; }))
        return false;

    const Occupied occupied = occupiedBy(faces);
    for (int i = 0; i < kTiles; ++i)
        if (occupied[std::size_t(i)])
            for (int under : neighbours()[std::size_t(i)].below)
                if (!occupied[std::size_t(under)])
                    return false;

    m_faces = faces;
    m_solution.clear();
    return true;
}

} // namespace mahjong
