#include "twistyview.h"

#include "legibility.h"
#include "scores.h"
#include "sound.h"
#include "theme.h"

#include <QAbstractButton>
#include <QActionGroup>
#include <QDataStream>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QSettings>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

// Longer than anyone spends on one puzzle, and small enough that a thousandth
// of it fits an int, which is what the clock is narrowed to for Scores.
constexpr qint64 kMaxPlayedMs = qint64(100) * 60 * 60 * 1000;
// How far the mouse moves before a press becomes a drag.
constexpr double kDragStart = 8.0;
// Degrees the view turns per pixel dragged, and per Ctrl and arrow key.
constexpr double kOrbitPerPixel = 0.45;
constexpr double kOrbitStep = 15.0;
// A turn the player makes takes this long, so it can be followed by eye. A
// queue of turns goes quicker so the puzzle never falls far behind the hand.
constexpr double kTurnMs = 280.0;
constexpr double kQueuedTurnMs = 150.0;
// A new scramble is shown turning, in this long whatever its length.
constexpr double kRevealMs = 1800.0;
constexpr double kCelebrateMs = 2200.0;
constexpr double kGravity = 900.0; // confetti, pixels a second a second

// Where the view starts, per shape: three faces of a cube, and an edge of the
// pyramid towards you so two of its faces show rather than one square on.
double defaultYaw(twisty::Shape shape)
{
    return shape == twisty::Shape::Pyramid ? 25.0 : -35.0;
}
constexpr double kDefaultPitch = 28.0;

const char* shapeId(twisty::Shape shape)
{
    switch (shape) {
    case twisty::Shape::Cube2: return "2x2";               // untranslated: settings key
    case twisty::Shape::Cube3: return "3x3";               // untranslated: settings key
    case twisty::Shape::Cube4: return "4x4";               // untranslated: settings key
    case twisty::Shape::Cube5: return "5x5";               // untranslated: settings key
    case twisty::Shape::Pyramid: return "pyramid";         // untranslated: settings key
    case twisty::Shape::Dodecahedron: return "dodecahedron"; // untranslated: settings key
    }
    return "3x3"; // untranslated: settings key
}

double ease(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// A turn goes a few degrees past and settles back, so it lands with a snap
// the way a real layer clicks into place.
double snap(double t)
{
    t = std::clamp(t, 0.0, 1.0) - 1.0;
    constexpr double kOvershoot = 1.2;
    return 1.0 + (kOvershoot + 1.0) * t * t * t + kOvershoot * t * t;
}

} // namespace

TwistyView::TwistyView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(TwistyView::minimumSizeHint());
    setFocusPolicy(Qt::StrongFocus);

    m_frame = new QTimer(this);
    m_frame->setInterval(16);
    connect(m_frame, &QTimer::timeout, this, &TwistyView::onFrame);
    m_tick = new QTimer(this);
    m_tick->setInterval(500);
    connect(m_tick, &QTimer::timeout, this, [this] { refresh(); update(); });

    const int stored = std::clamp(
        QSettings().value(QStringLiteral("twisty/shape"), int(twisty::Shape::Cube3)).toInt(), 0,
        twisty::kShapeCount - 1);
    m_puzzle = twisty::Puzzle(twisty::Shape(stored));
    m_yaw = defaultYaw(m_puzzle.shape());
    buildActions();
    newScramble(false);
}

