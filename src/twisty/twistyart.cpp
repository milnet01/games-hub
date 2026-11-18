#include "twistyart.h"

#include "theme.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRadialGradient>
#include <QTransform>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace twisty {

namespace {

constexpr double kDegrees = std::numbers::pi / 180.0;

// Twelve colours, each with its own mark. A light sticker carries a dark mark
// and a dark sticker a white one, so the mark always stands out.
const std::array<Paint, 12> kPaints { {
    { QColor(0xf4, 0xf4, 0xee), QColor(0x1a, 0x1a, 0x1a), Mark::Circle, QT_TRANSLATE_NOOP("TwistyView", "white") },
    { QColor(0xff, 0xd2, 0x1f), QColor(0x1a, 0x1a, 0x1a), Mark::Square, QT_TRANSLATE_NOOP("TwistyView", "yellow") },
    { QColor(0x1f, 0x9e, 0x48), QColor(0xff, 0xff, 0xff), Mark::Triangle, QT_TRANSLATE_NOOP("TwistyView", "green") },
    { QColor(0x1f, 0x5a, 0xcc), QColor(0xff, 0xff, 0xff), Mark::Plus, QT_TRANSLATE_NOOP("TwistyView", "blue") },
    { QColor(0xd0, 0x26, 0x26), QColor(0xff, 0xff, 0xff), Mark::Diamond, QT_TRANSLATE_NOOP("TwistyView", "red") },
    { QColor(0xff, 0x86, 0x1a), QColor(0x1a, 0x1a, 0x1a), Mark::Star, QT_TRANSLATE_NOOP("TwistyView", "orange") },
    { QColor(0x76, 0x3c, 0xc4), QColor(0xff, 0xff, 0xff), Mark::Ring, QT_TRANSLATE_NOOP("TwistyView", "purple") },
    { QColor(0xff, 0x8c, 0xc6), QColor(0x1a, 0x1a, 0x1a), Mark::Heart, QT_TRANSLATE_NOOP("TwistyView", "pink") },
    { QColor(0x6c, 0xcc, 0xf4), QColor(0x1a, 0x1a, 0x1a), Mark::Cross, QT_TRANSLATE_NOOP("TwistyView", "sky blue") },
    { QColor(0xa4, 0xdc, 0x2c), QColor(0x1a, 0x1a, 0x1a), Mark::Hexagon, QT_TRANSLATE_NOOP("TwistyView", "lime") },
    { QColor(0xe4, 0xcc, 0x98), QColor(0x1a, 0x1a, 0x1a), Mark::Bars, QT_TRANSLATE_NOOP("TwistyView", "sand") },
    { QColor(0x5a, 0x5e, 0x66), QColor(0xff, 0xff, 0xff), Mark::Drop, QT_TRANSLATE_NOOP("TwistyView", "grey") },
} };

// Which colour each face starts in. The cube's faces run up, down, front,
// back, right, left, in the usual colours. On the twelve-sided shape the
// colours most alike sit on opposite faces, which never touch.
int paintIndex(Shape shape, int face)
{
    static constexpr std::array<int, 4> kPyramid { 4, 2, 3, 1 };
    static constexpr std::array<int, 12> kDodecahedron { 0, 3, 1, 2, 4, 6, 9, 7, 11, 10, 8, 5 };
    switch (shape) {
    case Shape::Pyramid: return kPyramid[size_t(face) % kPyramid.size()];
    case Shape::Dodecahedron: return kDodecahedron[size_t(face) % kDodecahedron.size()];
    default: return face % 6;
    }
}

QPainterPath buildMarkPath(Mark mark)
{
    QPainterPath path;
    const auto polygon = [&](std::initializer_list<QPointF> points) {
        path.addPolygon(QPolygonF(QList<QPointF>(points)));
        path.closeSubpath();
    };
    const auto regular = [&](int corners, double radius, double turn) {
        QPolygonF poly;
        for (int i = 0; i < corners; ++i) {
            const double a = turn + 2.0 * std::numbers::pi * i / corners;
            poly << QPointF(radius * std::cos(a), radius * std::sin(a));
        }
        path.addPolygon(poly);
        path.closeSubpath();
    };
    switch (mark) {
    case Mark::Circle:
        path.addEllipse(QPointF(0, 0), 0.75, 0.75);
        break;
    case Mark::Square:
        path.addRect(QRectF(-0.55, -0.55, 1.1, 1.1));
        break;
    case Mark::Triangle:
        polygon({ { 0, 0.88 }, { -0.88, -0.66 }, { 0.88, -0.66 } });
        break;
    case Mark::Plus:
    case Mark::Cross: {
        QPainterPath plus;
        QPainterPath across;
        across.addRect(QRectF(-0.85, -0.24, 1.7, 0.48));
        QPainterPath down;
        down.addRect(QRectF(-0.24, -0.85, 0.48, 1.7));
        // United, not added: two rectangles added to one path cancel where
        // they cross, which left a hole in the middle of the plus.
        plus = across.united(down);
        path = mark == Mark::Plus ? plus : QTransform().rotate(45).map(plus);
        break;
    }
    case Mark::Diamond:
        polygon({ { 0, 0.92 }, { 0.68, 0 }, { 0, -0.92 }, { -0.68, 0 } });
        break;
    case Mark::Star: {
        QPolygonF star;
        for (int i = 0; i < 10; ++i) {
            const double a = std::numbers::pi / 2.0 + std::numbers::pi * i / 5.0;
            const double r = i % 2 == 0 ? 0.95 : 0.42;
            star << QPointF(r * std::cos(a), r * std::sin(a));
        }
        path.addPolygon(star);
        path.closeSubpath();
        break;
    }
    case Mark::Ring:
        path.setFillRule(Qt::OddEvenFill);
        path.addEllipse(QPointF(0, 0), 0.82, 0.82);
        path.addEllipse(QPointF(0, 0), 0.44, 0.44);
        break;
    case Mark::Heart:
        path.moveTo(0, -0.85);
        path.cubicTo(-0.6, -0.35, -1.0, 0.1, -0.75, 0.55);
        path.cubicTo(-0.5, 0.95, -0.08, 0.85, 0, 0.5);
        path.cubicTo(0.08, 0.85, 0.5, 0.95, 0.75, 0.55);
        path.cubicTo(1.0, 0.1, 0.6, -0.35, 0, -0.85);
        path.closeSubpath();
        break;
    case Mark::Hexagon:
        regular(6, 0.85, 0.0);
        break;
    case Mark::Bars:
        path.addRect(QRectF(-0.82, 0.14, 1.64, 0.42));
        path.addRect(QRectF(-0.82, -0.56, 1.64, 0.42));
        break;
    case Mark::Drop: {
        QPainterPath round;
        round.addEllipse(QPointF(0, -0.28), 0.6, 0.6);
        QPainterPath point;
        point.addPolygon(QPolygonF({ QPointF(0, 0.95), QPointF(-0.52, -0.02), QPointF(0.52, -0.02) }));
        point.closeSubpath();
        path = round.united(point);
        break;
    }
    }
    return path;
}

// Built once: three of the marks are made by joining shapes, which is far
// too slow to do for every sticker on every frame.
const QPainterPath& markPath(Mark mark)
{
    static const std::array<QPainterPath, 12> kPaths = [] {
        std::array<QPainterPath, 12> paths;
        for (size_t m = 0; m < paths.size(); ++m)
            paths[m] = buildMarkPath(Mark(m));
        return paths;
    }();
    return kPaths[size_t(mark)];
}

// `poly` with each corner rounded off by up to `radius` pixels, as one
// filled shape: far cheaper to draw than rounding it with a wide pen.
QPainterPath roundedPath(const QPolygonF& poly, double radius)
{
    QPainterPath path;
    const qsizetype n = poly.size();
    const auto cornerPoint = [&](qsizetype at, qsizetype towards) {
        const QPointF d = poly[towards] - poly[at];
        const double length = std::hypot(d.x(), d.y());
        const double r = std::min(radius, length / 2.0);
        return length > 1e-9 ? poly[at] + d * (r / length) : poly[at];
    };
    for (qsizetype i = 0; i < n; ++i) {
        const qsizetype prev = (i + n - 1) % n;
        const qsizetype next = (i + 1) % n;
        const QPointF in = cornerPoint(i, prev);
        const QPointF out = cornerPoint(i, next);
        if (i == 0)
            path.moveTo(in);
        else
            path.lineTo(in);
        path.quadTo(poly[i], out);
    }
    path.closeSubpath();
    return path;
}

// Sorts `points`, which lie in a plane across `axis`, anticlockwise about it.
void orderAbout(std::vector<Vec3>& points, const Vec3& axis)
{
    Vec3 centre;
    for (const Vec3& q : points)
        centre = centre + q;
    centre = centre * (1.0 / double(points.size()));
    const Vec3 seed = std::abs(axis.x) < 0.9 ? Vec3 { 1, 0, 0 } : Vec3 { 0, 1, 0 };
    const Vec3 u = axis.cross(seed).normalized();
    const Vec3 v = axis.cross(u);
    std::sort(points.begin(), points.end(), [&](const Vec3& a, const Vec3& b) {
        return std::atan2((a - centre).dot(v), (a - centre).dot(u))
            < std::atan2((b - centre).dot(v), (b - centre).dot(u));
    });
}

// Where the plane `axis`·p = `height` cuts the solid: the face a turning layer
// shows across its gap.
std::vector<Vec3> section(const Puzzle& puzzle, const Vec3& axis, double height)
{
    std::vector<Vec3> points;
    for (const auto& face : puzzle.faces()) {
        for (size_t i = 0; i < face.size(); ++i) {
            const Vec3& a = face[i];
            const Vec3& b = face[(i + 1) % face.size()];
            const double sa = axis.dot(a) - height;
            const double sb = axis.dot(b) - height;
            if ((sa > 0) == (sb > 0))
                continue;
            const Vec3 q = a + (b - a) * (sa / (sa - sb));
            const bool seen = std::any_of(points.begin(), points.end(),
                                          [&](const Vec3& o) { return (o - q).length() < 1e-6; });
            if (!seen)
                points.push_back(q);
        }
    }
    if (points.size() >= 3)
        orderAbout(points, axis);
    return points;
}

QColor shaded(const QColor& c, double light)
{
    return QColor::fromRgbF(float(std::clamp(c.redF() * light, 0.0, 1.0)),
                            float(std::clamp(c.greenF() * light, 0.0, 1.0)),
                            float(std::clamp(c.blueF() * light, 0.0, 1.0)));
}

// Where the light comes from, as the eye sees it: up, left and in front.
Vec3 lightFromEye()
{
    return Vec3 { -0.35, 0.55, 0.76 }.normalized();
}

// How brightly a surface facing `normal` (world space) is lit.
double lightFor(const Camera& camera, const Vec3& normal)
{
    const Vec3 n = camera.toCamera(normal);
    return 0.68 + 0.34 * std::max(0.0, n.dot(lightFromEye()));
}

double polygonArea(const QPolygonF& poly)
{
    double area = 0.0;
    for (qsizetype i = 0; i < poly.size(); ++i) {
        const QPointF& a = poly[i];
        const QPointF& b = poly[(i + 1) % poly.size()];
        area += a.x() * b.y() - b.x() * a.y();
    }
    return std::abs(area) / 2.0;
}

const QColor kBody(0x16, 0x16, 0x18);

} // namespace

