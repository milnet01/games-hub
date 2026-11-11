#include "mahjongview.h"

#include "dealseed.h"
#include "legibility.h"
#include "sound.h"
#include "theme.h"

#include <QDataStream>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

using mahjong::Face;
using mahjong::Suit;

namespace {

constexpr quint32 kBlobVersion = 1;
constexpr int kTickMs = 16;
constexpr double kFadeMs = 320.0;
constexpr double kHintMs = 3000.0;
// A tile is this much taller than it is wide.
constexpr double kTileAspect = 1.3;

// The ink for each suit: blue circles, green bamboo, red diamonds. Each suit
// also has its own SHAPE, so colour is never the only thing telling them apart.
constexpr QColor kCircleInk { 0x1d, 0x4f, 0xb0 };
constexpr QColor kBambooInk { 0x1a, 0x7a, 0x38 };
constexpr QColor kDiamondInk { 0xc0, 0x22, 0x22 };
constexpr QColor kWindInk { 0x1d, 0x2b, 0x4f };
constexpr QColor kFaceTop { 0xfb, 0xf7, 0xea };
constexpr QColor kFaceBottom { 0xec, 0xe2, 0xc6 };
constexpr QColor kTileBack { 0x2b, 0x7a, 0x5a };
constexpr QColor kTileBackDark { 0x17, 0x4a, 0x36 };
constexpr QColor kTileEdge { 0xd6, 0xca, 0xa8 };
constexpr QColor kHintGlow { 0x4c, 0xd2, 0xff };

const QString& dimKey()
{
    static const QString key = QStringLiteral("mahjong/dimHeld"); // untranslated: settings key
    return key;
}

// Where the pips of a numbered tile sit, in a unit square: the same patterns
// a playing card uses for its pips, so a count reads as a shape.
std::vector<QPointF> pipPattern(int n)
{
    const double l = 0.22;
    const double c = 0.5;
    const double r = 0.78;
    const double t = 0.16;
    const double m = 0.5;
    const double b = 0.84;
    switch (n) {
    case 1: return { { c, m } };
    case 2: return { { c, t }, { c, b } };
    case 3: return { { c, t }, { c, m }, { c, b } };
    case 4: return { { l, t }, { r, t }, { l, b }, { r, b } };
    case 5: return { { l, t }, { r, t }, { c, m }, { l, b }, { r, b } };
    case 6: return { { l, t }, { r, t }, { l, m }, { r, m }, { l, b }, { r, b } };
    case 7: return { { l, t }, { r, t }, { c, 0.33 }, { l, m }, { r, m }, { l, b }, { r, b } };
    case 8:
        return { { l, t }, { r, t }, { c, 0.33 }, { l, m }, { r, m }, { c, 0.67 }, { l, b }, { r, b } };
    default:
        return { { l, t }, { c, t }, { r, t }, { l, m }, { c, m }, { r, m }, { l, b }, { c, b }, { r, b } };
    }
}

double pipRadius(int n, double side)
{
    const double share = n == 1 ? 0.30 : n <= 3 ? 0.15 : n <= 5 ? 0.15 : n <= 8 ? 0.12 : 0.11;
    return side * share;
}

void paintCirclePip(QPainter& p, QPointF at, double r)
{
    p.setPen(Qt::NoPen);
    p.setBrush(kCircleInk);
    p.drawEllipse(at, r, r);
    p.setBrush(kFaceTop);
    p.drawEllipse(at, r * 0.62, r * 0.62);
    p.setBrush(kCircleInk);
    p.drawEllipse(at, r * 0.30, r * 0.30);
}

void paintBambooPip(QPainter& p, QPointF at, double r, double tall)
{
    // An upright stick with a joint across its middle.
    const QRectF stick(at.x() - r * 0.5, at.y() - tall / 2.0, r, tall);
    p.setPen(Qt::NoPen);
    p.setBrush(kBambooInk);
    p.drawRoundedRect(stick, r * 0.4, r * 0.4);
    p.setBrush(kFaceTop);
    p.drawRect(QRectF(stick.left(), at.y() - r * 0.09, stick.width(), r * 0.18));
}

void paintDiamondPip(QPainter& p, QPointF at, double r)
{
    QPainterPath d;
    const double h = r * 1.25;
    const double w = r * 0.95;
    d.moveTo(at.x(), at.y() - h);
    d.lineTo(at.x() + w, at.y());
    d.lineTo(at.x(), at.y() + h);
    d.lineTo(at.x() - w, at.y());
    d.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(kDiamondInk);
    p.drawPath(d);
}

void paintArrow(QPainter& p, QPointF centre, double length, double angleDegrees, const QColor& ink)
{
    p.save();
    p.translate(centre);
    p.rotate(angleDegrees);
    const double half = length / 2.0;
    p.setPen(QPen(ink, length * 0.13, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(QPointF(-half, 0), QPointF(half * 0.55, 0));
    QPainterPath head;
    head.moveTo(half, 0);
    head.lineTo(half * 0.25, -length * 0.28);
    head.lineTo(half * 0.25, length * 0.28);
    head.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    p.drawPath(head);
    p.restore();
}

void paintFlame(QPainter& p, const QRectF& r)
{
    // The red dragon: a tall flame.
    const QPointF c = r.center();
    const double w = r.width() * 0.36;
    const double h = r.height() * 0.40;
    QPainterPath flame;
    flame.moveTo(c.x(), c.y() - h);
    flame.cubicTo(c.x() + w * 0.3, c.y() - h * 0.4, c.x() + w * 1.1, c.y() - h * 0.1, c.x() + w * 0.8,
                  c.y() + h * 0.55);
    flame.cubicTo(c.x() + w * 0.6, c.y() + h * 1.0, c.x() - w * 0.6, c.y() + h * 1.0, c.x() - w * 0.8,
                  c.y() + h * 0.55);
    flame.cubicTo(c.x() - w * 1.1, c.y() - h * 0.1, c.x() - w * 0.3, c.y() - h * 0.4, c.x(), c.y() - h);
    p.setPen(Qt::NoPen);
    p.setBrush(kDiamondInk);
    p.drawPath(flame);
    p.setBrush(QColor(0xff, 0xc4, 0x4a));
    p.drawPath(QTransform()
                   .translate(c.x(), c.y() + h * 0.35)
                   .scale(0.45, 0.45)
                   .translate(-c.x(), -c.y())
                   .map(flame));
}

void paintTree(QPainter& p, const QRectF& r)
{
    // The green dragon: a fir tree of three tiers on a trunk.
    const QPointF c = r.center();
    const double w = r.width() * 0.40;
    const double tier = r.height() * 0.22;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x6b, 0x45, 0x22));
    p.drawRect(QRectF(c.x() - w * 0.14, c.y() + tier * 1.1, w * 0.28, tier * 0.75));
    p.setBrush(kBambooInk);
    for (int i = 0; i < 3; ++i) {
        const double top = c.y() - tier * 1.8 + i * tier * 0.95;
        const double half = w * (0.55 + 0.22 * i);
        QPainterPath t;
        t.moveTo(c.x(), top);
        t.lineTo(c.x() + half, top + tier * 1.25);
        t.lineTo(c.x() - half, top + tier * 1.25);
        t.closeSubpath();
        p.drawPath(t);
    }
}

void paintFrame(QPainter& p, const QRectF& r, bool legible)
{
    // The white dragon: an empty blue frame, as on a traditional set.
    const QRectF outer = r.adjusted(r.width() * 0.10, r.height() * 0.10, -r.width() * 0.10,
                                    -r.height() * 0.10);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(kCircleInk, r.width() * (legible ? 0.10 : 0.08), Qt::SolidLine, Qt::SquareCap,
                  Qt::MiterJoin));
    p.drawRect(outer);
    p.setPen(QPen(kCircleInk, r.width() * 0.035));
    p.drawRect(outer.adjusted(r.width() * 0.13, r.width() * 0.13, -r.width() * 0.13, -r.width() * 0.13));
}

