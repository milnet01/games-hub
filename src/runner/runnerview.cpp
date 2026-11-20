#include "runnerview.h"

#include "legibility.h"
#include "sound.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QSettings>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace {
constexpr int kFrameMs = 16;
// The most one tick may move the run on. A longer stall slows the game
// instead, so a hitch never carries the runner into an obstacle unseen.
constexpr double kLongestTick = 0.1;
using Course = RunnerCourse;

// A function for the reason scores.h's own keys are: nothing is built at
// static-initialisation time. The leaf starts "best_" so Scores::clear(),
// which matches by that shape, forgets the table with every other record.
QString tableKey() { return QStringLiteral("runner/best_scores"); } // untranslated: settings key

constexpr QColor kOutline { 0x14, 0x0c, 0x06 };
constexpr QColor kGold { 0xff, 0xd0, 0x3a };
constexpr QColor kPath { 0xc9, 0xa8, 0x6a };
constexpr QColor kRoot { 0xe0, 0x74, 0x2c };
constexpr QColor kStone { 0xa6, 0xb4, 0xbe };
constexpr QColor kWall { 0x8e, 0x7c, 0xa6 };
constexpr QColor kPanelEdge { 0xff, 0xc8, 0x5a };
constexpr QColor kInk { 0xfb, 0xf4, 0xe6 };

double unit(std::mt19937& rng)
{
    return double(rng()) / (double(std::mt19937::max()) + 1.0);
}

// A fixed pseudo-random number in 0..1 for a tile of scenery, so the same
// stretch of jungle looks the same every time it scrolls past.
double tileHash(long long tile, int salt)
{
    std::uint32_t h = std::uint32_t(std::uint64_t(tile) * 2654435761ULL) ^ (std::uint32_t(salt) * 40503U);
    h ^= h >> 15U;
    h *= 0x2c1b3c6dU;
    h ^= h >> 12U;
    h *= 0x297a2d39U;
    h ^= h >> 15U;
    return double(h) / 4294967296.0;
}

