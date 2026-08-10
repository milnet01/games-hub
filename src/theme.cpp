#include "theme.h"

#include <QLinearGradient>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>

namespace Theme {

void paintFelt(QPainter& p, const QRectF& r, const QColor& top, const QColor& bottom, double radius)
{
    QPainterPath path;
    if (radius > 0)
        path.addRoundedRect(r, radius, radius);
    else
        path.addRect(r);

    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, top);
    g.setColorAt(1.0, bottom);
    p.fillPath(path, g);

    // A wide, very soft highlight over the middle of the cloth. Without it a
    // large table reads as a flat block of colour.
    p.save();
    p.setClipPath(path);
    QRadialGradient glow(r.center() - QPointF(0, r.height() * 0.12), r.width() * 0.75);
    glow.setColorAt(0.0, QColor(255, 255, 255, 22));
    glow.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(r, glow);

    // Vignette at the edges, so the cloth falls away rather than stopping.
    QRadialGradient vignette(r.center(), std::max(r.width(), r.height()) * 0.72);
    vignette.setColorAt(0.55, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 60));
    p.fillRect(r, vignette);
    p.restore();
}

QRectF paintWoodFrame(QPainter& p, const QRectF& inner, double width, double radius)
{
    const QRectF frame = inner.adjusted(-width, -width, width, width);

    QPainterPath path;
    path.addRoundedRect(frame, radius, radius);

    QLinearGradient g(frame.topLeft(), frame.bottomLeft());
    g.setColorAt(0.0, kWoodLight);
    g.setColorAt(0.35, kWood);
    g.setColorAt(1.0, kWoodDark);
    p.fillPath(path, g);

    // Grain: a handful of darker streaks running the long way, clipped to the
    // frame and kept faint.
    p.save();
    QPainterPath ring = path;
    QPainterPath hole;
    hole.addRoundedRect(inner, std::max(0.0, radius - width * 0.5), std::max(0.0, radius - width * 0.5));
    ring = ring.subtracted(hole);
    p.setClipPath(ring);
    p.setPen(QPen(QColor(0, 0, 0, 34), std::max(1.0, width * 0.14)));
    for (int i = 0; i < 5; ++i) {
        const double t = (i + 0.5) / 5.0;
        const double y = frame.top() + frame.height() * t;
        p.drawLine(QPointF(frame.left(), y), QPointF(frame.right(), y + width * 0.3));
    }
    p.restore();

    // A bright top edge and a dark bottom one give the frame its thickness.
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 40), 1));
    p.drawPath(path);
    p.setPen(QPen(QColor(0, 0, 0, 90), 1));
    p.drawRoundedRect(inner.adjusted(-1, -1, 1, 1), radius * 0.4, radius * 0.4);

    return frame;
}

void paintContactShadow(QPainter& p, const QPointF& centre, double radius)
{
    p.save();
    p.setPen(Qt::NoPen);
    QRadialGradient g(centre + QPointF(radius * 0.10, radius * 0.16), radius * 1.35);
    g.setColorAt(0.0, QColor(0, 0, 0, 90));
    g.setColorAt(0.62, QColor(0, 0, 0, 55));
    g.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setBrush(g);
    p.drawEllipse(centre + QPointF(radius * 0.10, radius * 0.16), radius * 1.35, radius * 1.35);
    p.restore();
}

void paintDropShadow(QPainter& p, const QRectF& r, double radius, double depth)
{
    p.save();
    p.setPen(Qt::NoPen);
    // Three stacked translucent rects fake a blur far more cheaply than a real
    // one, and at card size the difference is invisible.
    for (int i = 3; i >= 1; --i) {
        const double spread = depth * i * 0.6;
        p.setBrush(QColor(0, 0, 0, 22));
        p.drawRoundedRect(r.adjusted(-spread * 0.4, spread * 0.2, spread * 0.4, spread),
                          radius + spread * 0.3, radius + spread * 0.3);
    }
    p.restore();
}

void paintInlay(QPainter& p, const QRectF& r, double radius, const QColor& colour)
{
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(colour, 1.2));
    p.drawRoundedRect(r, radius, radius);
}

} // namespace Theme
