#include "laneart.h"

#include <QLinearGradient>
#include <QMutex>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace LaneArt {
namespace {

constexpr double kPi = std::numbers::pi;

// A fixed number in 0..1 for (i, salt): the art is the same on every run and
// every machine, and owes nothing to the course's seed.
double noise(int i, int salt)
{
    std::uint32_t h = std::uint32_t(i) * 2654435761U ^ (std::uint32_t(salt) * 40503U + 0x9e3779b9U);
    h ^= h >> 15U;
    h *= 0x2c1b3c6dU;
    h ^= h >> 12U;
    h *= 0x297a2d39U;
    h ^= h >> 15U;
    return double(h) / 4294967296.0;
}

QImage blank(int w, int h)
{
    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    return image;
}

void begin(QPainter& p)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
}

QImage shrink(const QImage& full, int by)
{
    const QSize size(std::max(1, full.width() / by), std::max(1, full.height() / by));
    return full.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QImage hazeOf(const QImage& image)
{
    QImage out = image;
    QPainter p(&out);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(out.rect(), kHaze);
    return out;
}

// The scene is painted in bands at once, so the pictures made on first use
// are made under a lock.
QMutex hazeLock;

const QImage& hazedAt(const Sprite& sprite, size_t size, int step)
{
    const QMutexLocker lock(&hazeLock);
    QImage& out = sprite.hazed[size][size_t(step)];
    if (out.isNull()) {
        out = sprite.sizes[size].copy();
        QPainter p(&out);
        p.setOpacity(double(step) / (kHazeSteps - 1));
        p.drawImage(0, 0, hazeOf(sprite.sizes[size]));
    }
    return out;
}

Sprite finish(const QImage& full, double widthMetres, double heightMetres, bool misted)
{
    Sprite s;
    s.sizes = { full, shrink(full, 3), shrink(full, 9) };
    s.hazes = misted;
    s.widthMetres = widthMetres;
    s.heightMetres = heightMetres;
    return s;
}

// A heap of leaves: a dark scalloped blob, a mid-tone one over it, and a
// highlight where the sun, high on the right, catches it.
void leafCluster(QPainter& p, QPointF c, double r, const QColor& dark, const QColor& mid, const QColor& light,
                 bool lit)
{
    p.setPen(Qt::NoPen);
    p.setBrush(dark);
    p.drawEllipse(c, r, r * 0.88);
    for (int j = 0; j < 8; ++j) {
        const double a = j * kPi / 4.0;
        p.drawEllipse(c + QPointF(std::cos(a), std::sin(a)) * r * 0.82, r * 0.32, r * 0.3);
    }
    p.setBrush(mid);
    const QPointF m = c + QPointF(3, -r * 0.16);
    p.drawEllipse(m, r * 0.78, r * 0.66);
    for (int j = 0; j < 5; ++j) {
        const double a = -kPi + j * kPi / 4.0;
        p.drawEllipse(m + QPointF(std::cos(a), std::sin(a)) * r * 0.66, r * 0.26, r * 0.24);
    }
    if (lit) {
        p.setBrush(light);
        p.drawEllipse(c + QPointF(r * 0.22, -r * 0.36), r * 0.42, r * 0.3);
        p.drawEllipse(c + QPointF(r * 0.52, -r * 0.12), r * 0.2, r * 0.18);
    }
}

// A frond: a curved spine from `base`, with leaflets swept back along it.
void frond(QPainter& p, QPointF base, double angle, double length, double droop, double leaflet,
           const QColor& leaf, const QColor& spine)
{
    const QPointF dir(std::cos(angle), std::sin(angle));
    const QPointF ctrl = base + dir * length * 0.55 + QPointF(0, -length * 0.15);
    const QPointF tip = base + dir * length + QPointF(0, droop);
    const auto at = [&](double t) {
        const double u = 1.0 - t;
        return base * u * u + ctrl * 2.0 * u * t + tip * t * t;
    };
    p.setPen(QPen(leaf, std::max(1.5, leaflet * 0.16), Qt::SolidLine, Qt::RoundCap));
    for (int i = 2; i < 20; ++i) {
        const double t = i / 20.0;
        const QPointF a = at(t);
        QPointF d = at(t + 0.02) - at(t - 0.02);
        const double len = std::hypot(d.x(), d.y());
        if (len < 1e-6)
            continue;
        d /= len;
        const QPointF n(-d.y(), d.x());
        const double l = leaflet * (1.0 - std::abs(t - 0.4) * 1.1);
        if (l <= 0.0)
            continue;
        p.drawLine(a, a + (n * 0.9 + d * 0.5) * l);
        p.drawLine(a, a + (-n * 0.9 + d * 0.5) * l);
    }
    QPainterPath path;
    path.moveTo(base);
    path.quadTo(ctrl, tip);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(spine, std::max(1.5, leaflet * 0.12), Qt::SolidLine, Qt::RoundCap));
    p.drawPath(path);
}

