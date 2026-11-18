#pragma once

#include "twistypuzzle.h"

#include <QColor>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>

#include <vector>

class QPainter;

// Drawing Twisty Cube (GHUB-0253): the colours, the mark each colour carries,
// and the puzzle itself in 3D with QPainter. Shared by the game and its tile.
namespace twisty {

// Every colour carries a mark of its own, so a face can be told apart without
// judging colour alone (owner's ask). A colour keeps its mark on every shape.
enum class Mark { Circle, Square, Triangle, Plus, Diamond, Star, Ring, Heart, Cross, Hexagon, Bars, Drop };

struct Paint {
    QColor fill;
    QColor ink; // the mark's colour: dark on a light sticker, white on a dark one
    Mark mark = Mark::Circle;
    const char* name = nullptr; // for the rules text, translated there
};

// The colour a face of `shape` starts in.
const Paint& paintFor(Shape shape, int face);
// Every colour and mark the shape uses, in face order.
std::vector<Paint> palette(Shape shape);

// Where the eye is. The puzzle sits at the origin; the view turns it by
// `yaw` about the vertical and then tips it by `pitch` towards the viewer.
struct Camera {
    double yaw = -35.0;  // degrees
    double pitch = 28.0; // degrees
    QPointF centre;
    double scale = 100.0;   // pixels per puzzle unit at the centre's depth
    double distance = 10.0; // eye to centre, in puzzle units

    Vec3 toCamera(const Vec3& world) const;
    Vec3 eye() const;    // in world space
    Vec3 up() const;     // the screen's up, in world space
    QPointF project(const Vec3& world) const;

    // Sized so the whole of `puzzle` fits `area` at any orientation.
    static Camera fitting(const Puzzle& puzzle, const QRectF& area, double yaw, double pitch);
};

// A layer part-way through a turn.
struct Spin {
    int layer = -1;
    double angle = 0.0; // radians, the right-hand rule about the layer's axis
};

// What one paint put on screen, so a click can be traced back to a sticker.
struct Drawn {
    std::vector<QPolygonF> outline; // per sticker, in screen pixels
    std::vector<bool> facing;       // per sticker: facing the eye, so seen
};

// Paints the puzzle. `cursor` is a sticker to frame in gold, or -1. `marks`
// is false only where the stickers are too small for a mark to be anything
// but a speck: the hub's tile.
void paintPuzzle(QPainter& p, const Puzzle& puzzle, const Camera& camera, Spin spin,
                 int cursor, bool legible, Drawn* drawn = nullptr, bool marks = true);

// The puzzle's soft shadow on a floor just below it, shaped by its outline.
void paintShadow(QPainter& p, const Puzzle& puzzle, const Camera& camera);

// A mark filling the square -1..1 (y up), for a key or a legend.
void paintMark(QPainter& p, Mark mark, const QPointF& centre, double halfSize, const QColor& ink);

// The hub's tile: a 3x3 cube part-way through a turn.
void paintTile(QPainter& p, const QRectF& r);

} // namespace twisty
