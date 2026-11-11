#include "mahjongart.h"

#include <QCoreApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mahjong {

namespace {

// Where the pips of a numbered tile sit, in a unit square: the same patterns
// a playing card uses for its pips, so a count reads as a shape.
std::vector<QPointF> pipPattern(int n)
{
    const double l = 0.22;
    const double c = 0.5;
    const double r = 0.78;
    const double t = 0.16;
    const double m = 0.5;
    const double b = 0.84;
    switch (n) {
    case 1: return { { c, m } };
    case 2: return { { c, t }, { c, b } };
    case 3: return { { c, t }, { c, m }, { c, b } };
    case 4: return { { l, t }, { r, t }, { l, b }, { r, b } };
    case 5: return { { l, t }, { r, t }, { c, m }, { l, b }, { r, b } };
    case 6: return { { l, t }, { r, t }, { l, m }, { r, m }, { l, b }, { r, b } };
    case 7: return { { l, t }, { r, t }, { c, 0.33 }, { l, m }, { r, m }, { l, b }, { r, b } };
    case 8:
        return { { l, t }, { r, t }, { c, 0.33 }, { l, m }, { r, m }, { c, 0.67 }, { l, b }, { r, b } };
    default:
        return { { l, t }, { c, t }, { r, t }, { l, m }, { c, m }, { r, m }, { l, b }, { c, b }, { r, b } };
    }
}

double pipRadius(int n, double side)
{
    const double share = n == 1 ? 0.30 : n <= 3 ? 0.15 : n <= 5 ? 0.15 : n <= 8 ? 0.12 : 0.11;
    return side * share;
}

void paintCirclePip(QPainter& p, QPointF at, double r)
{
    p.setPen(Qt::NoPen);
    p.setBrush(kCircleInk);
    p.drawEllipse(at, r, r);
    p.setBrush(kFaceTop);
    p.drawEllipse(at, r * 0.62, r * 0.62);
    p.setBrush(kCircleInk);
    p.drawEllipse(at, r * 0.30, r * 0.30);
}

void paintBambooPip(QPainter& p, QPointF at, double r, double tall)
{
    // An upright stick with a joint across its middle.
    const QRectF stick(at.x() - r * 0.5, at.y() - tall / 2.0, r, tall);
    p.setPen(Qt::NoPen);
    p.setBrush(kBambooInk);
    p.drawRoundedRect(stick, r * 0.4, r * 0.4);
    p.setBrush(kFaceTop);
    p.drawRect(QRectF(stick.left(), at.y() - r * 0.09, stick.width(), r * 0.18));
}

void paintDiamondPip(QPainter& p, QPointF at, double r)
{
    QPainterPath d;
    const double h = r * 1.25;
    const double w = r * 0.95;
    d.moveTo(at.x(), at.y() - h);
    d.lineTo(at.x() + w, at.y());
    d.lineTo(at.x(), at.y() + h);
    d.lineTo(at.x() - w, at.y());
    d.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(kDiamondInk);
    p.drawPath(d);
}

void paintArrow(QPainter& p, QPointF centre, double length, double angleDegrees, const QColor& ink)
{
    p.save();
    p.translate(centre);
    p.rotate(angleDegrees);
    const double half = length / 2.0;
    p.setPen(QPen(ink, length * 0.13, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(QPointF(-half, 0), QPointF(half * 0.55, 0));
    QPainterPath head;
    head.moveTo(half, 0);
    head.lineTo(half * 0.25, -length * 0.28);
    head.lineTo(half * 0.25, length * 0.28);
    head.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    p.drawPath(head);
    p.restore();
}

void paintFlame(QPainter& p, const QRectF& r)
{
    // The red dragon: a tall flame.
    const QPointF c = r.center();
    const double w = r.width() * 0.36;
    const double h = r.height() * 0.40;
    QPainterPath flame;
    flame.moveTo(c.x(), c.y() - h);
    flame.cubicTo(c.x() + w * 0.3, c.y() - h * 0.4, c.x() + w * 1.1, c.y() - h * 0.1, c.x() + w * 0.8,
                  c.y() + h * 0.55);
    flame.cubicTo(c.x() + w * 0.6, c.y() + h * 1.0, c.x() - w * 0.6, c.y() + h * 1.0, c.x() - w * 0.8,
                  c.y() + h * 0.55);
    flame.cubicTo(c.x() - w * 1.1, c.y() - h * 0.1, c.x() - w * 0.3, c.y() - h * 0.4, c.x(), c.y() - h);
    p.setPen(Qt::NoPen);
    p.setBrush(kDiamondInk);
    p.drawPath(flame);
    p.setBrush(QColor(0xff, 0xc4, 0x4a));
    p.drawPath(QTransform()
                   .translate(c.x(), c.y() + h * 0.35)
                   .scale(0.45, 0.45)
                   .translate(-c.x(), -c.y())
                   .map(flame));
}

void paintTree(QPainter& p, const QRectF& r)
{
    // The green dragon: a fir tree of three tiers on a trunk.
    const QPointF c = r.center();
    const double w = r.width() * 0.40;
    const double tier = r.height() * 0.22;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x6b, 0x45, 0x22));
    p.drawRect(QRectF(c.x() - w * 0.14, c.y() + tier * 1.1, w * 0.28, tier * 0.75));
    p.setBrush(kBambooInk);
    for (int i = 0; i < 3; ++i) {
        const double top = c.y() - tier * 1.8 + i * tier * 0.95;
        const double half = w * (0.55 + 0.22 * i);
        QPainterPath t;
        t.moveTo(c.x(), top);
        t.lineTo(c.x() + half, top + tier * 1.25);
        t.lineTo(c.x() - half, top + tier * 1.25);
        t.closeSubpath();
        p.drawPath(t);
    }
}

void paintFrame(QPainter& p, const QRectF& r, bool legible)
{
    // The white dragon: an empty blue frame, as on a traditional set.
    const QRectF outer = r.adjusted(r.width() * 0.10, r.height() * 0.10, -r.width() * 0.10,
                                    -r.height() * 0.10);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(kCircleInk, r.width() * (legible ? 0.10 : 0.08), Qt::SolidLine, Qt::SquareCap,
                  Qt::MiterJoin));
    p.drawRect(outer);
    p.setPen(QPen(kCircleInk, r.width() * 0.035));
    p.drawRect(outer.adjusted(r.width() * 0.13, r.width() * 0.13, -r.width() * 0.13, -r.width() * 0.13));
}