// A carved line: cut in, so a dark groove with light catching its far lip.
void carve(QPainter& p, const QPainterPath& path, double width, const QColor& groove)
{
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 120), width * 0.45, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path.translated(width * 0.35, width * 0.35));
    p.setPen(QPen(groove, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
}

void speckle(QPainter& p, const QRectF& r, int count, int salt, int alpha)
{
    p.setPen(Qt::NoPen);
    for (int i = 0; i < count; ++i) {
        const QPointF at(r.left() + noise(i, salt) * r.width(), r.top() + noise(i, salt + 1) * r.height());
        const bool light = noise(i, salt + 2) < 0.5;
        p.setBrush(light ? QColor(255, 255, 255, alpha) : QColor(0, 0, 0, alpha));
        const double s = 0.8 + 1.8 * noise(i, salt + 3);
        p.drawEllipse(at, s, s);
    }
}

void mossOnTop(QPainter& p, double width, double top, int salt, double drips)
{
    const QColor moss(0x4e, 0x8c, 0x34);
    p.setPen(Qt::NoPen);
    for (int i = 0; i * 22.0 < width + 22.0; ++i) {
        const double x = i * 22.0 + noise(i, salt) * 10.0;
        p.setBrush(moss.darker(110));
        p.drawEllipse(QPointF(x, top + 6), 15, 10);
        const double len = drips * noise(i, salt + 1);
        if (len > 8.0)
            p.drawRoundedRect(QRectF(x - 3, top + 6, 6, len), 3, 3);
        p.setBrush(moss.lighter(125));
        p.drawEllipse(QPointF(x + 3, top + 3), 9, 5);
    }
}

// ---- trees ----

QImage broadleaf(int variant)
{
    const int w = 400;
    const int h = 520;
    const double g = 514.0;
    QImage image = blank(w, h);
    QPainter p(&image);
    begin(p);

    const QColor dark = variant == 0 ? QColor(0x1c, 0x38, 0x1c) : QColor(0x22, 0x40, 0x1a);
    const QColor mid = variant == 0 ? QColor(0x2e, 0x5c, 0x2a) : QColor(0x3e, 0x6c, 0x26);
    const QColor light = variant == 0 ? QColor(0x5c, 0x92, 0x40) : QColor(0x86, 0xb0, 0x44);

    QLinearGradient bark(150, 0, 256, 0);
    bark.setColorAt(0.0, QColor(0x2a, 0x1c, 0x12));
    bark.setColorAt(0.6, QColor(0x5a, 0x3e, 0x28));
    bark.setColorAt(1.0, QColor(0x86, 0x64, 0x42));
    p.setBrush(bark);
    QPainterPath trunk;
    trunk.moveTo(118, g);
    trunk.cubicTo(160, g - 30, 178, g - 120, 184, 250);
    trunk.lineTo(222, 250);
    trunk.cubicTo(226, g - 120, 244, g - 30, 290, g);
    trunk.closeSubpath();
    p.drawPath(trunk);
    // Bark furrows.
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 7; ++i) {
        const double x = 168 + i * 11.0;
        QPainterPath furrow;
        furrow.moveTo(x + (i - 3) * 6.0, g - 6);
        furrow.cubicTo(x - 4, g - 120, x + 5, g - 200, x + 2, 262);
        p.setPen(QPen(QColor(0x1a, 0x10, 0x08, 110), 2.4));
        p.drawPath(furrow);
    }
    // Boughs into the canopy.
    p.setPen(QPen(QColor(0x4a, 0x32, 0x20), 16, Qt::SolidLine, Qt::RoundCap));
    QPainterPath bough;
    bough.moveTo(192, 270);
    bough.quadTo(160, 226, 112, 196);
    bough.moveTo(214, 266);
    bough.quadTo(252, 218, 300, 186);
    p.drawPath(bough);

    struct Leaf {
        QPointF c;
        double r;
    };
    std::vector<Leaf> leaves;
    for (int i = 0; i < 15; ++i) {
        const double a = noise(i, 11 + variant) * 2.0 * kPi;
        const double d = std::sqrt(noise(i, 12 + variant));
        leaves.push_back({ QPointF(200 + std::cos(a) * d * 140, 150 + std::sin(a) * d * 82),
                           44 + 30 * noise(i, 13 + variant) });
    }
    std::sort(leaves.begin(), leaves.end(), [](const Leaf& a, const Leaf& b) { return a.c.y() > b.c.y(); });
    for (const Leaf& l : leaves)
        leafCluster(p, l.c, l.r, dark, mid, light, (l.c.x() - 200) - (l.c.y() - 150) > -50);

    // Vines hanging out of the canopy, with leaves along them.
    for (int i = 0; i < 5; ++i) {
        const double x = 110 + 48 * i + 20 * noise(i, 21 + variant);
        const double y = 200 + 30 * noise(i, 22 + variant);
        const double len = 80 + 150 * noise(i, 23 + variant);
        QPainterPath vine;
        vine.moveTo(x, y);
        vine.cubicTo(x + 12, y + len * 0.35, x - 12, y + len * 0.7, x + 4, y + len);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0x2a, 0x4a, 0x22), 3));
        p.drawPath(vine);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x3a, 0x6e, 0x2c));
        for (int k = 1; k < 6; ++k) {
            const QPointF at = vine.pointAtPercent(k / 6.0);
            p.drawEllipse(at + QPointF((k % 2) ? 5 : -5, 0), 6, 3.5);
        }
    }
    return image;
}

QImage palm()
{
    const int w = 380;
    const int h = 560;
    QImage image = blank(w, h);
    QPainter p(&image);
    begin(p);

    const QPointF base(176, 554);
    const QPointF c1(150, 420);
    const QPointF c2(236, 260);
    const QPointF crown(212, 118);
    const auto trunkAt = [&](double t) {
        const double u = 1.0 - t;
        return base * u * u * u + c1 * 3 * u * u * t + c2 * 3 * u * t * t + crown * t * t * t;
    };
    for (int i = 0; i < 26; ++i) {
        const double t0 = i / 26.0;
        const double t1 = (i + 1) / 26.0;
        const double width = 30.0 - 13.0 * t0;
        p.setPen(QPen(i % 2 ? QColor(0x6e, 0x52, 0x36) : QColor(0x86, 0x66, 0x44), width, Qt::SolidLine,
                      Qt::FlatCap));
        p.drawLine(trunkAt(t0), trunkAt(t1));
        p.setPen(QPen(QColor(0xb0, 0x90, 0x66, 120), width * 0.22, Qt::SolidLine, Qt::FlatCap));
        p.drawLine(trunkAt(t0) + QPointF(width * 0.28, 0), trunkAt(t1) + QPointF(width * 0.28, 0));
    }
    const QColor leafA(0x2c, 0x66, 0x2a);
    const QColor leafB(0x4a, 0x8c, 0x34);
    for (int i = 0; i < 10; ++i) {
        const double a = -kPi * (1.08 - 1.16 * i / 9.0);
        frond(p, crown, a, 150 + 40 * noise(i, 31), 70 + 50 * noise(i, 32), 34, i % 2 ? leafA : leafB,
              QColor(0x2a, 0x40, 0x18));
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x5a, 0x3a, 0x1c));
    for (int i = 0; i < 3; ++i)
        p.drawEllipse(crown + QPointF(-10 + 10 * i, 12 + (i % 2) * 6), 9, 9);
    return image;
}

