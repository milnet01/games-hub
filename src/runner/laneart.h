#pragma once

#include "runner/biomes.h"

#include <QColor>
#include <QImage>
#include <QRectF>

#include <array>
#include <vector>

class QPainter;
class QPainterPath;

// The painted art of Ruin Runner Lanes (GHUB-0241): scenery and the faces of
// obstacles, each drawn once at a generous size and kept, so every frame
// only scales finished pictures rather than painting them again. Pure
// presentation; nothing here changes the run.
namespace LaneArt {

// The haze the far course fades into. The view's sky and path use it too.
inline constexpr QColor kHaze { 0xc9, 0x9c, 0x7a };

// How finely the haze over a sprite is graded: one picture per step.
inline constexpr int kHazeSteps = 32;

// One picture at three sizes, so a far, small copy is scaled down from a
// small picture and does not shimmer. Art that stands in the haze keeps
// each size already part-way into it, a step at a time, made on first use:
// one draw a sprite rather than a picture and its haze over it (GHUB-0261).
struct Sprite {
    std::array<QImage, 3> sizes {};
    bool hazes = false;
    mutable std::array<std::array<QImage, kHazeSteps>, 3> hazed {};
    double widthMetres = 1.0;
    double heightMetres = 1.0;
    // What it fades into far off: the season's haze (GHUB-0260).
    QColor haze = kHaze;
};

struct Set {
    std::vector<Sprite> trees;   // broadleaf, broadleaf, palm, mossy
    std::vector<Sprite> ruins;   // column, carved head, stele
    std::vector<Sprite> plants;  // fern, leafy bush
    std::array<Sprite, 4> blockFaces;
    std::array<Sprite, 4> trunkFaces;
    std::array<Sprite, 4> gateFaces;
};

// Built on first use and kept for the life of the program: the jungle, or
// its trees, ruins and plants in another season's colours (GHUB-0260). The
// faces of obstacles are the same in every season. Safe to call from any
// thread; a season is made on first use, under a lock.
const Set& art(Biomes::Look look = Biomes::Look::Jungle);

// The sky behind the course, painted once per window size: dawn, clouds,
// mountains, the temple the path runs to, and the jungle either side down
// to `horizon`, then the jungle floor. `canvas` is where the play is drawn
// and `unit` is how many pixels a canvas unit is.
void paintSky(QPainter& p, const QRectF& all, const QRectF& canvas, double horizon, double unit);

// The leaves framing the top of the play, painted once per window size and
// laid over the scene.
void paintCanopy(QPainter& p, const QRectF& canvas, double unit);

// Painting helpers the side-on runner's art shares. A heap of leaves lit
// from the upper right when `lit`; a frond from `base` at `angle` radians;
// a groove cut along `path`; light and dark flecks over `r`.
void leafCluster(QPainter& p, QPointF c, double r, const QColor& dark, const QColor& mid, const QColor& light,
                 bool lit);
void frond(QPainter& p, QPointF base, double angle, double length, double droop, double leaflet,
           const QColor& leaf, const QColor& spine);
void carve(QPainter& p, const QPainterPath& path, double width, const QColor& groove);
void speckle(QPainter& p, const QRectF& r, int count, int salt, int alpha);

// Draws `sprite` into `target`, `mist` (0 to 1) of the way into the haze.
// `pixelsTall` is how tall the target is on the device, which picks the size.
void draw(QPainter& p, const Sprite& sprite, const QRectF& target, double mist, double pixelsTall);

} // namespace LaneArt
