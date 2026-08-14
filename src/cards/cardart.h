#pragma once

#include "card.h"

#include <QPainter>
#include <QRectF>

// Shared card drawing for Klondike, Spider and Hearts, so the three games look
// like one deck rather than three.
namespace CardArt {

// Playing-card proportion: height = width * 1.4.
constexpr double kAspect = 1.4;

// Below this width paintFace draws only the corner index — the pips and court
// letters are dropped, because they are unreadable smaller. A game that lays
// cards out must not go below it while Legibility is on, and must measure that
// against the SMALLEST scale it draws a card at: Canasta's melds are drawn at
// 0.74, so a meld needs cardWidth() >= 46 / 0.74 to show a face at all.
//
// This is the only definition of the number; scripts/legibility-check.py
// --thresholds fails if any other source states it as a literal.
inline constexpr double kFaceMinWidth = 46.0;

void paintFace(QPainter& p, const QRectF& r, const Card& c);

// `deck` picks the colourway: 0 is blue, 1 is red. Games dealt from a single
// pack take the default and are all blue; Canasta shuffles two packs together,
// so its stock shows both backs mixed, the way a real table does.
void paintBack(QPainter& p, const QRectF& r, int deck = 0);

// Dashed outline for an empty pile.
void paintSlot(QPainter& p, const QRectF& r, const QString& glyph = {});

// Ring drawn around a selected or highlighted card.
void paintHighlight(QPainter& p, const QRectF& r, const QColor& colour);

} // namespace CardArt