QImage mossy()
{
    const int w = 360;
    const int h = 560;
    const double g = 554.0;
    QImage image = blank(w, h);
    QPainter p(&image);
    begin(p);

    QLinearGradient bark(160, 0, 210, 0);
    bark.setColorAt(0.0, QColor(0x2e, 0x26, 0x1c));
    bark.setColorAt(1.0, QColor(0x6a, 0x5c, 0x48));
    p.setBrush(bark);
    QPainterPath trunk;
    trunk.moveTo(146, g);
    trunk.cubicTo(170, g - 80, 168, 300, 172, 170);
    trunk.lineTo(196, 170);
    trunk.cubicTo(200, 300, 200, g - 80, 226, g);
    trunk.closeSubpath();
    p.drawPath(trunk);
    p.setPen(QPen(QColor(0x3a, 0x30, 0x22), 10, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(180, 260), QPointF(110, 200));
    p.drawLine(QPointF(186, 230), QPointF(262, 170));

    const QColor dark(0x1a, 0x36, 0x2a);
    const QColor mid(0x2a, 0x56, 0x3c);
    const QColor light(0x5a, 0x8c, 0x58);
    for (int i = 0; i < 11; ++i) {
        const double a = noise(i, 41) * 2.0 * kPi;
        const double d = std::sqrt(noise(i, 42));
        const QPointF c(180 + std::cos(a) * d * 120, 118 + std::sin(a) * d * 64);
        leafCluster(p, c, 36 + 22 * noise(i, 43), dark, mid, light, c.x() - 180 - (c.y() - 118) > -30);
    }
    // Long grey-green moss hanging from the boughs.
    for (int i = 0; i < 14; ++i) {
        const double x = 90 + 13.0 * i + 8 * noise(i, 44);
        const double y = 150 + 50 * noise(i, 45);
        const double len = 70 + 160 * noise(i, 49);
        QPainterPath strand;
        strand.moveTo(x, y);
        strand.cubicTo(x + 6, y + len * 0.3, x - 6, y + len * 0.6, x + 2, y + len);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0x9a, 0xb0, 0x86, 200), 2.5 + 2.5 * noise(i, 47), Qt::SolidLine, Qt::RoundCap));
        p.drawPath(strand);
    }
    return image;
}

// ---- ruins ----

QImage column()
{
    const int w = 180;
    const int h = 420;
    QImage image = blank(w, h);
    QPainter p(&image);
    begin(p);
    const QColor stone(0x9a, 0x94, 0x86);
    const QPen edge(QColor(0x3a, 0x36, 0x2e, 170), 2);

    // Rubble, then the plinth in two steps.
    p.setBrush(stone.darker(120));
    p.drawEllipse(QPointF(18, 410), 14, 8);
    p.drawEllipse(QPointF(162, 412), 12, 7);
    p.setPen(edge);
    p.setBrush(stone.darker(105));
    p.drawRect(QRectF(16, 372, 148, 44));
    p.setBrush(stone);
    p.drawRect(QRectF(28, 346, 124, 28));

    QLinearGradient round(45, 0, 134, 0);
    round.setColorAt(0.0, QColor(0x6a, 0x66, 0x5c));
    round.setColorAt(0.4, QColor(0xbc, 0xb6, 0xa6));
    round.setColorAt(0.65, QColor(0xa6, 0xa0, 0x90));
    round.setColorAt(1.0, QColor(0x5a, 0x56, 0x4c));
    QPainterPath shaft;
    shaft.moveTo(45, 346);
    shaft.lineTo(45, 140);
    shaft.lineTo(60, 116);
    shaft.lineTo(72, 132);
    shaft.lineTo(88, 94);
    shaft.lineTo(101, 124);
    shaft.lineTo(117, 104);
    shaft.lineTo(134, 130);
    shaft.lineTo(134, 346);
    shaft.closeSubpath();
    p.setBrush(round);
    p.drawPath(shaft);
    // Flutes down the shaft.
    p.save();
    p.setClipPath(shaft);
    for (int i = 0; i < 6; ++i) {
        const double x = 56 + i * 13.0;
        p.setPen(QPen(QColor(0, 0, 0, 60), 3));
        p.drawLine(QPointF(x, 100), QPointF(x, 346));
        p.setPen(QPen(QColor(255, 255, 255, 50), 1.5));
        p.drawLine(QPointF(x + 3, 100), QPointF(x + 3, 346));
    }
    p.restore();
    // Moss, and a vine climbing it.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x4e, 0x8c, 0x34, 200));
    p.drawEllipse(QPointF(70, 128), 18, 9);
    p.drawEllipse(QPointF(110, 116), 14, 8);
    p.drawEllipse(QPointF(40, 372), 24, 8);
    QPainterPath vine;
    vine.moveTo(52, 410);
    vine.cubicTo(140, 330, 30, 260, 120, 190);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0x2a, 0x4e, 0x22), 3));
    p.drawPath(vine);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x3e, 0x7a, 0x2e));
    for (int k = 1; k < 9; ++k)
        p.drawEllipse(vine.pointAtPercent(k / 9.0) + QPointF((k % 2) ? 6 : -6, 0), 7, 4);
    return image;
}

