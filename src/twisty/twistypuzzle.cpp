#include "twistypuzzle.h"

#include <QDataStream>
#include <QIODevice>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace twisty {

namespace {

constexpr double kEps = 1e-6;
constexpr double kInf = std::numeric_limits<double>::infinity();

// Any unit vector at right angles to `n`.
Vec3 perpendicular(const Vec3& n)
{
    const Vec3 seed = std::abs(n.x) < 0.9 ? Vec3 { 1, 0, 0 } : Vec3 { 0, 1, 0 };
    return n.cross(seed).normalized();
}

// Orders `points`, which lie on one face, anticlockwise seen from outside
// along `normal`.
void orderAround(std::vector<Vec3>& points, const Vec3& normal)
{
    Vec3 centre;
    for (const Vec3& p : points)
        centre = centre + p;
    centre = centre * (1.0 / double(points.size()));
    const Vec3 u = perpendicular(normal);
    const Vec3 v = normal.cross(u);
    std::sort(points.begin(), points.end(), [&](const Vec3& a, const Vec3& b) {
        return std::atan2((a - centre).dot(v), (a - centre).dot(u))
            < std::atan2((b - centre).dot(v), (b - centre).dot(u));
    });
}

// Splits a convex polygon by the plane n·p = d. Returns false, leaving both
// halves empty, when the plane does not pass through it.
bool split(const std::vector<Vec3>& poly, const Vec3& n, double d,
           std::vector<Vec3>& above, std::vector<Vec3>& below)
{
    bool hasAbove = false;
    bool hasBelow = false;
    for (const Vec3& p : poly) {
        const double s = n.dot(p) - d;
        hasAbove = hasAbove || s > kEps;
        hasBelow = hasBelow || s < -kEps;
    }
    if (!hasAbove || !hasBelow)
        return false;
    above.clear();
    below.clear();
    for (size_t i = 0; i < poly.size(); ++i) {
        const Vec3& a = poly[i];
        const Vec3& b = poly[(i + 1) % poly.size()];
        const double sa = n.dot(a) - d;
        const double sb = n.dot(b) - d;
        if (sa >= -kEps)
            above.push_back(a);
        if (sa <= kEps)
            below.push_back(a);
        if ((sa > kEps && sb < -kEps) || (sa < -kEps && sb > kEps)) {
            const Vec3 cut = a + (b - a) * (sa / (sa - sb));
            above.push_back(cut);
            below.push_back(cut);
        }
    }
    return true;
}

struct Cut {
    Vec3 normal;
    double offset;
};

} // namespace

double Vec3::length() const
{
    return std::sqrt(dot(*this));
}

Vec3 Vec3::normalized() const
{
    const double l = length();
    return l > 0.0 ? *this * (1.0 / l) : *this;
}

Vec3 rotate(const Vec3& p, const Vec3& axis, double angle)
{
    // Rodrigues' formula.
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return p * c + axis.cross(p) * s + axis * (axis.dot(p) * (1.0 - c));
}

Puzzle::Puzzle(Shape shape)
    : m_shape(shape)
{
    build();
    reset();
}