const Paint& paintFor(Shape shape, int face)
{
    return kPaints[size_t(paintIndex(shape, face))];
}

std::vector<Paint> palette(Shape shape)
{
    const Puzzle puzzle(shape);
    std::vector<Paint> out;
    out.reserve(size_t(puzzle.faceCount()));
    for (int f = 0; f < puzzle.faceCount(); ++f)
        out.push_back(paintFor(shape, f));
    return out;
}

Vec3 Camera::toCamera(const Vec3& w) const
{
    const double b = yaw * kDegrees;
    const double a = pitch * kDegrees;
    const double x1 = w.x * std::cos(b) + w.z * std::sin(b);
    const double z1 = -w.x * std::sin(b) + w.z * std::cos(b);
    return { x1, w.y * std::cos(a) - z1 * std::sin(a), w.y * std::sin(a) + z1 * std::cos(a) };
}

namespace {
Vec3 toWorld(const Camera& c, const Vec3& v)
{
    const double b = c.yaw * kDegrees;
    const double a = c.pitch * kDegrees;
    const double y1 = v.y * std::cos(a) + v.z * std::sin(a);
    const double z1 = -v.y * std::sin(a) + v.z * std::cos(a);
    return { v.x * std::cos(b) - z1 * std::sin(b), y1, v.x * std::sin(b) + z1 * std::cos(b) };
}
} // namespace