QImage carvedHead()
{
    const int w = 240;
    const int h = 320;
    QImage image = blank(w, h);
    QPainter p(&image);
    begin(p);
    const QColor stone(0x8c, 0x92, 0x80);
    const QColor groove(0x3a, 0x40, 0x34);

    QLinearGradient shade(30, 0, 210, 0);
    shade.setColorAt(0.0, stone.darker(150));
    shade.setColorAt(0.55, stone.lighter(112));
    shade.setColorAt(1.0, stone.darker(125));
    p.setBrush(shade);
    p.setPen(QPen(QColor(0x2e, 0x32, 0x28, 180), 2));
    QPainterPath head;
    head.addRoundedRect(QRectF(30, 36, 180, 278), 64, 54);
    p.drawPath(head);
    // A carved band across the brow.
    p.setBrush(stone.darker(112));
    p.drawRoundedRect(QRectF(32, 40, 176, 56), 40, 24);
    for (int i = 0; i < 6; ++i)
        carve(p, [&] {
            QPainterPath r;
            r.moveTo(54 + i * 26, 60);
            r.lineTo(54 + i * 26, 84);
            return r;
        }(), 4, groove);
    // Heavy brow, deep eyes, broad nose, full lips.
    QPainterPath brow;
    brow.moveTo(60, 128);
    brow.quadTo(90, 112, 116, 128);
    brow.moveTo(124, 128);
    brow.quadTo(150, 112, 180, 128);
    carve(p, brow, 7, groove);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x2a, 0x2e, 0x24));
    for (int side = 0; side < 2; ++side) {
        QPainterPath eye;
        const double cx = side == 0 ? 88 : 152;
        eye.moveTo(cx - 22, 152);
        eye.quadTo(cx, 138, cx + 22, 152);
        eye.quadTo(cx, 162, cx - 22, 152);
        p.drawPath(eye);
    }
    QPainterPath nose;
    nose.moveTo(112, 150);
    nose.lineTo(104, 206);
    nose.quadTo(120, 218, 136, 206);
    nose.lineTo(128, 150);
    carve(p, nose, 5, groove);
    QPainterPath lips;
    lips.moveTo(88, 250);
    lips.quadTo(120, 238, 152, 250);
    lips.quadTo(120, 276, 88, 250);
    p.setPen(QPen(groove, 4));
    p.setBrush(stone.darker(118));
    p.drawPath(lips);
    p.drawLine(QPointF(90, 251), QPointF(150, 251));
    // Cracks, moss, and ferns at its foot.
    p.setPen(QPen(QColor(0x2a, 0x2e, 0x24, 150), 2));
    p.drawPolyline(QPolygonF({ QPointF(196, 120), QPointF(184, 150), QPointF(190, 172), QPointF(176, 206) }));
    mossOnTop(p, 120, 36, 51, 30);
    for (int i = 0; i < 7; ++i)
        frond(p, QPointF(30 + i * 30, 318), -kPi * (0.65 - 0.05 * i), 50, 10, 14, QColor(0x36, 0x7a, 0x30),
              QColor(0x24, 0x48, 0x1a));
    return image;
}

QImage stele()
{
    const int w = 170;
    const int h = 400;
    QImage image = blank(w, h);
    QPainter p(&image);
    begin(p);
    const QColor stone(0x9c, 0x98, 0x88);
    const QColor groove(0x40, 0x3c, 0x30);

    QPainterPath slab;
    slab.moveTo(25, 396);
    slab.lineTo(25, 90);
    slab.arcTo(QRectF(25, 30, 120, 120), 180, -180);
    slab.lineTo(145, 396);
    slab.closeSubpath();
    QLinearGradient shade(25, 0, 145, 0);
    shade.setColorAt(0.0, stone.darker(140));
    shade.setColorAt(0.6, stone.lighter(110));
    shade.setColorAt(1.0, stone.darker(120));
    p.setPen(QPen(QColor(0x30, 0x2e, 0x26, 170), 2));
    p.setBrush(shade);
    p.drawPath(slab);
    p.save();
    p.setClipPath(slab);
    speckle(p, QRectF(25, 30, 120, 366), 120, 61, 40);
    p.restore();
    // Panels of glyphs.
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 2; ++col) {
            const QRectF cell(40 + col * 48, 112 + row * 64, 40, 52);
            QPainterPath box;
            box.addRoundedRect(cell, 6, 6);
            carve(p, box, 3.5, groove);
            QPainterPath mark;
            const QPointF c = cell.center();
            switch ((row * 2 + col) % 4) {
            case 0: mark.addEllipse(c, 9, 9); break;
            case 1:
                mark.moveTo(c + QPointF(-10, 8));
                mark.lineTo(c + QPointF(0, -10));
                mark.lineTo(c + QPointF(10, 8));
                break;
            case 2:
                mark.moveTo(c + QPointF(-11, 0));
                mark.cubicTo(c + QPointF(-5, -12), c + QPointF(5, 12), c + QPointF(11, 0));
                break;
            default:
                mark.addEllipse(c + QPointF(-6, -6), 3, 3);
                mark.addEllipse(c + QPointF(6, 6), 3, 3);
                mark.addEllipse(c + QPointF(6, -6), 3, 3);
                break;
            }
            carve(p, mark, 3, groove);
        }
    }
    mossOnTop(p, 100, 36, 71, 40);
    return image;
}

// ---- plants ----

QImage fern()
{
    QImage image = blank(260, 150);
    QPainter p(&image);
    begin(p);
    for (int i = 0; i < 11; ++i) {
        const double a = -kPi * (0.94 - 0.88 * i / 10.0);
        frond(p, QPointF(130, 148), a, 90 + 40 * noise(i, 81), 30 + 30 * noise(i, 82), 22,
              i % 2 ? QColor(0x2e, 0x6a, 0x2a) : QColor(0x46, 0x8e, 0x38), QColor(0x22, 0x46, 0x1a));
    }
    return image;
}

