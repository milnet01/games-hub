#pragma once

#include <QByteArray>

#include <random>
#include <vector>

// Twisty Cube's rules: a solid cut into layers that turn (GHUB-0253).
//
// Every shape is built the same way. Take the solid, slice its faces with a
// set of flat cuts, and each piece of a face is a sticker. A layer is every
// sticker on one side of a cut, or between two parallel cuts, and turning it
// rotates those stickers about the cut's axis. What a turn does is worked out
// once, from the geometry, as a list of where each sticker goes.
//
// The position is never stored as colours. It is the scramble's turns and the
// player's turns, played from solved, so every position this class can hold —
// including one read back from a save — is one real turns can reach and real
// turns can undo. That is the "every one must be solvable" rule.
//
// The rules only: no widget, no painting, no clock.
namespace twisty {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(double k) const { return { x * k, y * k, z * k }; }
    double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const
    {
        return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x };
    }
    double length() const;
    Vec3 normalized() const;
};

// `p` turned by `angle` radians about the unit vector `axis`, anticlockwise
// seen from the axis tip looking back at the centre (the right-hand rule).
Vec3 rotate(const Vec3& p, const Vec3& axis, double angle);

enum class Shape : int {
    Cube2,
    Cube3,
    Cube4,
    Cube5,
    Pyramid,      // a tetrahedron; never the trademarked name
    Dodecahedron, // twelve faces; never the trademarked name
};
constexpr int kShapeCount = 6;

struct Sticker {
    std::vector<Vec3> corners; // convex, anticlockwise seen from outside
    Vec3 centre;               // the corners' average
    Vec3 normal;               // outward, unit length
    Vec3 up;                   // the way up of a mark here when solved, in the face
    int face = 0;              // the solid's face it lies on, and its solved colour
};

// One layer that turns. A sticker belongs to it when its centre, measured
// along the axis, lies strictly between `lo` and `hi`.
struct Layer {
    int axis = 0;
    double lo = 0.0;
    double hi = 0.0;
    int order = 4; // steps in a full turn: 4 on a cube, 3 or 5 elsewhere
    // step[i]: where the sticker at position i is after one clockwise step.
    std::vector<int> step;
};

struct Move {
    int layer = 0;
    bool clockwise = true; // clockwise seen from outside, looking at the axis tip
    bool operator==(const Move& o) const { return layer == o.layer && clockwise == o.clockwise; }
};

class Puzzle
{
public:
    explicit Puzzle(Shape shape = Shape::Cube3);

    Shape shape() const { return m_shape; }
    int faceCount() const { return int(m_faces.size()); }
    // The solid's faces, anticlockwise seen from outside. The view closes the
    // gap a turning layer opens with these.
    const std::vector<std::vector<Vec3>>& faces() const { return m_faces; }
    const std::vector<Sticker>& stickers() const { return m_stickers; }
    const std::vector<Vec3>& axes() const { return m_axes; }
    const std::vector<Layer>& layers() const { return m_layers; }
    // The farthest corner from the centre, so a view can scale every shape alike.
    double radius() const { return m_radius; }

    bool inLayer(int layer, int sticker) const;
    // The angle one step of `layer` turns through, in radians.
    double stepAngle(int layer) const;

    // The colour now at position `sticker`, as a face number.
    int colourAt(int sticker) const { return m_colours[size_t(sticker)]; }
    // Which way up the mark on the sticker now at `sticker` points. A mark is
    // printed on its sticker, so it turns with every turn that carries it --
    // and it is worked out from the turns, so a save needs nothing more.
    const Vec3& markUpAt(int sticker) const { return m_ups[size_t(sticker)]; }
    // Every face one colour, whichever colour that is.
    bool solved() const;

    // Back to solved, with no scramble and no turns.
    void reset();
    // Back to solved, then `scrambleLength()` random turns. The player's turns
    // start from here, so the move count and undo start from here too.
    void scramble(std::mt19937& rng);
    int scrambleLength() const;
    const std::vector<Move>& scrambleMoves() const { return m_scramble; }

    // The player's turns since the scramble, oldest first.
    const std::vector<Move>& history() const { return m_history; }
    int moveCount() const { return int(m_history.size()); }

    // Applies a turn and records it. Returns false, changing nothing, for a
    // layer that does not exist.
    bool turn(Move move);
    // Takes back the last recorded turn. Returns false when there is none.
    bool undo(Move* undone = nullptr);

    // The shape, the scramble and the player's turns. decode() replays them
    // from solved, and refuses anything it cannot, so a save can never hold
    // a position the turns do not reach.
    QByteArray encode() const;
    bool decode(const QByteArray& blob);

    // A save's history beyond this is refused rather than replayed.
    static constexpr int kMaxMoves = 200000;

private:
    void build();
    void apply(Move move);

    Shape m_shape;
    std::vector<std::vector<Vec3>> m_faces;
    std::vector<Sticker> m_stickers;
    std::vector<Vec3> m_axes;
    std::vector<Layer> m_layers;
    double m_radius = 1.0;

    std::vector<int> m_colours;
    std::vector<Vec3> m_ups;
    std::vector<Move> m_scramble;
    std::vector<Move> m_history;
};

} // namespace twisty