void TwistyView::buildActions()
{
    auto* scramble = new QAction(tr("New Scramble"), this);
    scramble->setShortcut(QKeySequence::New);
    connect(scramble, &QAction::triggered, this, [this] { newScramble(true); });
    m_actions.append(scramble);

    auto* undoAction = new QAction(tr("Undo"), this);
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, &TwistyView::undo);
    m_actions.append(undoAction);

    m_pauseAction = new QAction(tr("Pause"), this);
    m_pauseAction->setCheckable(true);
    // P pauses in every game that pauses (GHUB-0226).
    m_pauseAction->setShortcut(QKeySequence(Qt::Key_P));
    connect(m_pauseAction, &QAction::toggled, this, [this](bool on) {
        if (on == m_paused)
            return;
        if (on)
            m_elapsedMs = elapsedMs();
        else
            m_clock.restart();
        m_paused = on;
        update();
        refresh();
    });
    m_actions.append(m_pauseAction);

    auto* straighten = new QAction(tr("Straighten View"), this);
    straighten->setShortcut(QKeySequence(Qt::Key_Home));
    connect(straighten, &QAction::triggered, this, [this] {
        m_yaw = defaultYaw(m_puzzle.shape());
        m_pitch = kDefaultPitch;
        keepCursorInSight();
        update();
    });
    m_actions.append(straighten);

    auto* sep = new QAction(this);
    sep->setSeparator(true);
    m_actions.append(sep);

    m_shapeGroup = new QActionGroup(this);
    m_shapeGroup->setExclusive(true);
    for (int s = 0; s < twisty::kShapeCount; ++s) {
        const auto shape = twisty::Shape(s);
        auto* a = new QAction(shapeName(shape), this);
        a->setCheckable(true);
        a->setChecked(shape == m_puzzle.shape());
        a->setData(s);
        m_shapeGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, shape] { setShape(shape); });
        m_actions.append(a);
    }
}

QString TwistyView::shapeName(twisty::Shape shape) const
{
    switch (shape) {
    case twisty::Shape::Cube2: return tr("2×2");
    case twisty::Shape::Cube3: return tr("3×3");
    case twisty::Shape::Cube4: return tr("4×4");
    case twisty::Shape::Cube5: return tr("5×5");
    case twisty::Shape::Pyramid: return tr("Pyramid");
    case twisty::Shape::Dodecahedron: return tr("12 Sides");
    }
    return {};
}

QString TwistyView::bestKey() const
{
    return QStringLiteral("twisty/best_time_") + QLatin1String(shapeId(m_puzzle.shape())); // untranslated: settings key
}

// GHUB-0016. The rules as twisty::Puzzle plays them and the controls as the
// mouse and key handlers below take them.
QString TwistyView::rulesText() const
{
    return tr(
        "<h3>The aim</h3>"
        "<p>Turn the puzzle's layers until every face is one colour again.</p>"

        "<h3>How it plays</h3>"
        "<p>Every puzzle starts mixed up by real turns, so every one can be "
        "solved. Each colour also carries its own mark, such as a circle, a star "
        "or a heart, so a face can be told by its mark as well as its colour.</p>"
        "<p>There are six shapes. Choose one on the toolbar:</p>"
        "<ul>"
        "<li><b>2×2, 3×3, 4×4 and 5×5</b>: cubes, with six faces. Any slice of the cube "
        "turns, a quarter turn at a time.</li>"
        "<li><b>Pyramid</b>: four faces. Each corner turns a third of a turn, "
        "either the small tip alone or the tip with the layer under it.</li>"
        "<li><b>12 Sides</b>: twelve faces. Each face turns a fifth of a "
        "turn, taking the edges of its neighbours with it.</li>"
        "</ul>"
        "<p>The clock starts on your first turn, so you can look the puzzle over "
        "first. It stops while the game is paused or another game is on "
        "screen. Your best time is kept for each shape.</p>"

        "<h3>Controls</h3>"
        "<p><b>With the mouse:</b></p>"
        "<ul>"
        "<li>Drag a sticker the way you want its layer to go. The layer turns "
        "one step.</li>"
        "<li>Drag anywhere off the puzzle, or on a face's centre on the twelve-"
        "sided shape, to turn the whole puzzle round and see the other side.</li>"
        "</ul>"
        "<p><b>With the keyboard:</b></p>"
        "<ul>"
        "<li>The arrow keys move the gold frame from sticker to sticker.</li>"
        "<li>Shift and an arrow key turns the framed sticker's layer that way, as "
        "a drag would.</li>"
        "<li>Ctrl and an arrow key turns the whole puzzle round.</li>"
        "</ul>"
        "<p><b>On the toolbar:</b></p>"
        "<ul>"
        "<li><b>New Scramble</b> (Ctrl+N) mixes the puzzle up afresh.</li>"
        "<li><b>Undo</b> (Ctrl+Z) takes back your last turn. Undone turns still "
        "come off the move count.</li>"
        "<li><b>Pause</b> (P) stops the clock and hides the puzzle.</li>"
        "<li><b>Straighten View</b> (Home) turns the puzzle back to where it "
        "started.</li>"
        "<li>The six shapes. Choosing one starts a new scramble of it.</li>"
        "</ul>");
}