QImage bush()
{
    QImage image = blank(260, 170);
    QPainter p(&image);
    begin(p);
    for (int i = 0; i < 12; ++i) {
        const double a = -kPi * (0.9 - 0.8 * i / 11.0) + (noise(i, 91) - 0.5) * 0.2;
        const double len = 80 + 50 * noise(i, 92);
        const QPointF base(130, 166);
        const QPointF dir(std::cos(a), std::sin(a));
        const QPointF n(-dir.y(), dir.x());
        const QPointF tip = base + dir * len;
        QPainterPath leaf;
        leaf.moveTo(base);
        leaf.quadTo(base + dir * len * 0.5 + n * len * 0.24, tip);
        leaf.quadTo(base + dir * len * 0.5 - n * len * 0.24, base);
        QLinearGradient g(base, tip);
        g.setColorAt(0.0, QColor(0x1e, 0x44, 0x1c));
        g.setColorAt(1.0, i % 3 == 0 ? QColor(0x6a, 0xa4, 0x44) : QColor(0x4a, 0x8a, 0x36));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawPath(leaf);
        p.setPen(QPen(QColor(0x1a, 0x3a, 0x16, 160), 1.5));
        p.drawLine(base, tip);
    }
    // A few small white flowers.
    for (int i = 0; i < 4; ++i) {
        const QPointF c(70 + 40 * i + 10 * noise(i, 93), 60 + 40 * noise(i, 94));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xf4, 0xf0, 0xe4));
        for (int k = 0; k < 5; ++k) {
            const double a = k * 2.0 * kPi / 5.0;
            p.drawEllipse(c + QPointF(std::cos(a), std::sin(a)) * 5.0, 4, 4);
        }
        p.setBrush(QColor(0xf0, 0xc0, 0x30));
        p.drawEllipse(c, 3, 3);
    }
    return image;
}

// ---- the faces of obstacles ----

// A carved block, 1.38 m wide and 2.6 m tall: cool grey-blue stone, bevelled,
// with one of four carvings at eye height.
QImage blockFace(int look)
{
    const int w = 240;
    const int h = 452;
    QImage image = blank(w, h);
    QPainter p(&image);
    begin(p);
    const QColor stone(0xa6, 0xb4, 0xbe);
    const QColor groove(0x3c, 0x4a, 0x56);

    QLinearGradient face(0, 0, 0, h);
    face.setColorAt(0.0, stone.lighter(116));
    face.setColorAt(0.8, stone.darker(108));
    face.setColorAt(1.0, stone.darker(130));
    p.fillRect(image.rect(), face);
    speckle(p, QRectF(0, 0, w, h), 360, 100 + look, 34);
    // Bevel: light along the top and right, which face the sun.
    p.setPen(QPen(QColor(255, 255, 255, 110), 6));
    p.drawLine(QPointF(6, 6), QPointF(w - 6, 6));
    p.drawLine(QPointF(w - 6, 6), QPointF(w - 6, h - 6));
    p.setPen(QPen(QColor(0, 0, 0, 90), 6));
    p.drawLine(QPointF(6, 6), QPointF(6, h - 6));
    p.drawLine(QPointF(6, h - 6), QPointF(w - 6, h - 6));

    const QPointF c(120, 196);
    QPainterPath mark;
    switch (look) {
    case 0: {   // the sun
        mark.addEllipse(c, 40, 40);
        mark.addEllipse(c, 16, 16);
        for (int i = 0; i < 12; ++i) {
            const double a = i * kPi / 6.0;
            mark.moveTo(c + QPointF(std::cos(a), std::sin(a)) * 52);
            mark.lineTo(c + QPointF(std::cos(a), std::sin(a)) * 72);
        }
        break;
    }
    case 1: {   // a mask
        mark.moveTo(c + QPointF(-52, -36));
        mark.quadTo(c + QPointF(-28, -54), c + QPointF(-6, -36));
        mark.moveTo(c + QPointF(6, -36));
        mark.quadTo(c + QPointF(28, -54), c + QPointF(52, -36));
        for (int side = -1; side <= 1; side += 2) {
            const QPointF e = c + QPointF(side * 28, -14);
            mark.moveTo(e + QPointF(-18, 0));
            mark.quadTo(e + QPointF(0, -14), e + QPointF(18, 0));
            mark.quadTo(e + QPointF(0, 12), e + QPointF(-18, 0));
        }
        mark.moveTo(c + QPointF(0, -6));
        mark.lineTo(c + QPointF(-10, 26));
        mark.lineTo(c + QPointF(10, 26));
        mark.addRoundedRect(QRectF(c + QPointF(-34, 42), QSizeF(68, 26)), 6, 6);
        for (int i = 1; i < 5; ++i) {
            mark.moveTo(c + QPointF(-34 + i * 13.6, 42));
            mark.lineTo(c + QPointF(-34 + i * 13.6, 68));
        }
        break;
    }
    case 2: {   // a coiled serpent
        for (int i = 0; i <= 80; ++i) {
            const double a = i * 0.24;
            const double r = 4.0 + i * 0.78;
            const QPointF at = c + QPointF(std::cos(a) * r, std::sin(a) * r);
            if (i == 0)
                mark.moveTo(at);
            else
                mark.lineTo(at);
        }
        const QPointF head = c + QPointF(std::cos(80 * 0.24) * 66.4, std::sin(80 * 0.24) * 66.4);
        mark.addEllipse(head, 9, 7);
        break;
    }
    default: {  // a panel of glyphs
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 2; ++col) {
                const QRectF cell(c + QPointF(-58 + col * 62, -80 + row * 62), QSizeF(54, 54));
                mark.addRoundedRect(cell, 8, 8);
                const QPointF m = cell.center();
                if ((row + col) % 2 == 0)
                    mark.addEllipse(m, 10, 10);
                else {
                    mark.moveTo(m + QPointF(-12, 10));
                    mark.lineTo(m + QPointF(-4, -10));
                    mark.lineTo(m + QPointF(4, 10));
                    mark.lineTo(m + QPointF(12, -10));
                }
            }
        }
        break;
    }
    }
    carve(p, mark, 7, groove);
    // Cracks.
    p.setPen(QPen(QColor(0x2a, 0x34, 0x3c, 150), 2.2));
    p.drawPolyline(QPolygonF({ QPointF(w, 300 + 20 * look), QPointF(196, 320), QPointF(206, 350),
                               QPointF(170, 392) }));
    p.drawPolyline(QPolygonF({ QPointF(0, 90), QPointF(30, 104), QPointF(22, 128) }));
    mossOnTop(p, w, 0, 110 + look, 60);
    // Damp at the foot.
    QLinearGradient damp(0, h - 70, 0, h);
    damp.setColorAt(0.0, QColor(0x20, 0x30, 0x20, 0));
    damp.setColorAt(1.0, QColor(0x20, 0x30, 0x20, 120));
    p.fillRect(QRectF(0, h - 70, w, 70), damp);
    return image;
}

