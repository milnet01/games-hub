#pragma once

#include "mahjongtable.h"

// The computer seats of four-player Mahjong (GHUB-0239 § 4.8). One strength.
// No randomness: a seat facing the same tiles makes the same choice, so a
// seeded hand plays out the same every time.
namespace mahjong {

// How many tiles `tiles` (concealed) plus `melds` sets are from a ready hand,
// counting four sets and a pair: -1 is a complete hand, 0 is one tile from
// it. Without chows, only pungs and pairs count -- the All pungs reckoning.
int tilesFromReady(const std::vector<Face>& tiles, int melds, bool chows = true);

// The move `seat` makes now: one of hand.legalMoves(seat). It always wins
// when it can, aims for a hand worth the minimum, and claims a chow only when
// that suits the hand it is building.
Move chooseMove(const Hand& hand, int seat);

} // namespace mahjong
