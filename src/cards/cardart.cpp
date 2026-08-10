#include "cardart.h"

#include "theme.h"

#include <QLinearGradient>
#include <QPainterPath>

#include <algorithm>
#include <array>

namespace {

constexpr QColor kFaceTop { 0xff, 0xff, 0xfd };
constexpr QColor kFaceBottom { 0xef, 0xed, 0xe4 };
constexpr QColor kRed { 0xc0, 0x28, 0x28 };
constexpr QColor kBlack { 0x1c, 0x1f, 0x22 };
constexpr QColor kBackInk { 0x27, 0x46, 0x86 };
constexpr QColor kBackInkDark { 0x18, 0x2c, 0x58 };
constexpr QColor kBackInkRed { 0x9c, 0x27, 0x33 };
constexpr QColor kBackInkRedDark { 0x67, 0x16, 0x22 };
constexpr QColor kBackPaper { 0xf2, 0xf4, 0xf9 };

double corner(const QRectF& r) { return std::max(2.0, r.width() * 0.075); }

struct Pip {
    double x;
    double y;
};

// The traditional pip arrangements. Columns sit at 0.28 / 0.5 / 0.72 and the
// rows are spaced so each count reads at a glance, exactly as on a real deck.
std::vector<Pip> pipLayout(int rank)
{
    // Columns sit clear of the corner indices; rows run between them.
    constexpr double L = 0.32;
    constexpr double C = 0.50;
    constexpr double R = 0.68;

    // Rows run the full height of the pip field. Ranks 2-8 use three rows,
    // 9 and 10 use four, and the odd pip sits on a line between them — the
    // arrangement a real deck uses.
    constexpr double T = 0.00;
    constexpr double M = 0.50;
    constexpr double B = 1.00;

    switch (rank) {
    case 2:  return { { C, T }, { C, B } };
    case 3:  return { { C, T }, { C, M }, { C, B } };
    case 4:  return { { L, T }, { R, T }, { L, B }, { R, B } };
    case 5:  return { { L, T }, { R, T }, { C, M }, { L, B }, { R, B } };
    case 6:  return { { L, T }, { R, T }, { L, M }, { R, M }, { L, B }, { R, B } };
    case 7:  return { { L, T }, { R, T }, { C, 0.25 }, { L, M }, { R, M },
                      { L, B }, { R, B } };
    case 8:  return { { L, T }, { R, T }, { C, 0.25 }, { L, M }, { R, M },
                      { C, 0.75 }, { L, B }, { R, B } };
    case 9:  return { { L, T }, { R, T }, { L, 0.32 }, { R, 0.32 }, { C, M },
                      { L, 0.68 }, { R, 0.68 }, { L, B }, { R, B } };
    case 10: return { { L, T }, { R, T }, { C, 0.16 }, { L, 0.32 }, { R, 0.32 },
                      { L, 0.68 }, { R, 0.68 }, { C, 0.84 }, { L, B }, { R, B } };
    default: return {};
    }
}

// Draws a suit glyph centred on `centre`. Real decks invert the lower pips,
// but an upside-down heart reads as a spade at this size, so these stay
// upright — clearer, and still reads as a proper card.
void drawPip(QPainter& p, const QPointF& centre, double size, const QString& suit)
{
    p.save();
    p.translate(centre);
    QFont f = p.font();
    f.setPointSizeF(size);
    p.setFont(f);
    const QRectF box(-size * 1.2, -size * 1.2, size * 2.4, size * 2.4);
    p.drawText(box, Qt::AlignCenter, suit);
    p.restore();
}

// A court card gets a ruled panel rather than figure art: it reads as a face
// card at any size and never turns to mush when the cards are small.
void drawCourt(QPainter& p, const QRectF& r, const QString& rank, const QString& suit,
               const QColor& ink)
{
    const QRectF panel = r.adjusted(r.width() * 0.24, r.height() * 0.14,
                                    -r.width() * 0.24, -r.height() * 0.14);

    QColor tint = ink;
    tint.setAlpha(18);
    QPainterPath path;
    path.addRoundedRect(panel, r.width() * 0.05, r.width() * 0.05);
    p.fillPath(path, tint);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Theme::kGold, std::max(1.0, r.width() * 0.018)));
    p.drawPath(path);
    p.setPen(QPen(QColor(ink.red(), ink.green(), ink.blue(), 110),
                  std::max(0.8, r.width() * 0.012)));
    p.drawRoundedRect(panel.adjusted(r.width() * 0.028, r.height() * 0.02,
                                     -r.width() * 0.028, -r.height() * 0.02),
                      r.width() * 0.03, r.width() * 0.03);

    // Suit above and below, letter through the middle.
    const double pip = r.width() * 0.15;
    p.setPen(ink);
    drawPip(p, QPointF(panel.center().x(), panel.top() + panel.height() * 0.15), pip, suit);
    drawPip(p, QPointF(panel.center().x(), panel.bottom() - panel.height() * 0.15), pip, suit);

    QFont f = p.font();
    f.setPointSizeF(r.width() * 0.30);
    f.setBold(true);
    p.setFont(f);
    p.drawText(panel, Qt::AlignCenter, rank);
}

