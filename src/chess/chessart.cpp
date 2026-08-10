#include "chessart.h"

#include "theme.h"

#include <QLinearGradient>
#include <QPainterPath>

namespace ChessArt {

namespace {

// Every silhouette below is drawn in unit coordinates — x and y from 0 to 1
// across the piece's box — so one set of numbers serves every board size.
QPointF at(const QRectF& b, double x, double y)
{
    return { b.x() + x * b.width(), b.y() + y * b.height() };
}

// The foot every piece stands on: a stepped plinth, widest at the bottom.
void addBase(QPainterPath& path, const QRectF& b, double top)
{
    path.moveTo(at(b, 0.36, top));
    path.lineTo(at(b, 0.64, top));
    path.lineTo(at(b, 0.70, top + 0.05));
    path.lineTo(at(b, 0.30, top + 0.05));
    path.closeSubpath();
    path.addRoundedRect(QRectF(at(b, 0.24, top + 0.06),
                               QSizeF(b.width() * 0.52, b.height() * 0.10)),
                        b.width() * 0.03, b.width() * 0.03);
}

void addPawn(QPainterPath& path, const QRectF& b)
{
    path.addEllipse(at(b, 0.50, 0.29), b.width() * 0.135, b.height() * 0.135);
    path.moveTo(at(b, 0.37, 0.80));
    path.cubicTo(at(b, 0.41, 0.62), at(b, 0.43, 0.52), at(b, 0.42, 0.44));
    path.lineTo(at(b, 0.58, 0.44));
    path.cubicTo(at(b, 0.57, 0.52), at(b, 0.59, 0.62), at(b, 0.63, 0.80));
    path.closeSubpath();
    addBase(path, b, 0.79);
}

void addRook(QPainterPath& path, const QRectF& b)
{
    // Battlements: a bar with two square notches cut out of the top.
    path.moveTo(at(b, 0.27, 0.34));
    path.lineTo(at(b, 0.27, 0.16));
    path.lineTo(at(b, 0.36, 0.16));
    path.lineTo(at(b, 0.36, 0.23));
    path.lineTo(at(b, 0.44, 0.23));
    path.lineTo(at(b, 0.44, 0.16));
    path.lineTo(at(b, 0.56, 0.16));
    path.lineTo(at(b, 0.56, 0.23));
    path.lineTo(at(b, 0.64, 0.23));
    path.lineTo(at(b, 0.64, 0.16));
    path.lineTo(at(b, 0.73, 0.16));
    path.lineTo(at(b, 0.73, 0.34));
    path.closeSubpath();

    path.moveTo(at(b, 0.34, 0.34));
    path.lineTo(at(b, 0.66, 0.34));
    path.lineTo(at(b, 0.70, 0.79));
    path.lineTo(at(b, 0.30, 0.79));
    path.closeSubpath();
    addBase(path, b, 0.79);
}

void addKnight(QPainterPath& path, const QRectF& b)
{
    // A horse's head in profile, facing left.
    path.moveTo(at(b, 0.30, 0.80));
    path.lineTo(at(b, 0.31, 0.62));
    path.cubicTo(at(b, 0.26, 0.58), at(b, 0.22, 0.52), at(b, 0.20, 0.45));
    path.cubicTo(at(b, 0.24, 0.42), at(b, 0.28, 0.42), at(b, 0.31, 0.40));
    path.cubicTo(at(b, 0.33, 0.32), at(b, 0.37, 0.24), at(b, 0.42, 0.19));
    path.lineTo(at(b, 0.40, 0.10));
    path.lineTo(at(b, 0.50, 0.18));
    path.lineTo(at(b, 0.56, 0.09));
    path.lineTo(at(b, 0.60, 0.24));
    path.cubicTo(at(b, 0.68, 0.34), at(b, 0.71, 0.50), at(b, 0.70, 0.62));
    path.lineTo(at(b, 0.71, 0.80));
    path.closeSubpath();
    addBase(path, b, 0.79);
}

void addBishopSlit(QPainterPath& detail, const QRectF& b)
{
    // Without the cut in the mitre a bishop is just a tall pawn, and at board
    // size that is exactly what it looks like.
    detail.moveTo(at(b, 0.58, 0.27));
    detail.lineTo(at(b, 0.44, 0.44));
}

void addBishop(QPainterPath& path, const QRectF& b)
{
    path.addEllipse(at(b, 0.50, 0.13), b.width() * 0.055, b.height() * 0.055);
    // The mitre.
    path.moveTo(at(b, 0.50, 0.18));
    path.cubicTo(at(b, 0.64, 0.28), at(b, 0.68, 0.42), at(b, 0.64, 0.55));
    path.lineTo(at(b, 0.36, 0.55));
    path.cubicTo(at(b, 0.32, 0.42), at(b, 0.36, 0.28), at(b, 0.50, 0.18));
    path.closeSubpath();

    path.addRect(QRectF(at(b, 0.34, 0.55), QSizeF(b.width() * 0.32, b.height() * 0.06)));
    path.moveTo(at(b, 0.38, 0.61));
    path.lineTo(at(b, 0.62, 0.61));
    path.lineTo(at(b, 0.66, 0.79));
    path.lineTo(at(b, 0.34, 0.79));
    path.closeSubpath();
    addBase(path, b, 0.79);
}

void addQueen(QPainterPath& path, const QRectF& b)
{
    // A five-pointed coronet, tallest in the middle, with a pearl on each spike.
    constexpr double kPeakX[5] = { 0.28, 0.39, 0.50, 0.61, 0.72 };
    constexpr double kPeakY[5] = { 0.26, 0.19, 0.12, 0.19, 0.26 };
    constexpr double kValleyX[4] = { 0.335, 0.445, 0.555, 0.665 };

    path.moveTo(at(b, 0.24, 0.50));
    for (int i = 0; i < 5; ++i) {
        path.lineTo(at(b, kPeakX[i], kPeakY[i]));
        if (i < 4)
            path.lineTo(at(b, kValleyX[i], 0.40));
    }
    path.lineTo(at(b, 0.76, 0.50));
    path.closeSubpath();

    for (int i = 0; i < 5; ++i)
        path.addEllipse(at(b, kPeakX[i], kPeakY[i]), b.width() * 0.045, b.height() * 0.045);

    path.addRect(QRectF(at(b, 0.26, 0.50), QSizeF(b.width() * 0.48, b.height() * 0.07)));
    path.moveTo(at(b, 0.31, 0.57));
    path.lineTo(at(b, 0.69, 0.57));
    path.lineTo(at(b, 0.72, 0.79));
    path.lineTo(at(b, 0.28, 0.79));
    path.closeSubpath();
    addBase(path, b, 0.79);
}

void addKing(QPainterPath& path, const QRectF& b)
{
    // The cross that tells the king from the queen at a glance.
    path.addRect(QRectF(at(b, 0.465, 0.03), QSizeF(b.width() * 0.07, b.height() * 0.20)));
    path.addRect(QRectF(at(b, 0.38, 0.09), QSizeF(b.width() * 0.24, b.height() * 0.07)));

    // A domed crown rather than the queen's spikes.
    path.moveTo(at(b, 0.26, 0.52));
    path.cubicTo(at(b, 0.26, 0.32), at(b, 0.38, 0.22), at(b, 0.50, 0.22));
    path.cubicTo(at(b, 0.62, 0.22), at(b, 0.74, 0.32), at(b, 0.74, 0.52));
    path.closeSubpath();

    path.addRect(QRectF(at(b, 0.26, 0.52), QSizeF(b.width() * 0.48, b.height() * 0.07)));
    path.moveTo(at(b, 0.31, 0.59));
    path.lineTo(at(b, 0.69, 0.59));
    path.lineTo(at(b, 0.72, 0.79));
    path.lineTo(at(b, 0.28, 0.79));
    path.closeSubpath();
    addBase(path, b, 0.79);
}

} // namespace

void paintPiece(QPainter& p, const QRectF& box, chess::PieceType type, chess::Colour colour)
{
    if (type == chess::PieceType::None)
        return;

    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    // Lines that are stroked over the finished silhouette rather than filled
    // into it, because a hole in a filled path fights the winding rule.
    QPainterPath detail;
    switch (type) {
    case chess::PieceType::Pawn:   addPawn(path, box); break;
    case chess::PieceType::Knight: addKnight(path, box); break;
    case chess::PieceType::Bishop: addBishop(path, box); addBishopSlit(detail, box); break;
    case chess::PieceType::Rook:   addRook(path, box); break;
    case chess::PieceType::Queen:  addQueen(path, box); break;
    case chess::PieceType::King:   addKing(path, box); break;
    case chess::PieceType::None:   return;
    }

    const bool white = colour == chess::Colour::White;
    Theme::paintContactShadow(p, QPointF(box.center().x(), box.y() + box.height() * 0.87),
                              box.width() * 0.30);

    QLinearGradient g(box.topLeft(), box.bottomLeft());
    if (white) {
        g.setColorAt(0.0, QColor(0xfb, 0xf7, 0xee));
        g.setColorAt(1.0, QColor(0xc9, 0xbf, 0xac));
    } else {
        g.setColorAt(0.0, QColor(0x4a, 0x46, 0x41));
        g.setColorAt(1.0, QColor(0x16, 0x15, 0x13));
    }

    // A black piece on a dark square needs a light edge to stay readable, and a
    // white piece needs a dark one on a light square. Outlining each against
    // its opposite is what keeps both sets legible on both colours.
    const QPen edge(white ? QColor(0x2a, 0x25, 0x1f) : QColor(0xd8, 0xd2, 0xc6),
                    std::max(1.0, box.width() * 0.035));
    p.setBrush(g);
    p.setPen(edge);
    p.drawPath(path);

    if (!detail.isEmpty()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(edge.color(), edge.widthF(), Qt::SolidLine, Qt::RoundCap));
        p.drawPath(detail);
    }
}

} // namespace ChessArt