// ---------------------------------------------------------------------------
// The game
// ---------------------------------------------------------------------------

void TwistyView::setShape(twisty::Shape shape)
{
    QSettings().setValue(QStringLiteral("twisty/shape"), int(shape));
    m_puzzle = twisty::Puzzle(shape);
    for (QAction* a : m_shapeGroup->actions())
        a->setChecked(a->data().toInt() == int(shape));
    m_cursor = -1;
    m_yaw = defaultYaw(shape);
    m_pitch = kDefaultPitch;
    newScramble(isVisible());
}

void TwistyView::newScramble(bool animate)
{
    m_anim.reset();
    m_queue.clear();
    m_reveal.reset();
    m_celebrate = -1.0;
    m_puzzle.scramble(m_rng);
    if (animate) {
        m_reveal = twisty::Puzzle(m_puzzle.shape());
        m_revealNext = 0;
        m_frameClock.start();
        m_frame->start();
    }
    m_running = false;
    m_solved = false;
    m_paused = false;
    m_elapsedMs = 0;
    if (m_pauseAction != nullptr)
        m_pauseAction->setChecked(false);
    m_tick->stop();
    if (m_cursor >= int(m_puzzle.stickers().size()))
        m_cursor = -1;
    update();
    refresh();
}

void TwistyView::queueTurn(twisty::Move move)
{
    if (m_solved || m_paused || m_reveal)
        return;
    m_queue.push_back(move);
    if (!m_anim)
        startNext();
}

void TwistyView::startNext()
{
    if (m_queue.empty())
        return;
    const twisty::Move move = m_queue.front();
    m_queue.pop_front();
    m_puzzle.turn(move);
    if (!m_running) {
        m_running = true;
        m_clock.start();
        m_tick->start();
    }
    m_anim = Anim { move, 0.0, m_queue.empty() ? kTurnMs : kQueuedTurnMs };
    Sound::instance().play(Sound::kClick);
    m_frameClock.start();
    m_frame->start();
    refresh();
}

void TwistyView::onFrame()
{
    const double dt = double(m_frameClock.restart());
    if (m_reveal) {
        const auto& moves = m_puzzle.scrambleMoves();
        const double per = std::clamp(kRevealMs / double(moves.size()), 30.0, 110.0);
        if (!m_anim) {
            if (m_revealNext >= moves.size()) {
                m_reveal.reset();
            } else {
                const twisty::Move move = moves[m_revealNext++];
                m_reveal->turn(move);
                m_anim = Anim { move, 0.0, per };
            }
        }
    }
    if (m_anim) {
        m_anim->t += dt / m_anim->durationMs;
        if (m_anim->t >= 1.0)
            finishAnim();
    }
    if (m_celebrate >= 0.0) {
        m_celebrate += dt / kCelebrateMs;
        const double seconds = dt / 1000.0;
        for (Bit& b : m_confetti) {
            b.velocity += QPointF(0, kGravity * seconds);
            b.at += b.velocity * seconds;
            b.angle += b.spin * seconds;
        }
        if (m_celebrate >= 1.0) {
            m_celebrate = -1.0;
            m_confetti.clear();
        }
    }
    if (!m_anim && !m_reveal && m_celebrate < 0.0)
        m_frame->stop();
    update();
}

