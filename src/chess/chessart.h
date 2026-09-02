#pragma once

#include "chessboard.h"

#include <QColor>
#include <QPainter>
#include <QRectF>

// The chessmen, drawn rather than typed. The Unicode chess glyphs would be
// shorter but they come from whatever font the desktop happens to have, and
// half of those draw the white and black sets at different weights — so the
// pieces are painted, like every other picture in the collection.
namespace ChessArt {

// Draws one piece inside `box`, which should be roughly square. The silhouette
// is built in unit coordinates and scaled, so it is legible from a 24-pixel
// hub tile up to a full-window board.
// Hands the painter back as it found it, like CardArt. This one is shared with
// the hub tile, so a leaked pen or brush would land on two surfaces.
void paintPiece(QPainter& p, const QRectF& box, chess::PieceType type, chess::Colour colour);

// Board colours, shared with the hub tile so the miniature matches the game.
inline constexpr QColor kLightSquare { 0xe8, 0xdd, 0xbf };
inline constexpr QColor kDarkSquare { 0x58, 0x82, 0x58 };

} // namespace ChessArt