void paintFlower(QPainter& p, const QRectF& r, int rank)
{
    static const QColor petals[] = { QColor(0xe0, 0x4a, 0x8c), QColor(0xe8, 0x7a, 0x1c),
                                     QColor(0x8a, 0x4c, 0xc8), QColor(0x2c, 0x7c, 0xd8) };
    const QColor petal = petals[std::clamp(rank - 1, 0, 3)];
    const QPointF c = r.center() + QPointF(0, r.height() * 0.04);
    const double radius = std::min(r.width(), r.height()) * 0.36;
    p.setPen(QPen(kBambooInk, radius * 0.14, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(c, c + QPointF(0, radius * 1.35));
    p.setPen(Qt::NoPen);
    p.setBrush(petal);
    for (int i = 0; i < 5; ++i) {
        p.save();
        p.translate(c);
        p.rotate(i * 72.0);
        p.drawEllipse(QPointF(0, -radius * 0.55), radius * 0.30, radius * 0.50);
        p.restore();
    }
    p.setBrush(QColor(0xf2, 0xc8, 0x2a));
    p.drawEllipse(c, radius * 0.24, radius * 0.24);
}

void paintSeason(QPainter& p, const QRectF& r, int rank)
{
    const QPointF c = r.center();
    const double s = std::min(r.width(), r.height()) * 0.38;
    switch (rank) {
    case 1: { // spring: a sprout
        p.setPen(QPen(kBambooInk, s * 0.14, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(c + QPointF(0, s), c + QPointF(0, -s * 0.2));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x3c, 0xa8, 0x48));
        for (int side : { -1, 1 }) {
            p.save();
            p.translate(c + QPointF(side * s * 0.38, -s * 0.35));
            p.rotate(side * 40.0);
            p.drawEllipse(QPointF(0, 0), s * 0.26, s * 0.52);
            p.restore();
        }
        break;
    }
    case 2: { // summer: the sun
        p.setPen(QPen(QColor(0xe8, 0x8a, 0x10), s * 0.12, Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < 8; ++i) {
            const double a = i * std::numbers::pi / 4.0;
            const QPointF d(std::cos(a), std::sin(a));
            p.drawLine(c + d * s * 0.68, c + d * s * 1.0);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xf2, 0xb2, 0x1a));
        p.drawEllipse(c, s * 0.48, s * 0.48);
        break;
    }
    case 3: { // autumn: a falling leaf
        p.save();
        p.translate(c);
        p.rotate(-35.0);
        QPainterPath leaf;
        leaf.moveTo(0, -s);
        leaf.cubicTo(s * 0.7, -s * 0.5, s * 0.7, s * 0.5, 0, s * 0.85);
        leaf.cubicTo(-s * 0.7, s * 0.5, -s * 0.7, -s * 0.5, 0, -s);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xc8, 0x5a, 0x16));
        p.drawPath(leaf);
        p.setPen(QPen(QColor(0x7a, 0x30, 0x08), s * 0.07, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(0, -s * 0.8), QPointF(0, s * 1.05));
        p.restore();
        break;
    }
    default: { // winter: a snowflake
        p.setPen(QPen(kCircleInk, s * 0.11, Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < 3; ++i) {
            const double a = i * std::numbers::pi / 3.0 + std::numbers::pi / 2.0;
            const QPointF d(std::cos(a), std::sin(a));
            p.drawLine(c - d * s, c + d * s);
            for (int end : { -1, 1 }) {
                const QPointF tip = c + d * s * 0.62 * end;
                const QPointF n(-d.y(), d.x());
                p.drawLine(tip, tip + (d * end + n) * s * 0.22);
                p.drawLine(tip, tip + (d * end - n) * s * 0.22);
            }
        }
        break;
    }
    }
}

} // namespace

void paintTileFace(QPainter& p, const QRectF& r, Face face, bool legible, const QFont& base)
{
    const double w = r.width();
    const double radius = w * 0.10;
    // The art, inside a margin.
    const QRectF art = r.adjusted(w * 0.09, w * 0.09, -w * 0.09, -w * 0.09);
    const Suit suit = mahjong::suitOf(face);
    const int rank = mahjong::rankOf(face);
    const auto numeral = [&](const QColor& ink) {
        QFont f = base;
        f.setBold(true);
        f.setPixelSize(std::max(7, int(art.height() * (legible ? 0.22 : 0.18))));
        p.setFont(f);
        p.setPen(ink);
        p.drawText(art, Qt::AlignLeft | Qt::AlignTop, QString::number(rank));
    };
    switch (suit) {
    case Suit::Circles:
    case Suit::Bamboo:
    case Suit::Characters: {
        const QColor ink = suit == Suit::Circles ? kCircleInk
            : suit == Suit::Bamboo               ? kBambooInk
                                                 : kDiamondInk;
        numeral(ink);
        // The pips sit below the numeral's line, in a square-ish field.
        const QRectF field = art.adjusted(0, art.height() * 0.19, 0, 0);
        const double side = std::min(field.width(), field.height());
        const double r0 = pipRadius(rank, side) * (legible ? 1.08 : 1.0);
        for (const QPointF& at : pipPattern(rank)) {
            const QPointF c(field.left() + at.x() * field.width(),
                            field.top() + at.y() * field.height());
            if (suit == Suit::Circles)
                paintCirclePip(p, c, r0);
            else if (suit == Suit::Characters)
                paintDiamondPip(p, c, r0);
            else
                paintBambooPip(p, c, r0 * (rank == 1 ? 0.7 : 0.9),
                               rank == 1 ? field.height() * 0.7 : r0 * (rank >= 6 ? 2.2 : 2.6));
        }
        break;
    }
    case Suit::Wind: {
        static const char* const letters[] = {
            QT_TRANSLATE_NOOP("MahjongView", "E"), QT_TRANSLATE_NOOP("MahjongView", "S"),
            QT_TRANSLATE_NOOP("MahjongView", "W"), QT_TRANSLATE_NOOP("MahjongView", "N")
        };
        // East points right, south down, west left and north up, as on a map.
        static const double angles[] = { 0.0, 90.0, 180.0, 270.0 };
        QFont f = base;
        f.setBold(true);
        f.setPixelSize(std::max(8, int(art.height() * 0.44)));
        p.setFont(f);
        p.setPen(kWindInk);
        const QRectF top(art.left(), art.top(), art.width(), art.height() * 0.55);
        p.drawText(top, Qt::AlignCenter, QCoreApplication::translate("MahjongView", letters[rank - 1]));
        paintArrow(p, QPointF(art.center().x(), art.top() + art.height() * 0.77),
                   std::min(art.width(), art.height() * 0.40) * 0.85, angles[rank - 1],
                   kDiamondInk);
        break;
    }
    case Suit::Dragon:
        if (rank == 1)
            paintFlame(p, art);
        else if (rank == 2)
            paintTree(p, art);
        else
            paintFrame(p, art, legible);
        break;
    case Suit::Flower:
    case Suit::Season: {
        // A band of colour round the inside says which of the two families a
        // tile is in, since any two of one family match.
        const QColor band = suit == Suit::Flower ? QColor(0xe0, 0x4a, 0x8c) : QColor(0xd8, 0xa0, 0x20);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(band, std::max(1.5, w * 0.035)));
        p.drawRoundedRect(r.adjusted(w * 0.05, w * 0.05, -w * 0.05, -w * 0.05), radius * 0.7,
                          radius * 0.7);
        numeral(band.darker(130));
        const QRectF field = art.adjusted(0, art.height() * 0.12, 0, 0);
        if (suit == Suit::Flower)
            paintFlower(p, field, rank);
        else
            paintSeason(p, field, rank);
        break;
    }
    case Suit::None:
        break;
    }
}