void paintFlower(QPainter& p, const QRectF& r, int rank)
{
    static const QColor petals[] = { QColor(0xe0, 0x4a, 0x8c), QColor(0xe8, 0x7a, 0x1c),
                                     QColor(0x8a, 0x4c, 0xc8), QColor(0x2c, 0x7c, 0xd8) };
    const QColor petal = petals[std::clamp(rank - 1, 0, 3)];
    const QPointF c = r.center() + QPointF(0, r.height() * 0.04);
    const double radius = std::min(r.width(), r.height()) * 0.36;
    p.setPen(QPen(kBambooInk, radius * 0.14, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(c, c + QPointF(0, radius * 1.35));
    p.setPen(Qt::NoPen);
    p.setBrush(petal);
    for (int i = 0; i < 5; ++i) {
        p.save();
        p.translate(c);
        p.rotate(i * 72.0);
        p.drawEllipse(QPointF(0, -radius * 0.55), radius * 0.30, radius * 0.50);
        p.restore();
    }
    p.setBrush(QColor(0xf2, 0xc8, 0x2a));
    p.drawEllipse(c, radius * 0.24, radius * 0.24);
}

void paintSeason(QPainter& p, const QRectF& r, int rank)
{
    const QPointF c = r.center();
    const double s = std::min(r.width(), r.height()) * 0.38;
    switch (rank) {
    case 1: { // spring: a sprout
        p.setPen(QPen(kBambooInk, s * 0.14, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(c + QPointF(0, s), c + QPointF(0, -s * 0.2));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x3c, 0xa8, 0x48));
        for (int side : { -1, 1 }) {
            p.save();
            p.translate(c + QPointF(side * s * 0.38, -s * 0.35));
            p.rotate(side * 40.0);
            p.drawEllipse(QPointF(0, 0), s * 0.26, s * 0.52);
            p.restore();
        }
        break;
    }
    case 2: { // summer: the sun
        p.setPen(QPen(QColor(0xe8, 0x8a, 0x10), s * 0.12, Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < 8; ++i) {
            const double a = i * std::numbers::pi / 4.0;
            const QPointF d(std::cos(a), std::sin(a));
            p.drawLine(c + d * s * 0.68, c + d * s * 1.0);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xf2, 0xb2, 0x1a));
        p.drawEllipse(c, s * 0.48, s * 0.48);
        break;
    }
    case 3: { // autumn: a falling leaf
        p.save();
        p.translate(c);
        p.rotate(-35.0);
        QPainterPath leaf;
        leaf.moveTo(0, -s);
        leaf.cubicTo(s * 0.7, -s * 0.5, s * 0.7, s * 0.5, 0, s * 0.85);
        leaf.cubicTo(-s * 0.7, s * 0.5, -s * 0.7, -s * 0.5, 0, -s);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xc8, 0x5a, 0x16));
        p.drawPath(leaf);
        p.setPen(QPen(QColor(0x7a, 0x30, 0x08), s * 0.07, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(0, -s * 0.8), QPointF(0, s * 1.05));
        p.restore();
        break;
    }
    default: { // winter: a snowflake
        p.setPen(QPen(kCircleInk, s * 0.11, Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < 3; ++i) {
            const double a = i * std::numbers::pi / 3.0 + std::numbers::pi / 2.0;
            const QPointF d(std::cos(a), std::sin(a));
            p.drawLine(c - d * s, c + d * s);
            for (int end : { -1, 1 }) {
                const QPointF tip = c + d * s * 0.62 * end;
                const QPointF n(-d.y(), d.x());
                p.drawLine(tip, tip + (d * end + n) * s * 0.22);
                p.drawLine(tip, tip + (d * end - n) * s * 0.22);
            }
        }
        break;
    }
    }
}

} // namespace

MahjongView::MahjongView(QWidget* parent)
    : GameView(parent)
    , m_rng(dealSeed())
{
    setMinimumSize(MahjongView::minimumSizeHint());
    // GHUB-0168: without this the heap takes no keys at all.
    setFocusPolicy(Qt::StrongFocus);
    m_timer = new QTimer(this);
    m_timer->setInterval(kTickMs);
    connect(m_timer, &QTimer::timeout, this, &MahjongView::step);
    m_dimHeld = QSettings().value(dimKey(), true).toBool();
    buildActions();
    newGame();
}

void MahjongView::buildActions()
{
    auto* newAction = new QAction(tr("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MahjongView::newGame);
    m_actions.append(newAction);

    m_undoAction = new QAction(tr("Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &MahjongView::undo);
    m_actions.append(m_undoAction);

    auto* hint = new QAction(tr("Hint"), this);
    hint->setToolTip(tr("Light up a pair you can take (H)"));
    connect(hint, &QAction::triggered, this, &MahjongView::showHint);
    m_actions.append(hint);

    auto* shuffleAction = new QAction(tr("Shuffle"), this);
    shuffleAction->setToolTip(tr("Deal the tiles left onto the same places, so the heap can be "
                                 "cleared (S)"));
    connect(shuffleAction, &QAction::triggered, this, &MahjongView::shuffle);
    m_actions.append(shuffleAction);

    auto* separator = new QAction(this);
    separator->setSeparator(true);
    m_actions.append(separator);

    m_dimAction = new QAction(tr("Shade held tiles"), this);
    m_dimAction->setCheckable(true);
    m_dimAction->setChecked(m_dimHeld);
    m_dimAction->setToolTip(tr("Draw the tiles you cannot take yet a shade darker"));
    connect(m_dimAction, &QAction::toggled, this, &MahjongView::setDimHeld);
    m_actions.append(m_dimAction);
}

// GHUB-0016. The rules as mahjong::Board enforces them and the controls as
// mousePressEvent, keyPressEvent and buildActions take them.
QString MahjongView::rulesText() const
{
    return tr(
        "<h3>The aim</h3>"
        "<p>Clear the whole heap of tiles by taking them away in matching pairs.</p>"

        "<h3>How it plays</h3>"
        "<p>The heap holds 144 tiles in four layers. You may take two tiles at a time, "
        "and only when they match and both are <b>free</b>.</p>"
        "<p>A tile is free when no tile lies on top of it, and nothing touches its left "
        "side or its right side. A tile with neighbours on both sides is held until one "
        "of them goes.</p>"
        "<p>Two tiles match when they are the same tile. There are four of each, so any "
        "two of the four match. The flowers are the exception: any flower matches any "
        "other flower. The seasons are the same: any season matches any other season.</p>"
        "<p>The tiles are:</p>"
        "<ul>"
        "<li><b>Circles</b>, <b>bamboo</b> and <b>diamonds</b>, numbered 1 to 9. The "
        "number is the count of circles, sticks or diamonds, laid out in the same "
        "patterns as the pips on a playing card, and it is also written in the top "
        "corner. Traditional sets mark the third suit with Chinese characters; here it "
        "is red diamonds.</li>"
        "<li><b>Winds</b>: east, south, west and north, each with its letter and an "
        "arrow pointing that way on a map.</li>"
        "<li><b>Dragons</b>: a red flame, a green tree and an empty blue frame.</li>"
        "<li><b>Flowers</b>, in four colours, and <b>seasons</b>: a sprout, the sun, a "
        "falling leaf and a snowflake.</li>"
        "</ul>"
        "<p>Every new heap is dealt so that it can be cleared. Taking the wrong pair can "
        "still leave you with no free pair. Then shuffle, or undo and try another way.</p>"

        "<h3>Controls</h3>"
        "<p><b>With the mouse:</b></p>"
        "<ul>"
        "<li>Click a free tile to pick it up, then click its twin to take the pair.</li>"
        "<li>Click a picked tile again to put it down. Clicking a tile that does not match "
        "picks that tile instead.</li>"
        "<li>When the heap is clear, click to deal a new one.</li>"
        "</ul>"
        "<p><b>With the keyboard:</b></p>"
        "<ul>"
        "<li>The arrow keys move a frame between the free tiles. The frame shows once you "
        "press a key, and hides again when you click.</li>"
        "<li>Space or Enter picks the tile in the frame, or takes the pair.</li>"
        "<li>Escape puts a picked tile down.</li>"
        "<li><b>H</b> lights up a pair you can take, and <b>S</b> shuffles.</li>"
        "</ul>"
        "<p><b>On the toolbar:</b></p>"
        "<ul>"
        "<li><b>New Game</b> (Ctrl+N) deals a fresh heap.</li>"
        "<li><b>Undo</b> (Ctrl+Z) puts back the last pair taken, or undoes a shuffle.</li>"
        "<li><b>Hint</b> lights up a pair you can take.</li>"
        "<li><b>Shuffle</b> deals the tiles left onto the same places, so the heap can "
        "be cleared again.</li>"
        "<li><b>Shade held tiles</b> draws the tiles you cannot take yet a little darker. "
        "The choice is remembered.</li>"
        "</ul>");
}

void MahjongView::deactivate()
{
    // Nothing to come back to half-way through a fade or a glow.
    m_fades.clear();
    m_hint.reset();
    m_hintLeftMs = 0.0;
    m_timer->stop();
    update();
}

void MahjongView::newGame()
{
    m_board.deal(m_rng);
    m_history.clear();
    m_picked = -1;
    m_cursor = -1;
    m_message.clear();
    m_won = false;
    m_resumed = false;
    m_hint.reset();
    m_fades.clear();
    m_undoAction->setEnabled(false);
    settleCursor();
    refresh();
}

void MahjongView::undo()
{
    if (m_history.empty() || m_won)
        return;
    std::array<Face, mahjong::kTiles> faces = m_history.back();
    m_history.pop_back();
    m_board.restore(faces);
    m_picked = -1;
    m_hint.reset();
    m_fades.clear();
    m_message.clear();
    m_undoAction->setEnabled(!m_history.empty());
    settleCursor();
    refresh();
}

void MahjongView::showHint()
{
    if (m_won)
        return;
    m_hint = m_board.hint();
    if (!m_hint) {
        m_message = tr("No pair is free. Press S to shuffle, or undo.");
        refresh();
        return;
    }
    m_message.clear();
    m_hintLeftMs = kHintMs;
    m_timer->start();
    refresh();
}

void MahjongView::shuffle()
{
    if (m_won)
        return;
    const std::array<Face, mahjong::kTiles> before = m_board.faces();
    if (!m_board.reshuffle(m_rng)) {
        m_message = tr("These tiles cannot be dealt so the heap clears. Undo and try "
                       "another way.");
        refresh();
        return;
    }
    m_history.push_back(before);
    m_undoAction->setEnabled(true);
    m_picked = -1;
    m_hint.reset();
    m_message = tr("Shuffled.");
    Sound::instance().play(Sound::kShuffle);
    settleCursor();
    refresh();
}

void MahjongView::setDimHeld(bool dim)
{
    m_dimHeld = dim;
    QSettings().setValue(dimKey(), dim);
    update();
}

void MahjongView::pressAt(int slot)
{
    if (m_won) {
        newGame();
        return;
    }
    if (!m_board.present(slot))
        return;
    m_hint.reset();
    if (!m_board.isFree(slot)) {
        m_message = !m_board.uncovered(slot)
            ? tr("That tile is held: another lies on top of it.")
            : tr("That tile is held: tiles touch both its sides.");
        refresh();
        return;
    }
    m_message.clear();
    if (m_picked == slot) {
        m_picked = -1;
    } else if (m_picked >= 0 && mahjong::matches(m_board.at(m_picked), m_board.at(slot))) {
        take(m_picked, slot);
        return;
    } else {
        if (m_picked >= 0)
            m_message = tr("Those two do not match.");
        m_picked = slot;
        Sound::instance().play(Sound::kClick);
    }
    refresh();
}

void MahjongView::take(int a, int b, bool animate)
{
    const Face fa = m_board.at(a);
    const Face fb = m_board.at(b);
    const std::array<Face, mahjong::kTiles> before = m_board.faces();
    if (!m_board.take(a, b))
        return;
    m_history.push_back(before);
    m_undoAction->setEnabled(true);
    m_picked = -1;
    m_hint.reset();
    if (animate) {
        m_fades.push_back({ a, fa, 0.0 });
        m_fades.push_back({ b, fb, 0.0 });
        m_timer->start();
        Sound::instance().play(Sound::kDiscPlace);
    }
    settleCursor();
    if (m_board.cleared()) {
        m_won = true;
        m_undoAction->setEnabled(false);
        if (animate)
            Sound::instance().play(Sound::kWin);
    }
    refresh();
}

bool MahjongView::advanceForShot(int turns)
{
    for (int i = 0; i < turns; ++i) {
        const std::optional<mahjong::Pair> pair = m_board.hint();
        if (!pair)
            break;
        take(pair->first, pair->second, false);
    }
    m_fades.clear();
    m_timer->stop();
    return true;
}

QString MahjongView::boardSentence() const
{
    if (m_won)
        return tr("The heap is clear! Click for a new one.");
    if (!m_message.isEmpty())
        return m_message;
    if (m_board.freePairs().empty())
        return tr("No pair is free. Press S to shuffle, or undo.");
    if (m_picked >= 0)
        return tr("Now pick its twin, or click it again to put it down.");
    return tr("Pick two free tiles that match.");
}

QString MahjongView::captionText() const
{
    return boardSentence() + QStringLiteral("   ") + m_counts;
}

void MahjongView::refresh()
{
    update();
    m_counts = tr("%1 tiles left, %2 pairs free")
                   .arg(m_board.remaining())
                   .arg(m_board.freePairs().size());
    Q_EMIT statusChanged(boardSentence() + QStringLiteral("   ") + m_counts);
}

void MahjongView::step()
{
    for (Fade& f : m_fades)
        f.done = std::min(1.0, f.done + kTickMs / kFadeMs);
    m_fades.erase(std::remove_if(m_fades.begin(), m_fades.end(),
                                 [](const Fade& f) { return f.done >= 1.0; }),
                  m_fades.end());
    if (m_hintLeftMs > 0.0) {
        m_hintLeftMs = std::max(0.0, m_hintLeftMs - kTickMs);
        if (m_hintLeftMs <= 0.0)
            m_hint.reset();
    }
    if (m_fades.empty() && !m_hint)
        m_timer->stop();
    update();
}

// ---------------------------------------------------------------------------
// The cursor
// ---------------------------------------------------------------------------

void MahjongView::settleCursor()
{
    if (m_board.isFree(m_cursor))
        return;
    // To the free tile nearest where it stood, or the first free tile of a
    // fresh heap.
    const QPointF from = mahjong::Board::inBounds(m_cursor) ? faceRect(m_cursor).center() : QPointF();
    int best = -1;
    double bestDistance = 0.0;
    for (int s = 0; s < mahjong::kTiles; ++s) {
        if (!m_board.isFree(s))
            continue;
        const QPointF d = faceRect(s).center() - from;
        const double distance = d.x() * d.x() + d.y() * d.y();
        if (best < 0 || distance < bestDistance) {
            best = s;
            bestDistance = distance;
        }
    }
    m_cursor = best;
}

void MahjongView::stepCursor(int dx, int dy)
{
    settleCursor();
    if (m_cursor < 0)
        return;
    // The nearest free tile that way: distance along the direction, with
    // straying sideways costing double, so Right stays on its row.
    const QPointF from = faceRect(m_cursor).center();
    int best = -1;
    double bestScore = 0.0;
    for (int s = 0; s < mahjong::kTiles; ++s) {
        if (s == m_cursor || !m_board.isFree(s))
            continue;
        const QPointF d = faceRect(s).center() - from;
        const double along = d.x() * dx + d.y() * dy;
        const double across = std::abs(d.x() * dy) + std::abs(d.y() * dx);
        if (along <= 1.0)
            continue;
        const double score = along + 2.0 * across;
        if (best < 0 || score < bestScore) {
            best = s;
            bestScore = score;
        }
    }
    if (best >= 0)
        m_cursor = best;
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

namespace {
// The line under the heap. None under the legibility switch, where the
// caption plate says the same in larger type.
int sentenceHeight(int room)
{
    if (Legibility::instance().enabled())
        return 0;
    return std::clamp(int(room * 0.06), 22, 64);
}
constexpr double kMargin = 12.0;
} // namespace

double MahjongView::tileWidth() const
{
    const int room = height() - int(captionBand(QRectF(rect())));
    const double columns = mahjong::layoutWidth() / 2.0;
    const double rows = mahjong::layoutHeight() / 2.0;
    // The heap's span in tile widths: the columns, plus a depth step per
    // layer and the side of the bottom tile. A depth step is 0.11 of a width.
    const double across = columns + 0.11 * (mahjong::layoutLayers() + 1);
    const double down = rows * kTileAspect + 0.11 * (mahjong::layoutLayers() + 1);
    const double w = std::min((width() - 2 * kMargin) / across,
                              (room - sentenceHeight(room) - 2 * kMargin) / down);
    return std::max(8.0, w);
}

QPointF MahjongView::origin() const
{
    // Where the bottom layer's top-left corner goes. Upper layers step up and
    // left from it, so the heap is centred with them included.
    const double w = tileWidth();
    const double d = depth();
    const int room = height() - int(captionBand(QRectF(rect())));
    const int layers = mahjong::layoutLayers();
    const double spanW = mahjong::layoutWidth() / 2.0 * w + d * (layers + 1);
    const double spanH = mahjong::layoutHeight() / 2.0 * w * kTileAspect + d * (layers + 1);
    const double top = (room - sentenceHeight(room) - spanH) / 2.0;
    return { (width() - spanW) / 2.0 + d * (layers - 1), top + d * (layers - 1) };
}

QRectF MahjongView::faceRect(int slot) const
{
    if (!mahjong::Board::inBounds(slot))
        return {};
    const mahjong::Slot& s = mahjong::layout()[std::size_t(slot)];
    const double w = tileWidth();
    const double h = w * kTileAspect;
    const double d = depth();
    const QPointF o = origin();
    return { o.x() + s.x * w / 2.0 - s.z * d, o.y() + s.y * h / 2.0 - s.z * d, w, h };
}

QRectF MahjongView::sentenceRect() const
{
    const int room = height() - int(captionBand(QRectF(rect())));
    const int strip = sentenceHeight(room);
    return { 0.0, double(room - strip - 4), double(width()), double(strip) };
}

int MahjongView::slotAt(QPointF pos) const
{
    // Topmost first: the drawing order, backwards.
    for (int s = mahjong::kTiles - 1; s >= 0; --s)
        if (m_board.present(s) && faceRect(s).contains(pos))
            return s;
    return -1;
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void MahjongView::paintTile(QPainter& p, int slot, Face face, double alpha, double lift) const
{
    const bool legible = Legibility::instance().enabled();
    const QRectF r = faceRect(slot).translated(0, -lift);
    const double w = r.width();
    const double d = depth();
    const double radius = w * 0.10;
    p.save();
    p.setOpacity(alpha);

    // The body: green back, then the ivory edge, then the face on top. Only
    // the right and bottom show, since the tile beside or below covers the rest.
    p.setPen(Qt::NoPen);
    // A raised tile casts a shadow on the layer below, which is what tells
    // the layers apart when the same faces lie on each.
    if (mahjong::layout()[std::size_t(slot)].z > 0) {
        p.setBrush(QColor(0, 0, 0, 70));
        p.drawRoundedRect(r.translated(d * 2.2, d * 2.2).adjusted(-d * 0.5, -d * 0.5, d * 0.5, d * 0.5),
                          radius * 1.4, radius * 1.4);
    }
    p.setBrush(kTileBackDark);
    p.drawRoundedRect(r.translated(d, d), radius, radius);
    p.setBrush(kTileBack);
    p.drawRoundedRect(r.translated(d * 0.75, d * 0.75), radius, radius);
    p.setBrush(kTileEdge);
    p.drawRoundedRect(r.translated(d * 0.35, d * 0.35), radius, radius);

    const bool picked = slot == m_picked && alpha >= 1.0;
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, picked ? QColor(0xff, 0xec, 0xa8) : kFaceTop);
    g.setColorAt(1.0, picked ? QColor(0xf2, 0xc8, 0x5a) : kFaceBottom);
    p.setBrush(g);
    p.setPen(QPen(QColor(0x8a, 0x7a, 0x58), 1.0));
    p.drawRoundedRect(r, radius, radius);

    // The art, inside a margin.
    const QRectF art = r.adjusted(w * 0.09, w * 0.09, -w * 0.09, -w * 0.09);
    const Suit suit = mahjong::suitOf(face);
    const int rank = mahjong::rankOf(face);
    const auto numeral = [&](const QColor& ink) {
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(std::max(7, int(art.height() * (legible ? 0.22 : 0.18))));
        p.setFont(f);
        p.setPen(ink);
        p.drawText(art, Qt::AlignLeft | Qt::AlignTop, QString::number(rank));
    };
    switch (suit) {
    case Suit::Circles:
    case Suit::Bamboo:
    case Suit::Characters: {
        const QColor ink = suit == Suit::Circles ? kCircleInk
            : suit == Suit::Bamboo               ? kBambooInk
                                                 : kDiamondInk;
        numeral(ink);
        // The pips sit below the numeral's line, in a square-ish field.
        const QRectF field = art.adjusted(0, art.height() * 0.19, 0, 0);
        const double side = std::min(field.width(), field.height());
        const double r0 = pipRadius(rank, side) * (legible ? 1.08 : 1.0);
        for (const QPointF& at : pipPattern(rank)) {
            const QPointF c(field.left() + at.x() * field.width(),
                            field.top() + at.y() * field.height());
            if (suit == Suit::Circles)
                paintCirclePip(p, c, r0);
            else if (suit == Suit::Characters)
                paintDiamondPip(p, c, r0);
            else
                paintBambooPip(p, c, r0 * (rank == 1 ? 0.7 : 0.9),
                               rank == 1 ? field.height() * 0.7 : r0 * (rank >= 6 ? 2.2 : 2.6));
        }
        break;
    }
    case Suit::Wind: {
        static const char* const letters[] = {
            QT_TRANSLATE_NOOP("MahjongView", "E"), QT_TRANSLATE_NOOP("MahjongView", "S"),
            QT_TRANSLATE_NOOP("MahjongView", "W"), QT_TRANSLATE_NOOP("MahjongView", "N")
        };
        // East points right, south down, west left and north up, as on a map.
        static const double angles[] = { 0.0, 90.0, 180.0, 270.0 };
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(std::max(8, int(art.height() * 0.44)));
        p.setFont(f);
        p.setPen(kWindInk);
        const QRectF top(art.left(), art.top(), art.width(), art.height() * 0.55);
        p.drawText(top, Qt::AlignCenter, tr(letters[rank - 1]));
        paintArrow(p, QPointF(art.center().x(), art.top() + art.height() * 0.77),
                   std::min(art.width(), art.height() * 0.40) * 0.85, angles[rank - 1],
                   kDiamondInk);
        break;
    }
    case Suit::Dragon:
        if (rank == 1)
            paintFlame(p, art);
        else if (rank == 2)
            paintTree(p, art);
        else
            paintFrame(p, art, legible);
        break;
    case Suit::Flower:
    case Suit::Season: {
        // A band of colour round the inside says which of the two families a
        // tile is in, since any two of one family match.
        const QColor band = suit == Suit::Flower ? QColor(0xe0, 0x4a, 0x8c) : QColor(0xd8, 0xa0, 0x20);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(band, std::max(1.5, w * 0.035)));
        p.drawRoundedRect(r.adjusted(w * 0.05, w * 0.05, -w * 0.05, -w * 0.05), radius * 0.7,
                          radius * 0.7);
        numeral(band.darker(130));
        const QRectF field = art.adjusted(0, art.height() * 0.12, 0, 0);
        if (suit == Suit::Flower)
            paintFlower(p, field, rank);
        else
            paintSeason(p, field, rank);
        break;
    }
    case Suit::None:
        break;
    }

    // A held tile a shade darker, so the free ones stand out. Light enough
    // that its face still reads.
    if (alpha >= 1.0 && m_dimHeld && !m_board.isFree(slot) && !m_won) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x20, 0x18, 0x08, legible ? 40 : 60));
        p.drawRoundedRect(r, radius, radius);
    }

    if (alpha >= 1.0 && picked) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Theme::kGold.darker(140), std::max(2.5, w * (legible ? 0.07 : 0.05))));
        p.drawRoundedRect(r.adjusted(1, 1, -1, -1), radius, radius);
    }
    if (alpha >= 1.0 && m_hint && (m_hint->first == slot || m_hint->second == slot)) {
        // A slow pulse, stepped from the time left rather than read off a clock.
        const double pulse = 0.55 + 0.45 * std::sin(m_hintLeftMs / 160.0);
        QColor glow = kHintGlow;
        glow.setAlphaF(float(pulse));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(glow, std::max(3.0, w * 0.08)));
        p.drawRoundedRect(r.adjusted(-1, -1, 1, 1), radius, radius);
    }
    p.restore();
}