// A jester's cap: three lobes on a headband, each with a bell. Drawn rather
// than lettered because "JOKER" down the middle of a card this small is
// unreadable, and the silhouette survives being shrunk to a corner of a meld.
void drawJesterCap(QPainter& p, const QRectF& r, const QColor& ink)
{
    const double w = r.width();
    const QPointF c(r.center().x(), r.center().y() + r.height() * 0.06);

    const double lobe = w * 0.13;
    const double reach = w * 0.26;
    const struct { double dx; double dy; } kLobes[3] = {
        { -reach, -w * 0.16 }, { 0.0, -w * 0.30 }, { reach, -w * 0.16 },
    };

    QPainterPath cap;
    cap.moveTo(c.x() - w * 0.30, c.y());
    for (const auto& l : kLobes) {
        const QPointF tip(c.x() + l.dx, c.y() + l.dy);
        cap.quadTo(QPointF(tip.x() - lobe, tip.y()), tip);
        cap.quadTo(QPointF(tip.x() + lobe, tip.y()), QPointF(tip.x() + lobe * 0.9, c.y()));
    }
    cap.lineTo(c.x() + w * 0.30, c.y());
    cap.closeSubpath();

    QColor fill = ink;
    fill.setAlpha(190);
    p.setPen(QPen(ink, std::max(0.8, w * 0.02)));
    p.setBrush(fill);
    p.drawPath(cap);

    // Bells, and the headband under the lobes.
    p.setBrush(Theme::kGold);
    p.setPen(QPen(ink, std::max(0.6, w * 0.014)));
    for (const auto& l : kLobes)
        p.drawEllipse(QPointF(c.x() + l.dx, c.y() + l.dy - lobe * 0.2), w * 0.05, w * 0.05);

    p.setBrush(Theme::kGold);
    p.drawRoundedRect(QRectF(c.x() - w * 0.32, c.y(), w * 0.64, w * 0.10),
                      w * 0.05, w * 0.05);
    p.setBrush(Qt::NoBrush);
}

} // namespace

namespace CardArt {

void paintFace(QPainter& p, const QRectF& r, const Card& c)
{
    Theme::paintDropShadow(p, r, corner(r), std::max(1.5, r.width() * 0.035));

    QPainterPath path;
    path.addRoundedRect(r, corner(r), corner(r));

    QLinearGradient face(r.topLeft(), r.bottomLeft());
    face.setColorAt(0.0, kFaceTop);
    face.setColorAt(1.0, kFaceBottom);
    p.fillPath(path, face);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0, 0, 0, 60), 1));
    p.drawPath(path);

    // A joker carries no suit, so its corner index is the star alone.
    const bool joker = isJoker(c);
    const QColor ink = isRed(c) ? kRed : kBlack;
    const QString rank = rankLabel(c.rank);
    const QString suit = joker ? QString() : suitSymbol(c.suit);
    p.setPen(ink);

    // Corner index, top-left upright and bottom-right inverted, as on a real
    // card — it is what makes a fanned hand readable.
    const double pad = r.width() * 0.08;
    const double indexSize = std::max(6.0, r.width() * 0.17);

    for (int i = 0; i < 2; ++i) {
        const bool inverted = (i == 1);
        p.save();
        if (inverted) {
            p.translate(r.center());
            p.rotate(180);
            p.translate(-r.center());
        }
        QFont f = p.font();
        f.setBold(true);
        f.setPointSizeF(indexSize);
        p.setFont(f);
        // A narrow column: wide enough for "10", narrow enough to leave the
        // middle of the card free for the pips.
        const QRectF box(r.left() + pad, r.top() + pad * 0.4, r.width() * 0.24, r.height() * 0.20);
        p.drawText(box, Qt::AlignLeft | Qt::AlignTop, rank);

        f.setBold(false);
        f.setPointSizeF(indexSize * 0.88);
        p.setFont(f);
        p.drawText(QRectF(box.left(), box.top() + r.height() * 0.125, box.width(), r.height() * 0.18),
                   Qt::AlignLeft | Qt::AlignTop, suit);
        p.restore();
    }

    // Below this width the middle of the card is too small for pips to read,
    // so the corner index carries the card on its own.
    if (r.width() < 46)
        return;

    if (joker) {
        drawJesterCap(p, r, ink);
        return;
    }

    if (c.rank == kJack || c.rank == kQueen || c.rank == kKing) {
        drawCourt(p, r, rank, suit, ink);
        return;
    }

    if (c.rank == kAce) {
        QFont f = p.font();
        f.setPointSizeF(r.width() * 0.44);
        p.setFont(f);
        p.drawText(r, Qt::AlignCenter, suit);
        return;
    }

    // Pip columns are fractions of the card; the rows run between the two
    // indices rather than over them.
    const double pipSize = r.width() * 0.125;
    const double top = r.top() + r.height() * 0.145;
    const double span = r.height() * 0.71;
    for (const Pip& pip : pipLayout(c.rank))
        drawPip(p, QPointF(r.left() + r.width() * pip.x, top + span * pip.y), pipSize, suit);
}