// A fallen trunk lying across a lane, 1.47 m long and 0.55 m high, seen from
// the side: warm orange bark, the colour that says "jump".
QImage trunkFace(int look)
{
    const int w = 300;
    const int h = 112;
    QImage image = blank(w, h);
    QPainter p(&image);
    begin(p);
    QLinearGradient round(0, 0, 0, h);
    round.setColorAt(0.0, QColor(0xf4, 0x9a, 0x50));
    round.setColorAt(0.3, QColor(0xe4, 0x7c, 0x34));
    round.setColorAt(0.8, QColor(0x9c, 0x4a, 0x1a));
    round.setColorAt(1.0, QColor(0x5e, 0x2a, 0x0e));
    p.fillRect(image.rect(), round);
    // Bark ridges along the trunk.
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 8; ++i) {
        const double y = 10 + i * 12.5;
        QPainterPath ridge;
        ridge.moveTo(0, y);
        for (int k = 1; k <= 12; ++k)
            ridge.lineTo(k * 25.0, y + (noise(i * 13 + k, 120 + look) - 0.5) * 5.0);
        p.setPen(QPen(QColor(0x4a, 0x1e, 0x08, 110), 2.4));
        p.drawPath(ridge);
    }
    // Knots.
    for (int i = 0; i < 2; ++i) {
        const QPointF c(60 + 160 * i + 30 * noise(i, 125 + look), 52);
        p.setPen(QPen(QColor(0x4a, 0x1e, 0x08, 170), 3));
        p.setBrush(QColor(0xb0, 0x5a, 0x22));
        p.drawEllipse(c, 13, 9);
        p.drawEllipse(c, 6, 4);
    }
    // Light along the top, where the sun catches it.
    p.setPen(QPen(QColor(255, 220, 170, 120), 4));
    p.drawLine(QPointF(0, 4), QPointF(w, 4));
    if (look % 2 == 1) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x4e, 0x8c, 0x34, 220));
        p.drawEllipse(QPointF(90, 6), 34, 9);
        p.drawEllipse(QPointF(214, 4), 22, 7);
    }
    if (look == 2) {
        // Small pale mushrooms along the top.
        for (int i = 0; i < 3; ++i) {
            const QPointF c(170 + i * 18.0, 10);
            p.setPen(QPen(QColor(0x4a, 0x2a, 0x10), 1.5));
            p.setBrush(QColor(0xee, 0xe0, 0xc4));
            p.drawRect(QRectF(c.x() - 2, c.y(), 4, 10));
            p.drawChord(QRectF(c.x() - 9, c.y() - 7, 18, 14), 0, 180 * 16);
        }
    }
    return image;
}

// The slab of a gateway, 1.54 m wide and 1.5 m tall: purple stone, a carved
// frieze, and the gold band along its underside, the edge that matters.
QImage gateFace(int look)
{
    const int w = 300;
    const int h = 292;
    const int band = 44;
    QImage image = blank(w, h);
    QPainter p(&image);
    begin(p);
    const QColor stone(0x8e, 0x7c, 0xa6);
    const QColor groove(0x3e, 0x32, 0x52);

    QLinearGradient face(0, 0, 0, h - band);
    face.setColorAt(0.0, stone.lighter(122));
    face.setColorAt(1.0, stone.darker(115));
    p.fillRect(QRectF(0, 0, w, h - band), face);
    speckle(p, QRectF(0, 0, w, h - band), 220, 140 + look, 30);
    // A cornice along the top, with a row of teeth under it.
    p.setBrush(stone.lighter(135));
    p.setPen(QPen(groove, 2));
    p.drawRect(QRectF(0, 0, w, 34));
    p.setBrush(stone.darker(120));
    for (int i = 0; i < 15; ++i)
        p.drawRect(QRectF(6 + i * 20.0, 38, 10, 12));

    QPainterPath frieze;
    const double top = 70;
    const double bottom = h - band - 26;
    const double mid = (top + bottom) / 2.0;
    if (look % 2 == 0) {
        // A stepped fret.
        for (int i = 0; i < 6; ++i) {
            const double x = 14 + i * 45.5;
            frieze.moveTo(x, bottom);
            frieze.lineTo(x, top);
            frieze.lineTo(x + 34, top);
            frieze.lineTo(x + 34, mid + 10);
            frieze.lineTo(x + 14, mid + 10);
            frieze.lineTo(x + 14, mid - 12);
        }
    } else {
        // A zigzag between two rows of discs.
        frieze.moveTo(16, mid + 18);
        for (int i = 1; i <= 12; ++i)
            frieze.lineTo(16 + i * 22.5, (i % 2) ? mid - 18 : mid + 18);
        for (int i = 0; i < 7; ++i) {
            frieze.addEllipse(QPointF(24 + i * 42.0, top + 4), 7, 7);
            frieze.addEllipse(QPointF(24 + i * 42.0, bottom - 4), 7, 7);
        }
    }
    carve(p, frieze, 6, groove);

    // The gold band, with studs, under a dark line.
    QLinearGradient gold(0, h - band, 0, h);
    gold.setColorAt(0.0, QColor(0xff, 0xe0, 0x6a));
    gold.setColorAt(0.5, QColor(0xff, 0xc8, 0x2e));
    gold.setColorAt(1.0, QColor(0xb8, 0x86, 0x10));
    p.setPen(Qt::NoPen);
    p.fillRect(QRectF(0, h - band, w, band), gold);
    p.setPen(QPen(QColor(0x14, 0x0c, 0x06), 5));
    p.drawLine(QPointF(0, h - band), QPointF(w, h - band));
    p.setPen(QPen(QColor(0x7a, 0x58, 0x08), 1.5));
    p.setBrush(QColor(0xff, 0xf0, 0xa0));
    for (int i = 0; i < 9; ++i)
        p.drawEllipse(QPointF(18 + i * 33.0, h - band / 2.0), 6, 6);
    return image;
}