QString tileName(Face tile)
{
    // The number words as the table's sentences use them, so a tile reads
    // "Five of Bamboo" rather than "5 of Bamboo".
    static const char* const numbers[] = {
        QT_TRANSLATE_NOOP("MahjongArt", "One"),   QT_TRANSLATE_NOOP("MahjongArt", "Two"),
        QT_TRANSLATE_NOOP("MahjongArt", "Three"), QT_TRANSLATE_NOOP("MahjongArt", "Four"),
        QT_TRANSLATE_NOOP("MahjongArt", "Five"),  QT_TRANSLATE_NOOP("MahjongArt", "Six"),
        QT_TRANSLATE_NOOP("MahjongArt", "Seven"), QT_TRANSLATE_NOOP("MahjongArt", "Eight"),
        QT_TRANSLATE_NOOP("MahjongArt", "Nine"),
    };
    static const char* const winds[] = {
        QT_TRANSLATE_NOOP("MahjongArt", "East Wind"), QT_TRANSLATE_NOOP("MahjongArt", "South Wind"),
        QT_TRANSLATE_NOOP("MahjongArt", "West Wind"), QT_TRANSLATE_NOOP("MahjongArt", "North Wind"),
    };
    static const char* const dragons[] = {
        QT_TRANSLATE_NOOP("MahjongArt", "Red Dragon"), QT_TRANSLATE_NOOP("MahjongArt", "Green Dragon"),
        QT_TRANSLATE_NOOP("MahjongArt", "White Dragon"),
    };
    static const char* const seasons[] = {
        QT_TRANSLATE_NOOP("MahjongArt", "Spring"), QT_TRANSLATE_NOOP("MahjongArt", "Summer"),
        QT_TRANSLATE_NOOP("MahjongArt", "Autumn"), QT_TRANSLATE_NOOP("MahjongArt", "Winter"),
    };
    const auto tr = [](const char* text) { return QCoreApplication::translate("MahjongArt", text); };
    const int rank = rankOf(tile);
    switch (suitOf(tile)) {
    case Suit::Circles: return tr("%1 of Circles").arg(tr(numbers[rank - 1]));
    case Suit::Bamboo: return tr("%1 of Bamboo").arg(tr(numbers[rank - 1]));
    case Suit::Characters: return tr("%1 of Diamonds").arg(tr(numbers[rank - 1]));
    case Suit::Wind: return tr(winds[rank - 1]);
    case Suit::Dragon: return tr(dragons[rank - 1]);
    case Suit::Flower: return tr("Flower %1").arg(rank);
    case Suit::Season: return tr(seasons[rank - 1]);
    case Suit::None: break;
    }
    return {};
}

} // namespace mahjong
