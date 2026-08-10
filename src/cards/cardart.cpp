#include "cardart.h"

#include <QPainterPath>

#include <algorithm>

namespace {
constexpr QColor kFace { 0xfa, 0xfa, 0xf6 };
constexpr QColor kFaceEdge { 0x00, 0x00, 0x00 };
constexpr QColor kRed { 0xc4, 0x2d, 0x2d };
constexpr QColor kBlack { 0x1e, 0x21, 0x24 };
constexpr QColor kBackInk { 0x2c, 0x4c, 0x8f };
constexpr QColor kBackPaper { 0xe8, 0xed, 0xf6 };

double corner(const QRectF& r) { return std::max(2.0, r.width() * 0.08); }
}

namespace CardArt {

void paintFace(QPainter& p, const QRectF& r, const Card& c)
{
    QPainterPath path;
    path.addRoundedRect(r, corner(r), corner(r));
    p.fillPath(path, kFace);
    p.setPen(QPen(QColor(kFaceEdge.red(), kFaceEdge.green(), kFaceEdge.blue(), 70), 1));
    p.drawPath(path);

    const QColor ink = isRed(c) ? kRed : kBlack;
    const QString rank = rankLabel(c.rank);
    const QString suit = suitSymbol(c.suit);

    // Corner index, top-left. Small cards get the index only — a big central
    // pip on a 40px-wide card is unreadable mush.
    QFont indexFont = p.font();
    indexFont.setBold(true);
    indexFont.setPointSizeF(std::max(6.0, r.width() * 0.30));
    p.setFont(indexFont);
    p.setPen(ink);

    const double pad = r.width() * 0.08;
    // The box has to take a two-character rank: at half the card width, "10"
    // was being clipped to a stray stroke.
    const QRectF corner(r.left() + pad, r.top() + pad * 0.6, r.width() - 2 * pad,
                        r.height() * 0.34);
    p.drawText(corner, Qt::AlignLeft | Qt::AlignTop, rank);

    QFont suitFont = p.font();
    suitFont.setPointSizeF(std::max(6.0, r.width() * 0.26));
    p.setFont(suitFont);
    p.drawText(QRectF(corner.left(), corner.top() + r.height() * 0.19,
                      corner.width(), r.height() * 0.25),
               Qt::AlignLeft | Qt::AlignTop, suit);

    // Centre pip, only when there is room for it to read.
    if (r.width() >= 46) {
        QFont big = p.font();
        big.setPointSizeF(r.width() * 0.46);
        p.setFont(big);
        p.setPen(QColor(ink.red(), ink.green(), ink.blue(), 210));
        p.drawText(r.adjusted(r.width() * 0.18, r.height() * 0.22, -r.width() * 0.06, -r.height() * 0.06),
                   Qt::AlignCenter, suit);
    }
}

void paintBack(QPainter& p, const QRectF& r)
{
    QPainterPath path;
    path.addRoundedRect(r, corner(r), corner(r));
    p.fillPath(path, kBackPaper);
    p.setPen(QPen(QColor(0, 0, 0, 70), 1));
    p.drawPath(path);

    const QRectF inner = r.adjusted(r.width() * 0.10, r.height() * 0.07,
                                    -r.width() * 0.10, -r.height() * 0.07);
    QPainterPath innerPath;
    innerPath.addRoundedRect(inner, corner(r) * 0.6, corner(r) * 0.6);
    p.fillPath(innerPath, kBackInk);

    // A lattice, clipped to the inner panel — cheap and reads as a card back.
    p.save();
    p.setClipPath(innerPath);
    p.setPen(QPen(QColor(255, 255, 255, 45), 1));
    const double step = std::max(4.0, r.width() * 0.16);
    for (double x = inner.left() - inner.height(); x < inner.right(); x += step) {
        p.drawLine(QPointF(x, inner.top()), QPointF(x + inner.height(), inner.bottom()));
        p.drawLine(QPointF(x + inner.height(), inner.top()), QPointF(x, inner.bottom()));
    }
    p.restore();
}

void paintSlot(QPainter& p, const QRectF& r, const QString& glyph)
{
    QPainterPath path;
    path.addRoundedRect(r, corner(r), corner(r));
    p.fillPath(path, QColor(255, 255, 255, 18));
    p.setPen(QPen(QColor(255, 255, 255, 70), 1.5, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    if (!glyph.isEmpty()) {
        QFont f = p.font();
        f.setPointSizeF(std::max(8.0, r.width() * 0.40));
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 90));
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