Set build()
{
    Set s;
    s.trees.push_back(finish(broadleaf(0), 4.6, 6.0, true));
    s.trees.push_back(finish(broadleaf(1), 4.6, 6.0, true));
    s.trees.push_back(finish(palm(), 4.0, 5.9, true));
    s.trees.push_back(finish(mossy(), 3.8, 5.9, true));
    s.ruins.push_back(finish(column(), 1.3, 3.0, true));
    s.ruins.push_back(finish(carvedHead(), 2.0, 2.66, true));
    s.ruins.push_back(finish(stele(), 1.3, 3.06, true));
    s.plants.push_back(finish(fern(), 1.8, 1.04, true));
    s.plants.push_back(finish(bush(), 1.8, 1.18, true));
    for (int i = 0; i < 4; ++i) {
        s.blockFaces[size_t(i)] = finish(blockFace(i), 1.0, 1.0, false);
        s.trunkFaces[size_t(i)] = finish(trunkFace(i), 1.0, 1.0, false);
        s.gateFaces[size_t(i)] = finish(gateFace(i), 1.0, 1.0, false);
    }
    return s;
}

} // namespace

void paintSky(QPainter& p, const QRectF& all, const QRectF& canvas, double horizon, double unit)
{
    const double u = unit;
    const double cx = canvas.center().x();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    QLinearGradient sky(QPointF(0, all.top()), QPointF(0, horizon));
    sky.setColorAt(0.0, QColor(0x14, 0x1c, 0x3a));
    sky.setColorAt(0.35, QColor(0x3a, 0x3a, 0x6a));
    sky.setColorAt(0.62, QColor(0x8a, 0x5a, 0x78));
    sky.setColorAt(0.84, QColor(0xe0, 0x8a, 0x5a));
    sky.setColorAt(1.0, kHaze.lighter(108));
    p.fillRect(all, sky);

    const QPointF sun(cx + 50 * u, horizon - 42 * u);
    const double r = 20 * u;
    QRadialGradient glow(sun, r * 7.0);
    glow.setColorAt(0.0, QColor(255, 214, 150, 170));
    glow.setColorAt(0.3, QColor(255, 190, 130, 60));
    glow.setColorAt(1.0, QColor(255, 190, 130, 0));
    p.setBrush(glow);
    p.drawEllipse(sun, r * 7.0, r * 7.0);
    QRadialGradient core(sun, r);
    core.setColorAt(0.0, QColor(0xff, 0xfa, 0xe8));
    core.setColorAt(1.0, QColor(0xff, 0xd4, 0x8a));
    p.setBrush(core);
    p.drawEllipse(sun, r, r);

    // Banks of cloud, lit warm from below by the low sun.
    for (int i = 0; i < 8; ++i) {
        const QPointF c(all.left() + all.width() * noise(i, 201), horizon - (40 + 60 * noise(i, 202)) * u);
        for (int k = 0; k < 6; ++k) {
            const QPointF at = c + QPointF((k - 2.5) * 18 * u, (noise(k, 203 + i) - 0.5) * 6 * u);
            const double w = (22 + 26 * noise(k, 204 + i)) * u;
            p.setBrush(QColor(0x6a, 0x4a, 0x6a, 70));
            p.drawEllipse(at + QPointF(0, 3 * u), w, w * 0.24);
            p.setBrush(QColor(0xf4, 0xb8, 0x94, 80));
            p.drawEllipse(at, w * 0.9, w * 0.2);
        }
    }

    // Two ranges of mountains, the far one paler.
    for (int range = 0; range < 2; ++range) {
        QPainterPath hills;
        hills.moveTo(all.left(), horizon);
        const int points = 40;
        for (int i = 0; i <= points; ++i) {
            const double x = all.left() + all.width() * i / points;
            const double hgt = (range == 0 ? 30 : 16) + (range == 0 ? 22 : 12) * noise(i, 210 + range)
                               + 10 * std::sin(i * 0.7 + range);
            hills.lineTo(x, horizon - hgt * u);
        }
        hills.lineTo(all.right(), horizon);
        hills.closeSubpath();
        p.setBrush(range == 0 ? QColor(0x8a, 0x76, 0x8c) : QColor(0x6e, 0x66, 0x7a));
        p.drawPath(hills);
    }

    // The temple at the end of the path: stepped tiers, lit on the right,
    // a stair up the middle and a shrine on top, all softened by the haze.
    const double tier = 8 * u;
    for (int i = 0; i < 6; ++i) {
        const double w = (150 - 20 * i) * u;
        const QRectF t(cx - w / 2, horizon - (i + 1) * tier, w, tier + 0.5);
        p.setBrush(QColor(0x80, 0x72, 0x7c));
        p.drawRect(t);
        p.setBrush(QColor(0x9e, 0x8c, 0x8c));
        p.drawRect(QRectF(cx, t.top(), w / 2, t.height()));
        p.setBrush(QColor(0xb8, 0xa4, 0x9c));
        p.drawRect(QRectF(t.left(), t.top(), w, 1.2 * u));
    }
    p.setBrush(QColor(0xa8, 0x96, 0x92));
    p.drawRect(QRectF(cx - 9 * u, horizon - 6 * tier, 18 * u, 6 * tier));
    p.setPen(QPen(QColor(0x7a, 0x6c, 0x74), std::max(1.0, 0.8 * u)));
    for (int i = 1; i < 18; ++i)
        p.drawLine(QPointF(cx - 9 * u, horizon - i * tier / 3), QPointF(cx + 9 * u, horizon - i * tier / 3));
    p.setPen(Qt::NoPen);
    const QRectF shrine(cx - 16 * u, horizon - 6 * tier - 16 * u, 32 * u, 16 * u);
    p.setBrush(QColor(0x96, 0x86, 0x88));
    p.drawRect(shrine);
    p.setBrush(QColor(0x3a, 0x2c, 0x34));
    p.drawRect(QRectF(cx - 5 * u, shrine.top() + 5 * u, 10 * u, 11 * u));
    p.setBrush(QColor(0x88, 0x78, 0x80));
    p.drawRect(QRectF(cx - 10 * u, shrine.top() - 9 * u, 20 * u, 9 * u));

    // The jungle either side of the path, in two rows, the nearer darker.
    for (int row = 0; row < 2; ++row) {
        const QColor c = row == 0 ? QColor(0x56, 0x6a, 0x58) : QColor(0x3c, 0x52, 0x40);
        p.setBrush(c);
        for (int side = -1; side <= 1; side += 2) {
            const double from = cx + side * (row == 0 ? 80 : 110) * u;
            for (int i = 0; i < 26; ++i) {
                const double x = from + side * i * (12 + 6 * row) * u;
                const double rr = (12 + 10 * noise(i, 220 + row * 2 + (side > 0)) + 6 * row) * u;
                p.drawEllipse(QPointF(x, horizon - rr * (0.6 + 0.5 * noise(i, 230 + row))), rr, rr * 0.8);
            }
            p.drawRect(QRectF(side < 0 ? all.left() : from, horizon - (10 + 8 * row) * u,
                              side < 0 ? from - all.left() : all.right() - from, (10 + 8 * row) * u));
        }
    }

    QLinearGradient floor(QPointF(0, horizon), QPointF(0, all.bottom()));
    floor.setColorAt(0.0, QColor(0x5a, 0x66, 0x4c));
    floor.setColorAt(0.2, QColor(0x2c, 0x4a, 0x28));
    floor.setColorAt(1.0, QColor(0x12, 0x24, 0x10));
    p.fillRect(QRectF(all.left(), horizon, all.width(), all.bottom() - horizon), floor);
}

