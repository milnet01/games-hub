#pragma once

#include "mahjongtiles.h"

#include <QColor>
#include <QFont>
#include <QRectF>
#include <QString>

class QPainter;

// The tile art both Mahjong games draw with (GHUB-0238, shared by GHUB-0239).
// The view half: it paints, and it holds the words for a tile.
namespace mahjong {

// The ink for each suit: blue circles, green bamboo, red diamonds. Each suit
// also has its own SHAPE, so colour is never the only thing telling them apart.
inline constexpr QColor kCircleInk { 0x1d, 0x4f, 0xb0 };
inline constexpr QColor kBambooInk { 0x1a, 0x7a, 0x38 };
inline constexpr QColor kDiamondInk { 0xc0, 0x22, 0x22 };
inline constexpr QColor kWindInk { 0x1d, 0x2b, 0x4f };
// The ivory face, top to bottom, and the body under it.
inline constexpr QColor kFaceTop { 0xfb, 0xf7, 0xea };
inline constexpr QColor kFaceBottom { 0xec, 0xe2, 0xc6 };
inline constexpr QColor kTileBack { 0x2b, 0x7a, 0x5a };
inline constexpr QColor kTileBackDark { 0x17, 0x4a, 0x36 };
inline constexpr QColor kTileEdge { 0xd6, 0xca, 0xa8 };

// Draws one tile's markings -- the pips, the numeral, the wind, the dragon, the
// flower or season and its band -- on the face `face`. The ivory face itself,
// the body, the edge, the shadow and any highlight stay with the caller, which
// is what lets Solitaire colour a picked tile's face. `base` is the caller's
// widget font, which the numeral and the wind letter are drawn in.
void paintTileFace(QPainter& p, const QRectF& face, Face tile, bool legible, const QFont& base);

// With large play on, no face-up tile in either Mahjong game is drawn
// narrower than this (GHUB-0248, the owner's ruling of 2026-11-12). Each game
// raises its smallest window under the switch until its tiles clear it.
inline constexpr double kTileMinWidth = 40.0;

// The tile in words, for the table's sentences ("Five of Bamboo").
QString tileName(Face tile);

} // namespace mahjong
