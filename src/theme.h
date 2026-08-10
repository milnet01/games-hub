#pragma once

#include <QColor>
#include <QPainter>
#include <QRectF>

// Shared materials. Reversi set the house style — felt under a wooden frame,
// with pieces that carry a contact shadow — and these helpers are how the rest
// of the collection speaks the same visual language.
namespace Theme {

// Felt colours per game, so the tables are distinguishable but related.
inline constexpr QColor kFeltGreenTop { 0x23, 0x84, 0x55 };
inline constexpr QColor kFeltGreenBottom { 0x14, 0x5b, 0x39 };
inline constexpr QColor kFeltTealTop { 0x1c, 0x6b, 0x7e };
inline constexpr QColor kFeltTealBottom { 0x0d, 0x40, 0x4e };
inline constexpr QColor kFeltBlueTop { 0x2e, 0x40, 0x6e };
inline constexpr QColor kFeltBlueBottom { 0x18, 0x22, 0x40 };
inline constexpr QColor kFeltClaretTop { 0x6b, 0x24, 0x30 };
inline constexpr QColor kFeltClaretBottom { 0x3a, 0x11, 0x1a };

inline constexpr QColor kWood { 0x3a, 0x27, 0x19 };
inline constexpr QColor kWoodLight { 0x5c, 0x40, 0x28 };
inline constexpr QColor kWoodDark { 0x21, 0x16, 0x0e };

inline constexpr QColor kGold { 0xd8, 0xb0, 0x64 };

// Fills a rectangle with felt: a vertical gradient plus a faint centre glow,
// which reads as cloth catching the light rather than flat paint.
void paintFelt(QPainter& p, const QRectF& r, const QColor& top, const QColor& bottom,
               double radius = 0.0);

// Draws a wooden surround OUTSIDE `inner`, `width` units thick, and returns
// the frame rectangle. The grain is faked with a few darker streaks.
QRectF paintWoodFrame(QPainter& p, const QRectF& inner, double width, double radius = 8.0);

// A soft elliptical shadow under a round piece sitting on the table.
void paintContactShadow(QPainter& p, const QPointF& centre, double radius);

// A blurred-looking drop shadow behind a rounded rectangle, drawn as a few
// stacked translucent rounded rects — cheap, and enough to lift a card.
void paintDropShadow(QPainter& p, const QRectF& r, double radius, double depth = 3.0);

// A thin engraved line, used for inlays and panel edges.
void paintInlay(QPainter& p, const QRectF& r, double radius, const QColor& colour);

} // namespace Theme
