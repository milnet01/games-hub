#include "biomes.h"

#include <QCoreApplication>
#include <QThread>
#include <QtConcurrent/QtConcurrentMap>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>

namespace Biomes {
namespace {

// The run starts this far into the first stretch, so a change never comes
// with a hundred-metre banner of its own.
constexpr double kOffset = 50.0;

// How one look recolours the jungle: the leaves move to `hue` (degrees),
// give or take `spread` from patch to patch, with their saturation and
// value scaled; everything leans towards `tint`, and the sky towards `sky`.
struct Grade {
    bool on = false;
    double hue = 120.0;
    double spread = 0.0;
    double saturation = 1.0;
    double value = 1.0;
    QColor tint;
    double tintAmount = 0.0;
    QColor sky;
    double skyAmount = 0.0;
    bool snow = false;
};

const Grade& grade(Look look)
{
    static const std::array<Grade, kLooks> grades = [] {
        std::array<Grade, kLooks> g {};
        Grade& autumn = g[size_t(Look::Autumn)];
        autumn = { true, 28.0, 18.0, 1.45, 1.2, QColor(255, 196, 120), 0.08, QColor(255, 166, 104), 0.16, false };
        Grade& winter = g[size_t(Look::Winter)];
        winter = { true, 150.0, 8.0, 0.42, 0.82, QColor(200, 214, 236), 0.2, QColor(214, 224, 242), 0.38, true };
        Grade& dry = g[size_t(Look::Dry)];
        dry = { true, 52.0, 8.0, 0.8, 1.3, QColor(250, 216, 150), 0.14, QColor(255, 214, 150), 0.26, false };
        return g;
    }();
    return grades[size_t(look)];
}

Look lookOf(long long stretch)
{
    return Look(int(((stretch % kLooks) + kLooks) % kLooks));
}

std::uint32_t mix32(std::uint32_t h)
{
    h ^= h >> 15U;
    h *= 0x2c1b3c6dU;
    h ^= h >> 12U;
    h *= 0x297a2d39U;
    h ^= h >> 15U;
    return h;
}

double lattice(int i, int j)
{
    return double(mix32(std::uint32_t(i) * 73856093U ^ std::uint32_t(j) * 19349663U)) / 4294967296.0;
}

// Smooth noise in 0..1 over pixels, so leaves change colour in patches the
// size of a cluster rather than pixel by pixel.
double patch(int x, int y)
{
    constexpr int kCell = 28;
    const int i = x / kCell;
    const int j = y / kCell;
    const double fx = double(x % kCell) / kCell;
    const double fy = double(y % kCell) / kCell;
    const double sx = fx * fx * (3.0 - 2.0 * fx);
    const double sy = fy * fy * (3.0 - 2.0 * fy);
    const double top = lattice(i, j) + (lattice(i + 1, j) - lattice(i, j)) * sx;
    const double bottom = lattice(i, j + 1) + (lattice(i + 1, j + 1) - lattice(i, j + 1)) * sx;
    return top + (bottom - top) * sy;
}

void toHsv(double r, double g, double b, double& h, double& s, double& v)
{
    const double hi = std::max({ r, g, b });
    const double lo = std::min({ r, g, b });
    const double d = hi - lo;
    v = hi;
    s = hi > 0.0 ? d / hi : 0.0;
    if (d <= 0.0) {
        h = 0.0;
        return;
    }
    if (hi == r)
        h = 60.0 * std::fmod((g - b) / d, 6.0);
    else if (hi == g)
        h = 60.0 * ((b - r) / d + 2.0);
    else
        h = 60.0 * ((r - g) / d + 4.0);
    if (h < 0.0)
        h += 360.0;
}

void fromHsv(double h, double s, double v, double& r, double& g, double& b)
{
    h = std::fmod(std::fmod(h, 360.0) + 360.0, 360.0);
    const double c = v * s;
    const double x = c * (1.0 - std::abs(std::fmod(h / 60.0, 2.0) - 1.0));
    const double m = v - c;
    const int sector = int(h / 60.0);
    double rr = 0.0;
    double gg = 0.0;
    double bb = 0.0;
    switch (sector) {
    case 0: rr = c; gg = x; break;
    case 1: rr = x; gg = c; break;
    case 2: gg = c; bb = x; break;
    case 3: gg = x; bb = c; break;
    case 4: rr = x; bb = c; break;
    default: rr = c; bb = x; break;
    }
    r = rr + m;
    g = gg + m;
    b = bb + m;
}

double towards(double from, double to, double amount)
{
    return from + (to - from) * amount;
}

// Runs `work(first, last)` over `count` items split across the cores.
template <class F>
void split(int count, F work)
{
    const int parts = std::clamp(QThread::idealThreadCount(), 1, 8);
    std::vector<int> chunks(static_cast<size_t>(parts));
    std::iota(chunks.begin(), chunks.end(), 0);
    QtConcurrent::blockingMap(chunks, [&](int c) { work(count * c / parts, count * (c + 1) / parts); });
}

// A colour channel in 0..1, back to a premultiplied byte under alpha `a`.
int byte(double channel, int a)
{
    return int(std::lround(std::clamp(channel, 0.0, 1.0) * a));
}

constexpr double kSnow[] = { 0.95, 0.97, 1.0 };
// Snow lying on far land, in the sky picture, which has no tops to settle on.
constexpr double kSnowLand[] = { 0.86, 0.9, 0.95 };

} // namespace

Place at(double metres)
{
    const double m = std::max(0.0, metres) + kOffset;
    const auto stretch = (long long)std::floor(m / kMetresEach);
    const double into = m - double(stretch) * kMetresEach;
    Place p;
    p.from = lookOf(stretch);
    p.to = p.from;
    if (into < kChangeMetres / 2.0 && stretch > 0) {
        p.from = lookOf(stretch - 1);
        p.blend = 0.5 + into / kChangeMetres;
    } else if (into > kMetresEach - kChangeMetres / 2.0) {
        p.to = lookOf(stretch + 1);
        p.blend = (into - (kMetresEach - kChangeMetres / 2.0)) / kChangeMetres;
    }
    return p;
}

QString name(Look look)
{
    switch (look) {
    case Look::Autumn: return QCoreApplication::translate("Biomes", "Autumn woods");
    case Look::Winter: return QCoreApplication::translate("Biomes", "Snowfall");
    case Look::Dry: return QCoreApplication::translate("Biomes", "The dry season");
    case Look::Jungle: break;
    }
    return QCoreApplication::translate("Biomes", "The jungle");
}

void colour(QImage& image, Look look, Part part, double snowDepth)
{
    const Grade& g = grade(look);
    if (!g.on || image.isNull())
        return;
    if (image.format() != QImage::Format_ARGB32_Premultiplied && image.format() != QImage::Format_RGB32)
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    // A copy shares its pixels until written. Take this one's own before the
    // cores write rows at once, or each would try to at the same moment.
    image.bits();
    const int w = image.width();
    const int h = image.height();
    const double tint[] = { g.tint.redF(), g.tint.greenF(), g.tint.blueF() };
    const double sky[] = { g.sky.redF(), g.sky.greenF(), g.sky.blueF() };

    split(h, [&](int first, int last) {
        for (int y = first; y < last; ++y) {
            auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < w; ++x) {
                const QRgb px = line[x];
                const int a = qAlpha(px);
                if (a == 0)
                    continue;
                double c[] = { qRed(px) / double(a), qGreen(px) / double(a), qBlue(px) / double(a) };
                double hue = 0.0;
                double sat = 0.0;
                double val = 0.0;
                toHsv(c[0], c[1], c[2], hue, sat, val);
                if (sat >= 0.16 && val >= 0.06 && hue >= 55.0 && hue <= 175.0) {
                    const double moved = g.hue + g.spread * (2.0 * patch(x, y) - 1.0) + (hue - 115.0) * 0.25;
                    fromHsv(moved, std::min(1.0, sat * g.saturation), std::min(1.0, val * g.value), c[0], c[1], c[2]);
                    if (g.snow && part == Part::Sky)
                        for (int k = 0; k < 3; ++k)
                            c[k] = towards(c[k], kSnowLand[k] * std::max(0.55, val * 1.3), 0.6);
                }
                for (int k = 0; k < 3; ++k) {
                    if (part == Part::Sky)
                        c[k] = towards(c[k], sky[k], g.skyAmount);
                    c[k] = std::clamp(towards(c[k], tint[k], g.tintAmount), 0.0, 1.0);
                }
                line[x] = qRgba(byte(c[0], a), byte(c[1], a), byte(c[2], a), a);
            }
        }
    });