void paintBack(QPainter& p, const QRectF& r, int deck)
{
    const bool red = (deck % 2) != 0;
    const QColor backInk = red ? kBackInkRed : kBackInk;
    const QColor backInkDark = red ? kBackInkRedDark : kBackInkDark;

    Theme::paintDropShadow(p, r, corner(r), std::max(1.5, r.width() * 0.035));

    QPainterPath path;
    path.addRoundedRect(r, corner(r), corner(r));
    p.fillPath(path, kBackPaper);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0, 0, 0, 60), 1));
    p.drawPath(path);

    const QRectF inner = r.adjusted(r.width() * 0.09, r.height() * 0.065,
                                    -r.width() * 0.09, -r.height() * 0.065);
    QPainterPath innerPath;
    innerPath.addRoundedRect(inner, corner(r) * 0.55, corner(r) * 0.55);

    QLinearGradient ink(inner.topLeft(), inner.bottomRight());
    ink.setColorAt(0.0, backInk);
    ink.setColorAt(1.0, backInkDark);
    p.fillPath(innerPath, ink);

    // Diagonal lattice, clipped to the panel.
    p.save();
    p.setClipPath(innerPath);
    p.setPen(QPen(QColor(255, 255, 255, 42), std::max(0.7, r.width() * 0.012)));
    const double step = std::max(4.0, r.width() * 0.155);
    for (double x = inner.left() - inner.height(); x < inner.right() + inner.height(); x += step) {
        p.drawLine(QPointF(x, inner.top()), QPointF(x + inner.height(), inner.bottom()));
        p.drawLine(QPointF(x + inner.height(), inner.top()), QPointF(x, inner.bottom()));
    }
    // A centre medallion stops the lattice reading as pure texture.
    if (r.width() >= 40) {
        p.setPen(QPen(QColor(255, 255, 255, 70), std::max(0.8, r.width() * 0.016)));
        p.setBrush(QColor(backInkDark.red(), backInkDark.green(), backInkDark.blue(), 200));
        p.drawEllipse(inner.center(), r.width() * 0.17, r.width() * 0.17);
    }
    p.restore();

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 90), 1));
    p.drawPath(innerPath);
}

void paintSlot(QPainter& p, const QRectF& r, const QString& glyph)
{
    QPainterPath path;
    path.addRoundedRect(r, corner(r), corner(r));
    p.fillPath(path, QColor(0, 0, 0, 40));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 60), 1.4, Qt::DashLine));
    p.drawPath(path);

    if (!glyph.isEmpty()) {
        QFont f = p.font();
        f.setPointSizeF(std::max(8.0, r.width() * 0.38));
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 70));
        p.drawText(r, Qt::AlignCenter, glyph);
    }
}

void paintHighlight(QPainter& p, const QRectF& r, const QColor& colour)
{
    QPainterPath path;
    path.addRoundedRect(r.adjusted(-1.5, -1.5, 1.5, 1.5), corner(r), corner(r));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(colour, 2.5));
    p.drawPath(path);
}

} // namespace CardArt