void paintCanopy(QPainter& p, const QRectF& canvas, double unit)
{
    const double u = unit;
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor dark(0x0c, 0x1a, 0x0e);
    const QColor mid(0x18, 0x30, 0x18);
    const QColor light(0x2c, 0x4c, 0x26);
    for (int side = -1; side <= 1; side += 2) {
        const double edge = side < 0 ? canvas.left() : canvas.right();
        // Vines first, so the leaves hide where they hang from.
        for (int i = 0; i < 3; ++i) {
            const double x = edge - side * (30 + 40 * i + 12 * noise(i, 240 + (side > 0))) * u;
            const double len = (50 + 60 * noise(i, 242 + (side > 0))) * u;
            QPainterPath vine;
            vine.moveTo(x, canvas.top());
            vine.cubicTo(x + 6 * u, canvas.top() + len * 0.4, x - 6 * u, canvas.top() + len * 0.7, x, canvas.top() + len);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(mid, 2.2 * u, Qt::SolidLine, Qt::RoundCap));
            p.drawPath(vine);
            p.setPen(Qt::NoPen);
            p.setBrush(light);
            for (int k = 1; k <= 5; ++k)
                p.drawEllipse(vine.pointAtPercent(k / 5.0) + QPointF(((k % 2) ? 3 : -3) * u, 0), 4 * u, 2.2 * u);
        }
        for (int i = 0; i < 7; ++i) {
            const double x = edge - side * (i * 26 + 10 * noise(i, 250 + (side > 0))) * u;
            const double y = canvas.top() + (i * 3 - 14 + 12 * noise(i, 252 + (side > 0))) * u;
            leafCluster(p, QPointF(x, y), (40 - 4 * i) * u, dark, mid, light, side > 0);
        }
    }
}

const Set& art()
{
    static const Set set = build();
    return set;
}

void draw(QPainter& p, const Sprite& sprite, const QRectF& target, double mist, double pixelsTall)
{
    size_t size = 0;
    if (pixelsTall < sprite.sizes[2].height() * 1.5)
        size = 2;
    else if (pixelsTall < sprite.sizes[1].height() * 1.5)
        size = 1;
    const int step = sprite.hazes ? int(std::lround(std::clamp(mist, 0.0, 1.0) * (kHazeSteps - 1))) : 0;
    p.drawImage(target, step == 0 ? sprite.sizes[size] : hazedAt(sprite, size, step));
}

} // namespace LaneArt