void MahjongView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF all(rect());
    Theme::paintFelt(p, all, Theme::kFeltTealTop, Theme::kFeltTealBottom);

    for (int s = 0; s < mahjong::kTiles; ++s)
        if (m_board.present(s))
            paintTile(p, s, m_board.at(s), 1.0, 0.0);

    // A taken pair lifts off and fades where it lay, over the heap.
    for (const Fade& f : m_fades)
        paintTile(p, f.slot, f.face, 1.0 - f.done, f.done * tileWidth() * 0.4);

    const bool legible = Legibility::instance().enabled();
    if (keyboardCursorShown() && m_board.isFree(m_cursor))
        Theme::paintCardCursor(p, faceRect(m_cursor), legible);

    // The sentence under the heap: the status bar is not read during play.
    // Under the legibility switch the caption plate carries it instead.
    if (!legible) {
        const QRectF strip = sentenceRect();
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(std::max(12, int(strip.height() * 0.5)));
        p.setFont(f);
        p.setPen(QColor(0xf4, 0xf0, 0xe0));
        p.drawText(strip, Qt::AlignCenter | Qt::TextWordWrap, boardSentence());
    }

    paintStatusCaption(p, all);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void MahjongView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_won) {
        newGame();
        return;
    }
    const int slot = slotAt(event->position());
    if (slot < 0)
        return;
    // The cursor follows the mouse onto a tile it may stand on.
    if (m_board.isFree(slot))
        m_cursor = slot;
    pressAt(slot);
}