void TwistyView::finishAnim()
{
    m_anim.reset();
    if (m_reveal)
        return;
    if (m_puzzle.solved() && m_puzzle.moveCount() > 0 && !m_solved) {
        m_queue.clear();
        onSolved();
        return;
    }
    startNext();
}

void TwistyView::settle()
{
    while (m_reveal || m_anim || !m_queue.empty()) {
        if (m_reveal && !m_anim) {
            const auto& moves = m_puzzle.scrambleMoves();
            if (m_revealNext >= moves.size()) {
                m_reveal.reset();
                continue;
            }
            m_reveal->turn(moves[m_revealNext++]);
            m_anim = Anim {};
        }
        if (m_anim)
            finishAnim();
        else
            startNext();
    }
    m_celebrate = -1.0;
    m_confetti.clear();
    m_frame->stop();
    update();
}

void TwistyView::onSolved()
{
    m_solved = true;
    m_elapsedMs = elapsedMs();
    m_running = false;
    m_tick->stop();
    const int seconds = int(m_elapsedMs / 1000);
    const int moves = m_puzzle.moveCount();
    const bool newBest = Scores::instance().recordLow(bestKey(), seconds);
    Sound::instance().play(Sound::kWin);
    m_celebrate = 0.0;
    throwConfetti();
    m_frameClock.start();
    m_frame->start();
    refresh();
    announceLater(int(kCelebrateMs) + 200, [this, seconds, moves, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(tr("Solved"));
        box.setText(tr("Every face is one colour!"));
        const QString time = timeText(qint64(seconds) * 1000);
        box.setInformativeText(newBest
            ? tr("%1 in %n turns, a new best time.", nullptr, moves).arg(time)
            : tr("%1 in %n turns. Best time: %2.", nullptr, moves)
                  .arg(time, timeText(qint64(Scores::instance().best(bestKey(), seconds)) * 1000)));
        QAbstractButton* again = box.addButton(tr("New Scramble"), QMessageBox::AcceptRole);
        box.addButton(tr("Close"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == again)
            newScramble(true);
    });
}

void TwistyView::throwConfetti()
{
    // Its own small generator, seeded from the solve, so a celebration never
    // moves the scrambles that --seed pins.
    std::minstd_rand fx(unsigned(m_puzzle.moveCount()) + 1u);
    const auto unit = [&] { return double(fx() - fx.min()) / double(fx.max() - fx.min()); };
    const QPointF from = puzzleArea().center();
    m_confetti.clear();
    for (int i = 0; i < 120; ++i) {
        const double angle = (-90.0 + (unit() - 0.5) * 120.0) * std::numbers::pi / 180.0;
        const double speed = 380.0 + 420.0 * unit();
        m_confetti.push_back({ from, QPointF(std::cos(angle), std::sin(angle)) * speed, 360.0 * unit(),
                               (unit() - 0.5) * 900.0, int(unit() * m_puzzle.faceCount()) % m_puzzle.faceCount() });
    }
}

void TwistyView::undo()
{
    if (m_solved || m_paused || m_reveal)
        return;
    settle();
    twisty::Move undone;
    if (!m_puzzle.undo(&undone))
        return;
    // Shown turning back the way it came.
    m_anim = Anim { { undone.layer, !undone.clockwise }, 0.0, kTurnMs };
    Sound::instance().play(Sound::kClick);
    m_frameClock.start();
    m_frame->start();
    refresh();
}

void TwistyView::activate()
{
    if (m_suspended) {
        m_suspended = false;
        m_clock.restart();
        if (m_running && !m_solved)
            m_tick->start();
    }
    setFocus();
    refresh();
}

void TwistyView::deactivate()
{
    settle();
    if (m_suspended)
        return;
    m_elapsedMs = elapsedMs();
    m_suspended = true;
    m_tick->stop();
}

bool TwistyView::advanceForShot(int turns)
{
    settle();
    for (int i = 0; i < turns; ++i)
        m_puzzle.turn({ int(m_rng() % unsigned(m_puzzle.layers().size())), (m_rng() & 1u) != 0 });
    update();
    refresh();
    return true;
}

qint64 TwistyView::elapsedMs() const
{
    const bool ticking = m_running && !m_paused && !m_suspended && !m_solved;
    return std::min(kMaxPlayedMs, m_elapsedMs + (ticking ? m_clock.elapsed() : 0));
}

QString TwistyView::timeText(qint64 ms) const
{
    const qint64 seconds = ms / 1000;
    return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

void TwistyView::refresh()
{
    QString line;
    if (m_solved)
        line = tr("Solved in %1 and %n turns.", nullptr, m_puzzle.moveCount()).arg(timeText(m_elapsedMs));
    else if (m_paused)
        line = tr("Paused. Press P to carry on.");
    else if (!m_running)
        line = tr("Drag a sticker to turn its layer. The clock starts on your first turn.");
    else
        line = tr("%1   Turns %2   Time %3")
                   .arg(shapeName(m_puzzle.shape()))
                   .arg(m_puzzle.moveCount())
                   .arg(timeText(elapsedMs()));
    if (Scores::instance().has(bestKey()))
        line = tr("%1   Best %2").arg(line, timeText(qint64(Scores::instance().best(bestKey())) * 1000));
    Q_EMIT statusChanged(line);
}

// ---------------------------------------------------------------------------
// Where things are
// ---------------------------------------------------------------------------

QRectF TwistyView::puzzleArea() const
{
    const bool legible = Legibility::instance().enabled();
    const double hud = legible ? 64.0 : 48.0;
    QRectF area = QRectF(rect()).adjusted(12, hud, -12, -12);
    area.setBottom(area.bottom() - captionBand(QRectF(rect())));
    return area;
}

twisty::Camera TwistyView::camera() const
{
    double yaw = m_yaw;
    if (m_celebrate >= 0.0)
        yaw += 360.0 * ease(m_celebrate / 0.75);
    return twisty::Camera::fitting(shown(), puzzleArea(), yaw, m_pitch);
}

twisty::Spin TwistyView::spin() const
{
    if (!m_anim)
        return {};
    const double step = shown().stepAngle(m_anim->move.layer);
    const double signedStep = m_anim->move.clockwise ? -step : step;
    // The puzzle already holds the turned position, so the layer is drawn
    // turned back by what is left of the turn.
    return { m_anim->move.layer, signedStep * (snap(m_anim->t) - 1.0) };
}

void TwistyView::outlines(std::vector<QPolygonF>& outline, std::vector<bool>& facing) const
{
    const twisty::Camera cam = camera();
    const twisty::Vec3 eye = cam.eye();
    const auto& stickers = shown().stickers();
    outline.assign(stickers.size(), {});
    facing.assign(stickers.size(), false);
    for (size_t i = 0; i < stickers.size(); ++i) {
        const twisty::Sticker& s = stickers[i];
        facing[i] = s.normal.dot(eye - s.centre) > 0.0;
        for (const twisty::Vec3& c : s.corners)
            outline[i] << cam.project(c);
    }
}

int TwistyView::stickerAt(const QPointF& point) const
{
    std::vector<QPolygonF> outline;
    std::vector<bool> facing;
    outlines(outline, facing);
    for (size_t i = 0; i < outline.size(); ++i)
        if (facing[i] && outline[i].containsPoint(point, Qt::OddEvenFill))
            return int(i);
    return -1;
}

QPointF TwistyView::stickerCentre(int sticker) const
{
    return camera().project(shown().stickers()[size_t(sticker)].centre);
}

int TwistyView::centreSticker() const
{
    std::vector<QPolygonF> outline;
    std::vector<bool> facing;
    outlines(outline, facing);
    const QPointF middle = puzzleArea().center();
    int best = -1;
    double bestDistance = 0.0;
    for (size_t i = 0; i < outline.size(); ++i) {
        if (!facing[i])
            continue;
        const QPointF d = outline[i].boundingRect().center() - middle;
        const double distance = std::hypot(d.x(), d.y());
        if (best < 0 || distance < bestDistance) {
            best = int(i);
            bestDistance = distance;
        }
    }
    return best;
}

void TwistyView::keepCursorInSight()
{
    if (m_cursor < 0)
        return;
    const twisty::Sticker& s = shown().stickers()[size_t(m_cursor)];
    if (s.normal.dot(camera().eye() - s.centre) <= 0.0)
        m_cursor = centreSticker();
}

void TwistyView::moveCursor(const QPointF& direction)
{
    if (m_cursor < 0) {
        m_cursor = centreSticker();
        update();
        return;
    }
    std::vector<QPolygonF> outline;
    std::vector<bool> facing;
    outlines(outline, facing);
    const QPointF from = stickerCentre(m_cursor);
    const auto& stickers = shown().stickers();
    int best = -1;
    double bestScore = 0.0;
    for (size_t i = 0; i < stickers.size(); ++i) {
        if (!facing[i] || int(i) == m_cursor)
            continue;
        const QPointF w = stickerCentre(int(i)) - from;
        const double along = w.x() * direction.x() + w.y() * direction.y();
        const double across = std::abs(w.x() * direction.y() - w.y() * direction.x());
        // Ahead, and within about fifty degrees of straight ahead.
        if (along <= 1.0 || across > along * 1.2)
            continue;
        const double score = along + 2.0 * across;
        if (best < 0 || score < bestScore) {
            best = int(i);
            bestScore = score;
        }
    }
    if (best >= 0)
        m_cursor = best;
    update();
}

bool TwistyView::turnFrom(int sticker, const QPointF& direction)
{
    const twisty::Puzzle& puzzle = shown();
    if (sticker < 0 || sticker >= int(puzzle.stickers().size()) || m_solved || m_paused || m_reveal)
        return false;
    const double length = std::hypot(direction.x(), direction.y());
    if (length < 1e-6)
        return false;
    const twisty::Camera cam = camera();
    const twisty::Sticker& s = puzzle.stickers()[size_t(sticker)];
    const QPointF from = cam.project(s.centre);

    // Of the layers this sticker turns with, the one that moves it most
    // nearly the way the drag went. A layer whose axis runs through the
    // sticker's own face only spins it on the spot, so it is left out. Where
    // two layers move it alike -- the pyramid's tip and the tip with the layer
    // under it -- the smaller one wins, so a tip turns alone.
    int best = -1;
    double bestCos = 0.0;
    int bestSize = 0;
    for (int l = 0; l < int(puzzle.layers().size()); ++l) {
        if (!puzzle.inLayer(l, sticker))
            continue;
        const twisty::Vec3& axis = puzzle.axes()[size_t(puzzle.layers()[size_t(l)].axis)];
        if (std::abs(axis.dot(s.normal)) > 0.95)
            continue;
        const QPointF moved = cam.project(twisty::rotate(s.centre, axis, -0.05)) - from;
        const double speed = std::hypot(moved.x(), moved.y());
        if (speed < 1e-3)
            continue;
        const double cosine = (moved.x() * direction.x() + moved.y() * direction.y()) / (speed * length);
        int size = 0;
        for (int i = 0; i < int(puzzle.stickers().size()); ++i)
            size += puzzle.inLayer(l, i) ? 1 : 0;
        const bool better = best < 0 || std::abs(cosine) > std::abs(bestCos) + 1e-6
            || (std::abs(cosine) > std::abs(bestCos) - 1e-6 && size < bestSize);
        if (better) {
            best = l;
            bestCos = cosine;
            bestSize = size;
        }
    }
    if (best < 0 || std::abs(bestCos) < 0.3)
        return false;
    queueTurn({ best, bestCos > 0.0 });
    return true;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void TwistyView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton)
        return;
    m_pressPos = event->position();
    m_lastPos = m_pressPos;
    m_pressSticker = event->button() == Qt::LeftButton ? stickerAt(m_pressPos) : -1;
    m_drag = Drag::Pending;
}

void TwistyView::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF at = event->position();
    if (m_drag == Drag::Pending) {
        const QPointF d = at - m_pressPos;
        if (std::hypot(d.x(), d.y()) < kDragStart)
            return;
        if (m_pressSticker >= 0 && turnFrom(m_pressSticker, d)) {
            m_cursor = m_pressSticker;
            m_drag = Drag::Turned;
            return;
        }
        m_drag = Drag::Orbit;
    }
    if (m_drag == Drag::Orbit) {
        const QPointF d = at - m_lastPos;
        m_yaw += d.x() * kOrbitPerPixel;
        m_pitch = std::clamp(m_pitch + d.y() * kOrbitPerPixel, -89.0, 89.0);
        m_lastPos = at;
        keepCursorInSight();
        update();
    }
}

