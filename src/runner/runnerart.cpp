#include "runnerart.h"

#include "runner/laneart.h"
#include "runner/runnercourse.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace RunnerArt {
namespace {

using Course = RunnerCourse;
constexpr double kPi = std::numbers::pi;
constexpr double kGround = Course::kGroundY;

constexpr QColor kOutline { 0x14, 0x0c, 0x06 };
constexpr QColor kGold { 0xff, 0xd0, 0x3a };
constexpr QColor kRoot { 0xe0, 0x74, 0x2c };
constexpr QColor kStone { 0xa6, 0xb4, 0xbe };
constexpr QColor kWall { 0x8e, 0x7c, 0xa6 };

// A fixed number in 0..1 for (i, salt), so the art is the same every run.
double noise(int i, int salt)
{
    std::uint32_t h = std::uint32_t(i) * 2654435761U ^ (std::uint32_t(salt) * 40503U + 0x7f4a7c15U);
    h ^= h >> 15U;
    h *= 0x2c1b3c6dU;
    h ^= h >> 12U;
    h *= 0x297a2d39U;
    h ^= h >> 15U;
    return double(h) / 4294967296.0;
}

QColor withAlpha(QColor c, int alpha)
{
    c.setAlpha(alpha);
    return c;
}

QImage blank(int w, int h)
{
    QImage image(std::max(1, w), std::max(1, h), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    return image;
}

// Paints `paint(x)` at x, and again a period over where it crosses an end,
// so a strip joins itself without a seam.
template <class F>
void wrapped(double x, double half, double period, F paint)
{
    paint(x);
    if (x - half < 0.0)
        paint(x + period);
    if (x + half > period)
        paint(x - period);
}

// A strip `period` units long from `top` to `bottom`, painted in course
// units. The picture is a whole number of pixels long, so it joins itself.
template <class F>
Strip makeStrip(const Frame& f, double top, double bottom, double period, double factor, F paint)
{
    Strip s;
    s.top = top;
    s.period = period;
    s.factor = factor;
    if (bottom - top < 1.0)
        return s;
    const int w = int(std::lround(period * f.pixels));
    s.image = blank(w, int(std::ceil((bottom - top) * f.pixels)));
    QPainter p(&s.image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setPen(Qt::NoPen);
    p.scale(double(w) / period, f.pixels);
    p.translate(0, -top);
    paint(p);
    return s;
}

template <class F>
Piece makePiece(const Frame& f, const QRectF& box, F paint)
{
    Piece piece;
    piece.offset = box.topLeft();
    piece.image = blank(int(std::ceil(box.width() * f.pixels)), int(std::ceil(box.height() * f.pixels)));
    QPainter p(&piece.image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(f.pixels, f.pixels);
    p.translate(-box.left(), -box.top());
    paint(p);
    return piece;
}

// Lays `colour` over what is already painted, never over the clear.
void tintOver(QImage& image, const QColor& colour)
{
    if (image.isNull())
        return;
    QPainter p(&image);
    p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    p.fillRect(image.rect(), colour);
}

// Haze rising from the foot of a strip: none at `from`, `amount` at its foot.
void mistAtFoot(QImage& image, const QColor& haze, double from, double amount)
{
    if (image.isNull())
        return;
    QPainter p(&image);
    p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    QLinearGradient g(QPointF(0, image.height() * from), QPointF(0, image.height()));
    g.setColorAt(0.0, withAlpha(haze, 0));
    g.setColorAt(1.0, withAlpha(haze, int(255 * amount)));
    p.fillRect(image.rect(), g);
}

// A ridge line that repeats every `period`: a sum of waves that each fit a
// whole number of times. Between -1 and 1.
double ridge(double x, double period, int salt)
{
    constexpr int kWaves[] = { 2, 3, 5, 8, 13, 21 };
    constexpr double kWeight[] = { 1.0, 0.6, 0.45, 0.25, 0.14, 0.07 };
    double h = 0.0;
    double sum = 0.0;
    for (int i = 0; i < 6; ++i) {
        h += kWeight[i] * std::sin(2.0 * kPi * kWaves[i] * x / period + 2.0 * kPi * noise(i, salt));
        sum += kWeight[i];
    }
    return h / sum;
}

void drawSprite(QPainter& p, const LaneArt::Sprite& sprite, double centreX, double baseY, double height,
                double pixels)
{
    const double width = height * sprite.widthMetres / sprite.heightMetres;
    LaneArt::draw(p, sprite, QRectF(centreX - width / 2.0, baseY - height, width, height), 0.0, height * pixels);
}

// ---- the sky ----

QImage paintSky(const Palette& c, const Frame& f, QSize size)
{
    QImage image(size, QImage::Format_RGB32);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.scale(f.pixels, f.pixels);
    p.translate(-f.left, -f.top);
    const QRectF all(f.left, f.top, f.right - f.left, f.bottom - f.top);

    QLinearGradient sky(QPointF(0, std::min(f.top, 0.0)), QPointF(0, kGround - 40.0));
    sky.setColorAt(0.0, c.skyTop);
    sky.setColorAt(0.42, c.skyMid);
    sky.setColorAt(0.8, c.skyLow);
    sky.setColorAt(1.0, c.skyHorizon);
    p.fillRect(all, sky);

    // The last stars, fading towards the dawn.
    for (int i = 0; i < 70; ++i) {
        const QPointF at(all.left() + all.width() * noise(i, 1), all.top() + (kGround - all.top()) * 0.4 * noise(i, 2));
        const double fade = 1.0 - (at.y() - all.top()) / ((kGround - all.top()) * 0.4);
        p.setBrush(QColor(255, 250, 235, int(120 * fade * noise(i, 3))));
        const double r = 0.5 + 0.7 * noise(i, 4);
        p.drawEllipse(at, r, r);
    }

    const QPointF sun(Course::kWidth * 0.72, Course::kHeight * 0.36);
    const double r = Course::kHeight * 0.09;
    QRadialGradient glow(sun, r * 6.0);
    glow.setColorAt(0.0, withAlpha(c.sunGlow, 170));
    glow.setColorAt(0.25, withAlpha(c.sunGlow, 70));
    glow.setColorAt(1.0, withAlpha(c.sunGlow, 0));
    p.setBrush(glow);
    p.drawEllipse(sun, r * 6.0, r * 6.0);
    QRadialGradient core(sun, r);
    core.setColorAt(0.0, c.sun.lighter(112));
    core.setColorAt(1.0, c.sun);
    p.setBrush(core);
    p.drawEllipse(sun, r, r);

    // Banks of cloud, lit from below by the low sun.
    for (int i = 0; i < 9; ++i) {
        const QPointF centre(all.left() + all.width() * noise(i, 10), 30.0 + 140.0 * noise(i, 11));
        for (int k = 0; k < 7; ++k) {
            const QPointF at = centre + QPointF((k - 3) * 15.0, (noise(k, 12 + i) - 0.5) * 6.0);
            const double w = 16.0 + 22.0 * noise(k, 30 + i);
            p.setBrush(withAlpha(c.cloudShade, 90));
            p.drawEllipse(at + QPointF(0, 3.0), w, w * 0.26);
            p.setBrush(withAlpha(c.cloudLit, 95));
            p.drawEllipse(at + QPointF(0, 1.0), w * 0.9, w * 0.2);
        }
    }
    return image;
}

// ---- scenery strips ----

Strip ridges(const Palette& c, const Frame& f)
{
    constexpr double kPeriod = 1280.0;
    Strip s = makeStrip(f, 40.0, kGround + 2.0, kPeriod, 0.05, [&](QPainter& p) {
        for (int range = 0; range < 2; ++range) {
            const double base = kGround - (range == 0 ? 100.0 : 58.0);
            const double amp = range == 0 ? 70.0 : 42.0;
            QPainterPath hills;
            hills.moveTo(0, kGround + 2.0);
            for (int k = 0; k * 4.0 <= kPeriod; ++k)
                hills.lineTo(k * 4.0, base - amp * (0.55 + 0.45 * ridge(k * 4.0, kPeriod, 50 + range)));
            hills.lineTo(kPeriod, kGround + 2.0);
            hills.closeSubpath();
            const QColor body = range == 0 ? c.ridgeFar : c.ridgeNear;
            QLinearGradient g(QPointF(0, base - amp), QPointF(0, kGround));
            g.setColorAt(0.0, body.lighter(108));
            g.setColorAt(1.0, body);
            p.setBrush(g);
            p.drawPath(hills);
            if (c.snowCaps) {
                QPainterPath line;
                line.moveTo(0, base - amp * 1.2);
                for (int k = 0; k * 4.0 <= kPeriod; ++k) {
                    const double x = k * 4.0;
                    line.lineTo(x, base - amp * (0.72 + 0.04 * std::sin(x * 0.11)) + 3.0 * ridge(x, kPeriod, 70));
                }
                line.lineTo(kPeriod, base - amp * 1.2);
                line.closeSubpath();
                p.setBrush(QColor(0xf4, 0xf6, 0xfa, range == 0 ? 200 : 230));
                p.drawPath(hills.intersected(line));
            }
        }
    });
    mistAtFoot(s.image, c.haze, 0.45, 0.8);
    return s;
}

Strip temples(const Palette& c, const Frame& f)
{
    constexpr double kPeriod = 1280.0;
    Strip s = makeStrip(f, 80.0, kGround + 2.0, kPeriod, 0.13, [&](QPainter& p) {
        const double foot = kGround - 44.0;
        for (int t = 0; t < 3; ++t) {
            const double base = 120.0 + 50.0 * noise(t, 80);
            const int tiers = 5 + int(noise(t, 81) * 2.0);
            const double tier = 13.0 + 3.0 * noise(t, 82);
            const double centre = 180.0 + 420.0 * t + 70.0 * noise(t, 83);
            wrapped(centre, base / 2.0 + 30.0, kPeriod, [&](double x) {
                for (int i = 0; i < tiers; ++i) {
                    const double w = base * (1.0 - 0.15 * i);
                    const QRectF r(x - w / 2.0, foot - tier * (i + 1), w, tier + 0.5);
                    p.setBrush(c.temple);
                    p.drawRect(r);
                    p.setBrush(c.templeLit);
                    p.drawRect(QRectF(x + w * 0.1, r.top(), w * 0.4, r.height()));
                    p.setBrush(c.templeLit.lighter(112));
                    p.drawRect(QRectF(r.left(), r.top(), r.width(), 1.4));
                }
                const double stair = base * 0.16;
                const double top = foot - tier * tiers;
                p.setBrush(c.temple.darker(112));
                p.drawRect(QRectF(x - stair / 2.0, top, stair, foot - top));
                p.setPen(QPen(c.temple.darker(135), 0.9));
                for (int k = 1; top + k * 3.0 < foot; ++k)
                    p.drawLine(QPointF(x - stair / 2.0, top + k * 3.0), QPointF(x + stair / 2.0, top + k * 3.0));
                p.setPen(Qt::NoPen);
                const QRectF shrine(x - base * 0.13, top - 20.0, base * 0.26, 20.0);
                p.setBrush(c.templeLit);
                p.drawRect(shrine);
                p.setBrush(c.temple.darker(170));
                p.drawRect(QRectF(x - 4.0, shrine.top() + 7.0, 8.0, 13.0));
                p.setBrush(c.temple);
                p.drawRect(QRectF(shrine.left() - 3.0, shrine.top() - 5.0, shrine.width() + 6.0, 5.0));
            });
            // The jungle growing round its foot.
            for (int k = 0; k < 7; ++k) {
                const double x = centre + (k - 3) * base * 0.22 + 8.0 * noise(k, 84 + t);
                wrapped(x, 30.0, kPeriod, [&](double at) {
                    LaneArt::leafCluster(p, QPointF(at, foot - 4.0 - 10.0 * noise(k, 86 + t)),
                                         12.0 + 9.0 * noise(k, 88 + t), c.leafDark, c.leafMid, c.leafLight, true);
                });
            }
        }
    });
    tintOver(s.image, withAlpha(c.haze, 120));
    mistAtFoot(s.image, c.haze, 0.5, 0.7);
    return s;
}

Strip midJungle(const Palette& c, const Frame& f)
{
    constexpr double kPeriod = 1280.0;
    const LaneArt::Set& art = LaneArt::art();
    Strip s = makeStrip(f, 50.0, kGround + 2.0, kPeriod, 0.32, [&](QPainter& p) {
        for (int i = 0; i < 6; ++i) {
            const LaneArt::Sprite& tree = art.trees[size_t(i) % art.trees.size()];
            const double height = 110.0 + 45.0 * noise(i, 100);
            const double width = height * tree.widthMetres / tree.heightMetres;
            const double x = i * (kPeriod / 6.0) + 60.0 * noise(i, 101);
            wrapped(x, width / 2.0, kPeriod, [&](double at) {
                drawSprite(p, tree, at, kGround - 36.0, height, f.pixels);
            });
        }
        // Ruins stand back here, in the haze, their feet hidden in
        // the undergrowth, so none is mistaken for an obstacle on the path.
        for (int i = 0; i < 4; ++i) {
            const LaneArt::Sprite& ruin = art.ruins[size_t(i) % art.ruins.size()];
            const double height = (i % 3 == 1 ? 56.0 : 70.0) + 10.0 * noise(i, 112);
            const double width = height * ruin.widthMetres / ruin.heightMetres;
            const double x = 140.0 + 320.0 * i + 60.0 * noise(i, 113);
            wrapped(x, width / 2.0, kPeriod, [&](double at) {
                drawSprite(p, ruin, at, kGround - 26.0, height, f.pixels);
            });
        }
        // The canopy in front of their trunks.
        for (int i = 0; i * 24.0 < kPeriod; ++i) {
            const double x = i * 24.0;
            const double r = 22.0 + 14.0 * noise(i, 102);
            wrapped(x, r * 1.4, kPeriod, [&](double at) {
                LaneArt::leafCluster(p, QPointF(at, kGround - 58.0 - 16.0 * noise(i, 103)), r, c.leafDark, c.leafMid,
                                     c.leafLight, true);
            });
        }
        p.setBrush(c.leafDark);
        p.drawRect(QRectF(0, kGround - 52.0, kPeriod, 54.0));
    });
    tintOver(s.image, withAlpha(c.haze, 105));
    mistAtFoot(s.image, c.haze, 0.55, 0.55);
    return s;
}

Strip nearJungle(const Palette& c, const Frame& f)
{
    constexpr double kPeriod = 1280.0;
    const LaneArt::Set& art = LaneArt::art();
    Strip s = makeStrip(f, f.top, kGround + 4.0, kPeriod, 0.6, [&](QPainter& p) {
        for (int i = 0; i < 3; ++i) {
            const LaneArt::Sprite& tree = art.trees[size_t(i * 3 + 1) % art.trees.size()];
            const double height = 260.0 + 60.0 * noise(i, 110);
            const double width = height * tree.widthMetres / tree.heightMetres;
            const double x = 150.0 + 420.0 * i + 80.0 * noise(i, 111);
            wrapped(x, width / 2.0, kPeriod, [&](double at) {
                drawSprite(p, tree, at, kGround + 6.0, height, f.pixels);
            });
        }
        for (int i = 0; i < 14; ++i) {
            const LaneArt::Sprite& plant = art.plants[size_t(i) % art.plants.size()];
            const double height = 26.0 + 16.0 * noise(i, 114);
            const double width = height * plant.widthMetres / plant.heightMetres;
            const double x = i * (kPeriod / 14.0) + 50.0 * noise(i, 115);
            wrapped(x, width / 2.0, kPeriod, [&](double at) {
                drawSprite(p, plant, at, kGround + 4.0, height, f.pixels);
            });
        }
    });
    // In the shade of the canopy, and a little into the haze.
    tintOver(s.image, QColor(8, 16, 10, 70));
    tintOver(s.image, withAlpha(c.haze, 30));
    return s;
}

Strip ground(const Palette& c, const Frame& f)
{
    constexpr double kPeriod = 960.0;
    const double bottom = std::max(f.bottom + 1.0, kGround + 40.0);
    return makeStrip(f, kGround - 12.0, bottom, kPeriod, 1.0, [&](QPainter& p) {
        // The earth.
        QLinearGradient earth(QPointF(0, kGround + 18.0), QPointF(0, std::max(bottom, kGround + 120.0)));
        earth.setColorAt(0.0, c.earthTop);
        earth.setColorAt(1.0, c.earthBottom);
        p.setBrush(earth);
        p.drawRect(QRectF(0, kGround + 18.0, kPeriod, bottom - kGround));
        for (int band = 0; band < 4; ++band) {
            QPainterPath layer;
            const double y = kGround + 52.0 + band * 30.0;
            layer.moveTo(0, y);
            const int steps = int(kPeriod / 8.0);
            for (int k = 0; k <= steps; ++k)
                layer.lineTo(k * 8.0, y + 6.0 * ridge(k * 8.0, kPeriod, 120 + band));
            for (int k = steps; k >= 0; --k)
                layer.lineTo(k * 8.0, y + 9.0 + 5.0 * ridge(k * 8.0, kPeriod, 130 + band));
            layer.closeSubpath();
            p.setBrush(withAlpha(c.earthBottom, 70));
            p.drawPath(layer);
        }
        // Roots threading the earth.
        p.setBrush(Qt::NoBrush);
        for (int i = 0; i < 4; ++i) {
            const double x = kPeriod * noise(i, 140);
            wrapped(x, 50.0, kPeriod, [&](double at) {
                QPainterPath root;
                root.moveTo(at, kGround + 30.0);
                root.cubicTo(at + 20.0 * noise(i, 141), kGround + 50.0, at - 30.0 * noise(i, 142), kGround + 60.0,
                             at + 40.0 * (noise(i, 143) - 0.5), kGround + 80.0 + 30.0 * noise(i, 144));
                p.setPen(QPen(c.earthBottom.darker(120), 2.4, Qt::SolidLine, Qt::RoundCap));
                p.drawPath(root);
                p.setPen(QPen(c.earthTop.lighter(115), 0.9, Qt::SolidLine, Qt::RoundCap));
                p.drawPath(root.translated(-0.6, -0.8));
            });
        }
        p.setPen(Qt::NoPen);
        // Pebbles.
        for (int i = 0; i < 80; ++i) {
            const double x = kPeriod * noise(i, 150);
            const double y = kGround + 40.0 + (bottom - kGround - 44.0) * noise(i, 151);
            const double r = 1.6 + 4.0 * noise(i, 152);
            wrapped(x, r, kPeriod, [&](double at) {
                p.setBrush(c.earthTop.darker(140));
                p.drawEllipse(QPointF(at, y + 0.8), r, r * 0.7);
                p.setBrush(c.earthTop.lighter(110 + int(30 * noise(i, 153))));
                p.drawEllipse(QPointF(at, y), r * 0.9, r * 0.62);
                p.setBrush(QColor(255, 255, 255, 40));
                p.drawEllipse(QPointF(at - r * 0.3, y - r * 0.25), r * 0.35, r * 0.2);
            });
        }

        // Two courses of worn blocks along the top: the path, and the wall
        // that holds it up.
        const auto course = [&](double top, double height, double shortest, double spread, int salt, int darker) {
            double x = 0.0;
            int i = 0;
            while (x < kPeriod - 0.01) {
                double w = shortest + spread * noise(i, salt);
                if (kPeriod - x - w < shortest)
                    w = kPeriod - x;
                const QRectF stone(x + 0.6, top + 0.5, w - 1.2, height - 1.0);
                const QColor face = c.path.darker(darker).lighter(92 + int(18 * noise(i, salt + 1)));
                QLinearGradient g(stone.topLeft(), stone.bottomLeft());
                g.setColorAt(0.0, face.lighter(112));
                g.setColorAt(1.0, face.darker(112));
                p.setBrush(g);
                p.drawRoundedRect(stone, 2.0, 2.0);
                p.setBrush(QColor(255, 255, 255, 55));
                p.drawRect(QRectF(stone.left() + 1.0, stone.top() + 0.4, stone.width() - 2.0, 1.4));
                p.setBrush(QColor(0, 0, 0, 45));
                p.drawRect(QRectF(stone.left() + 1.0, stone.bottom() - 1.6, stone.width() - 2.0, 1.4));
                if (noise(i, salt + 2) < 0.35) {
                    QPainterPath crack;
                    const double cx = stone.left() + stone.width() * (0.2 + 0.6 * noise(i, salt + 3));
                    crack.moveTo(cx, stone.top() + 1.0);
                    crack.lineTo(cx + 3.0, stone.top() + height * 0.4);
                    crack.lineTo(cx - 1.0, stone.top() + height * 0.7);
                    crack.lineTo(cx + 4.0, stone.bottom() - 1.0);
                    LaneArt::carve(p, crack, 0.9, c.pathJoint);
                    p.setPen(Qt::NoPen);
                }
                if (noise(i, salt + 4) < 0.3) {
                    p.setBrush(c.moss);
                    p.drawEllipse(QPointF(stone.left() + 1.0, top + 2.0), 4.5, 2.4);
                    p.setBrush(c.moss.lighter(125));
                    p.drawEllipse(QPointF(stone.left() + 2.0, top + 1.2), 2.4, 1.2);
                }
                x += w;
                ++i;
            }
        };
        p.setBrush(c.pathJoint);
        p.drawRect(QRectF(0, kGround, kPeriod, 32.0));
        course(kGround, 18.0, 38.0, 26.0, 160, 100);
        course(kGround + 18.0, 14.0, 28.0, 24.0, 170, 125);
        // A shadow under the wall's foot.
        const double foot = kGround + 32.0;
        const double shade = 14.0;
        QLinearGradient under(QPointF(0, foot), QPointF(0, foot + shade));
        under.setColorAt(0.0, QColor(0, 0, 0, 120));
        under.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setBrush(under);
        p.drawRect(QRectF(0, foot, kPeriod, shade));

        // The edge everything stands on, then grass along it.
        p.setPen(QPen(kOutline, f.outline));
        p.drawLine(QPointF(0, kGround), QPointF(kPeriod, kGround));
        p.setPen(Qt::NoPen);
        for (int i = 0; i * 7.0 < kPeriod; ++i) {
            const double x = i * 7.0 + 3.0 * noise(i, 180);
            const int blades = 2 + int(noise(i, 181) * 4.0);
            wrapped(x, 8.0, kPeriod, [&](double at) {
                for (int b = 0; b < blades; ++b) {
                    const double h = 3.0 + 7.0 * noise(i * 7 + b, 182);
                    const double lean = (noise(i * 7 + b, 183) - 0.5) * 5.0;
                    const double bx = at + b * 1.6;
                    p.setPen(QPen(b % 2 ? c.grass : c.grassLight, 1.3, Qt::SolidLine, Qt::RoundCap));
                    p.drawLine(QPointF(bx, kGround + 1.0), QPointF(bx + lean, kGround - h));
                }
                if (noise(i, 184) < 0.07) {
                    p.setPen(Qt::NoPen);
                    const QPointF head(at + 2.0, kGround - 7.0);
                    p.setPen(QPen(c.grass.darker(120), 0.9));
                    p.drawLine(QPointF(at + 2.0, kGround), head);
                    p.setPen(Qt::NoPen);
                    p.setBrush(c.flower);
                    for (int k = 0; k < 5; ++k) {
                        const double a = k * 2.0 * kPi / 5.0;
                        p.drawEllipse(head + QPointF(std::cos(a), std::sin(a)) * 1.6, 1.3, 1.3);
                    }
                    p.setBrush(QColor(0xff, 0xd8, 0x50));
                    p.drawEllipse(head, 0.9, 0.9);
                }
            });
        }
    });
}

Strip front(const Palette& c, const Frame& f)
{
    constexpr double kPeriod = 1280.0;
    const double top = kGround + 36.0;
    const double bottom = f.bottom + 1.0;
    return makeStrip(f, top, bottom, kPeriod, 1.3, [&](QPainter& p) {
        const double room = bottom - top - 6.0;
        if (room < 14.0)
            return;
        for (int i = 0; i < 12; ++i) {
            const double x = i * (kPeriod / 12.0) + 50.0 * noise(i, 200);
            const double length = room * (0.55 + 0.45 * noise(i, 201));
            wrapped(x, length, kPeriod, [&](double at) {
                const int fronds = 5 + int(noise(i, 202) * 3.0);
                for (int k = 0; k < fronds; ++k) {
                    const double a = -kPi * (0.12 + 0.76 * (k + 0.5) / fronds);
                    const double len = length * (0.7 + 0.3 * noise(i * 9 + k, 203));
                    LaneArt::frond(p, QPointF(at, bottom + 4.0), a, len, len * 0.25, len * 0.2, c.frontLeaf,
                                   c.frontLeaf.lighter(130));
                }
            });
        }
    });
}

Strip vines(const Palette& c, const Frame& f)
{
    constexpr double kPeriod = 1280.0;
    const double reach = 120.0;
    return makeStrip(f, f.top, std::max(f.top + 40.0, reach), kPeriod, 1.12, [&](QPainter& p) {
        const QColor dark = c.leafDark.darker(150);
        const QColor mid = c.leafDark;
        const QColor light = c.leafMid.darker(110);
        for (int i = 0; i < 5; ++i) {
            const double x = i * (kPeriod / 5.0) + 90.0 * noise(i, 210);
            const double end = std::max(f.top + 30.0, 20.0 + 90.0 * noise(i, 211));
            wrapped(x, 20.0, kPeriod, [&](double at) {
                QPainterPath vine;
                vine.moveTo(at, f.top);
                vine.cubicTo(at + 8.0, f.top + (end - f.top) * 0.35, at - 8.0, f.top + (end - f.top) * 0.7, at + 2.0,
                             end);
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(mid, 1.6, Qt::SolidLine, Qt::RoundCap));
                p.drawPath(vine);
                p.setPen(Qt::NoPen);
                const int leaves = std::max(2, int((end - f.top) / 10.0));
                for (int k = 1; k <= leaves; ++k) {
                    const QPointF at2 = vine.pointAtPercent(double(k) / leaves);
                    p.setBrush(k % 3 ? light : mid);
                    p.save();
                    p.translate(at2);
                    p.rotate(k % 2 ? 35.0 : -35.0);
                    p.drawEllipse(QPointF(k % 2 ? 3.5 : -3.5, 0), 4.2, 2.0);
                    p.restore();
                }
            });
        }
        for (int i = 0; i * 20.0 < kPeriod; ++i) {
            const double x = i * 20.0;
            const double r = 12.0 + 10.0 * noise(i, 212);
            wrapped(x, r * 1.3, kPeriod, [&](double at) {
                LaneArt::leafCluster(p, QPointF(at, f.top - 6.0 + 8.0 * noise(i, 213)), r, dark, mid, light, true);
            });
        }
    });
}

// ---- light ----

QImage paintLight(const Palette& c, const Frame& f, QSize size)
{
    QImage image = blank(size.width(), size.height());
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.scale(f.pixels, f.pixels);
    p.translate(-f.left, -f.top);
    const QPointF sun(Course::kWidth * 0.72, Course::kHeight * 0.36);
    p.save();
    p.setClipRect(QRectF(f.left, f.top, f.right - f.left, kGround - f.top));
    for (int i = 0; i < 7; ++i) {
        const double a = kPi * (0.56 + 0.055 * i + 0.02 * noise(i, 220));
        const double spread = 0.018 + 0.014 * noise(i, 221);
        const double len = 700.0;
        const QPointF a1 = sun + QPointF(std::cos(a - spread), std::sin(a - spread)) * len;
        const QPointF a2 = sun + QPointF(std::cos(a + spread), std::sin(a + spread)) * len;
        QLinearGradient ray(sun, (a1 + a2) / 2.0);
        ray.setColorAt(0.0, withAlpha(c.shaft, 0));
        ray.setColorAt(0.12, withAlpha(c.shaft, 42));
        ray.setColorAt(0.6, withAlpha(c.shaft, 16));
        ray.setColorAt(1.0, withAlpha(c.shaft, 0));
        p.setBrush(ray);
        p.drawPolygon(QPolygonF({ sun, a1, a2 }));
    }
    p.restore();
    QRadialGradient vignette(QPointF(Course::kWidth * 0.5, Course::kHeight * 0.52), 520.0);
    vignette.setColorAt(0.5, QColor(10, 5, 0, 0));
    vignette.setColorAt(1.0, QColor(10, 5, 0, 150));
    p.setBrush(vignette);
    p.drawRect(QRectF(f.left, f.top, f.right - f.left, f.bottom - f.top));
    return image;
}

} // namespace

QImage softDot(int size, const QColor& colour)
{
    QImage image = blank(size, size);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    QRadialGradient g(QPointF(size / 2.0, size / 2.0), size / 2.0);
    g.setColorAt(0.0, colour);
    g.setColorAt(1.0, withAlpha(colour, 0));
    p.setBrush(g);
    p.drawEllipse(QPointF(size / 2.0, size / 2.0), size / 2.0, size / 2.0);
    return image;
}

namespace {

// A soft shadow on the ground at `centreX`, `half` units wide either side.
void groundShadow(QPainter& p, double centreX, double half, int alpha)
{
    p.save();
    p.translate(centreX, 0.5);
    p.scale(1.0, 0.16);
    QRadialGradient g(QPointF(0, 0), half);
    g.setColorAt(0.0, QColor(0, 0, 0, alpha));
    g.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(QPointF(0, 0), half, half);
    p.restore();
}

// ---- obstacles ----

void outlinedLine(QPainter& p, QPointF a, QPointF b, double width, const QColor& colour, double outline)
{
    p.setPen(QPen(kOutline, width + 2.0 * outline, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(a, b);
    p.setPen(QPen(colour, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(a, b);
}

void leaf(QPainter& p, QPointF at, double angle, double length, double outline)
{
    p.save();
    p.translate(at);
    p.rotate(angle);
    p.setPen(QPen(kOutline, outline * 0.5));
    p.setBrush(QColor(0x5a, 0xa8, 0x3a));
    p.drawEllipse(QPointF(length / 2.0, 0), length / 2.0, length * 0.22);
    p.restore();
}

Piece stone(const Frame& f, int look)
{
    const double w = Course::kStoneWidth;
    const double h = Course::kStoneHeight;
    return makePiece(f, QRectF(-14.0, -h - 8.0, w + 28.0, h + 14.0), [&](QPainter& p) {
        const double o = f.outline;
        groundShadow(p, w / 2.0, w * 0.85, 130);
        const QRectF block(0, -h, w, h);
        QLinearGradient face(block.topLeft(), block.bottomLeft());
        face.setColorAt(0.0, kStone.lighter(126));
        face.setColorAt(1.0, kStone.darker(122));
        p.setPen(QPen(kOutline, o * 1.6));
        p.setBrush(face);
        p.drawRoundedRect(block, 4, 4);
        LaneArt::speckle(p, block.adjusted(3, 3, -3, -3), 26, 230 + look, 36);
        // Worn edges: light along the top and left, dark along the right
        // and foot.
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 120), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF({ QPointF(2.6, -3.0), QPointF(2.6, -h + 2.6), QPointF(w - 3.0, -h + 2.6) }));
        p.setPen(QPen(QColor(0, 0, 0, 80), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF({ QPointF(w - 2.6, -h + 3.5), QPointF(w - 2.6, -2.6), QPointF(3.5, -2.6) }));
        if (look % 2 == 0) {
            p.setPen(Qt::NoPen);
            p.setBrush(kStone.darker(150));
            p.drawPolygon(QPolygonF({ QPointF(w - 11.0, -h + 0.8), QPointF(w - 0.8, -h + 9.0),
                                      QPointF(w - 0.8, -h + 2.0), QPointF(w - 3.0, -h + 0.8) }));
        } else {
            QPainterPath crack;
            crack.moveTo(w * 0.18, -h + 1.0);
            crack.lineTo(w * 0.28, -h * 0.82);
            crack.lineTo(w * 0.22, -h * 0.66);
            crack.lineTo(w * 0.32, -h * 0.55);
            LaneArt::carve(p, crack, 1.2, kStone.darker(175));
        }
        // The carving: one of four marks, cut into the face.
        const QPointF c(w / 2.0, -h / 2.0 + 3.0);
        const QColor groove = kStone.darker(190);
        QPainterPath mark;
        switch (look) {
        case 0:
            for (int i = 0; i <= 40; ++i) {
                const double a = i * 0.42;
                const double r = 1.0 + i * 0.27;
                const QPointF pt = c + QPointF(std::cos(a) * r, std::sin(a) * r);
                if (i == 0)
                    mark.moveTo(pt);
                else
                    mark.lineTo(pt);
            }
            break;
        case 1:
            mark.addEllipse(c, 10, 6);
            mark.addEllipse(c, 2.5, 2.5);
            break;
        case 2:
            mark.addEllipse(c, 5, 5);
            for (int i = 0; i < 8; ++i) {
                const double a = i * kPi / 4.0;
                mark.moveTo(c + QPointF(std::cos(a) * 7, std::sin(a) * 7));
                mark.lineTo(c + QPointF(std::cos(a) * 11, std::sin(a) * 11));
            }
            break;
        default:
            mark.moveTo(c + QPointF(-12, 4));
            for (int i = 1; i <= 6; ++i)
                mark.lineTo(c + QPointF(-12 + i * 4, (i % 2) ? -4 : 4));
            break;
        }
        LaneArt::carve(p, mark, 2.6, groove);
        // Moss on top, with a drip or two.
        p.setPen(Qt::NoPen);
        const QColor moss(0x5c, 0xa0, 0x3a);
        for (int i = 0; i < 6; ++i) {
            const double x = 3.0 + i * 6.8 + 2.0 * noise(i, 240 + look);
            p.setBrush(moss.darker(118));
            p.drawEllipse(QPointF(x, -h + 1.0), 5.2, 3.2);
            const double drip = 9.0 * noise(i, 250 + look);
            if (drip > 4.0)
                p.drawRoundedRect(QRectF(x - 1.4, -h, 2.8, drip), 1.4, 1.4);
            p.setBrush(moss.lighter(122));
            p.drawEllipse(QPointF(x + 1.0, -h - 0.4), 3.0, 1.6);
        }
    });
}

Piece root(const Frame& f, int look)
{
    const double w = Course::kRootWidth;
    const double h = Course::kRootHeight;
    return makePiece(f, QRectF(-14.0, -h * 1.5 - 10.0, w + 28.0, h * 1.5 + 16.0), [&](QPainter& p) {
        const double o = f.outline;
        const double bend = 4.0 * (look - 1.5);
        groundShadow(p, w / 2.0, w * 0.8, 100);
        // Earth heaved up where it breaks the path.
        p.setPen(QPen(kOutline, o * 0.7));
        p.setBrush(QColor(0x4e, 0x32, 0x1a));
        p.drawEllipse(QPointF(3.0, 1.5), 8.0, 4.0);
        p.drawEllipse(QPointF(w - 3.0, 1.5), 8.0, 4.0);
        QPainterPath arc;
        arc.moveTo(2, 2);
        arc.cubicTo(QPointF(4 + bend, -h * 1.25), QPointF(w - 6 + bend, -h * 1.2), QPointF(w - 2, 2));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(kOutline, 9.0 + 2.0 * o, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(arc);
        p.setPen(QPen(kRoot, 9.0, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(arc);
        p.setPen(QPen(kRoot.darker(135), 3.4, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(arc.translated(0.4, 2.4));
        p.setPen(QPen(kRoot.lighter(140), 2.0, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(arc.translated(-1.0, -2.5));
        // Bark: short grooves across the grain, and a knot.
        p.setPen(QPen(kRoot.darker(165), 1.0, Qt::SolidLine, Qt::RoundCap));
        for (int k = 0; k < 9; ++k) {
            const double t = 0.14 + 0.085 * k;
            const QPointF at = arc.pointAtPercent(t);
            const double a = -arc.angleAtPercent(t) * kPi / 180.0 + kPi / 2.0 + 0.4 * (noise(k, 260) - 0.5);
            const QPointF d(std::cos(a) * 2.6, std::sin(a) * 2.6);
            p.drawLine(at - d, at + d);
        }
        const QPointF knot = arc.pointAtPercent(0.5);
        p.setBrush(kRoot.darker(150));
        p.setPen(QPen(kRoot.darker(185), 0.8));
        p.drawEllipse(knot, 2.0, 1.5);
        // A rootlet or two, and a sprig of leaves.
        const QPointF from(w * 0.6, -h * 0.7);
        const QPointF tip(w * 0.85, -h * 1.05);
        outlinedLine(p, from, tip, 3.0, kRoot, o);
        if (look % 2 == 1)
            outlinedLine(p, QPointF(w * 0.3, -h * 0.75), QPointF(w * 0.18, -h * 1.0), 2.4, kRoot, o);
        leaf(p, tip, -60.0 - 10.0 * look, 7.0, o);
        leaf(p, tip, -10.0 + 8.0 * look, 6.0, o);
    });
}

Piece wall(const Frame& f, int look)
{
    const double w = Course::kArchWidth;
    const double top = f.top - 10.0 - kGround;
    const double under = -Course::kArchClearance;
    return makePiece(f, QRectF(-14.0, top, w + 28.0, 8.0 - top), [&](QPainter& p) {
        const double o = f.outline;
        groundShadow(p, w / 2.0, w * 0.75, 70);
        const QRectF body(0, top, w, under - top);
        // Mortar first; the blocks are laid over it.
        p.setPen(QPen(kOutline, o * 1.6));
        p.setBrush(kWall.darker(155));
        p.drawRect(body);
        p.setPen(Qt::NoPen);
        const double courseHeight = 22.0;
        int row = 0;
        for (; under - 20.0 - row * courseHeight > top; ++row) {
            const double y = under - 20.0 - row * courseHeight;
            const double y0 = std::max(top, y - courseHeight);
            const std::vector<double> joints = row % 2 ? std::vector<double> { 0.0, w * 0.25, w * 0.75, w }
                                                       : std::vector<double> { 0.0, w * 0.5, w };
            for (size_t k = 0; k + 1 < joints.size(); ++k) {
                const int id = row * 5 + int(k);
                const QRectF block = QRectF(joints[k], y0, joints[k + 1] - joints[k], y - y0).adjusted(1.1, 1.1, -1.1, -1.1);
                if (block.width() < 2.0 || block.height() < 2.0)
                    continue;
                p.setBrush(kWall.lighter(92 + int(22 * noise(id, 270 + look))));
                p.drawRect(block);
                p.setBrush(QColor(255, 255, 255, 55));
                p.drawRect(QRectF(block.left(), block.top(), block.width(), 1.3));
                p.setBrush(QColor(0, 0, 0, 60));
                p.drawRect(QRectF(block.left(), block.bottom() - 1.5, block.width(), 1.5));
                if (noise(id, 280 + look) < 0.22) {
                    p.setBrush(QColor(0x5c, 0x9a, 0x3a));
                    p.drawEllipse(QPointF(block.left() + block.width() * 0.3, block.top() + 1.0), 5.0, 2.6);
                    p.drawEllipse(QPointF(block.left() + block.width() * 0.5, block.top() + 1.5), 3.6, 2.0);
                }
            }
        }
        // The shaded side, so the wall reads as a block, not a sheet.
        p.setBrush(QColor(0, 0, 0, 55));
        p.drawRect(QRectF(w - 5.0, top, 5.0, under - 20.0 - top));
        // Vines hanging down the face, stopping above the lintel.
        for (int v = 0; v < 2; ++v) {
            const double x = w * (0.18 + 0.6 * v + 0.12 * noise(v, 290 + look));
            const double end = under - 26.0 - 40.0 * noise(v, 292 + look);
            if (end <= top + 10.0)
                continue;
            QPainterPath vine;
            vine.moveTo(x, top);
            vine.cubicTo(x + 5.0, top + (end - top) * 0.4, x - 5.0, top + (end - top) * 0.7, x, end);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0x2a, 0x50, 0x24), 1.8, Qt::SolidLine, Qt::RoundCap));
            p.drawPath(vine);
            p.setPen(Qt::NoPen);
            const int leaves = std::max(2, int((end - top) / 9.0));
            for (int k = 1; k <= leaves; ++k) {
                const QPointF at = vine.pointAtPercent(double(k) / leaves);
                p.setBrush(k % 2 ? QColor(0x4c, 0x8c, 0x34) : QColor(0x3a, 0x70, 0x2a));
                p.drawEllipse(at + QPointF(k % 2 ? 2.8 : -2.8, 0), 3.4, 1.8);
            }
        }
        // The carved lintel at the bottom, with the gold band under it.
        const QRectF lintel(-4, under - 20, w + 8, 20);
        QLinearGradient lit(lintel.topLeft(), lintel.bottomLeft());
        lit.setColorAt(0.0, kWall.lighter(140));
        lit.setColorAt(1.0, kWall.lighter(118));
        p.setPen(QPen(kOutline, o * 1.6));
        p.setBrush(lit);
        p.drawRect(lintel);
        QPainterPath zig;
        for (int i = 0; i < 5; ++i) {
            const double x = lintel.left() + 6 + i * (lintel.width() - 12) / 4.0;
            zig.moveTo(x - 4, lintel.center().y() + 2);
            zig.lineTo(x, lintel.center().y() - 5);
            zig.lineTo(x + 4, lintel.center().y() + 2);
        }
        LaneArt::carve(p, zig, 2.2, kWall.darker(175));
        p.setPen(QPen(kOutline, o));
        p.setBrush(kGold);
        p.drawRect(QRectF(lintel.left(), under - 5, lintel.width(), 5));
    });
}

} // namespace

const Palette& jungle()
{
    static const Palette p = [] {
        Palette c;
        c.skyTop = QColor(0x1d, 0x2c, 0x4a);
        c.skyMid = QColor(0x4a, 0x5a, 0x7c);
        c.skyLow = QColor(0xb0, 0x86, 0x74);
        c.skyHorizon = QColor(0xe4, 0xaa, 0x78);
        c.sun = QColor(0xff, 0xe2, 0xa8);
        c.sunGlow = QColor(255, 214, 150);
        c.cloudShade = QColor(0x5a, 0x4a, 0x6a);
        c.cloudLit = QColor(0xf4, 0xb8, 0x94);
        c.haze = QColor(0xc0, 0x98, 0x80);
        c.ridgeFar = QColor(0x7a, 0x72, 0x88);
        c.ridgeNear = QColor(0x58, 0x60, 0x70);
        c.temple = QColor(0x5c, 0x62, 0x5e);
        c.templeLit = QColor(0x7e, 0x7c, 0x70);
        c.leafDark = QColor(0x1a, 0x3a, 0x22);
        c.leafMid = QColor(0x2c, 0x5a, 0x30);
        c.leafLight = QColor(0x4c, 0x80, 0x40);
        c.path = QColor(0xc9, 0xa8, 0x6a);
        c.pathJoint = QColor(0x6a, 0x50, 0x30);
        c.moss = QColor(0x5c, 0x9a, 0x3a);
        c.earthTop = QColor(0x6a, 0x48, 0x28);
        c.earthBottom = QColor(0x22, 0x16, 0x0c);
        c.grass = QColor(0x4c, 0x8c, 0x30);
        c.grassLight = QColor(0x7c, 0xb8, 0x48);
        c.flower = QColor(0xf4, 0xe8, 0xf0);
        c.frontLeaf = QColor(0x0c, 0x1c, 0x10);
        c.shaft = QColor(255, 226, 170);
        return c;
    }();
    return p;
}

Layers build(const Palette& palette, const Frame& frame)
{
    Layers l;
    l.frame = frame;
    const QSize size(std::max(1, int(std::ceil((frame.right - frame.left) * frame.pixels))),
                     std::max(1, int(std::ceil((frame.bottom - frame.top) * frame.pixels))));
    l.sky = paintSky(palette, frame, size);
    l.back.push_back(ridges(palette, frame));
    l.back.push_back(temples(palette, frame));
    l.back.push_back(midJungle(palette, frame));
    l.back.push_back(nearJungle(palette, frame));
    l.ground = ground(palette, frame);
    l.front = front(palette, frame);
    l.vines = vines(palette, frame);
    l.light = paintLight(palette, frame, size);
    for (int i = 0; i < 4; ++i) {
        l.stones[size_t(i)] = stone(frame, i);
        l.roots[size_t(i)] = root(frame, i);
        l.walls[size_t(i)] = wall(frame, i);
    }
    const int dot = std::max(4, int(std::lround(8.0 * frame.pixels)));
    l.mote = softDot(dot, QColor(255, 255, 255, 230));
    l.glow = softDot(dot * 3, QColor(255, 220, 120, 150));
    l.shadow = blank(dot * 4, dot);
    {
        QPainter p(&l.shadow);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.scale(dot * 4 / 2.0, dot / 2.0);
        p.translate(1.0, 1.0);
        QRadialGradient g(QPointF(0, 0), 1.0);
        g.setColorAt(0.0, QColor(0, 0, 0, 150));
        g.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(QPointF(0, 0), 1.0, 1.0);
    }
    return l;
}

Layers dressed(const Layers& jungle, Biomes::Look look)
{
    Layers l = jungle;
    if (look == Biomes::Look::Jungle)
        return l;
    const double snow = 5.0 * l.frame.pixels;
    Biomes::colour(l.sky, look, Biomes::Part::Sky, 0.0);
    for (Strip& strip : l.back)
        Biomes::colour(strip.image, look, Biomes::Part::Scenery, snow);
    Biomes::colour(l.ground.image, look, Biomes::Part::Ground, snow * 0.8);
    Biomes::colour(l.front.image, look, Biomes::Part::Scenery, snow * 0.6);
    Biomes::colour(l.vines.image, look, Biomes::Part::Scenery, snow * 0.6);
    return l;
}

void paintWeather(QPainter& p, Biomes::Look look, double share, double time, double drift, const QRectF& area,
                  double unit, const QImage& mote)
{
    if (share <= 0.0)
        return;
    const auto wrap = [](double v, double span) { return v - span * std::floor(v / span); };
    const double w = area.width() + 40.0 * unit;
    const double h = area.height() + 40.0 * unit;
    const auto place = [&](double x, double y) { return QPointF(area.left() - 20.0 * unit + wrap(x, w),
                                                                area.top() - 20.0 * unit + wrap(y, h)); };
    p.save();
    p.setPen(Qt::NoPen);
    switch (Biomes::weather(look)) {
    case Biomes::Weather::Motes:
    case Biomes::Weather::Dust: {
        const bool dust = Biomes::weather(look) == Biomes::Weather::Dust;
        const int count = int((dust ? 50 : 34) * share);
        for (int i = 0; i < count; ++i) {
            const double depth = 0.4 + 0.6 * noise(i, 300);
            const double speed = dust ? 22.0 + 30.0 * noise(i, 301) : 6.0 + 10.0 * noise(i, 301);
            const QPointF at = place(noise(i, 302) * w + time * speed * unit - drift * depth,
                                     noise(i, 303) * h * 0.8 + std::sin(time * 0.8 + i) * 6.0 * unit);
            const double twinkle = 0.5 + 0.5 * std::sin(time * 1.7 + i * 2.1);
            const double r = (dust ? 1.4 + 2.0 * noise(i, 304) : 2.0 + 3.0 * noise(i, 304)) * depth * unit;
            p.setOpacity((dust ? 0.25 + 0.25 * twinkle : 0.12 + 0.4 * twinkle) * depth);
            if (dust) {
                p.setBrush(QColor(0xe8, 0xc8, 0x90));
                p.drawEllipse(at, r * 0.6, r * 0.6);
            } else {
                p.drawImage(QRectF(at.x() - r, at.y() - r, r * 2.0, r * 2.0), mote);
            }
        }
        break;
    }
    case Biomes::Weather::Snow: {
        const int count = int(70 * share);
        for (int i = 0; i < count; ++i) {
            const double depth = 0.35 + 0.65 * noise(i, 310);
            const QPointF at = place(noise(i, 311) * w + std::sin(time * 1.1 + i) * 10.0 * unit - drift * depth,
                                     noise(i, 312) * h + time * (22.0 + 30.0 * noise(i, 313)) * depth * unit);
            const double r = (1.4 + 2.2 * noise(i, 314)) * depth * unit;
            p.setOpacity(0.55 + 0.4 * depth);
            p.drawImage(QRectF(at.x() - r, at.y() - r, r * 2.0, r * 2.0), mote);
        }
        break;
    }
    case Biomes::Weather::Leaves: {
        const int count = int(16 * share);
        for (int i = 0; i < count; ++i) {
            const QPointF at = place(noise(i, 320) * w - drift * 0.9 + std::sin(time * 1.3 + i) * 14.0 * unit,
                                     noise(i, 321) * h + time * (14.0 + 12.0 * noise(i, 322)) * unit);
            p.setOpacity(1.0);
            p.save();
            p.translate(at);
            p.rotate(std::sin(time * 2.0 + i) * 70.0 + 360.0 * noise(i, 323));
            const QColor c = Biomes::leafColour(look, i);
            p.setBrush(c);
            p.drawEllipse(QPointF(0, 0), 3.6 * unit, 1.5 * unit);
            p.setPen(QPen(c.darker(150), 0.5 * unit));
            p.drawLine(QPointF(-3.2 * unit, 0), QPointF(3.2 * unit, 0));
            p.setPen(Qt::NoPen);
            p.restore();
        }
        break;
    }
    }
    p.restore();
}

void drawStrip(QPainter& p, const Strip& strip, const Frame& frame, double scroll, int sceneWidth)
{
    if (strip.image.isNull())
        return;
    const int period = strip.image.width();
    const double shift = std::fmod(std::max(0.0, scroll) * strip.factor, strip.period);
    int x = int(std::lround((-shift - frame.left) * frame.pixels));
    x %= period;
    if (x > 0)
        x -= period;
    const int y = int(std::lround((strip.top - frame.top) * frame.pixels));
    for (; x < sceneWidth; x += period)
        p.drawImage(QPoint(x, y), strip.image);
}

void drawPiece(QPainter& p, const Piece& piece, const Frame& frame, QPointF base)
{
    const QPointF at = (base + piece.offset - QPointF(frame.left, frame.top)) * frame.pixels;
    p.drawImage(QPoint(int(std::lround(at.x())), int(std::lround(at.y()))), piece.image);
}

} // namespace RunnerArt
