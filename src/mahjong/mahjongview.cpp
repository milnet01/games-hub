#include "mahjongview.h"

#include "mahjongart.h"
#include "dealseed.h"
#include "legibility.h"
#include "sound.h"
#include "theme.h"

#include <QDataStream>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <utility>

using mahjong::Face;
using mahjong::kFaceBottom;
using mahjong::kFaceTop;
using mahjong::kTileBack;
using mahjong::kTileBackDark;
using mahjong::kTileEdge;

namespace {

constexpr quint32 kBlobVersion = 1;
constexpr int kTickMs = 16;
constexpr double kFadeMs = 320.0;
constexpr double kHintMs = 3000.0;
// A tile is this much taller than it is wide.
constexpr double kTileAspect = 1.3;

constexpr QColor kHintGlow { 0x4c, 0xd2, 0xff };

const QString& dimKey()
{
    static const QString key = QStringLiteral("mahjong/dimHeld"); // untranslated: settings key
    return key;
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

QSize MahjongView::minimumSizeHint() const
{
    // Tall enough that large play's tiles clear mahjong::kTileMinWidth, and the
    // same with the switch off, so turning it on never grows the window.
    return { 660, 500 };
}

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

    // The markings, from the art both Mahjong games share.
    mahjong::paintTileFace(p, r, face, legible, font());

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
