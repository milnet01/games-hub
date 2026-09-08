#pragma once

#include "card.h"

#include <QPainter>
#include <QFont>
#include <QRectF>

// Shared card drawing for every card game here, so they all look like one
// deck rather than several.
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

// Every function here hands the painter back exactly as it found it: pen,
// brush, font and transform. That is a POSTCONDITION callers may rely on, and
// it was not always true -- paintFace has two paths, and a cached blit touched
// nothing while the live fallback leaked the last font and ink pen, so a caller
// that worked broke as soon as a card was rotated or the window grew past the
// cache's size limit.
void paintFace(QPainter& p, const QRectF& r, const Card& c);

// Clear space, in pixels, between the corner index and the nearest pip under
// it. Negative means they overlap, which is what a ten did until GHUB-0188:
// the index column ends at 0.32 of the card and the left pip column is CENTRED
// there, so a two-character rank met it whatever the font did.
//
// paintFace places the index through the same call, so this cannot drift from
// what is drawn -- and that is the point. No rendered picture answers "does it
// still clear at a different card size", because a picture is one size.
// A card that draws no pips answers with the card's width, meaning nothing to
// clear.
double indexPipGap(const Card& c, const QRectF& r, const QFont& base);

// How far the corner numeral's ink runs PAST the room it is allowed. Zero or
// less is correct. This is the half a test may assert: the room is a fraction
// of the card and the solve is this code's, so the answer is a property of the
// code on every platform. indexPipGap() above is the half a test may only
// REPORT -- it measures a suit glyph, and a runner with an empty font database
// has no opinion worth asserting on.
double indexOverflow(const Card& c, const QRectF& r, const QFont& base);

// `deck` picks the colourway: 0 is blue, 1 is red. Games dealt from a single
// pack take the default and are all blue; Canasta shuffles two packs together,
// so its stock shows both backs mixed, the way a real table does.
void paintBack(QPainter& p, const QRectF& r, int deck = 0);

// Dashed outline for an empty pile.
void paintSlot(QPainter& p, const QRectF& r, const QString& glyph = {});

// The point size paintSlot will draw `label` at inside `r`, and the width that
// label then takes. Exposed so a test can ask whether a word still fits the
// narrowest slot a game draws -- drawText clips to the rect, so a label that
// does not fit is a stroke of a word and looks like a rendering fault rather
// than a label that is too long.
double slotLabelPointSize(const QFont& base, const QRectF& r, const QString& label);
double slotLabelWidth(const QFont& base, const QRectF& r, const QString& label);

// Ring drawn around a selected or highlighted card.
void paintHighlight(QPainter& p, const QRectF& r, const QColor& colour);

} // namespace CardArt