// A line drawn twice: dark and wide, then its colour on top, so every limb
// and root carries an outline whatever it crosses.
void outlinedLine(QPainter& p, QPointF a, QPointF b, double width, const QColor& colour, double outline)
{
    p.setPen(QPen(kOutline, width + 2.0 * outline, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(a, b);
    p.setPen(QPen(colour, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(a, b);
}

// A point `length` from `from`, at `angle` radians from straight down,
// positive towards the front (right).
QPointF limbEnd(QPointF from, double angle, double length)
{
    return from + QPointF(std::sin(angle) * length, std::cos(angle) * length);
}

QPainterPath heartPath(QPointF centre, double size)
{
    QPainterPath h;
    const double s = size / 2.0;
    h.moveTo(centre + QPointF(0, s * 0.9));
    h.cubicTo(centre + QPointF(-s * 1.4, -s * 0.1), centre + QPointF(-s * 0.7, -s * 1.3),
              centre + QPointF(0, -s * 0.45));
    h.cubicTo(centre + QPointF(s * 0.7, -s * 1.3), centre + QPointF(s * 1.4, -s * 0.1),
              centre + QPointF(0, s * 0.9));
    return h;
}
}

RunnerView::RunnerView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(RunnerView::minimumSizeHint());
    setFocusPolicy(Qt::StrongFocus);

    m_timer = new QTimer(this);
    m_timer->setInterval(kFrameMs);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &RunnerView::tick);

    m_table = highScores();
    buildActions();
    newGame();
}

void RunnerView::buildActions()
{
    auto* newAction = new QAction(tr("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &RunnerView::newGame);
    m_actions.append(newAction);

    m_pause = new QAction(tr("Pause"), this);
    m_pause->setCheckable(true);
    // P, as in every game that pauses. Space jumps here, so it cannot pause.
    m_pause->setShortcut(QKeySequence(Qt::Key_P));
    connect(m_pause, &QAction::toggled, this, [this] {
        releaseKeys();
        runClock();
        refresh();
        update();
        setFocus();
    });
    m_actions.append(m_pause);
}

void RunnerView::newGame()
{
    m_course.newGame();
    m_course.takeEvents();
    m_recorded = false;
    m_rank = -1;
    m_particles.clear();
    m_popups.clear();
    m_shake = m_flash = 0.0;
    m_bannerAge = 10.0;
    m_banner.clear();
    releaseKeys();
    m_pause->setChecked(false);
    runClock();
    setFocus();
    refresh();
    update();
}

void RunnerView::releaseKeys()
{
    m_duckKey = m_duckButton = false;
    m_course.setDuck(false);
}

void RunnerView::activate()
{
    m_active = true;
    setFocus();
    runClock();
    refresh();
}

void RunnerView::deactivate()
{
    m_active = false;
    // A key let go of while another game had focus never reaches this view.
    releaseKeys();
    runClock();
}

void RunnerView::runClock()
{
    // The clock runs while the page is shown and not paused, Ready included,
    // so the scarf and the coins move before the run starts.
    if (m_active && !m_pause->isChecked() && !m_heldForShot) {
        m_timer->start();
        m_clock.start();
    } else
        m_timer->stop();
}

QList<int> RunnerView::highScores()
{
    QList<int> table;
    const QVariantList stored = QSettings().value(tableKey()).toList();
    for (const QVariant& v : stored)
        if (v.toInt() > 0)
            table.append(v.toInt());
    std::sort(table.begin(), table.end(), std::greater<>());
    while (table.size() > kTableSize)
        table.removeLast();
    return table;
}

int RunnerView::recordScore(int score)
{
    if (score <= 0)
        return -1;
    QList<int> table = highScores();
    int at = 0;
    while (at < table.size() && table[at] >= score)
        ++at;
    if (at >= kTableSize)
        return -1;
    table.insert(at, score);
    while (table.size() > kTableSize)
        table.removeLast();
    QVariantList stored;
    for (int v : table)
        stored.append(v);
    QSettings().setValue(tableKey(), stored);
    return at;
}

void RunnerView::gameOver()
{
    if (m_recorded)
        return;
    m_recorded = true;
    m_rank = recordScore(m_course.score());
    m_table = highScores();
}

bool RunnerView::advanceForShot(int turns)
{
    // One turn is one second of running with the course's own pilot at the
    // keys, driven through the course alone, so no sound plays and no effect
    // is left in the air. The clock is held from here on, so the picture is
    // of this moment and a seeded shot is the same every time: the clock
    // moves by wall time, and the slow first paint would carry the run on.
    m_heldForShot = true;
    runClock();
    if (turns > 0)
        m_course.start();
    for (int f = 0; f < turns * 60 && !m_course.over(); ++f) {
        const Course::Input in = m_course.pilot();
        if (in.jump)
            m_course.jump();
        m_course.setDuck(in.duck);
        m_course.advance(1.0 / 60.0);
    }
    m_course.takeEvents();
    m_particles.clear();
    m_popups.clear();
    m_shake = m_flash = 0.0;
    m_bannerAge = 10.0;
    if (m_course.over())
        gameOver();
    refresh();
    update();
    return true;
}

void RunnerView::runUnattendedForTest(double seconds)
{
    m_course.start();
    m_course.setDuck(false);
    const int frames = int(std::ceil(seconds * 60.0));
    for (int f = 0; f < frames && !m_course.over(); ++f) {
        m_course.advance(1.0 / 60.0);
        readEvents();
    }
    refresh();
}

void RunnerView::tick()
{
    const double dt = std::min(kLongestTick, double(m_clock.restart()) / 1000.0);
    m_time += dt;

    m_course.setDuck(m_duckKey || m_duckButton);
    m_course.advance(dt);
    readEvents();

    for (Particle& q : m_particles) {
        q.at += q.velocity * dt;
        q.velocity *= 0.97;
        q.velocity.ry() += 260.0 * dt;
        // Particles stay where they burst while the course moves past, which
        // reads as being left behind.
        if (m_course.phase() == Course::Phase::Running)
            q.at.rx() -= m_course.speed() * dt;
        q.life -= dt;
    }
    std::erase_if(m_particles, [](const Particle& q) { return q.life <= 0.0; });
    for (Popup& u : m_popups) {
        u.age += dt;
        u.at.ry() -= 30.0 * dt;
    }
    std::erase_if(m_popups, [](const Popup& u) { return u.age >= 0.9; });

    m_shake = std::max(0.0, m_shake - dt * 2.5);
    m_flash = std::max(0.0, m_flash - dt * 3.0);
    m_bannerAge += dt;

    refresh();
    update();
}

void RunnerView::readEvents()
{
    for (const Course::Event& e : m_course.takeEvents()) {
        switch (e.kind) {
        case Course::EventKind::Started:
            break;
        case Course::EventKind::Jumped:
            Sound::instance().play(Sound::kJump);
            burst(e.at, kPath, 6, 50.0, 0.0);
            break;
        case Course::EventKind::Landed:
            burst(e.at, kPath.lighter(110), 8, 60.0, 0.0);
            break;
        case Course::EventKind::Coin:
            Sound::instance().play(Sound::kCoin);
            burst(e.at, kGold, 10, 70.0, 20.0);
            m_popups.push_back({ e.at, QStringLiteral("+%1").arg(e.value), 0.0, kGold });
            break;
        case Course::EventKind::Hit:
        case Course::EventKind::Fell:
            Sound::instance().play(Sound::kThud);
            burst(e.at, QColor(0xff, 0xb0, 0x60), 22, 140.0, 40.0);
            burst(e.at, QColor(255, 255, 255), 8, 80.0, 40.0);
            m_shake = 1.0;
            m_flash = 1.0;
            if (m_course.hearts() > 0) {
                m_banner = m_course.hearts() == 1 ? tr("Ouch! Last heart.")
                                                  : tr("Ouch! %1 hearts left.").arg(m_course.hearts());
                m_bannerAge = 0.0;
            }
            break;
        case Course::EventKind::Milestone:
            m_banner = tr("%1 metres!").arg(e.value);
            m_bannerAge = 0.0;
            break;
        case Course::EventKind::GameOver:
            Sound::instance().play(Sound::kLose);
            gameOver();
            break;
        }
    }
}

void RunnerView::burst(QPointF at, const QColor& colour, int count, double speed, double lift)
{
    for (int i = 0; i < count; ++i) {
        // Upward half-circle, so dust kicks up rather than into the ground.
        const double angle = std::numbers::pi * (1.0 + unit(m_fx));
        const double v = speed * (0.3 + 0.7 * unit(m_fx));
        const double life = 0.3 + 0.4 * unit(m_fx);
        m_particles.push_back({ at, QPointF(std::cos(angle) * v, std::sin(angle) * v - lift), life, life,
                                1.5 + unit(m_fx) * 2.0, colour.lighter(90 + int(unit(m_fx) * 40)) });
    }
}

void RunnerView::refresh()
{
    QString state;
    if (m_course.over())
        state = tr("Game over");
    else if (m_pause->isChecked())
        state = tr("Paused");
    else if (m_course.phase() == Course::Phase::Ready)
        state = tr("Press Space to start");
    else
        state = tr("%1 m").arg(m_course.metres());

    const int best = std::max(m_table.isEmpty() ? 0 : m_table.first(), m_course.score());
    const QString status = tr("%1   Score %2   Hearts %3   Best %4")
                               .arg(state)
                               .arg(m_course.score())
                               .arg(m_course.hearts())
                               .arg(best);
    if (status != m_statusShown) {
        m_statusShown = status;
        Q_EMIT statusChanged(status);
    }
}

// ---- geometry ----

double RunnerView::scale() const
{
    const double band = captionBand(QRectF(rect()));
    return std::max(0.2, std::min((width() - 16.0) / Course::kWidth,
                                  (height() - 16.0 - band) / Course::kHeight));
}

QRectF RunnerView::courseRect() const
{
    const double s = scale();
    const double w = Course::kWidth * s;
    const double h = Course::kHeight * s;
    const double band = captionBand(QRectF(rect()));
    return { (width() - w) / 2.0, (height() - band - h) / 2.0, w, h };
}

double RunnerView::outlineWidth() const
{
    return Legibility::instance().enabled() ? 2.4 : 1.6;
}

QFont RunnerView::courseFont(double units, bool bold) const
{
    QFont f = font();
    f.setBold(bold);
    const double grow = Legibility::instance().enabled() ? 1.25 : 1.0;
    f.setPixelSize(std::max(10, int(std::lround(units * grow * scale()))));
    return f;
}

void RunnerView::resizeEvent(QResizeEvent* event)
{
    GameView::resizeEvent(event);
    m_backdrop = QPixmap();
}

// ---- the sky: drawn once per size ----

void RunnerView::buildBackdrop()
{
    const qreal dpr = devicePixelRatioF();
    m_backdrop = QPixmap(QSize(std::max(1, int(width() * dpr)), std::max(1, int(height() * dpr))));
    m_backdrop.setDevicePixelRatio(dpr);
    QPainter p(&m_backdrop);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF all(rect());
    const QRectF f = courseRect();
    const double horizon = f.top() + Course::kGroundY * scale();
    // Dawn over the jungle, held soft so nothing in the sky competes with
    // an obstacle.
    QLinearGradient sky(QPointF(0, 0), QPointF(0, horizon));
    sky.setColorAt(0.0, QColor(0x1d, 0x2c, 0x4a));
    sky.setColorAt(0.45, QColor(0x4a, 0x5a, 0x78));
    sky.setColorAt(0.8, QColor(0xb0, 0x86, 0x70));
    sky.setColorAt(1.0, QColor(0xd8, 0xa0, 0x6e));
    p.fillRect(all, sky);

    const QPointF sun(f.left() + f.width() * 0.72, f.top() + f.height() * 0.42);
    const double r = f.height() * 0.11;
    QRadialGradient glow(sun, r * 4.0);
    glow.setColorAt(0.0, QColor(255, 220, 160, 120));
    glow.setColorAt(1.0, QColor(255, 220, 160, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(sun, r * 4.0, r * 4.0);
    p.setBrush(QColor(0xff, 0xe2, 0xa8));
    p.drawEllipse(sun, r, r);
}

// ---- painting ----

void RunnerView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_backdrop.isNull() || m_backdrop.size() != QSize(int(width() * devicePixelRatioF()),
                                                          int(height() * devicePixelRatioF())))
        buildBackdrop();
    p.drawPixmap(0, 0, m_backdrop);

    const QRectF f = courseRect();
    const double s = scale();
    p.save();
    if (m_shake > 0.0) {
        const double amount = 5.0 * m_shake * m_shake;
        p.translate(std::sin(m_time * 71.0) * amount, std::cos(m_time * 53.0) * amount);
    }
    p.translate(f.topLeft());
    p.scale(s, s);

    paintScenery(p);
    paintGround(p);
    // The play itself stays inside the course, so an obstacle never shows
    // before it is laid, however wide the window.
    p.save();
    p.setClipRect(QRectF(0, -f.top() / s, Course::kWidth, height() / s));
    for (const Course::Obstacle& o : m_course.obstacles())
        paintObstacle(p, o);
    paintCoins(p);
    paintRunner(p);
    paintEffects(p);
    p.restore();
    paintWarnings(p);
    p.restore();

    // Beyond the course, the scenery is dimmed, so the play reads as a
    // window and nothing past its edges looks playable.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 120));
    p.drawRect(QRectF(0, 0, f.left(), height()));
    p.drawRect(QRectF(f.right(), 0, width() - f.right(), height()));

    if (m_flash > 0.0)
        p.fillRect(rect(), QColor(255, 80, 40, int(80 * m_flash)));

    paintHud(p);
    paintOverlay(p);
    paintStatusCaption(p, QRectF(rect()));
}

void RunnerView::paintScenery(QPainter& p) const
{
    const QRectF f = courseRect();
    const double s = scale();
    const double left = -f.left() / s;
    const double right = (width() - f.left()) / s;
    const double top = -f.top() / s;
    const double ground = Course::kGroundY;
    const double scroll = m_course.distance();

    // Far hills, barely moving.
    {
        QPainterPath hills;
        hills.moveTo(left, ground);
        const int points = int(std::ceil((right - left) / 8.0)) + 1;
        for (int i = 0; i <= points; ++i) {
            const double x = left + 8.0 * i;
            const double w = x + scroll * 0.08;
            const double h = 70.0 + 24.0 * std::sin(w * 0.013) + 16.0 * std::sin(w * 0.031 + 1.7)
                             + 8.0 * std::sin(w * 0.071 + 0.4);
            hills.lineTo(x, ground - 40.0 - h);
        }
        hills.lineTo(left + 8.0 * points, ground);
        hills.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x5a, 0x6c, 0x7a, 200));
        p.drawPath(hills);
    }

    // Stepped temples among the hills.
    {
        const double pitch = 240.0;
        const double shift = scroll * 0.22;
        const long long first = (long long)std::floor((left + shift) / pitch) - 1;
        const long long last = (long long)std::floor((right + shift) / pitch) + 1;
        p.setBrush(QColor(0x40, 0x52, 0x50));
        for (long long t = first; t <= last; ++t) {
            if (tileHash(t, 1) < 0.35)
                continue;
            const double base = 80.0 + 60.0 * tileHash(t, 2);
            const double cx = double(t) * pitch + pitch * (0.3 + 0.4 * tileHash(t, 3)) - shift;
            const int steps = 4 + int(tileHash(t, 4) * 2.0);
            const double stepH = 12.0 + 4.0 * tileHash(t, 5);
            QPainterPath temple;
            for (int i = 0; i < steps; ++i) {
                const double w = base * (1.0 - 0.17 * i);
                temple.addRect(QRectF(cx - w / 2.0, ground - 30.0 - stepH * (i + 1), w, stepH + 0.5));
            }
            const double shrine = base * 0.22;
            temple.addRect(QRectF(cx - shrine / 2.0, ground - 30.0 - stepH * steps - 16.0, shrine, 16.5));
            p.drawPath(temple.simplified());
        }
    }

    // The tree line: a band of canopy, darker and quicker.
    {
        const double pitch = 38.0;
        const double shift = scroll * 0.45;
        const long long first = (long long)std::floor((left + shift) / pitch) - 2;
        const long long last = (long long)std::floor((right + shift) / pitch) + 2;
        p.setBrush(QColor(0x24, 0x44, 0x2c));
        p.drawRect(QRectF(left, ground - 34.0, right - left, 34.0));
        for (long long t = first; t <= last; ++t) {
            const double r = 22.0 + 16.0 * tileHash(t, 6);
            const double cx = double(t) * pitch - shift;
            p.drawEllipse(QPointF(cx, ground - 30.0 - r * 0.5), r, r * 0.8);
        }
        p.setBrush(QColor(0x1a, 0x33, 0x20));
        for (long long t = first; t <= last; ++t) {
            const double r = 14.0 + 10.0 * tileHash(t, 7);
            const double cx = double(t) * pitch + pitch * 0.5 - shift;
            p.drawEllipse(QPointF(cx, ground - 14.0), r, r * 0.7);
        }
    }

    // A few leaves hanging into the top of the view, held faint.
    {
        const double pitch = 90.0;
        const double shift = scroll * 1.15;
        const long long first = (long long)std::floor((left + shift) / pitch) - 1;
        const long long last = (long long)std::floor((right + shift) / pitch) + 1;
        p.setPen(QPen(QColor(0x1e, 0x3a, 0x22, 170), 2.0));
        for (long long t = first; t <= last; ++t) {
            if (tileHash(t, 8) < 0.5)
                continue;
            const double x = double(t) * pitch - shift;
            const double len = 20.0 + 40.0 * tileHash(t, 9);
            p.drawLine(QPointF(x, top), QPointF(x + 4.0, len));
            p.setBrush(QColor(0x2a, 0x55, 0x2e, 170));
            p.drawEllipse(QPointF(x + 4.0, len), 6.0, 3.5);
        }
    }
}

void RunnerView::paintGround(QPainter& p) const
{
    const QRectF f = courseRect();
    const double s = scale();
    const double left = -f.left() / s;
    const double right = (width() - f.left()) / s;
    const double bottom = (height() - f.top()) / s;
    const double ground = Course::kGroundY;
    const double scroll = m_course.distance();
    const double outline = outlineWidth();

    QLinearGradient earth(QPointF(0, ground), QPointF(0, bottom));
    earth.setColorAt(0.0, QColor(0x6a, 0x48, 0x28));
    earth.setColorAt(1.0, QColor(0x2a, 0x1a, 0x0e));
    p.setPen(Qt::NoPen);
    p.setBrush(earth);
    p.drawRect(QRectF(left, ground, right - left, bottom - ground));

    // The path: a course of worn blocks along the top.
    const double blockW = 48.0;
    const double blockH = 16.0;
    p.setBrush(kPath);
    p.drawRect(QRectF(left, ground, right - left, blockH));
    p.setPen(QPen(QColor(0x6a, 0x50, 0x30), 1.4));
    const double offset = std::fmod(scroll, blockW);
    const int blocks = int(std::ceil((right - left) / blockW)) + 2;
    for (int i = 0; i <= blocks; ++i) {
        const double x = left - offset - blockW + blockW * i;
        p.drawLine(QPointF(x, ground), QPointF(x, ground + blockH));
    }
    p.drawLine(QPointF(left, ground + blockH), QPointF(right, ground + blockH));
    p.setPen(QPen(kOutline, outline));
    p.drawLine(QPointF(left, ground), QPointF(right, ground));

    // Grass tufts on the lip.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x4c, 0x8c, 0x30));
    const double tuft = 22.0;
    const double tuftOffset = std::fmod(scroll, tuft);
    const int tufts = int(std::ceil((right - left) / tuft)) + 2;
    for (int i = 0; i <= tufts; ++i) {
        const double x = left - tuftOffset - tuft + tuft * i;
        QPainterPath g;
        g.moveTo(x, ground + 1);
        g.lineTo(x + 3, ground - 5);
        g.lineTo(x + 6, ground + 1);
        g.lineTo(x + 8, ground - 3);
        g.lineTo(x + 10, ground + 1);
        g.closeSubpath();
        p.drawPath(g);
    }

    // Pits cut through it all; one that has already cost a heart carries a
    // plank, since the runner crosses it as if it were ground.
    for (const Course::Obstacle& o : m_course.obstacles()) {
        if (o.kind != Course::Kind::Gap)
            continue;
        const QRectF pit(o.x, ground - 1.0, o.width, bottom - ground + 2.0);
        QLinearGradient dark(pit.topLeft(), pit.bottomLeft());
        dark.setColorAt(0.0, QColor(0x10, 0x08, 0x04));
        dark.setColorAt(1.0, QColor(0x00, 0x00, 0x00));
        p.setPen(Qt::NoPen);
        p.setBrush(dark);
        p.drawRect(pit);
        p.setPen(QPen(kOutline, outline * 1.5));
        p.drawLine(pit.topLeft(), QPointF(pit.left(), bottom));
        p.drawLine(pit.topRight(), QPointF(pit.right(), bottom));
        // Pale lips, so the edges stand out against the dark.
        p.setPen(QPen(kPath.lighter(130), 3.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(o.x - 6, ground + 1.5), QPointF(o.x - 1, ground + 1.5));
        p.drawLine(QPointF(o.right() + 1, ground + 1.5), QPointF(o.right() + 6, ground + 1.5));
        if (o.spent) {
            const QRectF plank(o.x - 6, ground - 4, o.width + 12, 8);
            p.setPen(QPen(kOutline, outline));
            p.setBrush(QColor(0x9a, 0x6a, 0x3a));
            p.drawRoundedRect(plank, 2, 2);
        }
    }
}

void RunnerView::paintObstacle(QPainter& p, const Course::Obstacle& o) const
{
    const double ground = Course::kGroundY;
    const double outline = outlineWidth();
    switch (o.kind) {
    case Course::Kind::Gap:
        return;   // part of the ground
    case Course::Kind::Root: {
        // A gnarled root arching out of the path.
        const double h = Course::kRootHeight;
        const double bend = 4.0 * (o.look - 1.5);
        QPainterPath root;
        root.moveTo(o.x + 2, ground + 2);
        root.cubicTo(QPointF(o.x + 4 + bend, ground - h * 1.25), QPointF(o.right() - 6 + bend, ground - h * 1.2),
                     QPointF(o.right() - 2, ground + 2));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(kOutline, 9.0 + 2.0 * outline, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(root);
        p.setPen(QPen(kRoot, 9.0, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(root);
        p.setPen(QPen(kRoot.lighter(140), 2.0, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(root.translated(-1.0, -2.5));
        // A rootlet to one side.
        outlinedLine(p, QPointF(o.x + o.width * 0.6, ground - h * 0.7),
                     QPointF(o.x + o.width * 0.85, ground - h * 1.05), 3.0, kRoot, outline);
        return;
    }
    case Course::Kind::Stone: {
        // A carved block, cooler in colour than the path it stands on.
        const QRectF block(o.x, ground - Course::kStoneHeight, o.width, Course::kStoneHeight);
        p.setPen(QPen(kOutline, outline * 1.6));
        QLinearGradient face(block.topLeft(), block.bottomLeft());
        face.setColorAt(0.0, kStone.lighter(118));
        face.setColorAt(1.0, kStone.darker(115));
        p.setBrush(face);
        p.drawRoundedRect(block, 4, 4);
        // Moss on top.
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x5c, 0xa0, 0x3a));
        p.drawRoundedRect(QRectF(block.left() + 2, block.top() - 2, block.width() - 4, 6), 3, 3);
        // The carving: one of four marks.
        const QPointF c = block.center() + QPointF(0, 3);
        p.setPen(QPen(kStone.darker(190), 2.6, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        switch (o.look) {
        case 0: {
            QPainterPath spiral;
            for (int i = 0; i <= 40; ++i) {
                const double a = i * 0.42;
                const double r = 1.0 + i * 0.27;
                const QPointF pt = c + QPointF(std::cos(a) * r, std::sin(a) * r);
                if (i == 0)
                    spiral.moveTo(pt);
                else
                    spiral.lineTo(pt);
            }
            p.drawPath(spiral);
            break;
        }
        case 1:
            p.drawEllipse(c, 10, 6);
            p.setBrush(kStone.darker(190));
            p.drawEllipse(c, 3, 3);
            break;
        case 2:
            p.drawEllipse(c, 5, 5);
            for (int i = 0; i < 8; ++i) {
                const double a = i * std::numbers::pi / 4.0;
                p.drawLine(c + QPointF(std::cos(a) * 7, std::sin(a) * 7),
                           c + QPointF(std::cos(a) * 11, std::sin(a) * 11));
            }
            break;
        default: {
            QPainterPath zig;
            zig.moveTo(c + QPointF(-12, 4));
            for (int i = 1; i <= 6; ++i)
                zig.lineTo(c + QPointF(-12 + i * 4, (i % 2) ? -4 : 4));
            p.drawPath(zig);
            break;
        }
        }
        return;
    }
    case Course::Kind::Arch: {
        // A wall of the ruin coming down from above, leaving room only to
        // duck under it. Its underside carries a bright band, since that
        // edge is the one that matters.
        const QRectF f = courseRect();
        const double top = -f.top() / scale() - 10.0;
        const double underside = ground - Course::kArchClearance;
        const QRectF wall(o.x, top, o.width, underside - top);
        QLinearGradient face(wall.topLeft(), wall.topRight());
        face.setColorAt(0.0, kWall.lighter(115));
        face.setColorAt(1.0, kWall.darker(118));
        p.setPen(QPen(kOutline, outline * 1.6));
        p.setBrush(face);
        p.drawRect(wall);
        // Courses of stone.
        p.setPen(QPen(kWall.darker(150), 1.4));
        const int courses = int((underside - top) / 22.0);
        for (int i = 1; i <= courses; ++i) {
            const double y = underside - 22.0 * i;
            p.drawLine(QPointF(o.x + 1, y), QPointF(o.right() - 1, y));
        }
        // The carved lintel at the bottom, with a warning band under it.
        const QRectF lintel(o.x - 4, underside - 20, o.width + 8, 20);
        p.setPen(QPen(kOutline, outline * 1.6));
        p.setBrush(kWall.lighter(130));
        p.drawRect(lintel);
        p.setPen(QPen(kWall.darker(170), 2.2));
        for (int i = 0; i < 5; ++i) {
            const double x = lintel.left() + 6 + i * (lintel.width() - 12) / 4.0;
            p.drawLine(QPointF(x - 4, lintel.center().y() + 4), QPointF(x, lintel.center().y() - 4));
            p.drawLine(QPointF(x, lintel.center().y() - 4), QPointF(x + 4, lintel.center().y() + 4));
        }
        p.setPen(QPen(kOutline, outline));
        p.setBrush(kGold);
        p.drawRect(QRectF(lintel.left(), underside - 5, lintel.width(), 5));
        return;
    }
    }
}

void RunnerView::paintCoins(QPainter& p) const
{
    const double outline = outlineWidth();
    for (const Course::Coin& c : m_course.coins()) {
        if (c.taken)
            continue;
        // A slow spin: the coin narrows and widens, never vanishing.
        const double turn = std::abs(std::cos(m_time * 3.0 + c.at.x() * 0.04));
        const double rx = Course::kCoinRadius * (0.35 + 0.65 * turn);
        const double ry = Course::kCoinRadius;
        p.setPen(QPen(kOutline, outline));
        QRadialGradient shine(c.at + QPointF(-rx * 0.3, -ry * 0.3), ry * 1.2);
        shine.setColorAt(0.0, kGold.lighter(140));
        shine.setColorAt(1.0, kGold.darker(120));
        p.setBrush(shine);
        p.drawEllipse(c.at, rx, ry);
        p.setPen(QPen(kGold.darker(150), 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(c.at, rx * 0.6, ry * 0.6);
    }
}

void RunnerView::paintRunner(QPainter& p) const
{
    // Blinks while it cannot be hurt, so the safe spell can be seen.
    if (m_course.hurtLeft() > 0.0 && std::fmod(m_course.hurtLeft(), 0.2) < 0.1)
        p.setOpacity(0.35);

    const double outline = outlineWidth();
    const double feet = m_course.feetY();
    const double x = Course::kRunnerX;
    const QColor skin(0xf2, 0xc4, 0x90);
    const QColor hair(0x3a, 0x24, 0x18);
    const QColor tunic(0xf2, 0x8a, 0x22);
    const QColor trousers(0x3c, 0x4c, 0x70);
    const QColor boot(0x5a, 0x38, 0x1c);
    const QColor scarf(0x1c, 0xc8, 0xb4);

    const bool sliding = m_course.ducking() || m_course.over();
    const bool flying = m_course.airborne() && !sliding;
    const bool running = m_course.phase() == Course::Phase::Running;
    const double phase = m_course.distance() / 16.0;

    QPointF hip;
    QPointF shoulder;
    QPointF head;
    // Back leg, front leg, back arm, front arm: each a joint and an end.
    QPointF knee[2];
    QPointF foot[2];
    QPointF elbow[2];
    QPointF hand[2];

    if (sliding) {
        hip = { x - 2, feet - 9 };
        shoulder = { x - 10, feet - 22 };
        head = { x - 12, feet - 28 };
        knee[0] = hip + QPointF(3, 7);
        foot[0] = knee[0] + QPointF(-8, 1);
        knee[1] = hip + QPointF(11, 4);
        foot[1] = knee[1] + QPointF(12, 3);
        elbow[0] = shoulder + QPointF(-4, 8);
        hand[0] = elbow[0] + QPointF(-2, 7);
        elbow[1] = shoulder + QPointF(8, -1);
        hand[1] = elbow[1] + QPointF(8, -3);
    } else {
        // Measured down from the top of the standing box: the head fills
        // its top, the shoulders sit under the chin.
        const double top = feet - Course::kStandHeight;
        hip = { x, feet - 27 };
        shoulder = { x + 3, top + 12 };
        head = { x + 5, top + 5 };
        for (int i = 0; i < 2; ++i) {
            const double side = i == 0 ? -1.0 : 1.0;
            double thigh;
            double shin;
            double upper;
            double fore;
            if (flying) {
                thigh = i == 0 ? -0.4 : 1.2;
                shin = i == 0 ? -1.4 : 0.1;
                upper = i == 0 ? -2.2 : 2.5;
                fore = upper + 0.5;
            } else if (running) {
                const double swing = std::sin(phase) * side;
                thigh = 0.75 * swing;
                // The knee folds most as the leg comes through behind.
                shin = thigh - 0.3 - 0.9 * std::max(0.0, -std::cos(phase) * side);
                upper = -0.8 * swing;
                fore = upper + 1.3;
            } else {
                // Standing ready, breathing.
                thigh = 0.12 * side;
                shin = thigh;
                upper = 0.15 * side + 0.04 * std::sin(m_time * 2.0);
                fore = upper + 0.4;
            }
            knee[i] = limbEnd(hip, thigh, 14.0);
            foot[i] = limbEnd(knee[i], shin, 13.0);
            elbow[i] = limbEnd(shoulder, upper, 11.0);
            hand[i] = limbEnd(elbow[i], fore, 10.0);
        }
    }

    const auto leg = [&](int i) {
        outlinedLine(p, hip, knee[i], 6.5, trousers, outline);
        outlinedLine(p, knee[i], foot[i], 5.5, trousers, outline);
        outlinedLine(p, foot[i], foot[i] + QPointF(4.5, 0.5), 5.5, boot, outline);
    };
    const auto arm = [&](int i) {
        outlinedLine(p, shoulder, elbow[i], 4.5, i == 0 ? tunic.darker(120) : tunic, outline);
        outlinedLine(p, elbow[i], hand[i], 4.0, skin, outline);
    };

    leg(0);
    arm(0);

    // The scarf streams out behind and ripples.
    {
        QPainterPath tail;
        const QPointF neck = shoulder + QPointF(-1, -2);
        tail.moveTo(neck);
        const double wind = running ? 1.0 : 0.35;
        for (int i = 1; i <= 4; ++i)
            tail.lineTo(neck + QPointF(-6.0 * i * wind - (running ? 0 : 1.5 * i),
                                       std::sin(m_time * 14.0 + i * 1.3) * 2.2 * i * wind + i * (running ? 0.6 : 3.0)));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(kOutline, 5.0 + 2.0 * outline, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(tail);
        p.setPen(QPen(scarf, 5.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(tail);
    }

    outlinedLine(p, hip, shoulder, 13.0, tunic, outline);
    // The belt.
    const QPointF waist = hip + (shoulder - hip) * 0.15;
    outlinedLine(p, waist + QPointF(-5, 0), waist + QPointF(5, 0), 2.5, boot, 0.0);
    outlinedLine(p, shoulder + QPointF(-4, 0), shoulder + QPointF(4, 0), 5.0, scarf, 0.0);

    p.setPen(QPen(kOutline, outline));
    p.setBrush(skin);
    p.drawEllipse(head, 7.5, 7.5);
    // Hair over the back and top of the head.
    QPainterPath cap;
    cap.moveTo(head + QPointF(4, -5));
    cap.arcTo(QRectF(head.x() - 8, head.y() - 8, 16, 16), 50, 170);
    cap.closeSubpath();
    p.setBrush(hair);
    p.drawPath(cap);
    p.setPen(Qt::NoPen);
    p.setBrush(kOutline);
    p.drawEllipse(head + QPointF(3.5, -0.5), 1.4, 1.6);

    leg(1);
    arm(1);
    p.setOpacity(1.0);
}

void RunnerView::paintWarnings(QPainter& p) const
{
    // Before an obstacle comes into view, a badge at the right edge says
    // what it will need: an arrow up to jump, down to duck.
    const double outline = outlineWidth();
    const double lead = m_course.speed() * 1.1;
    for (const Course::Obstacle& o : m_course.obstacles()) {
        const double before = o.x - Course::kWidth;
        if (before < 0.0 || before > lead)
            continue;
        const double near = 1.0 - before / lead;
        const bool duck = o.kind == Course::Kind::Arch;
        const QPointF c(Course::kWidth - 24.0, Course::kGroundY - 92.0);
        const double r = 15.0 + 3.0 * near;
        p.setOpacity(0.35 + 0.65 * near);
        p.setPen(QPen(kOutline, outline * 1.4));
        p.setBrush(kGold);
        p.drawEllipse(c, r, r);
        QPainterPath arrow;
        const double d = duck ? 1.0 : -1.0;
        arrow.moveTo(c + QPointF(0, d * r * 0.62));
        arrow.lineTo(c + QPointF(-r * 0.5, d * r * 0.02));
        arrow.lineTo(c + QPointF(-r * 0.2, d * r * 0.02));
        arrow.lineTo(c + QPointF(-r * 0.2, -d * r * 0.58));
        arrow.lineTo(c + QPointF(r * 0.2, -d * r * 0.58));
        arrow.lineTo(c + QPointF(r * 0.2, d * r * 0.02));
        arrow.lineTo(c + QPointF(r * 0.5, d * r * 0.02));
        arrow.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(kOutline);
        p.drawPath(arrow);
        p.setOpacity(1.0);
        break;   // only the nearest
    }
}

void RunnerView::paintEffects(QPainter& p) const
{
    p.setPen(Qt::NoPen);
    for (const Particle& q : m_particles) {
        QColor c = q.colour;
        const double share = q.life / q.maxLife;
        c.setAlphaF(float(std::clamp(share, 0.0, 1.0)));
        p.setBrush(c);
        p.drawEllipse(q.at, q.size * (0.5 + 0.5 * share), q.size * (0.5 + 0.5 * share));
    }
    QFont f = font();
    f.setBold(true);
    f.setPixelSize(Legibility::instance().enabled() ? 18 : 14);
    p.setFont(f);
    for (const Popup& u : m_popups) {
        QColor c = u.colour.lighter(120);
        c.setAlphaF(float(std::clamp(1.1 - u.age, 0.0, 1.0)));
        const QRectF box(u.at.x() - 30, u.at.y() - 22, 60, 20);
        p.setPen(QColor(0, 0, 0, int(c.alpha() * 0.8)));
        p.drawText(box.translated(1, 1), Qt::AlignCenter, u.text);
        p.setPen(c);
        p.drawText(box, Qt::AlignCenter, u.text);
    }
}

void RunnerView::paintHud(QPainter& p) const
{
    const QRectF f = courseRect();
    const double s = scale();
    const QRectF strip(f.left() + 10 * s, f.top() + 6 * s, f.width() - 20 * s, 26 * s);
    const int best = std::max(m_table.isEmpty() ? 0 : m_table.first(), m_course.score());

    const auto shadowed = [&](const QRectF& r, int align, const QString& text, const QColor& c) {
        p.setPen(QColor(0, 0, 0, 200));
        p.drawText(r.translated(1.5, 1.5), align, text);
        p.setPen(c);
        p.drawText(r, align, text);
    };

    // Hearts: full for each left, an empty outline for each lost.
    const double size = 20 * s * (Legibility::instance().enabled() ? 1.2 : 1.0);
    for (int i = 0; i < Course::kHearts; ++i) {
        const QPointF c(strip.left() + size * 0.6 + i * size * 1.3, strip.center().y());
        const QPainterPath h = heartPath(c, size);
        p.setPen(QPen(kOutline, std::max(1.5, 2.0 * s)));
        p.setBrush(i < m_course.hearts() ? QColor(0xf0, 0x3a, 0x40) : QColor(0, 0, 0, 90));
        p.drawPath(h);
    }

    p.setFont(courseFont(17));
    shadowed(strip, Qt::AlignHCenter | Qt::AlignVCenter, tr("Score %1").arg(m_course.score()), kInk);
    shadowed(strip, Qt::AlignRight | Qt::AlignVCenter, tr("Best %1").arg(best), kGold);
    p.setFont(courseFont(13, false));
    const QRectF under = strip.translated(0, strip.height());
    shadowed(under, Qt::AlignHCenter | Qt::AlignVCenter,
             tr("%1 m   %2 coins").arg(m_course.metres()).arg(m_course.coinsTaken()), kInk);
}

void RunnerView::paintPanel(QPainter& p, const QRectF& r) const
{
    QLinearGradient fill(r.topLeft(), r.bottomLeft());
    fill.setColorAt(0.0, QColor(0x2a, 0x1e, 0x12, 238));
    fill.setColorAt(1.0, QColor(0x14, 0x0e, 0x08, 238));
    p.setPen(QPen(kPanelEdge, 2.5));
    p.setBrush(fill);
    p.drawRoundedRect(r, 14, 14);
}

void RunnerView::paintOverlay(QPainter& p) const
{
    const QRectF f = courseRect();
    const double s = scale();
    const bool legible = Legibility::instance().enabled();

    const auto line = [&](const QRectF& r, const QString& text, double size, const QColor& c,
                          bool bold = true) {
        p.setFont(courseFont(size, bold));
        p.setPen(QColor(0, 0, 0, 220));
        p.drawText(r.translated(1.5, 1.5), Qt::AlignCenter | Qt::TextWordWrap, text);
        p.setPen(c);
        p.drawText(r, Qt::AlignCenter | Qt::TextWordWrap, text);
    };

    if (m_bannerAge < 1.8 && !m_banner.isEmpty() && !m_course.over() && !m_pause->isChecked()) {
        const double fade = std::clamp(std::min(m_bannerAge * 5.0, (1.8 - m_bannerAge) * 2.5), 0.0, 1.0);
        p.save();
        p.setOpacity(fade);
        line(QRectF(f.left(), f.top() + 70 * s, f.width(), 40 * s), m_banner, 26, kGold);
        p.restore();
    }

    const double panelW = std::min(f.width() - 30 * s, 380 * s);
    const auto panelAt = [&](double heightUnits) {
        const double h = std::min(f.height() - 20 * s, heightUnits * s * (legible ? 1.2 : 1.0));
        // Right of the runner, so the panel never hides it.
        const double centre = std::max(f.center().x(), f.left() + Course::kRunnerX * s + 40 * s + panelW / 2.0);
        return QRectF(centre - panelW / 2.0, f.top() + 160 * s - h / 2.0, panelW, h);
    };

    if (m_course.over()) {
        const QRectF panel = panelAt(110 + 22 * double(std::max<qsizetype>(1, m_table.size())));
        paintPanel(p, panel);
        double y = panel.top() + 10 * s;
        const double rowH = (legible ? 26 : 22) * s;
        line(QRectF(panel.left(), y, panel.width(), rowH * 1.3), tr("Out of hearts. The run is over."), 18,
             QColor(0xff, 0x9a, 0x70));
        y += rowH * 1.4;
        line(QRectF(panel.left(), y, panel.width(), rowH),
             m_rank == 0 ? tr("Score %1 — a new best!").arg(m_course.score())
                         : tr("Score %1").arg(m_course.score()),
             17, kInk);
        y += rowH * 1.1;
        for (int i = 0; i < m_table.size(); ++i) {
            const bool mine = i == m_rank;
            const QRectF row(panel.left() + 40 * s, y, panel.width() - 80 * s, rowH);
            if (mine) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(kGold.red(), kGold.green(), kGold.blue(), 60));
                p.drawRoundedRect(row.adjusted(-8 * s, 1, 8 * s, -1), 6, 6);
            }
            p.setFont(courseFont(16, mine));
            p.setPen(mine ? kGold : kInk);
            p.drawText(row, Qt::AlignLeft | Qt::AlignVCenter, tr("%1.").arg(i + 1));
            p.drawText(row, Qt::AlignRight | Qt::AlignVCenter, QString::number(m_table[i]));
            y += rowH;
        }
        y += rowH * 0.2;
        line(QRectF(panel.left(), y, panel.width(), rowH), tr("Space or a click runs again"), 14, kGold,
             false);
        return;
    }

    if (m_pause->isChecked()) {
        const QRectF panel = panelAt(80);
        paintPanel(p, panel);
        line(panel.adjusted(0, 8 * s, 0, -panel.height() / 2), tr("Paused"), 24, kGold);
        line(panel.adjusted(0, panel.height() / 2, 0, -8 * s), tr("Press P to carry on"), 15, kInk, false);
        return;
    }

    if (m_course.phase() == Course::Phase::Ready) {
        const QRectF panel = panelAt(150);
        paintPanel(p, panel);
        const double rowH = panel.height() / 6.0;
        double y = panel.top() + rowH * 0.3;
        line(QRectF(panel.left(), y, panel.width(), rowH * 1.4), tr("Ruin Runner"), 28, kGold);
        y += rowH * 1.5;
        line(QRectF(panel.left(), y, panel.width(), rowH), tr("Space or Up jumps.  Down ducks."), 15, kInk,
             false);
        y += rowH;
        line(QRectF(panel.left(), y, panel.width(), rowH), tr("Three hearts.  P pauses."), 15, kInk, false);
        y += rowH;
        line(QRectF(panel.left(), y, panel.width(), rowH), tr("Press Space to start"), 17, kGold);
    }
}

// ---- input ----

void RunnerView::keyPressEvent(QKeyEvent* event)
{
    if (m_pause->isChecked()) {
        GameView::keyPressEvent(event);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Space:
    case Qt::Key_Up:
    case Qt::Key_W:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (event->isAutoRepeat())
            break;
        if (m_course.over()) {
            newGame();
            break;
        }
        m_course.jump();
        break;
    case Qt::Key_Down:
    case Qt::Key_S:
        m_duckKey = true;
        break;
    default:
        GameView::keyPressEvent(event);
        return;
    }
    m_course.setDuck(m_duckKey || m_duckButton);
    readEvents();
    update();
}

void RunnerView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) {
        GameView::keyReleaseEvent(event);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Down:
    case Qt::Key_S:
        m_duckKey = false;
        break;
    default:
        GameView::keyReleaseEvent(event);
        return;
    }
    m_course.setDuck(m_duckKey || m_duckButton);
}

void RunnerView::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    if (m_pause->isChecked())
        return;
    if (event->button() == Qt::LeftButton) {
        if (m_course.over()) {
            newGame();
            return;
        }
        m_course.jump();
    } else if (event->button() == Qt::RightButton) {
        m_duckButton = true;
    }
    m_course.setDuck(m_duckKey || m_duckButton);
    readEvents();
    update();
}

void RunnerView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        m_duckButton = false;
        m_course.setDuck(m_duckKey || m_duckButton);
    }
    GameView::mouseReleaseEvent(event);
}

void RunnerView::focusOutEvent(QFocusEvent* event)
{
    // A key let go of elsewhere never comes back here, so it would stick.
    releaseKeys();
    GameView::focusOutEvent(event);
}

// How to play (GHUB-0016). Written for this collection from what this file
// and runnercourse.cpp do: the points are kCoinPoints and kUnitsPerMetre, the
// hearts kHearts, the keys keyPressEvent() and buildActions().
QString RunnerView::rulesText() const
{
    return tr(
        "<h3>The aim</h3>"
        "<p>Run through the jungle ruins for as long as you can. Jump and duck "
        "past what is in your way, pick up coins, and beat your best score.</p>"

        "<h3>How it plays</h3>"
        "<ul>"
        "<li>The run waits until you press Space.</li>"
        "<li>The runner keeps running on its own. The pace starts gently and rises "
        "the longer the run lasts, up to a top speed.</li>"
        "<li>Jump over roots, carved stones and pits.</li>"
        "<li>Duck under the walls that come down from above. "
        "Each has a gold band along its lower edge.</li>"
        "<li>Before something comes into view, a gold badge at the right edge shows "
        "an arrow: up means jump, down means duck.</li>"
        "<li>Every jump is the same height. A jump pressed just before landing "
        "happens as soon as you land.</li>"
        "<li>Some coins follow the curve of a good jump, as a hint of when to go.</li>"
        "<li>You have three hearts. Each hit or fall costs one, and the runner "
        "flickers for a moment, when nothing can hurt it. The third hit ends the run.</li>"
        "</ul>"

        "<h3>Scoring</h3>"
        "<ul>"
        "<li>One point for every metre run.</li>"
        "<li>Five points for every coin.</li>"
        "<li>Your five best scores are kept, and shown when a run ends.</li>"
        "</ul>"

        "<h3>Controls</h3>"
        "<ul>"
        "<li>Space, Up, W or Enter: jump.</li>"
        "<li>Down or S: duck, for as long as you hold it. In the air it drops you faster.</li>"
        "<li>A left click jumps. Holding the right mouse button ducks.</li>"
        "<li>P, or the Pause button: pause, and press again to carry on.</li>"
        "<li>When a run ends, Space or a click starts the next.</li>"
        "<li>New Game button, or Ctrl+N: start again.</li>"
        "<li>If you leave for another game, this one pauses until you come back.</li>"
        "</ul>");
}