void MahjongView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Left: stepCursor(-1, 0); break;
    case Qt::Key_Right: stepCursor(1, 0); break;
    case Qt::Key_Up: stepCursor(0, -1); break;
    case Qt::Key_Down: stepCursor(0, 1); break;
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_won) {
            newGame();
            return;
        }
        settleCursor();
        if (m_cursor >= 0)
            pressAt(m_cursor);
        return;
    case Qt::Key_Escape:
        m_picked = -1;
        m_message.clear();
        refresh();
        return;
    case Qt::Key_H: showHint(); return;
    case Qt::Key_S: shuffle(); return;
    default:
        GameView::keyPressEvent(event);
        return;
    }
    update();
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

// The tiles still in place and the cursor. The undo history is not kept: a
// resumed heap starts a fresh one.
QByteArray MahjongView::saveState() const
{
    if (m_won || (m_history.empty() && !m_resumed))
        return {};
    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << kBlobVersion;
    for (Face f : m_board.faces())
        out << quint8(f);
    out << qint16(m_cursor);
    return blob;
}

bool MahjongView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    in >> version;
    if (version != kBlobVersion || in.status() != QDataStream::Ok)
        return false;
    std::array<Face, mahjong::kTiles> faces {};
    for (Face& f : faces) {
        quint8 value = 0;
        in >> value;
        f = value;
    }
    qint16 cursor = -1;
    in >> cursor;
    if (in.status() != QDataStream::Ok)
        return false;

    // Into a board of its own, so a refused blob leaves the heap on screen
    // alone. A cleared heap is a finished game, and finished games are not kept.
    mahjong::Board board;
    if (!board.restore(faces) || board.cleared())
        return false;

    m_board = std::move(board);
    m_history.clear();
    m_picked = -1;
    m_cursor = cursor;
    m_message.clear();
    m_won = false;
    m_resumed = true;
    m_hint.reset();
    m_fades.clear();
    m_undoAction->setEnabled(false);
    settleCursor();
    refresh();
    return true;
}
