#include "mahjongtiles.h"

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

int copiesOf(Face face)
{
    const Suit s = suitOf(face);
    if (s == Suit::None)
        return 0;
    return s == Suit::Flower || s == Suit::Season ? 1 : 4;
}

std::vector<Face> fullSet()
{
    std::vector<Face> set;
    set.reserve(kTiles);
    for (Face f = 1; f <= kLastFace; ++f)
        for (int i = 0; i < copiesOf(f); ++i)
            set.push_back(f);
    return set;
}

} // namespace mahjong