    // Snow on the tops of things: down each column, the first few pixels of
    // anything solid under something clear.
    if (!g.snow || part == Part::Sky || snowDepth < 1.0 || !image.hasAlphaChannel())
        return;
    split(w, [&](int first, int last) {
        for (int x = first; x < last; ++x) {
            const double depth = snowDepth * (0.6 + 0.8 * patch(x * 3, 0));
            int run = -1;
            for (int y = 0; y < h; ++y) {
                auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
                const QRgb px = line[x];
                const int a = qAlpha(px);
                if (a < 110) {
                    run = -1;
                    continue;
                }
                ++run;
                if (run >= depth)
                    continue;
                const double f = 0.92 * std::pow(1.0 - run / depth, 0.5);
                const double c[] = { qRed(px) / double(a), qGreen(px) / double(a), qBlue(px) / double(a) };
                line[x] = qRgba(byte(towards(c[0], kSnow[0], f), a), byte(towards(c[1], kSnow[1], f), a),
                                byte(towards(c[2], kSnow[2], f), a), a);
            }
        }
    });
}

QColor haze(Look look, const QColor& jungleHaze)
{
    switch (look) {
    case Look::Autumn: return { 0xd2, 0x9c, 0x78 };
    case Look::Winter: return { 0xc6, 0xce, 0xdc };
    case Look::Dry: return { 0xd8, 0xb8, 0x88 };
    case Look::Jungle: break;
    }
    return jungleHaze;
}

Weather weather(Look look)
{
    switch (look) {
    case Look::Autumn: return Weather::Leaves;
    case Look::Winter: return Weather::Snow;
    case Look::Dry: return Weather::Dust;
    case Look::Jungle: break;
    }
    return Weather::Motes;
}

QColor leafColour(Look look, int which)
{
    static const QColor autumn[] = { { 0xe0, 0x7a, 0x2a }, { 0xc8, 0x40, 0x1e }, { 0xe8, 0xb0, 0x30 }, { 0xa8, 0x60, 0x2a } };
    static const QColor jungle[] = { { 0x6a, 0x8c, 0x2c }, { 0xb0, 0x8a, 0x2c } };
    static const QColor dry[] = { { 0xb8, 0xa0, 0x50 }, { 0xa0, 0x80, 0x40 } };
    const auto index = size_t(std::abs(which));
    switch (look) {
    case Look::Autumn: return autumn[index % 4];
    case Look::Dry: return dry[index % 2];
    case Look::Winter: return { 0xf4, 0xf6, 0xfa };
    case Look::Jungle: break;
    }
    return jungle[index % 2];
}

} // namespace Biomes