void TwistyView::mouseReleaseEvent(QMouseEvent*)
{
    // A click on a sticker puts the keyboard's frame there.
    if (m_drag == Drag::Pending && m_pressSticker >= 0) {
        m_cursor = m_pressSticker;
        update();
    }
    m_drag = Drag::None;
    m_pressSticker = -1;
}

void TwistyView::keyPressEvent(QKeyEvent* event)
{
    QPointF direction;
    switch (event->key()) {
    case Qt::Key_Left: direction = { -1, 0 }; break;
    case Qt::Key_Right: direction = { 1, 0 }; break;
    case Qt::Key_Up: direction = { 0, -1 }; break;
    case Qt::Key_Down: direction = { 0, 1 }; break;
    default:
        GameView::keyPressEvent(event);
        return;
    }
    if (event->modifiers() & Qt::ControlModifier) {
        m_yaw += direction.x() * kOrbitStep;
        m_pitch = std::clamp(m_pitch + direction.y() * kOrbitStep, -89.0, 89.0);
        keepCursorInSight();
        update();
        return;
    }
    if (event->modifiers() & Qt::ShiftModifier) {
        if (m_cursor < 0)
            m_cursor = centreSticker();
        turnFrom(m_cursor, direction);
        update();
        return;
    }
    moveCursor(direction);
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void TwistyView::paintHud(QPainter& p) const
{
    const bool legible = Legibility::instance().enabled();
    QFont font = this->font();
    font.setBold(true);
    font.setPointSizeF(legible ? 18.0 : 13.0);
    p.setFont(font);
    const QFontMetricsF fm(font);
    const double h = fm.height() + 12.0;
    const QRectF plate(10, 8, width() - 20.0, h);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 110));
    p.drawRoundedRect(plate, 8, 8);

    p.setPen(QColor(0xf2, 0xee, 0xe4));
    p.drawText(plate.adjusted(14, 0, -14, 0), Qt::AlignLeft | Qt::AlignVCenter,
               shapeName(m_puzzle.shape()));
    QString right = tr("Turns %1   Time %2").arg(m_puzzle.moveCount()).arg(timeText(m_solved ? m_elapsedMs : elapsedMs()));
    if (Scores::instance().has(bestKey()))
        right = tr("%1   Best %2").arg(right, timeText(qint64(Scores::instance().best(bestKey())) * 1000));
    p.drawText(plate.adjusted(14, 0, -14, 0), Qt::AlignRight | Qt::AlignVCenter, right);
}