Vec3 Camera::eye() const
{
    return toWorld(*this, { 0, 0, distance });
}

Vec3 Camera::up() const
{
    return toWorld(*this, { 0, 1, 0 });
}

QPointF Camera::project(const Vec3& world) const
{
    const Vec3 c = toCamera(world);
    const double f = distance / std::max(1e-6, distance - c.z);
    return centre + QPointF(c.x * f * scale, -c.y * f * scale);
}

Camera Camera::fitting(const Puzzle& puzzle, const QRectF& area, double yaw, double pitch)
{
    Camera c;
    c.yaw = yaw;
    c.pitch = pitch;
    c.centre = area.center();
    const double r = puzzle.radius();
    c.distance = 6.0 * r;
    // The nearest corner is the one drawn largest, so fit that.
    const double reach = r * c.distance / (c.distance - r);
    c.scale = 0.47 * std::min(area.width(), area.height()) / reach;
    return c;
}

void paintShadow(QPainter& p, const Puzzle& puzzle, const Camera& camera)
{
    // Every corner dropped straight down onto a floor below the puzzle, as
    // the eye sees it; the shadow is the outline of what lands there.
    const double floor = -1.2 * puzzle.radius();
    std::vector<QPointF> points;
    for (const auto& face : puzzle.faces()) {
        for (const Vec3& v : face) {
            const Vec3 c = camera.toCamera(v);
            const double f = camera.distance / std::max(1e-6, camera.distance - c.z);
            points.push_back(camera.centre + QPointF(c.x * f * camera.scale, -floor * f * camera.scale));
        }
    }
    if (points.size() < 3)
        return;
    // Its outline, by the monotone chain.
    std::sort(points.begin(), points.end(), [](const QPointF& a, const QPointF& b) {
        return a.x() < b.x() || (a.x() == b.x() && a.y() < b.y());
    });
    const auto turn = [](const QPointF& o, const QPointF& a, const QPointF& b) {
        return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x());
    };
    std::vector<QPointF> hull(points.size() * 2);
    size_t k = 0;
    for (const QPointF& q : points) {
        while (k >= 2 && turn(hull[k - 2], hull[k - 1], q) <= 0)
            --k;
        hull[k++] = q;
    }
    for (size_t i = points.size() - 1, lower = k + 1; i > 0; --i) {
        while (k >= lower && turn(hull[k - 2], hull[k - 1], points[i - 1]) <= 0)
            --k;
        hull[k++] = points[i - 1];
    }
    hull.resize(k > 0 ? k - 1 : 0);
    QPointF middle;
    for (const QPointF& q : hull)
        middle += q;
    middle /= double(std::max<size_t>(1, hull.size()));

    // Soft at its edge: the same outline laid down shrinking, faint each time,
    // so it is darkest under the middle of the puzzle.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 16));
    for (int ring = 0; ring < 9; ++ring) {
        const double grow = 1.22 - 0.05 * ring;
        QPolygonF poly;
        for (const QPointF& q : hull)
            poly << middle + (q - middle) * grow;
        p.drawPolygon(poly);
    }
}

