#pragma once

#include <QColor>
#include <QImage>
#include <QString>

// The stretches a Ruin Runner run passes through, both runners alike
// (GHUB-0260): the jungle, then autumn woods, snowfall and the dry season,
// and round again. Each is the same art in its own colours, made once from
// the finished jungle pictures, with its own weather. Pure presentation:
// the course is the same whatever the season.
namespace Biomes {

enum class Look { Jungle, Autumn, Winter, Dry };
inline constexpr int kLooks = 4;

// Metres of each stretch, and of the change from one to the next.
inline constexpr double kMetresEach = 400.0;
inline constexpr double kChangeMetres = 30.0;

// Where a point of the run is: in `from`, `blend` (0 to 1) of the way into
// `to`. Outside a change, `to` is `from` and `blend` is 0.
struct Place {
    Look from = Look::Jungle;
    Look to = Look::Jungle;
    double blend = 0.0;
    // The look that has the most of the picture.
    Look main() const { return blend < 0.5 ? from : to; }
};
Place at(double metres);

// What the banner calls a stretch as the run enters it.
QString name(Look look);

// Which picture is being coloured, since each takes the season differently.
enum class Part {
    Sky,     // opaque, sky and far land; takes the season's light
    Scenery, // trees, ruins, plants; snow settles on their tops
    Ground,  // the path and the grass along it; snow settles on it
};

// Colours `image` for `look`, in place. `snowDepth` is how deep, in pixels,
// snow lies on the top of anything when the look has snow. The jungle is
// left as it is. Leaves obstacles alone: they are never passed here.
void colour(QImage& image, Look look, Part part, double snowDepth);

// The colour of the haze far things fade into, in `look`.
QColor haze(Look look, const QColor& jungleHaze);

// What drifts in the air.
enum class Weather { Motes, Leaves, Snow, Dust };
Weather weather(Look look);
// The colours of falling leaves, for the looks that have them.
QColor leafColour(Look look, int which);

} // namespace Biomes
