#pragma once

#include "card.h"

#include <QPainter>
#include <QRectF>

// Shared card drawing for Klondike, Spider and Hearts, so the three games look
// like one deck rather than three.
namespace CardArt {

// Playing-card proportion: height = width * 1.4.
constexpr double kAspect = 1.4;

void paintFace(QPainter& p, const QRectF& r, const Card& c);
void paintBack(QPainter& p, const QRectF& r);

// Dashed outline for an empty pile.
void paintSlot(QPainter& p, const QRectF& r, const QString& glyph = {});

// Ring drawn around a selected or highlighted card.
void paintHighlight(QPainter& p, const QRectF& r, const QColor& colour);

} // namespace CardArt