void TwistyView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(rect());
    // A pool of light behind the puzzle, falling away to dark corners.
    QRadialGradient back(puzzleArea().center() - QPointF(0, r.height() * 0.08),
                         std::max(r.width(), r.height()) * 0.72);
    back.setColorAt(0.0, QColor(0x4a, 0x56, 0x74));
    back.setColorAt(0.45, QColor(0x26, 0x2d, 0x40));
    back.setColorAt(1.0, QColor(0x0c, 0x0e, 0x16));
    p.fillRect(r, back);

    const bool legible = Legibility::instance().enabled();
    if (m_paused) {
        QFont font = this->font();
        font.setPointSizeF(legible ? 22.0 : 16.0);
        font.setBold(true);
        p.setFont(font);
        p.setPen(QColor(0xf2, 0xee, 0xe4));
        p.drawText(r, Qt::AlignCenter, tr("Paused\nPress P to carry on"));
    } else {
        const twisty::Camera cam = camera();
        twisty::paintShadow(p, shown(), cam);
        twisty::paintPuzzle(p, shown(), cam, spin(), keyboardCursorShown() ? m_cursor : -1, legible);
    }
    for (const Bit& b : m_confetti) {
        const twisty::Paint& paint = twisty::paintFor(m_puzzle.shape(), b.face);
        p.save();
        p.translate(b.at);
        p.rotate(b.angle);
        // Turning in the air: its width swings as it tumbles.
        const double w = 11.0 * std::abs(std::cos(b.angle * std::numbers::pi / 90.0)) + 2.0;
        p.setPen(Qt::NoPen);
        p.setBrush(paint.fill);
        p.drawRect(QRectF(-w / 2, -4.0, w, 8.0));
        p.restore();
    }
    paintHud(p);

    if (m_solved && m_celebrate < 0.0) {
        QFont font = this->font();
        font.setPointSizeF(legible ? 24.0 : 18.0);
        font.setBold(true);
        p.setFont(font);
        const QString text = tr("Solved!");
        const QFontMetricsF fm(font);
        const QRectF plate(QPointF(0, 0), QSizeF(fm.horizontalAdvance(text) + 40, fm.height() + 16));
        const QRectF at = plate.translated(r.center().x() - plate.width() / 2, puzzleArea().top() + 6);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 150));
        p.drawRoundedRect(at, 10, 10);
        p.setPen(Theme::kGold);
        p.drawText(at, Qt::AlignCenter, text);
    }
    paintStatusCaption(p, r);
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

