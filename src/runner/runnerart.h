#pragma once

#include <QColor>
#include <QImage>
#include <QPointF>

#include <array>
#include <vector>

class QPainter;

// The painted art of the side-on Ruin Runner (GHUB-0262): the sky, the
// scenery, the path and the obstacles, painted once for a window size and
// kept, so a frame only copies finished pictures. Pure presentation; nothing
// here changes the run.
namespace RunnerArt {

// The colours of one stretch of the world.
struct Palette {
    QColor skyTop, skyMid, skyLow, skyHorizon;
    QColor sun, sunGlow;
    QColor cloudShade, cloudLit;
    QColor haze;
    QColor ridgeFar, ridgeNear;
    bool snowCaps = false;
    QColor temple, templeLit;
    QColor leafDark, leafMid, leafLight;
    QColor path, pathJoint, moss;
    QColor earthTop, earthBottom;
    QColor grass, grassLight, flower;
    QColor frontLeaf;
    QColor shaft;
};

const Palette& jungle();

// What the art is painted for: how sharp, and which part of the course the
// scene shows, in course units.
struct Frame {
    double pixels = 1.0; // scene pixels to a course unit
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
    double outline = 1.6;
    bool operator==(const Frame&) const = default;
};

// A band of scenery that repeats every `period` course units and moves
// `factor` units for each unit the course moves.
struct Strip {
    QImage image;
    double top = 0.0;
    double period = 1.0;
    double factor = 1.0;
};

// A picture placed at `offset` course units from where its obstacle meets
// the ground.
struct Piece {
    QImage image;
    QPointF offset;
};

struct Layers {
    Frame frame;
    QImage sky;              // the whole scene, opaque
    std::vector<Strip> back; // far to near, behind the path
    Strip ground;            // the path, its grass and the earth under it
    Strip front;             // ferns before the earth
    Strip vines;             // hanging into the top
    QImage light;            // over everything: shafts of sun and a vignette
    std::array<Piece, 4> stones;
    std::array<Piece, 4> roots;
    std::array<Piece, 4> walls;
    QImage mote;   // a soft white dot
    QImage shadow; // a soft dark ellipse
    QImage glow;   // a soft gold dot
};

Layers build(const Palette& palette, const Frame& frame);

// Copies `strip` across a scene `sceneWidth` pixels wide, `scroll` course
// units along. The painter is in scene pixels.
void drawStrip(QPainter& p, const Strip& strip, const Frame& frame, double scroll, int sceneWidth);

// Copies `piece` for an obstacle meeting the ground at `base`, on whole
// scene pixels. The painter is in scene pixels.
void drawPiece(QPainter& p, const Piece& piece, const Frame& frame, QPointF base);

} // namespace RunnerArt