void Puzzle::build()
{
    std::vector<Cut> cuts;
    // Each layer as {axis, lo, hi, order}; the steps are worked out below.
    std::vector<Layer> layers;

    switch (m_shape) {
    case Shape::Cube2:
    case Shape::Cube3:
    case Shape::Cube4:
    case Shape::Cube5: {
        const int n = 2 + int(m_shape) - int(Shape::Cube2);
        // Faces in the order the view colours them: up, down, front, back,
        // right, left.
        const Vec3 normals[] = { { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 },
                                 { 0, 0, -1 }, { 1, 0, 0 }, { -1, 0, 0 } };
        for (const Vec3& normal : normals) {
            const Vec3 u = perpendicular(normal);
            const Vec3 v = normal.cross(u);
            m_faces.push_back({ normal - u - v, normal + u - v, normal + u + v, normal - u + v });
        }
        m_axes = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
        for (int axis = 0; axis < 3; ++axis) {
            for (int k = 1; k < n; ++k)
                cuts.push_back({ m_axes[size_t(axis)], -1.0 + 2.0 * k / n });
            for (int k = 0; k < n; ++k) {
                const double lo = k == 0 ? -kInf : -1.0 + 2.0 * k / n;
                const double hi = k == n - 1 ? kInf : -1.0 + 2.0 * (k + 1) / n;
                layers.push_back({ axis, lo, hi, 4, {} });
            }
        }
        break;
    }
    case Shape::Pyramid: {
        Vec3 corners[] = { { 1, 1, 1 }, { 1, -1, -1 }, { -1, 1, -1 }, { -1, -1, 1 } };
        // Stood on its base with the first corner straight up, so the view's
        // usual angle looks down on two faces rather than along one.
        const Vec3 first = corners[0].normalized();
        const Vec3 up { 0, 1, 0 };
        const Vec3 tilt = first.cross(up).normalized();
        const double angle = std::acos(first.dot(up));
        for (Vec3& c : corners)
            c = rotate(c, tilt, angle);
        for (int i = 0; i < 4; ++i) {
            std::vector<Vec3> face;
            for (int j = 0; j < 4; ++j)
                if (j != i)
                    face.push_back(corners[j]);
            orderAround(face, (corners[i] * -1.0).normalized());
            m_faces.push_back(face);
            m_axes.push_back(corners[i].normalized());
        }
        // Along an axis the corner stands at sqrt 3 and the face opposite it
        // at -sqrt 3 / 3. The two cuts divide that height in three, so every
        // face is nine triangles: a tip, a middle layer of three, and a base.
        const double tip = std::sqrt(3.0);
        const double base = -tip / 3.0;
        const double cut1 = base + (tip - base) / 3.0;
        const double cut2 = base + 2.0 * (tip - base) / 3.0;
        for (int axis = 0; axis < 4; ++axis) {
            cuts.push_back({ m_axes[size_t(axis)], cut1 });
            cuts.push_back({ m_axes[size_t(axis)], cut2 });
            layers.push_back({ axis, cut2, kInf, 3, {} }); // the tip alone
            layers.push_back({ axis, cut1, kInf, 3, {} }); // the tip and the layer under it
        }
        break;
    }
    case Shape::Dodecahedron: {
        const double phi = (1.0 + std::sqrt(5.0)) / 2.0;
        std::vector<Vec3> vertices;
        for (int sx : { -1, 1 })
            for (int sy : { -1, 1 })
                for (int sz : { -1, 1 })
                    vertices.push_back({ double(sx), double(sy), double(sz) });
        for (int a : { -1, 1 }) {
            for (int b : { -1, 1 }) {
                vertices.push_back({ 0, a / phi, b * phi });
                vertices.push_back({ a / phi, b * phi, 0 });
                vertices.push_back({ b * phi, 0, a / phi });
            }
        }
        // The face normals point at an icosahedron's corners.
        for (int a : { -1, 1 }) {
            for (int b : { -1, 1 }) {
                m_axes.push_back(Vec3 { 0, b * phi, double(a) }.normalized());
                m_axes.push_back(Vec3 { b * phi, double(a), 0 }.normalized());
                m_axes.push_back(Vec3 { double(a), 0, b * phi }.normalized());
            }
        }
        double inradius = 0.0;
        for (const Vec3& v : vertices)
            inradius = std::max(inradius, v.dot(m_axes[0]));
        for (const Vec3& normal : m_axes) {
            std::vector<Vec3> face;
            for (const Vec3& v : vertices)
                if (v.dot(normal) > inradius - kEps)
                    face.push_back(v);
            orderAround(face, normal);
            m_faces.push_back(face);
        }
        // Each face's cut crosses its five neighbours on a line parallel to
        // the edge it shares with them, `inset` in from that edge. The face
        // is then a centre, five edge pieces and five corners. Just under half
        // the face's apothem makes the three about one size, so no sticker's
        // mark is much smaller than another's; a shallower cut made the
        // centre huge and the edges thin strips.
        const double edge = 2.0 / phi;
        const double apothem = edge / (2.0 * std::tan(std::numbers::pi / 5.0));
        const double inset = 0.48 * apothem;
        const double dihedral = std::acos(-1.0 / std::sqrt(5.0));
        const double depth = inset * std::sin(std::numbers::pi - dihedral);
        for (int axis = 0; axis < 12; ++axis) {
            cuts.push_back({ m_axes[size_t(axis)], inradius - depth });
            layers.push_back({ axis, inradius - depth, kInf, 5, {} });
        }
        break;
    }
    }

    // Every face starts as one polygon, and each cut splits whatever it crosses.
    m_stickers.clear();
    for (size_t f = 0; f < m_faces.size(); ++f) {
        std::vector<std::vector<Vec3>> pieces { m_faces[f] };
        for (const Cut& cut : cuts) {
            std::vector<std::vector<Vec3>> next;
            for (const auto& piece : pieces) {
                std::vector<Vec3> above;
                std::vector<Vec3> below;
                if (split(piece, cut.normal, cut.offset, above, below)) {
                    next.push_back(above);
                    next.push_back(below);
                } else {
                    next.push_back(piece);
                }
            }
            pieces = std::move(next);
        }
        const Vec3 normal = (m_faces[f][1] - m_faces[f][0]).cross(m_faces[f][2] - m_faces[f][0]).normalized();
        // A face's marks stand the way the room's up runs across it. A face
        // lying flat has none, so its marks point away from the front, as
        // they read when you look down on it from there.
        Vec3 up = Vec3 { 0, 1, 0 } - normal * normal.y;
        if (up.length() < 0.1)
            up = normal.y > 0 ? Vec3 { 0, 0, -1 } : Vec3 { 0, 0, 1 };
        up = up.normalized();
        for (auto& piece : pieces) {
            Sticker s;
            for (const Vec3& p : piece)
                s.centre = s.centre + p;
            s.centre = s.centre * (1.0 / double(piece.size()));
            s.corners = std::move(piece);
            s.normal = normal;
            s.up = up;
            s.face = int(f);
            m_stickers.push_back(std::move(s));
        }
    }

    m_radius = 0.0;
    for (const auto& face : m_faces)
        for (const Vec3& p : face)
            m_radius = std::max(m_radius, p.length());

    // One clockwise step of each layer, from where each sticker's centre lands.
    m_layers = std::move(layers);
    for (size_t l = 0; l < m_layers.size(); ++l) {
        Layer& layer = m_layers[l];
        const Vec3& axis = m_axes[size_t(layer.axis)];
        const double angle = -2.0 * std::numbers::pi / layer.order;
        layer.step.assign(m_stickers.size(), -1);
        for (size_t i = 0; i < m_stickers.size(); ++i) {
            if (!inLayer(int(l), int(i))) {
                layer.step[i] = int(i);
                continue;
            }
            const Vec3 to = rotate(m_stickers[i].centre, axis, angle);
            for (size_t j = 0; j < m_stickers.size(); ++j) {
                if ((m_stickers[j].centre - to).length() < 1e-4) {
                    layer.step[i] = int(j);
                    break;
                }
            }
        }
    }
}