void paintMark(QPainter& p, Mark mark, const QPointF& centre, double halfSize, const QColor& ink)
{
    const QTransform t(halfSize, 0, 0, -halfSize, centre.x(), centre.y());
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    p.drawPath(t.map(markPath(mark)));
}

void paintPuzzle(QPainter& p, const Puzzle& puzzle, const Camera& camera, Spin spin,
                 int cursor, bool legible, Drawn* drawn, bool marks)
{
    const auto& stickers = puzzle.stickers();
    const Vec3 eye = camera.eye();
    const Vec3 lightWorld = toWorld(camera, lightFromEye());
    if (drawn != nullptr) {
        drawn->outline.assign(stickers.size(), {});
        drawn->facing.assign(stickers.size(), false);
    }

    // With a layer turning, the puzzle is three solid pieces: below the
    // layer, the layer, and above it. Each is convex, so within one a surface
    // facing the eye is never hidden by another; and the pieces are drawn far
    // to near along the axis.
    const bool spinning = spin.layer >= 0;
    Vec3 axis;
    double lo = 0.0;
    double hi = 0.0;
    if (spinning) {
        const Layer& layer = puzzle.layers()[size_t(spin.layer)];
        axis = puzzle.axes()[size_t(layer.axis)];
        lo = layer.lo;
        hi = layer.hi;
    }
    const auto groupOf = [&](const Vec3& centre) {
        if (!spinning)
            return 0;
        const double h = centre.dot(axis);
        return h <= lo ? 0 : (h >= hi ? 2 : 1);
    };
    std::array<int, 3> order { 0, 1, 2 };
    if (spinning) {
        const double e = eye.dot(axis);
        if (e < lo)
            order = { 2, 1, 0 };
        else if (e <= hi)
            order = { 0, 2, 1 };
    }
    const auto moved = [&](const Vec3& v, int group) {
        return group == 1 ? rotate(v, axis, spin.angle) : v;
    };

    const auto drawCap = [&](double height, const Vec3& normal, int group) {
        if (!std::isfinite(height))
            return;
        std::vector<Vec3> cap = section(puzzle, axis, height);
        if (cap.size() < 3)
            return;
        const Vec3 n = moved(normal, group);
        const Vec3 at = moved(cap.front(), group);
        if (n.dot(eye - at) <= 0.0)
            return;
        QPolygonF poly;
        for (const Vec3& v : cap)
            poly << camera.project(moved(v, group));
        p.setPen(QPen(kBody, 1.0));
        p.setBrush(shaded(QColor(0x22, 0x22, 0x26), lightFor(camera, n)));
        p.drawPolygon(poly);
    };

    for (int group : order) {
        if (spinning) {
            if (group == 0)
                drawCap(lo, axis, 0);
            if (group == 1) {
                drawCap(lo, axis * -1.0, 1);
                drawCap(hi, axis, 1);
            }
            if (group == 2)
                drawCap(hi, axis * -1.0, 2);
        }
        for (size_t i = 0; i < stickers.size(); ++i) {
            const Sticker& s = stickers[i];
            if (groupOf(s.centre) != group)
                continue;
            const Vec3 centre = moved(s.centre, group);
            const Vec3 normal = moved(s.normal, group);
            if (normal.dot(eye - centre) <= 0.0)
                continue;

            QPolygonF body;
            for (const Vec3& c : s.corners)
                body << camera.project(moved(c, group));
            if (drawn != nullptr) {
                drawn->outline[i] = body;
                drawn->facing[i] = true;
            }
            const double light = lightFor(camera, normal);
            p.setPen(QPen(kBody, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(shaded(kBody, light * 1.4));
            p.drawPolygon(body);

            // The coloured tile: inset from the edges, its corners rounded.
            constexpr double kInset = 0.86;
            QPolygonF corners;
            for (const Vec3& c : s.corners)
                corners << camera.project(moved(s.centre + (c - s.centre) * kInset, group));
            const double size = std::sqrt(polygonArea(body));
            const QPainterPath tile = roundedPath(corners, 0.10 * size);
            const Paint& paint = paintFor(puzzle.shape(), puzzle.colourAt(int(i)));
            const QColor fill = shaded(paint.fill, light);
            const QRectF box = corners.boundingRect();

            // A soft shadow under the sticker, down and right of it, as if it
            // stood a little proud of the plastic.
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 70));
            p.drawPath(tile.translated(0.025 * size, 0.04 * size));

            QLinearGradient base(box.topLeft(), box.bottomRight());
            base.setColorAt(0.0, fill.lighter(112));
            base.setColorAt(0.6, fill);
            base.setColorAt(1.0, fill.darker(110));
            p.setBrush(base);
            p.drawPath(tile);

            // Its raised edge: lit on the side the light comes from, in shade
            // on the other.
            QLinearGradient rim(box.topLeft(), box.bottomRight());
            rim.setColorAt(0.0, QColor(255, 255, 255, 120));
            rim.setColorAt(0.45, QColor(255, 255, 255, 0));
            rim.setColorAt(0.55, QColor(0, 0, 0, 0));
            rim.setColorAt(1.0, QColor(0, 0, 0, 80));
            p.setPen(QPen(QBrush(rim), std::max(1.0, 0.035 * size)));
            p.setBrush(Qt::NoBrush);
            p.drawPath(tile);

            // The mark, printed on the sticker: it lies in the face, at the
            // angle its piece has been turned to, and as large as the
            // sticker's narrowest part allows.
            double inradius = 1e9;
            for (size_t k = 0; k < s.corners.size(); ++k) {
                const Vec3& a = s.corners[k];
                const Vec3& b = s.corners[(k + 1) % s.corners.size()];
                const Vec3 along = (b - a).normalized();
                const Vec3 off = (s.centre - a) - along * (s.centre - a).dot(along);
                inradius = std::min(inradius, off.length());
            }
            const Vec3 up = moved(puzzle.markUpAt(int(i)), group);
            const Vec3 right = up.cross(normal);
            const double half = inradius * kInset * 0.69;
            const QPointF c0 = camera.project(centre);
            const QPointF u = camera.project(centre + right * half) - c0;
            const QPointF v = camera.project(centre + up * half) - c0;
            const QTransform t(u.x(), u.y(), v.x(), v.y(), c0.x(), c0.y());
            if (marks) {
                p.setPen(Qt::NoPen);
                p.setBrush(paint.ink);
                p.drawPath(t.map(markPath(paint.mark)));
            }

            // Gloss: a sheen across the upper part of every sticker, brighter
            // where the face turns towards the light as seen from the eye.
            const Vec3 toEye = (eye - centre).normalized();
            const Vec3 halfway = (lightWorld + toEye).normalized();
            const double spec = std::pow(std::max(0.0, normal.dot(halfway)), 28.0);
            QLinearGradient gloss(box.topLeft(), box.bottomRight());
            gloss.setColorAt(0.0, QColor(255, 255, 255, int(40 + 150 * spec)));
            gloss.setColorAt(0.42, QColor(255, 255, 255, int(12 + 60 * spec)));
            gloss.setColorAt(0.43, QColor(255, 255, 255, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(gloss);
            p.drawPath(tile);

            if (int(i) == cursor) {
                const double w = legible ? 5.0 : 3.5;
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QColor(0x10, 0x10, 0x10), w + 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p.drawPolygon(body);
                p.setPen(QPen(Theme::kGold, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p.drawPolygon(body);
            }
        }
    }
}

void paintTile(QPainter& p, const QRectF& r)
{
    QPainterPath frame;
    frame.addRoundedRect(r, 8, 8);
    QRadialGradient back(r.center(), std::max(r.width(), r.height()) * 0.7);
    back.setColorAt(0.0, QColor(0x3a, 0x44, 0x5c));
    back.setColorAt(1.0, QColor(0x14, 0x18, 0x24));
    p.fillPath(frame, back);
    p.save();
    p.setClipPath(frame);
    p.setRenderHint(QPainter::Antialiasing);
    static const Puzzle cube(Shape::Cube3);
    Camera camera = Camera::fitting(cube, r, -32.0, 26.0);
    // Fitted for any angle; at this one the cube can come a little nearer.
    camera.scale *= 1.18;
    // The top layer a little way round, so the tile says "it turns".
    paintPuzzle(p, cube, camera, { 5, 0.42 }, -1, false, nullptr, false);
    p.restore();
}

} // namespace twisty