QByteArray TwistyView::saveState() const
{
    // A solved puzzle, or one not yet touched, is not worth coming back to:
    // a fresh scramble is as good.
    if (m_solved || m_puzzle.moveCount() == 0)
        return {};
    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1) << m_puzzle.encode() << qint64(elapsedMs()) << qint8(m_paused ? 1 : 0)
        << m_yaw << m_pitch << qint32(m_cursor);
    return blob;
}

bool TwistyView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    QByteArray puzzleBlob;
    qint64 elapsed = 0;
    qint8 paused = 0;
    double yaw = 0.0;
    double pitch = 0.0;
    qint32 cursor = -1;
    in >> version;
    if (in.status() != QDataStream::Ok || version != 1)
        return false;
    in >> puzzleBlob >> elapsed >> paused >> yaw >> pitch >> cursor;
    if (in.status() != QDataStream::Ok || !in.atEnd())
        return false;

    twisty::Puzzle puzzle;
    if (!puzzle.decode(puzzleBlob))
        return false;
    // saveState() writes neither a solved puzzle nor an untouched one.
    if (puzzle.solved() || puzzle.moveCount() == 0)
        return false;
    if (elapsed < 0 || elapsed > kMaxPlayedMs || (paused != 0 && paused != 1))
        return false;
    if (!std::isfinite(yaw) || !std::isfinite(pitch) || std::abs(yaw) > 1e6 || std::abs(pitch) > 89.0)
        return false;
    if (cursor < -1 || cursor >= qint32(puzzle.stickers().size()))
        return false;

    settle();
    m_puzzle = std::move(puzzle);
    m_reveal.reset();
    m_elapsedMs = elapsed;
    m_running = true;
    m_solved = false;
    m_paused = paused == 1;
    m_yaw = std::fmod(yaw, 360.0);
    m_pitch = pitch;
    m_cursor = cursor;
    m_pauseAction->setChecked(m_paused);
    for (QAction* a : m_shapeGroup->actions())
        a->setChecked(a->data().toInt() == int(m_puzzle.shape()));
    QSettings().setValue(QStringLiteral("twisty/shape"), int(m_puzzle.shape()));
    // The hub restores while this page is still hidden, so the clock waits
    // for activate() like any other return to the game.
    m_suspended = true;
    m_clock.restart();
    update();
    refresh();
    return true;
}