bool Puzzle::inLayer(int layer, int sticker) const
{
    const Layer& l = m_layers[size_t(layer)];
    const double h = m_stickers[size_t(sticker)].centre.dot(m_axes[size_t(l.axis)]);
    return h > l.lo && h < l.hi;
}

double Puzzle::stepAngle(int layer) const
{
    return 2.0 * std::numbers::pi / m_layers[size_t(layer)].order;
}

bool Puzzle::solved() const
{
    std::vector<int> seen(m_faces.size(), -1);
    for (size_t i = 0; i < m_stickers.size(); ++i) {
        int& colour = seen[size_t(m_stickers[i].face)];
        if (colour < 0)
            colour = m_colours[i];
        else if (colour != m_colours[i])
            return false;
    }
    return true;
}

void Puzzle::reset()
{
    m_colours.resize(m_stickers.size());
    m_ups.resize(m_stickers.size());
    for (size_t i = 0; i < m_stickers.size(); ++i) {
        m_colours[i] = m_stickers[i].face;
        m_ups[i] = m_stickers[i].up;
    }
    m_scramble.clear();
    m_history.clear();
}

int Puzzle::scrambleLength() const
{
    switch (m_shape) {
    case Shape::Cube2: return 15;
    case Shape::Cube3: return 25;
    case Shape::Cube4: return 40;
    case Shape::Cube5: return 60;
    case Shape::Pyramid: return 20;
    case Shape::Dodecahedron: return 70;
    }
    return 25;
}

