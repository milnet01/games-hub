#pragma once

#include <QColor>
#include <QImage>
#include <QRectF>

#include <array>
#include <vector>

class QPainter;

// The painted art of Ruin Runner Lanes (GHUB-0241): scenery and the faces of
// obstacles, each drawn once at a generous size and kept, so every frame
// only scales finished pictures rather than painting them again. Pure
// presentation; nothing here changes the run.
namespace LaneArt {

// The haze the far course fades into. The view's sky and path use it too.
inline constexpr QColor kHaze { 0xc9, 0x9c, 0x7a };

// One picture at three sizes, so a far, small copy is scaled down from a
// small picture and does not shimmer. `misted` holds the same shapes filled
// with the haze, laid over at the strength of the haze where it stands;
// it is empty for art that never stands in the haze.
struct Sprite {
    std::array<QImage, 3> sizes {};
    std::array<QImage, 3> misted {};
    double widthMetres = 1.0;
    double heightMetres = 1.0;
};

struct Set {
    std::vector<Sprite> trees;   // broadleaf, broadleaf, palm, mossy
    std::vector<Sprite> ruins;   // column, carved head, stele
    std::vector<Sprite> plants;  // fern, leafy bush
    std::array<Sprite, 4> blockFaces;
    std::array<Sprite, 4> trunkFaces;
    std::array<Sprite, 4> gateFaces;
};

// Built on first use and kept for the life of the program.
const Set& art();

// The sky behind the course, painted once per window size: dawn, clouds,
// mountains, the temple the path runs to, and the jungle either side down
// to `horizon`, then the jungle floor. `canvas` is where the play is drawn
// and `unit` is how many pixels a canvas unit is.
void paintSky(QPainter& p, const QRectF& all, const QRectF& canvas, double horizon, double unit);

// The leaves framing the top of the play, painted once per window size and
// laid over the scene.
void paintCanopy(QPainter& p, const QRectF& canvas, double unit);

// Draws `sprite` into `target`, `mist` (0 to 1) of the way into the haze.
// `pixelsTall` is how tall the target is on the device, which picks the size.
void draw(QPainter& p, const Sprite& sprite, const QRectF& target, double mist, double pixelsTall);

} // namespace LaneArt