void Puzzle::scramble(std::mt19937& rng)
{
    // Drawn from rng() directly rather than through a std distribution, whose
    // output the standard leaves to each library: one seed must scramble the
    // same on Windows as here.
    reset();
    const auto count = unsigned(m_layers.size());
    while (int(m_scramble.size()) < scrambleLength()) {
        const Move move { int(rng() % count), (rng() & 1u) != 0 };
        // Never straight back the way the last turn came: a pair like that
        // is a turn of the scramble wasted.
        if (!m_scramble.empty() && m_scramble.back().layer == move.layer
            && m_scramble.back().clockwise != move.clockwise)
            continue;
        apply(move);
        m_scramble.push_back(move);
    }
    // A scramble that happens to land solved is no puzzle. Cannot happen in
    // practice, but a 2x2 with a short scramble is the place it would.
    if (solved())
        scramble(rng);
}

bool Puzzle::turn(Move move)
{
    if (move.layer < 0 || move.layer >= int(m_layers.size()))
        return false;
    apply(move);
    m_history.push_back(move);
    return true;
}

bool Puzzle::undo(Move* undone)
{
    if (m_history.empty())
        return false;
    const Move last = m_history.back();
    m_history.pop_back();
    apply({ last.layer, !last.clockwise });
    if (undone != nullptr)
        *undone = last;
    return true;
}

void Puzzle::apply(Move move)
{
    const Layer& layer = m_layers[size_t(move.layer)];
    const std::vector<int>& step = layer.step;
    const Vec3& axis = m_axes[size_t(layer.axis)];
    const double angle = (move.clockwise ? -2.0 : 2.0) * std::numbers::pi / layer.order;
    std::vector<int> next(m_colours.size());
    std::vector<Vec3> nextUps(m_ups.size());
    for (size_t i = 0; i < m_colours.size(); ++i) {
        // Sticker `from` lands at `to`.
        const size_t from = move.clockwise ? i : size_t(step[i]);
        const size_t to = move.clockwise ? size_t(step[i]) : i;
        next[to] = m_colours[from];
        // Not "from == to": a centre on the axis stays put but still spins.
        if (!inLayer(move.layer, int(from))) {
            nextUps[to] = m_ups[from];
            continue;
        }
        // Turned with its piece, then laid flat on its new face again, so a
        // long game's rounding never tips a mark out of its sticker.
        const Vec3& n = m_stickers[to].normal;
        const Vec3 up = rotate(m_ups[from], axis, angle);
        nextUps[to] = (up - n * up.dot(n)).normalized();
    }
    m_colours = std::move(next);
    m_ups = std::move(nextUps);
}

namespace {
constexpr quint32 kMagic = 0x54575354; // "TWST"
constexpr quint16 kVersion = 1;

void writeMoves(QDataStream& out, const std::vector<Move>& moves)
{
    out << quint32(moves.size());
    for (const Move& m : moves)
        out << quint16(m.layer) << quint8(m.clockwise ? 1 : 0);
}

bool readMoves(QDataStream& in, int layerCount, std::vector<Move>& moves)
{
    quint32 count = 0;
    in >> count;
    if (in.status() != QDataStream::Ok || count > quint32(Puzzle::kMaxMoves))
        return false;
    moves.clear();
    moves.reserve(count);
    for (quint32 i = 0; i < count; ++i) {
        quint16 layer = 0;
        quint8 clockwise = 0;
        in >> layer >> clockwise;
        if (in.status() != QDataStream::Ok || int(layer) >= layerCount || clockwise > 1)
            return false;
        moves.push_back({ int(layer), clockwise == 1 });
    }
    return true;
}
} // namespace

QByteArray Puzzle::encode() const
{
    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << kMagic << kVersion << qint32(m_shape);
    writeMoves(out, m_scramble);
    writeMoves(out, m_history);
    return blob;
}

bool Puzzle::decode(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 magic = 0;
    quint16 version = 0;
    qint32 shape = -1;
    in >> magic >> version >> shape;
    if (in.status() != QDataStream::Ok || magic != kMagic || version != kVersion
        || shape < 0 || shape >= kShapeCount)
        return false;

    Puzzle loaded { Shape(shape) };
    std::vector<Move> scramble;
    std::vector<Move> history;
    const int layerCount = int(loaded.m_layers.size());
    if (!readMoves(in, layerCount, scramble) || !readMoves(in, layerCount, history) || !in.atEnd())
        return false;
    for (const Move& m : scramble)
        loaded.apply(m);
    loaded.m_scramble = std::move(scramble);
    for (const Move& m : history)
        loaded.apply(m);
    loaded.m_history = std::move(history);
    *this = std::move(loaded);
    return true;
}

} // namespace twisty
