#include "laneview.h"

#include "legibility.h"
#include "runner/bands.h"
#include "runner/biomes.h"
#include "runner/runnerart.h"
#include "runner/framepacer.h"
#include "runner/laneart.h"
#include "runner/runnerview.h"
#include "sound.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace {
// The sharpest the scene is painted, in pixels to a canvas unit: 1920 by
// 1080. A frame costs by the pixel, and painted at 4K one took about 50 ms.
constexpr double kSceneScaleCap = 3.0;
using Course = LaneCourse;
using LaneArt::kHaze;

// The canvas everything is drawn on, scaled to fit the widget.
constexpr double kCanvasW = 640.0;
constexpr double kCanvasH = 360.0;
// The camera: this high above the path and this far behind the runner, in
// metres, and this far from its picture plane, in canvas units. Together
// they put the runner's feet a little above the bottom of the canvas, room
// for the lane signs under them, and the far end of the course just under
// the horizon.
constexpr double kHorizon = 106.0;
constexpr double kCameraHeight = 3.4;
constexpr double kCameraBack = 7.0;
constexpr double kFocal = 432.0;
// Nothing nearer the camera than this is drawn.
constexpr double kNearest = 1.2 - kCameraBack;
// Metres between lane centres, and from the middle to a kerb's outer edge.
constexpr double kLaneWidth = 1.6;
constexpr double kPathHalf = 1.5 * kLaneWidth;
constexpr double kKerbWidth = 0.38;
constexpr double kKerbHeight = 0.2;
// Flagstones are this long along the path.
constexpr double kFlagstone = 2.2;
// The camera follows the runner across this share of the way, so a lane
// switch is felt without the path swinging about.
constexpr double kCameraFollow = 0.35;
// A row is signed this many seconds before it reaches the runner.
constexpr double kSignSeconds = 2.6;
// A coin missed is no longer drawn this far behind the runner, in metres.
constexpr double kCoinGone = -1.5;

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

// A fixed pseudo-random number in 0..1 for a piece of the course's look, so
// the same stretch looks the same every time it comes past.
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

// `c` seen through `amount` of the haze, 0 to 1.
QColor hazed(const QColor& c, double amount)
{
    const double a = std::clamp(amount, 0.0, 1.0);
    return QColor::fromRgbF(float(c.redF() * (1.0 - a) + kHaze.redF() * a),
                            float(c.greenF() * (1.0 - a) + kHaze.greenF() * a),
                            float(c.blueF() * (1.0 - a) + kHaze.blueF() * a), c.alphaF());
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

QPolygonF quad(QPointF a, QPointF b, QPointF c, QPointF d)
{
    return QPolygonF({ a, b, c, d });
}

// Scenery away from the path is a little hazed even near by, more with
// distance, and lost in the haze at the far end.
double airAt(double ahead, double clear)
{
    return std::clamp(ahead / 140.0, 0.0, 0.45) + (1.0 - clear) * 0.55;
}
}

LaneView::LaneView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(LaneView::minimumSizeHint());
    setFocusPolicy(Qt::StrongFocus);

    m_pacer = new FramePacer(this, [this](double dt) { tick(dt); });
    connect(&m_dressing, &QFutureWatcherBase::finished, this, qOverload<>(&QWidget::update));

    m_table = RunnerView::highScores();
    scatterLeaves();
    buildActions();
    newGame();
}

LaneView::~LaneView()
{
    m_warm.waitForFinished();
    m_dressing.waitForFinished();
}

void LaneView::buildActions()
{
    auto* newAction = new QAction(tr("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &LaneView::newGame);
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

void LaneView::newGame()
{
    m_course.newGame();
    m_course.takeEvents();
    m_recorded = false;
    m_rank = -1;
    m_particles.clear();
    m_popups.clear();
    m_shake = m_flash = 0.0;
    m_cameraX = laneX(m_course.lane()) * kCameraFollow;
    m_stride = 0.0;
    m_bannerAge = 10.0;
    m_landAge = 10.0;
    m_lookShown = Biomes::at(m_course.distance()).main();
    m_banner.clear();
    releaseKeys();
    m_pause->setChecked(false);
    runClock();
    setFocus();
    refresh();
    update();
}

void LaneView::scatterLeaves()
{
    m_leaves.clear();
    for (int i = 0; i < 9; ++i) {
        Leaf l;
        l.at = QPointF(unit(m_fx) * kCanvasW, unit(m_fx) * kCanvasH);
        l.angle = unit(m_fx) * 360.0;
        l.spin = (unit(m_fx) - 0.5) * 160.0;
        l.fall = 14.0 + 16.0 * unit(m_fx);
        l.sway = 0.8 + 1.4 * unit(m_fx);
        m_leaves.push_back(l);
    }
}

void LaneView::releaseKeys()
{
    m_slideKey = m_slideButton = false;
    m_course.setSlide(false);
}

void LaneView::activate()
{
    if (!m_warm.isValid())
        m_warm = QtConcurrent::run([] {
            for (int i = 1; i < Biomes::kLooks; ++i)
                LaneArt::art(Biomes::Look(i));
        });
    m_active = true;
    setFocus();
    runClock();
    refresh();
}

void LaneView::deactivate()
{
    m_active = false;
    releaseKeys();
    runClock();
}

void LaneView::runClock()
{
    if (m_active && !m_pause->isChecked() && !m_heldForShot) {
        m_pacer->start();
    } else
        m_pacer->stop();
}

void LaneView::gameOver()
{
    if (m_recorded)
        return;
    m_recorded = true;
    m_rank = RunnerView::recordScore(m_course.score());
    m_table = RunnerView::highScores();
}

bool LaneView::advanceForShot(int turns)
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
        if (in.steer != 0)
            m_course.steer(in.steer);
        if (in.jump)
            m_course.jump();
        m_course.setSlide(in.slide);
        m_course.advance(1.0 / 60.0);
    }
    m_course.takeEvents();
    m_particles.clear();
    m_popups.clear();
    m_shake = m_flash = 0.0;
    m_bannerAge = 10.0;
    m_landAge = 10.0;
    m_lookShown = Biomes::at(m_course.distance()).main();
    m_cameraX = laneX(m_course.lane()) * kCameraFollow;
    m_stride = m_course.distance() / 0.9;
    if (m_course.over())
        gameOver();
    refresh();
    update();
    return true;
}

void LaneView::runUnattendedForTest(double seconds)
{
    m_course.start();
    m_course.setSlide(false);
    const int frames = int(std::ceil(seconds * 60.0));
    for (int f = 0; f < frames && !m_course.over(); ++f) {
        m_course.advance(1.0 / 60.0);
        readEvents();
    }
    refresh();
}

void LaneView::tick(double dt)
{
    m_time += dt;

    m_course.setSlide(m_slideKey || m_slideButton);
    m_course.advance(dt);
    readEvents();
    m_landAge += dt;

    // A banner names each new season as the run enters it (GHUB-0260).
    const Biomes::Look look = Biomes::at(m_course.distance()).main();
    if (look != m_lookShown) {
        m_lookShown = look;
        m_banner = Biomes::name(look);
        m_bannerAge = 0.0;
    }

    m_cameraX += (laneX(m_course.lane()) * kCameraFollow - m_cameraX) * std::min(1.0, dt * 8.0);

    // Dust where a foot comes down, and a trail of it under a slide.
    const double stride = m_course.distance() / 0.9;
    const bool onGround = m_course.phase() == Course::Phase::Running && !m_course.airborne();
    if (onGround && !m_course.sliding()) {
        for (int i = 0; i < 2; ++i) {
            const double phase = i * std::numbers::pi;
            if (std::sin(m_stride + phase) > 0.0 && std::sin(stride + phase) <= 0.0)
                burst(project(laneX(m_course.lane()) + (i == 0 ? -0.13 : 0.13), 0, 0), kPath.lighter(115), 3,
                      35.0, 0.0);
        }
    } else if (onGround && unit(m_fx) < 0.6) {
        burst(project(laneX(m_course.lane()), 0, 0.1), kPath.lighter(110), 1, 50.0, 10.0);
    }
    m_stride = stride;

    for (Particle& q : m_particles) {
        q.at += q.velocity * dt;
        q.velocity *= 0.97;
        q.velocity.ry() += 260.0 * dt;
        q.life -= dt;
    }
    std::erase_if(m_particles, [](const Particle& q) { return q.life <= 0.0; });
    for (Popup& u : m_popups) {
        u.age += dt;
        u.at.ry() -= 30.0 * dt;
    }
    std::erase_if(m_popups, [](const Popup& u) { return u.age >= 0.9; });

    // Leaves drift down and sway, faster and outwards as the pace rises; one
    // that leaves the picture starts again at the top.
    const double rush = m_course.phase() == Course::Phase::Running ? m_course.speed() * 2.0 : 0.0;
    for (Leaf& l : m_leaves) {
        l.at.ry() += (l.fall + rush) * dt;
        l.at.rx() += std::sin(m_time * l.sway + l.spin) * 16.0 * dt + (l.at.x() - kCanvasW / 2.0) * rush * dt / 300.0;
        l.angle += l.spin * dt;
        if (l.at.y() > kCanvasH + 10.0 || l.at.x() < -20.0 || l.at.x() > kCanvasW + 20.0) {
            l.at = QPointF(unit(m_fx) * kCanvasW, -10.0);
            l.fall = 14.0 + 16.0 * unit(m_fx);
        }
    }

    m_shake = std::max(0.0, m_shake - dt * 2.5);
    m_flash = std::max(0.0, m_flash - dt * 3.0);
    m_bannerAge += dt;

    refresh();
    update();
}

void LaneView::readEvents()
{
    for (const Course::Event& e : m_course.takeEvents()) {
        const QPointF at = project(laneX(e.lane), e.height, 0.0);
        switch (e.kind) {
        case Course::EventKind::Started:
        case Course::EventKind::Switched:
            break;
        case Course::EventKind::Jumped:
            Sound::instance().play(Sound::kJump);
            burst(at, kPath, 8, 60.0, 0.0);
            break;
        case Course::EventKind::Landed:
            m_landAge = 0.0;
            burst(at, kPath.lighter(110), 10, 70.0, 0.0);
            break;
        case Course::EventKind::Coin:
            Sound::instance().play(Sound::kCoin);
            burst(at, kGold, 12, 90.0, 20.0);
            burst(at, QColor(255, 255, 230), 5, 60.0, 30.0);
            m_popups.push_back({ at, QStringLiteral("+%1").arg(e.value), 0.0, kGold });
            break;
        case Course::EventKind::Hit:
        case Course::EventKind::Fell:
            Sound::instance().play(Sound::kThud);
            burst(at, QColor(0xff, 0xb0, 0x60), 22, 160.0, 40.0);
            burst(at, QColor(255, 255, 255), 8, 90.0, 40.0);
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

void LaneView::burst(QPointF at, const QColor& colour, int count, double speed, double lift)
{
    for (int i = 0; i < count; ++i) {
        const double angle = std::numbers::pi * (1.0 + unit(m_fx));
        const double v = speed * (0.3 + 0.7 * unit(m_fx));
        const double life = 0.3 + 0.4 * unit(m_fx);
        m_particles.push_back({ at, QPointF(std::cos(angle) * v, std::sin(angle) * v - lift), life, life,
                                2.0 + unit(m_fx) * 2.5, colour.lighter(90 + int(unit(m_fx) * 40)) });
    }
}

void LaneView::refresh()
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

std::array<LaneView::Sign, LaneCourse::kLanes> LaneView::signs() const
{
    std::array<Sign, Course::kLanes> out {};
    out.fill(Sign::None);
    if (m_course.phase() != Course::Phase::Running)
        return out;
    // The next row, found the way the pilot finds it.
    const auto& obstacles = m_course.obstacles();
    const auto next = std::find_if(obstacles.begin(), obstacles.end(), [](const Course::Obstacle& o) {
        return o.far() > -Course::kRunnerDepth / 2.0;
    });
    if (next == obstacles.end() || next->z < Course::kRunnerDepth || next->z > m_course.speed() * kSignSeconds)
        return out;
    for (const Course::Obstacle& o : obstacles) {
        if (o.row != next->row)
            continue;
        switch (o.kind) {
        case Course::Kind::Root:
        case Course::Kind::Pit: out[size_t(o.lane)] = Sign::Jump; break;
        case Course::Kind::Lintel: out[size_t(o.lane)] = Sign::Slide; break;
        case Course::Kind::Block: out[size_t(o.lane)] = Sign::GoRound; break;
        }
    }
    return out;
}

// ---- geometry ----

double LaneView::scale() const
{
    const double band = captionBand(QRectF(rect()));
    return std::max(0.2, std::min((width() - 16.0) / kCanvasW, (height() - 16.0 - band) / kCanvasH));
}

QRectF LaneView::canvasRect() const
{
    const double s = scale();
    const double w = kCanvasW * s;
    const double h = kCanvasH * s;
    const double band = captionBand(QRectF(rect()));
    return { (width() - w) / 2.0, (height() - band - h) / 2.0, w, h };
}

QPointF LaneView::project(double across, double up, double ahead) const
{
    const double d = std::max(ahead, kNearest) + kCameraBack;
    return { kCanvasW / 2.0 + kFocal * (across - m_cameraX) / d, kHorizon + kFocal * (kCameraHeight - up) / d };
}

double LaneView::sizeAt(double ahead)
{
    return kFocal / (std::max(ahead, kNearest) + kCameraBack);
}

double LaneView::laneX(double lane)
{
    return (lane - 1.0) * kLaneWidth;
}

double LaneView::clearness(double ahead)
{
    // Out of the haze over the far quarter of the course, so a row is never
    // seen to appear from nothing.
    return std::clamp((Course::kSightDepth - 2.0 - ahead) / 22.0, 0.0, 1.0);
}

double LaneView::outlineWidth() const
{
    return Legibility::instance().enabled() ? 2.4 : 1.6;
}

QFont LaneView::canvasFont(double units, bool bold) const
{
    QFont f = font();
    f.setBold(bold);
    const double grow = Legibility::instance().enabled() ? 1.25 : 1.0;
    f.setPixelSize(std::max(10, int(std::lround(units * grow * scale()))));
    return f;
}

void LaneView::resizeEvent(QResizeEvent* event)
{
    GameView::resizeEvent(event);
    m_backdrop = {};
}

// ---- painted once per size ----

void LaneView::buildLayers()
{
    const qreal dpr = devicePixelRatioF();
    const QSize size(std::max(1, int(width() * dpr)), std::max(1, int(height() * dpr)));
    {
        const QRectF canvas = canvasRect();
        const double s = scale();
        m_backdrop[0] = QImage(size, QImage::Format_RGB32);
        m_backdrop[0].setDevicePixelRatio(dpr);
        QPainter p(&m_backdrop[0]);
        LaneArt::paintSky(p, QRectF(rect()), canvas, canvas.top() + kHorizon * s, s);
    }

    // The scene's layers are drawn at the scene's own size, where the canvas
    // is the whole picture.
    const double s = m_scenePixels;
    const QRectF f(QPointF(0, 0), QSizeF(m_scene.size()));
    m_sceneSky[0] = QImage(m_scene.size(), QImage::Format_RGB32);
    {
        QPainter p(&m_sceneSky[0]);
        LaneArt::paintSky(p, f, f, kHorizon * s, s);
    }

    // Over the scene: shafts of low sun, the leaves framing the top, and a
    // soft darkening towards the corners that draws the eye down the path.
    m_sceneOverlay[0] = QImage(m_scene.size(), QImage::Format_ARGB32_Premultiplied);
    m_sceneOverlay[0].fill(Qt::transparent);
    QPainter p(&m_sceneOverlay[0]);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    const QPointF sun(f.center().x() + 50 * s, f.top() + (kHorizon - 42) * s);
    for (int i = 0; i < 6; ++i) {
        const double a = std::numbers::pi * (0.18 + 0.13 * i);
        const double spread = 0.035 + 0.02 * (i % 2);
        const double len = f.height() * 1.4;
        const QPointF a1 = sun + QPointF(std::cos(a - spread), std::sin(a - spread)) * len;
        const QPointF a2 = sun + QPointF(std::cos(a + spread), std::sin(a + spread)) * len;
        QLinearGradient ray(sun, (a1 + a2) / 2.0);
        ray.setColorAt(0.0, QColor(255, 226, 170, 54));
        ray.setColorAt(1.0, QColor(255, 226, 170, 0));
        p.setBrush(ray);
        p.drawPolygon(QPolygonF({ sun, a1, a2 }));
    }
    p.save();
    p.setClipRect(f);
    LaneArt::paintCanopy(p, f, s);
    p.restore();
    QRadialGradient vignette(f.center() + QPointF(0, f.height() * 0.1), std::hypot(f.width(), f.height()) * 0.62);
    vignette.setColorAt(0.55, QColor(10, 5, 0, 0));
    vignette.setColorAt(1.0, QColor(10, 5, 0, 150));
    p.setBrush(vignette);
    p.drawRect(f);
    p.end();

    // The same in each season's colours (GHUB-0260), in the background.
    m_seasonsReady = false;
    m_dressing.setFuture(QtConcurrent::run([backdrop = m_backdrop[0], sky = m_sceneSky[0], overlay = m_sceneOverlay[0], s] {
        Seasons out;
        for (int i = 1; i < Biomes::kLooks; ++i) {
            const auto look = Biomes::Look(i);
            const auto at = size_t(i);
            out.backdrop[at] = backdrop;
            Biomes::colour(out.backdrop[at], look, Biomes::Part::Sky, 0.0);
            out.sky[at] = sky;
            Biomes::colour(out.sky[at], look, Biomes::Part::Sky, 0.0);
            out.overlay[at] = overlay;
            Biomes::colour(out.overlay[at], look, Biomes::Part::Scenery, 3.0 * s);
        }
        return out;
    }));
    m_mote = RunnerArt::softDot(std::max(4, int(std::lround(8.0 * s))), QColor(255, 255, 255, 230));
}

// ---- painting ----

void LaneView::paintEvent(QPaintEvent*)
{
    const QRectF f = canvasRect();
    const double s = scale();
    const qreal dpr = devicePixelRatioF();

    // The scene: everything on the canvas, painted at no more than
    // kSceneScaleCap pixels to a canvas unit.
    m_scenePixels = std::min(s * dpr, kSceneScaleCap);
    const QSize scenePx(int(std::ceil(kCanvasW * m_scenePixels)), int(std::ceil(kCanvasH * m_scenePixels)));
    const QSize want(int(width() * dpr), int(height() * dpr));
    if (m_backdrop[0].isNull() || m_backdrop[0].size() != want || m_scene.size() != scenePx) {
        m_scene = QImage(scenePx, QImage::Format_RGB32);
        buildLayers();
    }
    // A held shot waits for the seasons, so it is the same every time.
    if (!m_seasonsReady && m_heldForShot)
        m_dressing.waitForFinished();
    if (!m_seasonsReady && m_dressing.isFinished()) {
        const Seasons seasons = m_dressing.result();
        if (seasons.sky[1].size() == m_sceneSky[0].size() && seasons.backdrop[1].size() == m_backdrop[0].size()) {
            for (int i = 1; i < Biomes::kLooks; ++i) {
                m_backdrop[size_t(i)] = seasons.backdrop[size_t(i)];
                m_sceneSky[size_t(i)] = seasons.sky[size_t(i)];
                m_sceneOverlay[size_t(i)] = seasons.overlay[size_t(i)];
            }
            m_seasonsReady = true;
        }
    }
    const auto season = [&](Biomes::Look look) { return m_seasonsReady ? size_t(look) : size_t(0); };
    QPointF shake;
    if (m_shake > 0.0) {
        const double amount = 5.0 * m_shake * m_shake * m_scenePixels / s;
        shake = QPointF(std::sin(m_time * 71.0) * amount, std::cos(m_time * 53.0) * amount);
    }

    // The sky, the ground and everything standing on it are most of a
    // frame's cost, so they are painted in bands of rows, one to a core, at
    // once (GHUB-0261). Each band paints onto its own rows of m_scene and
    // reads nothing another band writes; the hazed art made on first use is
    // made under a lock. The picture matches one painter's but for a few
    // shades of rounding in a gradient. Nothing here draws text.
    // The season here, and while it changes, the next laid over it
    // (GHUB-0260). The art of every season in sight is made before any band
    // reads it.
    const Biomes::Place place = Biomes::at(m_course.distance());
    for (const Biomes::Place& seen : { place, Biomes::at(m_course.distance() + Course::kSightDepth) }) {
        LaneArt::art(seen.from);
        LaneArt::art(seen.to);
    }
    const auto seasons = [&](QPainter& q, const std::array<QImage, Biomes::kLooks>& images, QPointF at) {
        q.drawImage(at, images[season(place.from)]);
        if (place.blend > 0.0) {
            q.setOpacity(place.blend);
            q.drawImage(at, images[season(place.to)]);
            q.setOpacity(1.0);
        }
    };
    Bands::paint(m_scene, [&](QPainter& q) {
        q.setRenderHint(QPainter::Antialiasing, true);
        q.setRenderHint(QPainter::SmoothPixmapTransform, true);
        seasons(q, m_sceneSky, QPointF(0, 0));
        q.translate(shake);
        q.scale(m_scenePixels, m_scenePixels);
        q.setClipRect(QRectF(0, 0, kCanvasW, kCanvasH));
        paintGround(q);
        paintWorld(q);
    });

    {
        QPainter q(&m_scene);
        q.setRenderHint(QPainter::Antialiasing, true);
        q.setRenderHint(QPainter::SmoothPixmapTransform, true);
        q.save();
        q.translate(shake);
        q.scale(m_scenePixels, m_scenePixels);
        q.setClipRect(QRectF(0, 0, kCanvasW, kCanvasH));
        paintEffects(q);
        paintAir(q);
        paintLeaves(q);
        q.restore();
        seasons(q, m_sceneOverlay, QPointF(0, 0));
        q.scale(m_scenePixels, m_scenePixels);
        paintSigns(q);
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.save();
    p.setClipRegion(QRegion(rect()).subtracted(QRegion(f.toAlignedRect())));
    seasons(p, m_backdrop, QPointF(0, 0));
    p.restore();
    // At the scene's own size this is a plain copy, on a whole pixel; past
    // the cap it is stretched. The image keeps a pixel ratio of 1, since a
    // painter opened on it later would scale by any other.
    const QPointF corner(std::round(f.left() * dpr) / dpr, std::round(f.top() * dpr) / dpr);
    p.drawImage(QRectF(corner, QSizeF(m_scene.size()) * (s / m_scenePixels)), m_scene);

    // Beyond the canvas, the scenery is dimmed, so nothing past its edges
    // looks playable.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 120));
    p.drawRect(QRectF(0, 0, f.left(), height()));
    p.drawRect(QRectF(f.right(), 0, width() - f.right(), height()));

    if (m_flash > 0.0)
        p.fillRect(rect(), QColor(255, 80, 40, int(80 * m_flash)));

    paintHud(p);
    paintOverlay(p);
    paintStatusCaption(p, QRectF(rect()));
    m_pacer->painted();
}

void LaneView::paintGround(QPainter& p) const
{
    const double outline = outlineWidth();
    const double far = Course::kSightDepth;
    const double near = kNearest;
    const double travelled = m_course.distance();

    // Mortar first, seen between the flagstones, misting into the haze.
    QLinearGradient mortar(QPointF(0, kHorizon), QPointF(0, kCanvasH));
    mortar.setColorAt(0.0, kHaze);
    mortar.setColorAt(0.15, hazed(QColor(0x7a, 0x62, 0x40), 0.4));
    mortar.setColorAt(1.0, QColor(0x5a, 0x44, 0x2a));
    p.setPen(Qt::NoPen);
    p.setBrush(mortar);
    p.drawPolygon(quad(project(-kPathHalf, 0, near), project(kPathHalf, 0, near), project(kPathHalf, 0, far),
                       project(-kPathHalf, 0, far)));

    // Flagstones, laid in courses that alternate by half a lane like brick,
    // each a shade of its own; a few cracked or mossed. They keep their
    // place on the course, so the same stones come past each time.
    const double gap = 0.06;
    const long long firstRow = (long long)std::floor((travelled + near) / kFlagstone);
    const long long lastRow = (long long)std::floor((travelled + far * 0.8) / kFlagstone);
    for (long long row = lastRow; row >= firstRow; --row) {
        const double z0 = double(row) * kFlagstone - travelled;
        const double z1 = z0 + kFlagstone;
        const double clear = clearness(z0);
        const double fog = std::max(1.0 - clear, std::clamp((z0 - 20.0) / 70.0, 0.0, 0.5));
        const bool shifted = (row % 2) != 0;
        const int stones = shifted ? 4 : 3;
        for (int i = 0; i < stones; ++i) {
            double x0 = -kPathHalf + i * kLaneWidth - (shifted ? kLaneWidth / 2.0 : 0.0);
            double x1 = x0 + kLaneWidth;
            x0 = std::max(x0, -kPathHalf);
            x1 = std::min(x1, kPathHalf);
            const double ga = std::max(z0 + gap, near);
            const double gb = z1 - gap;
            if (gb <= ga)
                continue;
            const double shade = tileHash(row * 7 + i, 3);
            QColor stone = kPath.lighter(92 + int(shade * 22));
            if (tileHash(row * 7 + i, 4) < 0.15)
                stone = stone.darker(112);
            p.setBrush(hazed(stone, fog));
            const QPolygonF flag = quad(project(x0 + gap, 0, ga), project(x1 - gap, 0, ga), project(x1 - gap, 0, gb),
                                        project(x0 + gap, 0, gb));
            p.drawPolygon(flag);
            if (z0 > 30.0)
                continue;
            // The near lip of the stone catches the light; its far edge is
            // in its own shadow.
            p.setBrush(hazed(stone.lighter(122), fog));
            p.drawPolygon(quad(project(x0 + gap, 0, ga), project(x1 - gap, 0, ga), project(x1 - gap, 0, ga + 0.09),
                               project(x0 + gap, 0, ga + 0.09)));
            p.setBrush(QColor(0, 0, 0, int(45 * clear)));
            p.drawPolygon(quad(project(x0 + gap, 0, gb - 0.12), project(x1 - gap, 0, gb - 0.12),
                               project(x1 - gap, 0, gb), project(x0 + gap, 0, gb)));
            // A worn, lighter middle where feet have passed.
            const double mx = (x0 + x1) / 2.0;
            p.setBrush(hazed(stone.lighter(104), fog));
            p.drawPolygon(quad(project(mx - 0.4, 0, ga + 0.4), project(mx + 0.4, 0, ga + 0.4),
                               project(mx + 0.4, 0, gb - 0.4), project(mx - 0.4, 0, gb - 0.4)));
            const double mark = tileHash(row * 7 + i, 5);
            if (mark < 0.12) {
                p.setPen(QPen(QColor(0x5a, 0x44, 0x2a, 160), std::max(0.6, sizeAt(z0) * 0.03)));
                p.drawPolyline(QPolygonF({ project(x0 + 0.3, 0, ga + 0.3), project(mx, 0, ga + 0.9),
                                           project(mx + 0.2, 0, gb - 0.6), project(x1 - 0.3, 0, gb - 0.2) }));
                p.setPen(Qt::NoPen);
            } else if (mark < 0.2) {
                p.setBrush(QColor(0x4e, 0x8c, 0x34, 170));
                p.drawPolygon(quad(project(x0 + gap, 0, ga), project(x0 + 0.5, 0, ga), project(x0 + 0.3, 0, ga + 0.8),
                                   project(x0 + gap, 0, ga + 0.6)));
            }
        }
    }
    // Beyond the stones, the path is one hazed sweep.
    QLinearGradient distance(QPointF(0, kHorizon), QPointF(0, project(0, 0, far * 0.8).y()));
    distance.setColorAt(0.0, kHaze);
    distance.setColorAt(1.0, hazed(kPath, 0.6));
    p.setBrush(distance);
    p.drawPolygon(quad(project(-kPathHalf, 0, far * 0.8), project(kPathHalf, 0, far * 0.8),
                       project(kPathHalf, 0, far), project(-kPathHalf, 0, far)));

    // Kerbs: a raised edge of stone each side, its inner face in shadow.
    for (int side = -1; side <= 1; side += 2) {
        const double inner = side * kPathHalf;
        const double outer = side * (kPathHalf + kKerbWidth);
        p.setPen(Qt::NoPen);
        QLinearGradient face(QPointF(0, kHorizon), QPointF(0, kCanvasH));
        face.setColorAt(0.0, kHaze);
        face.setColorAt(0.2, hazed(QColor(0x8a, 0x80, 0x6a), 0.3));
        face.setColorAt(1.0, QColor(0x6a, 0x60, 0x4c));
        p.setBrush(face);
        p.drawPolygon(quad(project(inner, 0, near), project(inner, kKerbHeight, near), project(inner, kKerbHeight, far),
                           project(inner, 0, far)));
        QLinearGradient top(QPointF(0, kHorizon), QPointF(0, kCanvasH));
        top.setColorAt(0.0, kHaze);
        top.setColorAt(0.2, hazed(QColor(0xc4, 0xba, 0xa0), 0.3));
        top.setColorAt(1.0, QColor(0xb0, 0xa6, 0x8c));
        p.setBrush(top);
        p.drawPolygon(quad(project(inner, kKerbHeight, near), project(outer, kKerbHeight, near),
                           project(outer, kKerbHeight, far), project(inner, kKerbHeight, far)));
        // Joints in the kerb, at every half flagstone.
        const double pitch = kFlagstone / 2.0;
        const double offset = std::fmod(travelled, pitch);
        for (int i = 0;; ++i) {
            const double z = near + pitch - offset + i * pitch;
            if (z > 40.0)
                break;
            QColor c(0x4a, 0x40, 0x30);
            c.setAlphaF(float(0.7 * clearness(z)));
            p.setPen(QPen(c, std::max(0.5, sizeAt(z) * 0.03)));
            p.drawLine(project(inner, 0, z), project(inner, kKerbHeight, z));
            p.drawLine(project(inner, kKerbHeight, z), project(outer, kKerbHeight, z));
        }
        p.setPen(QPen(kOutline, outline));
        p.drawLine(project(inner, kKerbHeight, near), project(inner, kKerbHeight, far * 0.85));
    }

    for (const Course::Obstacle& o : m_course.obstacles())
        if (o.kind == Course::Kind::Pit && o.far() > near)
            paintPit(p, o);
}

void LaneView::paintPit(QPainter& p, const Course::Obstacle& o) const
{
    const double outline = outlineWidth();
    const double depth = 1.4;
    const double x0 = laneX(o.lane) - kLaneWidth / 2.0 + 0.06;
    const double x1 = laneX(o.lane) + kLaneWidth / 2.0 - 0.06;
    const double z0 = std::max(o.z, kNearest);
    const double z1 = o.far();
    p.setOpacity(clearness(o.z));

    const QPolygonF hole = quad(project(x0, 0, z0), project(x1, 0, z0), project(x1, 0, z1), project(x0, 0, z1));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x06, 0x03, 0x02));
    p.drawPolygon(hole);
    // The inside: the far wall, lit, and whichever side wall faces the
    // camera, in shadow, so the depth reads.
    p.save();
    QPainterPath clip;
    clip.addPolygon(hole);
    p.setClipPath(clip, Qt::IntersectClip);
    QLinearGradient far(project(0, 0, z1), project(0, -depth, z1));
    far.setColorAt(0.0, QColor(0x7a, 0x56, 0x30));
    far.setColorAt(1.0, QColor(0x10, 0x08, 0x04));
    p.setBrush(far);
    p.drawPolygon(quad(project(x0, 0, z1), project(x1, 0, z1), project(x1, -depth, z1), project(x0, -depth, z1)));
    p.setBrush(QColor(0x2a, 0x1a, 0x0c));
    if (m_cameraX > x0)
        p.drawPolygon(quad(project(x0, 0, z0), project(x0, 0, z1), project(x0, -depth, z1), project(x0, -depth, z0)));
    if (m_cameraX < x1)
        p.drawPolygon(quad(project(x1, 0, z0), project(x1, 0, z1), project(x1, -depth, z1), project(x1, -depth, z0)));
    p.restore();

    p.setPen(QPen(kOutline, outline));
    p.setBrush(Qt::NoBrush);
    p.drawPolygon(hole);
    // Pale lips, so the edges stand out against the dark.
    p.setPen(QPen(kPath.lighter(140), std::max(1.5, sizeAt(o.z) * 0.07), Qt::SolidLine, Qt::RoundCap));
    p.drawLine(project(x0, 0, z0), project(x1, 0, z0));
    p.drawLine(project(x0, 0, z1), project(x1, 0, z1));
    if (o.spent) {
        // A plank across a pit that has cost a heart.
        const double mid = laneX(o.lane);
        QLinearGradient wood(project(mid - 0.35, 0, z0), project(mid + 0.35, 0, z0));
        wood.setColorAt(0.0, QColor(0x8a, 0x5a, 0x2c));
        wood.setColorAt(1.0, QColor(0xb0, 0x7a, 0x44));
        p.setPen(QPen(kOutline, outline));
        p.setBrush(wood);
        p.drawPolygon(quad(project(mid - 0.35, 0.04, z0 - 0.3), project(mid + 0.35, 0.04, z0 - 0.3),
                           project(mid + 0.35, 0.04, z1 + 0.3), project(mid - 0.35, 0.04, z1 + 0.3)));
    }
    p.setOpacity(1.0);
}

void LaneView::paintWorld(QPainter& p) const
{
    // Everything that stands on the course, far to near, so the near hides
    // the far: scenery, obstacles, coins and the runner, in one order.
    enum class Thing { Scenery, Obstacle, Coin, Runner };
    struct Item {
        double z = 0.0;
        Thing thing = Thing::Scenery;
        size_t index = 0;
        double x = 0.0;
        const LaneArt::Sprite* sprite = nullptr;
    };
    std::vector<Item> items;
    items.reserve(96);

    const double travelled = m_course.distance();
    // Each thing is in the season of the place it stands, so the next season
    // comes into sight from the far end of the path (GHUB-0260). In a
    // change, each takes one season or the other.
    std::array<const LaneArt::Set*, Biomes::kLooks> sets {};
    const auto setAt = [&](double metres, long long tile, int salt) -> const LaneArt::Set& {
        const Biomes::Place here = Biomes::at(metres);
        const Biomes::Look look = here.blend > tileHash(tile, salt) ? here.to : here.from;
        const LaneArt::Set*& set = sets[size_t(look)];
        if (set == nullptr)
            set = &LaneArt::art(look);
        return *set;
    };
    // Trees and ruins, then smaller plants between them, along both sides.
    const auto line = [&](double pitch, int salt, double from, double spread, bool plants) {
        const long long first = (long long)std::floor((travelled - 3.0) / pitch);
        const long long last = (long long)std::floor((travelled + Course::kSightDepth) / pitch);
        for (long long t = first; t <= last; ++t) {
            const double ahead = double(t) * pitch - travelled;
            if (ahead < -3.0 || ahead > Course::kSightDepth)
                continue;
            for (int side = -1; side <= 1; side += 2) {
                const long long tile = t * 2 + (side > 0 ? 1 : 0);
                const double roll = tileHash(tile, salt);
                const LaneArt::Set& art = setAt(double(t) * pitch, tile, salt + 2);
                const LaneArt::Sprite* sprite = nullptr;
                if (plants)
                    sprite = roll < 0.25 ? nullptr : &art.plants[size_t(roll * 97) % art.plants.size()];
                else if (roll < 0.24)
                    sprite = &art.ruins[size_t(roll * 997) % art.ruins.size()];
                else
                    sprite = &art.trees[size_t(roll * 97) % art.trees.size()];
                if (sprite == nullptr)
                    continue;
                const double x = side * (kPathHalf + kKerbWidth + from + spread * tileHash(tile, salt + 1));
                items.push_back({ ahead, Thing::Scenery, 0, x, sprite });
            }
        }
    };
    line(5.0, 1, 1.4, 2.6, false);
    line(2.5, 7, 0.45, 0.5, true);

    const auto& obstacles = m_course.obstacles();
    for (size_t i = 0; i < obstacles.size(); ++i)
        if (obstacles[i].kind != Course::Kind::Pit && obstacles[i].far() > kNearest)
            // An obstacle the runner has reached is drawn after it: a lintel
            // overhead hides the runner's head, not the other way round.
            items.push_back({ obstacles[i].z > 0.0 ? obstacles[i].z : -0.5, Thing::Obstacle, i });
    const auto& coins = m_course.coins();
    // A coin missed is gone once behind the runner, rather than swelling
    // past the camera.
    for (size_t i = 0; i < coins.size(); ++i)
        if (!coins[i].taken && coins[i].z > kCoinGone)
            items.push_back({ coins[i].z, Thing::Coin, i });
    items.push_back({ 0.0, Thing::Runner });

    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.z > b.z; });

    const double pixels = m_scenePixels;
    for (const Item& item : items) {
        switch (item.thing) {
        case Thing::Scenery: {
            const double k = sizeAt(item.z);
            const QPointF base = project(item.x, 0, item.z);
            const double w = item.sprite->widthMetres * k;
            const double h = item.sprite->heightMetres * k;
            LaneArt::draw(p, *item.sprite, QRectF(base.x() - w / 2.0, base.y() - h, w, h),
                          airAt(item.z, clearness(item.z)), h * pixels);
            break;
        }
        case Thing::Obstacle: paintObstacle(p, obstacles[item.index]); break;
        case Thing::Coin: paintCoin(p, coins[item.index]); break;
        case Thing::Runner: paintRunner(p); break;
        }
    }
}

void LaneView::paintObstacle(QPainter& p, const Course::Obstacle& o) const
{
    const double outline = outlineWidth();
    const double z0 = std::max(o.z, kNearest);
    const double z1 = o.far();
    const double pixels = m_scenePixels;
    const LaneArt::Set& art = LaneArt::art();
    p.setOpacity(clearness(o.z));

    // A box between x0..x1 across, y0..y1 up and z0..z1 ahead: the faces
    // the camera can see, darkest at the side, lightest on top. Returns the
    // front face, for the carving.
    const auto box = [&](double x0, double x1, double y0, double y1, const QColor& colour, const QColor& top,
                         const QColor& side) {
        p.setPen(QPen(kOutline, outline));
        if (y1 < kCameraHeight) {
            p.setBrush(top);
            p.drawPolygon(quad(project(x0, y1, z0), project(x1, y1, z0), project(x1, y1, z1), project(x0, y1, z1)));
        }
        p.setBrush(side);
        if (x0 > m_cameraX)
            p.drawPolygon(quad(project(x0, y0, z0), project(x0, y1, z0), project(x0, y1, z1), project(x0, y0, z1)));
        if (x1 < m_cameraX)
            p.drawPolygon(quad(project(x1, y0, z0), project(x1, y1, z0), project(x1, y1, z1), project(x1, y0, z1)));
        const QRectF front(project(x0, y1, z0), project(x1, y0, z0));
        p.setBrush(colour);
        p.drawRect(front);
        return front;
    };
    const auto carved = [&](const LaneArt::Sprite& face, const QRectF& front) {
        LaneArt::draw(p, face, front, 0.0, front.height() * pixels);
        p.setPen(QPen(kOutline, outline * 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(front);
    };

    // A soft shadow on the path under anything standing.
    const double cx = laneX(o.lane);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 70));
    p.drawPolygon(quad(project(cx - kLaneWidth * 0.5, 0, z0 - 0.2), project(cx + kLaneWidth * 0.5, 0, z0 - 0.2),
                       project(cx + kLaneWidth * 0.46, 0, z1 + 0.4), project(cx - kLaneWidth * 0.46, 0, z1 + 0.4)));

    const size_t look = size_t(o.look) % 4;
    switch (o.kind) {
    case Course::Kind::Pit:
        break;   // part of the ground
    case Course::Kind::Root: {
        // A fallen trunk across the lane; its cut ends show pale wood.
        const double half = kLaneWidth * 0.46;
        const QRectF front = box(cx - half, cx + half, 0.0, Course::kRootHeight, kRoot, QColor(0xf4, 0x9a, 0x50),
                                 QColor(0xe8, 0xb8, 0x80));
        carved(art.trunkFaces[look], front);
        break;
    }
    case Course::Kind::Block: {
        const double half = kLaneWidth * 0.43;
        const QRectF front = box(cx - half, cx + half, 0.0, Course::kBlockHeight, kStone, QColor(0x5c, 0xa0, 0x3a),
                                 kStone.darker(140));
        carved(art.blockFaces[look], front);
        break;
    }
    case Course::Kind::Lintel: {
        // A gateway: two carved posts and a slab across them.
        const double half = kLaneWidth * 0.48;
        const double post = kLaneWidth * 0.11;
        const double top = Course::kLintelClearance + 1.5;
        for (int side = -1; side <= 1; side += 2) {
            const double a = side < 0 ? cx - half : cx + half - post;
            const QRectF face = box(a, a + post, 0.0, top, kWall, kWall.lighter(125), kWall.darker(140));
            p.setPen(QPen(kWall.darker(170), std::max(0.6, sizeAt(z0) * 0.03)));
            p.drawLine(QPointF(face.center().x(), face.top() + 2), QPointF(face.center().x(), face.bottom() - 2));
        }
        const QRectF slab = box(cx - half, cx + half, Course::kLintelClearance, top, kWall, kWall.lighter(125),
                                kWall.darker(140));
        carved(art.gateFaces[look], slab);
        break;
    }
    }
    p.setOpacity(1.0);
}

void LaneView::paintCoin(QPainter& p, const Course::Coin& c) const
{
    const double k = sizeAt(c.z);
    const QPointF at = project(laneX(c.lane), c.height, c.z);
    const double turn = std::abs(std::cos(m_time * 3.0 + c.z * 0.5));
    const double ry = 0.26 * k;
    const double rx = ry * (0.3 + 0.7 * turn);
    // Fades as it passes the runner, gone by kCoinGone.
    const double passing = c.z >= 0.0 ? 1.0 : std::clamp(1.0 - c.z / kCoinGone, 0.0, 1.0);
    p.setOpacity(clearness(c.z) * passing);
    // A soft glow, the rim, the face, and a stamped star.
    QRadialGradient glow(at, ry * 2.2);
    glow.setColorAt(0.0, QColor(255, 220, 120, 90));
    glow.setColorAt(1.0, QColor(255, 220, 120, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(at, ry * 2.2, ry * 2.2);
    p.setPen(QPen(kOutline, std::min(outlineWidth(), std::max(0.6, ry * 0.2))));
    p.setBrush(QColor(0xc8, 0x8a, 0x10));
    p.drawEllipse(at, rx, ry);
    QRadialGradient shine(at + QPointF(-rx * 0.3, -ry * 0.35), ry * 1.1);
    shine.setColorAt(0.0, QColor(0xff, 0xf4, 0xb0));
    shine.setColorAt(0.5, kGold);
    shine.setColorAt(1.0, QColor(0xd8, 0x98, 0x18));
    p.setPen(Qt::NoPen);
    p.setBrush(shine);
    p.drawEllipse(at, rx * 0.78, ry * 0.78);
    if (rx > 3.0) {
        QPainterPath star;
        for (int i = 0; i < 10; ++i) {
            const double a = -std::numbers::pi / 2.0 + i * std::numbers::pi / 5.0;
            const double r = (i % 2 == 0) ? 0.45 : 0.2;
            const QPointF pt = at + QPointF(std::cos(a) * rx * r, std::sin(a) * ry * r);
            if (i == 0)
                star.moveTo(pt);
            else
                star.lineTo(pt);
        }
        star.closeSubpath();
        p.setBrush(QColor(0xc0, 0x80, 0x10, 170));
        p.drawPath(star);
    }
    // Now and then a glint crosses it.
    const double glint = std::fmod(m_time * 0.7 + c.z * 0.13, 1.0);
    if (glint < 0.12) {
        const double g = std::sin(glint / 0.12 * std::numbers::pi) * ry * 0.9;
        const QPointF gc = at + QPointF(rx * 0.35, -ry * 0.4);
        p.setPen(QPen(QColor(255, 255, 240, 230), std::max(0.6, ry * 0.12), Qt::SolidLine, Qt::RoundCap));
        p.drawLine(gc - QPointF(g, 0), gc + QPointF(g, 0));
        p.drawLine(gc - QPointF(0, g), gc + QPointF(0, g));
    }
    p.setOpacity(1.0);
}

void LaneView::paintRunner(QPainter& p) const
{
    const double outline = outlineWidth();
    const double x = laneX(m_course.lane());
    const double k = sizeAt(0.0);

    // The shadow stays on the path, shrinking as the runner rises.
    const QPointF ground = project(x, 0, 0);
    const double lift = std::clamp(1.0 - m_course.height() / 2.0, 0.3, 1.0);
    QRadialGradient shadow(ground, 0.4 * k * lift);
    shadow.setColorAt(0.0, QColor(0, 0, 0, int(130 * lift)));
    shadow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(shadow);
    p.drawEllipse(ground, 0.4 * k * lift, 0.13 * k * lift);

    if (m_course.hurtLeft() > 0.0 && std::fmod(m_course.hurtLeft(), 0.2) < 0.1)
        p.setOpacity(0.35);

    const QColor skin(0xf2, 0xc4, 0x90);
    const QColor hair(0x3a, 0x24, 0x18);
    const QColor tunic(0xf2, 0x8a, 0x22);
    const QColor trousers(0x3c, 0x4c, 0x70);
    const QColor boot(0x5a, 0x38, 0x1c);
    const QColor sole(0xc8, 0xa4, 0x78);
    const QColor scarf(0x1c, 0xc8, 0xb4);
    const QColor leather(0x7a, 0x4a, 0x24);

    const bool sliding = m_course.sliding() || m_course.over();
    const bool flying = m_course.airborne() && !sliding;
    const bool running = m_course.phase() == Course::Phase::Running;
    const double stride = m_course.distance() / 0.9;

    // Drawn in metres about the feet, y up, then placed and leant into a
    // switch.
    p.save();
    p.translate(project(x, m_course.height(), 0));
    p.scale(k, -k);
    p.rotate(std::clamp((m_course.targetLane() - m_course.lane()) * 14.0, -10.0, 10.0));
    if (m_landAge < 0.16 && !sliding) {
        const double squash = 1.0 - m_landAge / 0.16;
        p.scale(1.0 + 0.08 * squash, 1.0 - 0.12 * squash);
    }
    const double o = outline / k;
    const auto limb = [&](QPointF a, QPointF b, double w, const QColor& c) {
        p.setPen(QPen(kOutline, w + 2.0 * o, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(a, b);
        p.setPen(QPen(c, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(a, b);
        // Light down the sun's side.
        p.setPen(QPen(c.lighter(130), w * 0.3, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(a + QPointF(w * 0.22, 0), b + QPointF(w * 0.22, 0));
    };

    const double bob = running && !sliding && !flying ? std::abs(std::sin(stride)) * 0.035 : 0.0;
    const double hipY = (sliding ? 0.32 : 0.92) + bob;
    const double shoulderY = (sliding ? 0.78 : 1.42) + bob;
    const double headY = (sliding ? 0.92 : 1.57) + bob;

    // Legs: from behind, a foot lifted is a foot drawn higher and shorter,
    // and its sole shows.
    for (int i = 0; i < 2; ++i) {
        const double side = i == 0 ? -1.0 : 1.0;
        double footUp = 0.0;
        if (flying)
            footUp = 0.32;
        else if (running && !sliding)
            footUp = std::max(0.0, std::sin(stride + i * std::numbers::pi)) * 0.34;
        const QPointF hip(side * 0.1, hipY);
        const QPointF foot(side * 0.13, sliding ? 0.06 : footUp);
        const QPointF knee((hip.x() + foot.x()) / 2.0 + side * 0.03, (hip.y() + foot.y()) / 2.0 + footUp * 0.3);
        limb(hip, knee, 0.15, trousers);
        limb(knee, foot, 0.13, trousers);
        limb(foot, foot + QPointF(0, 0.05), 0.16, boot);
        if (footUp > 0.1) {
            p.setPen(QPen(kOutline, o));
            p.setBrush(sole);
            p.drawRoundedRect(QRectF(foot.x() - 0.075, foot.y() - 0.04, 0.15, 0.07), 0.03, 0.03);
        }
    }

    // Arms swing against the legs.
    for (int i = 0; i < 2; ++i) {
        const double side = i == 0 ? -1.0 : 1.0;
        const double swing = running && !sliding ? std::sin(stride + i * std::numbers::pi) * 0.1 : 0.0;
        const QPointF shoulder(side * 0.2, shoulderY - 0.04);
        const QPointF elbow(side * 0.27, shoulderY - 0.26 + swing);
        const QPointF hand(side * (flying ? 0.42 : 0.24), shoulderY - (flying ? 0.2 : 0.46) + swing * 1.6);
        limb(shoulder, elbow, 0.1, tunic.darker(108));
        limb(elbow, hand, 0.085, skin);
    }

    // The tunic, lit from the right, with a belt and a satchel strap.
    QPainterPath body;
    body.moveTo(-0.17, hipY - 0.07);
    body.lineTo(0.17, hipY - 0.07);
    body.lineTo(0.22, shoulderY);
    body.quadTo(0.0, shoulderY + 0.05, -0.22, shoulderY);
    body.closeSubpath();
    QLinearGradient cloth(QPointF(-0.22, 0), QPointF(0.22, 0));
    cloth.setColorAt(0.0, tunic.darker(125));
    cloth.setColorAt(0.65, tunic);
    cloth.setColorAt(1.0, tunic.lighter(118));
    p.setPen(QPen(kOutline, o));
    p.setBrush(cloth);
    p.drawPath(body);
    p.setPen(QPen(tunic.darker(140), o * 0.8));
    p.drawLine(QPointF(0.0, hipY), QPointF(0.0, shoulderY - 0.05));
    p.setPen(QPen(kOutline, o));
    p.setBrush(leather);
    p.drawRect(QRectF(-0.175, hipY - 0.01, 0.35, 0.06));
    p.setBrush(kGold);
    p.drawRect(QRectF(-0.03, hipY - 0.005, 0.06, 0.05));
    p.setPen(QPen(leather, 0.045, Qt::SolidLine, Qt::FlatCap));
    p.drawLine(QPointF(-0.19, shoulderY - 0.02), QPointF(0.16, hipY + 0.02));
    p.setPen(QPen(kOutline, o));
    p.setBrush(leather);
    p.drawRoundedRect(QRectF(0.11, hipY - 0.1, 0.13, 0.13), 0.03, 0.03);

    // The pack on its back, a rolled blanket strapped over it.
    {
        const double mid = (hipY + shoulderY) / 2.0 + 0.03;
        p.setPen(QPen(kOutline, o));
        QLinearGradient hide(QPointF(-0.16, 0), QPointF(0.16, 0));
        hide.setColorAt(0.0, leather.darker(120));
        hide.setColorAt(0.7, leather);
        hide.setColorAt(1.0, leather.lighter(122));
        p.setBrush(hide);
        p.drawRoundedRect(QRectF(-0.16, mid - 0.2, 0.32, 0.4), 0.06, 0.06);
        p.setBrush(leather.lighter(112));
        p.drawRoundedRect(QRectF(-0.15, mid + 0.04, 0.3, 0.15), 0.05, 0.05);
        p.setBrush(kGold.darker(115));
        p.drawRect(QRectF(-0.025, mid + 0.02, 0.05, 0.05));
        QLinearGradient roll(QPointF(0, shoulderY - 0.02), QPointF(0, shoulderY + 0.1));
        roll.setColorAt(0.0, QColor(0xa8, 0x94, 0x6c));
        roll.setColorAt(1.0, QColor(0xd8, 0xc8, 0xa0));
        p.setBrush(roll);
        p.drawRoundedRect(QRectF(-0.2, shoulderY - 0.03, 0.4, 0.11), 0.055, 0.055);
        p.setPen(QPen(leather.darker(140), 0.025));
        p.drawLine(QPointF(-0.1, shoulderY - 0.03), QPointF(-0.1, shoulderY + 0.08));
        p.drawLine(QPointF(0.1, shoulderY - 0.03), QPointF(0.1, shoulderY + 0.08));
    }

    // The scarf, streaming back towards the camera, which from behind is
    // down and out to one side.
    QPainterPath tail;
    const QPointF neck(0.04, shoulderY + 0.02);
    tail.moveTo(neck);
    for (int i = 1; i <= 4; ++i)
        tail.lineTo(neck + QPointF(0.05 * i + std::sin(m_time * 14.0 + i * 1.3) * 0.025 * i, -0.09 * i));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(kOutline, 0.08 + 2.0 * o, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(tail);
    p.setPen(QPen(scarf, 0.08, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(tail);
    limb(QPointF(-0.13, shoulderY + 0.01), QPointF(0.13, shoulderY + 0.01), 0.08, scarf);

    // The back of the head: hair with a sheen, the ears showing.
    p.setPen(QPen(kOutline, o));
    p.setBrush(skin);
    p.drawEllipse(QPointF(-0.13, headY), 0.035, 0.05);
    p.drawEllipse(QPointF(0.13, headY), 0.035, 0.05);
    QRadialGradient sheen(QPointF(0.05, headY + 0.06), 0.16);
    sheen.setColorAt(0.0, hair.lighter(170));
    sheen.setColorAt(1.0, hair);
    p.setBrush(sheen);
    p.drawEllipse(QPointF(0, headY), 0.13, 0.14);
    // A tail of hair that swings with the stride.
    {
        const double swing = running && !sliding ? std::sin(stride) * 0.05 : std::sin(m_time * 2.0) * 0.01;
        QPainterPath tail;
        tail.moveTo(0.0, headY - 0.06);
        tail.quadTo(QPointF(swing * 0.5, headY - 0.16), QPointF(swing, headY - 0.25));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(kOutline, 0.07 + 2.0 * o, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(tail);
        p.setPen(QPen(hair, 0.07, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(tail);
        p.setPen(QPen(hair.lighter(160), 0.02, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(tail.translated(0.015, 0));
    }

    p.restore();
    p.setOpacity(1.0);
}

void LaneView::paintLeaves(QPainter& p) const
{
    // Leaves fall where the season has them, in its colours.
    const Biomes::Look look = Biomes::at(m_course.distance()).main();
    const Biomes::Weather weather = Biomes::weather(look);
    if (weather != Biomes::Weather::Motes && weather != Biomes::Weather::Leaves)
        return;
    p.setPen(Qt::NoPen);
    int which = 0;
    for (const Leaf& l : m_leaves) {
        const QColor colour = Biomes::leafColour(look, which++);
        p.save();
        p.translate(l.at);
        p.rotate(l.angle);
        p.setBrush(colour);
        p.drawEllipse(QPointF(0, 0), 4.5, 2.0);
        p.setPen(QPen(colour.darker(150), 0.6));
        p.drawLine(QPointF(-4, 0), QPointF(4, 0));
        p.restore();
    }
}

void LaneView::paintAir(QPainter& p) const
{
    // What drifts in the air is the season's (GHUB-0260): motes in the
    // jungle, leaves in autumn, snow, and dust in the dry season.
    const Biomes::Place place = Biomes::at(m_course.distance());
    const QRectF area(0, 0, kCanvasW, kCanvasH);
    RunnerArt::paintWeather(p, place.from, 1.0 - place.blend, m_time, 0.0, area, 1.0, m_mote);
    if (place.blend > 0.0)
        RunnerArt::paintWeather(p, place.to, place.blend, m_time, 0.0, area, 1.0, m_mote);
}

void LaneView::paintEffects(QPainter& p) const
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

void LaneView::paintSign(QPainter& p, QPointF c, double r, Sign sign, double outline)
{
    // A round badge: dark with a gold rim and a gold arrow for something to
    // jump or slide, red with a white cross for a block to go round.
    QRadialGradient fill(c + QPointF(-r * 0.3, -r * 0.3), r * 1.3);
    if (sign == Sign::GoRound) {
        fill.setColorAt(0.0, QColor(0xe8, 0x50, 0x40));
        fill.setColorAt(1.0, QColor(0x98, 0x1c, 0x18));
    } else {
        fill.setColorAt(0.0, QColor(0x3a, 0x44, 0x66));
        fill.setColorAt(1.0, QColor(0x14, 0x1a, 0x30));
    }
    p.setPen(QPen(kOutline, outline * 1.4));
    p.setBrush(fill);
    p.drawEllipse(c, r, r);
    p.setPen(QPen(sign == Sign::GoRound ? QColor(0xff, 0xe8, 0xe0) : kGold, r * 0.14));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, r * 0.86, r * 0.86);
    if (sign == Sign::GoRound) {
        p.setPen(QPen(QColor(255, 255, 255), r * 0.26, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(c + QPointF(-r * 0.36, -r * 0.36), c + QPointF(r * 0.36, r * 0.36));
        p.drawLine(c + QPointF(r * 0.36, -r * 0.36), c + QPointF(-r * 0.36, r * 0.36));
        return;
    }
    const double d = sign == Sign::Slide ? 1.0 : -1.0;
    QPainterPath arrow;
    arrow.moveTo(c + QPointF(0, d * r * 0.62));
    arrow.lineTo(c + QPointF(-r * 0.5, d * r * 0.02));
    arrow.lineTo(c + QPointF(-r * 0.2, d * r * 0.02));
    arrow.lineTo(c + QPointF(-r * 0.2, -d * r * 0.58));
    arrow.lineTo(c + QPointF(r * 0.2, -d * r * 0.58));
    arrow.lineTo(c + QPointF(r * 0.2, d * r * 0.02));
    arrow.lineTo(c + QPointF(r * 0.5, d * r * 0.02));
    arrow.closeSubpath();
    p.setPen(QPen(kOutline, outline));
    p.setBrush(kGold);
    p.drawPath(arrow);
}

void LaneView::paintSigns(QPainter& p) const
{
    // Under each lane, a sign for what the next row holds in it, for the
    // last couple of seconds before the row arrives.
    const std::array<Sign, Course::kLanes> now = signs();
    if (std::all_of(now.begin(), now.end(), [](Sign s) { return s == Sign::None; }))
        return;
    const double r = Legibility::instance().enabled() ? 18.0 : 15.0;
    const double pulse = 1.0 + 0.06 * std::sin(m_time * 9.0);
    for (int lane = 0; lane < Course::kLanes; ++lane) {
        if (now[size_t(lane)] == Sign::None)
            continue;
        const QPointF c(project(laneX(lane), 0, -0.6).x(), kCanvasH - r - 3.0);
        paintSign(p, c, r * pulse, now[size_t(lane)], outlineWidth());
    }
}

void LaneView::paintHud(QPainter& p) const
{
    const QRectF f = canvasRect();
    const double s = scale();
    const bool legible = Legibility::instance().enabled();
    const QRectF strip(f.left() + 10 * s, f.top() + 6 * s, f.width() - 20 * s, 26 * s);
    const int best = std::max(m_table.isEmpty() ? 0 : m_table.first(), m_course.score());

    const auto shadowed = [&](const QRectF& r, int align, const QString& text, const QColor& c) {
        p.setPen(QColor(0, 0, 0, 200));
        p.drawText(r.translated(1.5, 1.5), align, text);
        p.setPen(c);
        p.drawText(r, align, text);
    };
    // Dark plaques behind each part, so they read over the leaves.
    const auto plaque = [&](const QRectF& r) {
        p.setPen(QPen(QColor(kPanelEdge.red(), kPanelEdge.green(), kPanelEdge.blue(), 110), 1.5));
        p.setBrush(QColor(0x14, 0x0e, 0x08, 170));
        p.drawRoundedRect(r, 8 * s, 8 * s);
    };

    const double size = 20 * s * (legible ? 1.2 : 1.0);
    plaque(QRectF(strip.left() - 4 * s, strip.top() - 2 * s, size * 4.2, strip.height() + 4 * s));
    for (int i = 0; i < Course::kHearts; ++i) {
        const QPointF c(strip.left() + size * 0.65 + i * size * 1.3, strip.center().y());
        QRadialGradient gloss(c + QPointF(-size * 0.15, -size * 0.2), size * 0.7);
        gloss.setColorAt(0.0, QColor(0xff, 0x9a, 0x90));
        gloss.setColorAt(1.0, QColor(0xd8, 0x20, 0x30));
        p.setPen(QPen(kOutline, std::max(1.5, 2.0 * s)));
        if (i < m_course.hearts())
            p.setBrush(gloss);
        else
            p.setBrush(QColor(0, 0, 0, 90));
        p.drawPath(heartPath(c, size));
    }

    p.setFont(canvasFont(17));
    const QFontMetricsF fm(p.font());
    const QString score = tr("Score %1").arg(m_course.score());
    const QString bestText = tr("Best %1").arg(best);
    const double scoreW = fm.horizontalAdvance(score) + 24 * s;
    const double bestW = fm.horizontalAdvance(bestText) + 20 * s;
    plaque(QRectF(strip.center().x() - scoreW / 2.0, strip.top() - 2 * s, scoreW, strip.height() * 1.9 + 4 * s));
    plaque(QRectF(strip.right() - bestW + 4 * s, strip.top() - 2 * s, bestW, strip.height() + 4 * s));
    shadowed(strip, Qt::AlignHCenter | Qt::AlignVCenter, score, kInk);
    shadowed(strip, Qt::AlignRight | Qt::AlignVCenter, bestText, kGold);
    p.setFont(canvasFont(13, false));
    const QRectF under = strip.translated(0, strip.height() * 0.9);
    shadowed(under, Qt::AlignHCenter | Qt::AlignVCenter,
             tr("%1 m   %2 coins").arg(m_course.metres()).arg(m_course.coinsTaken()), kInk);
}

void LaneView::paintPanel(QPainter& p, const QRectF& r) const
{
    QLinearGradient fill(r.topLeft(), r.bottomLeft());
    fill.setColorAt(0.0, QColor(0x2a, 0x1e, 0x12, 238));
    fill.setColorAt(1.0, QColor(0x14, 0x0e, 0x08, 238));
    p.setPen(QPen(kPanelEdge, 2.5));
    p.setBrush(fill);
    p.drawRoundedRect(r, 14, 14);
    p.setPen(QPen(QColor(kPanelEdge.red(), kPanelEdge.green(), kPanelEdge.blue(), 90), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(5, 5, -5, -5), 10, 10);
}

void LaneView::paintOverlay(QPainter& p) const
{
    const QRectF f = canvasRect();
    const double s = scale();
    const bool legible = Legibility::instance().enabled();

    const auto line = [&](const QRectF& r, const QString& text, double size, const QColor& c,
                          bool bold = true) {
        p.setFont(canvasFont(size, bold));
        p.setPen(QColor(0, 0, 0, 220));
        p.drawText(r.translated(1.5, 1.5), Qt::AlignCenter | Qt::TextWordWrap, text);
        p.setPen(c);
        p.drawText(r, Qt::AlignCenter | Qt::TextWordWrap, text);
    };

    if (m_bannerAge < 1.8 && !m_banner.isEmpty() && !m_course.over() && !m_pause->isChecked()) {
        const double fade = std::clamp(std::min(m_bannerAge * 5.0, (1.8 - m_bannerAge) * 2.5), 0.0, 1.0);
        p.save();
        p.setOpacity(fade);
        line(QRectF(f.left(), f.top() + 64 * s, f.width(), 40 * s), m_banner, 26, kGold);
        p.restore();
    }

    // Panels sit in the sky, above the path, so the runner stays in view.
    const double panelW = std::min(f.width() - 30 * s, 400 * s);
    const auto panelAt = [&](double heightUnits, double centre = 150.0) {
        const double h = std::min(f.height() - 20 * s, heightUnits * s * (legible ? 1.2 : 1.0));
        return QRectF(f.center().x() - panelW / 2.0, f.top() + centre * s - h / 2.0, panelW, h);
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
            p.setFont(canvasFont(16, mine));
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
        // Higher than the others, so the runner waiting below stays in view.
        const QRectF panel = panelAt(172, 126.0);
        paintPanel(p, panel);
        const double rowH = panel.height() / 8.0;
        double y = panel.top() + rowH * 0.35;
        line(QRectF(panel.left(), y, panel.width(), rowH * 1.4), tr("Ruin Runner Lanes"), 28, kGold);
        y += rowH * 1.5;
        line(QRectF(panel.left(), y, panel.width(), rowH), tr("Left and Right change lane."), 15, kInk, false);
        y += rowH;
        line(QRectF(panel.left(), y, panel.width(), rowH), tr("Space or Up jumps.  Down slides."), 15, kInk,
             false);
        y += rowH * 1.2;
        // What the signs under the lanes mean.
        const double r = rowH * 0.62;
        const std::array<Sign, 3> legend { Sign::Jump, Sign::Slide, Sign::GoRound };
        const std::array<QString, 3> words { tr("Jump"), tr("Slide"), tr("Go round") };
        const double third = panel.width() / 3.0;
        for (size_t i = 0; i < legend.size(); ++i) {
            const double cx = panel.left() + third * (double(i) + 0.5);
            paintSign(p, QPointF(cx, y + r), r, legend[i], outlineWidth());
            line(QRectF(cx - third / 2.0, y + 2.0 * r + 2 * s, third, rowH * 0.8), words[i], 13, kInk, false);
        }
        y += 2.0 * r + rowH * 1.0;
        line(QRectF(panel.left(), y, panel.width(), rowH), tr("Press Space to start"), 17, kGold);
    }
}

// ---- input ----

void LaneView::keyPressEvent(QKeyEvent* event)
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
    case Qt::Key_Left:
    case Qt::Key_A:
    case Qt::Key_Right:
    case Qt::Key_D:
        // One lane a press; a held key does not sweep across.
        if (!event->isAutoRepeat() && !m_course.over())
            m_course.steer(event->key() == Qt::Key_Left || event->key() == Qt::Key_A ? -1 : 1);
        break;
    case Qt::Key_Down:
    case Qt::Key_S:
        m_slideKey = true;
        break;
    default:
        GameView::keyPressEvent(event);
        return;
    }
    m_course.setSlide(m_slideKey || m_slideButton);
    readEvents();
    update();
}

void LaneView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) {
        GameView::keyReleaseEvent(event);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Down:
    case Qt::Key_S:
        m_slideKey = false;
        break;
    default:
        GameView::keyReleaseEvent(event);
        return;
    }
    m_course.setSlide(m_slideKey || m_slideButton);
}

void LaneView::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    if (m_pause->isChecked())
        return;
    if (event->button() == Qt::LeftButton) {
        if (m_course.over()) {
            newGame();
            return;
        }
        // The left third of the play steers left, the right third right, and
        // the middle jumps.
        const QRectF f = canvasRect();
        const double across = (event->position().x() - f.left()) / f.width();
        if (across < 1.0 / 3.0)
            m_course.steer(-1);
        else if (across > 2.0 / 3.0)
            m_course.steer(1);
        else
            m_course.jump();
    } else if (event->button() == Qt::RightButton) {
        m_slideButton = true;
    }
    m_course.setSlide(m_slideKey || m_slideButton);
    readEvents();
    update();
}

void LaneView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        m_slideButton = false;
        m_course.setSlide(m_slideKey || m_slideButton);
    }
    GameView::mouseReleaseEvent(event);
}

void LaneView::focusOutEvent(QFocusEvent* event)
{
    releaseKeys();
    GameView::focusOutEvent(event);
}

// How to play (GHUB-0016). Written for this collection from what this file
// and lanecourse.cpp do: the points are RunnerCourse::kCoinPoints and a metre
// each, the hearts kHearts, the keys keyPressEvent() and buildActions().
QString LaneView::rulesText() const
{
    return tr(
        "<h3>The aim</h3>"
        "<p>Run into the jungle ruins along three lanes for as long as you can. "
        "Change lane, jump and slide past what is in your way, pick up coins, and "
        "beat your best score. The best scores are shared with Ruin Runner.</p>"

        "<h3>How it plays</h3>"
        "<ul>"
        "<li>The run waits until you press Space.</li>"
        "<li>The runner keeps running on its own. The pace starts gently and rises "
        "the longer the run lasts, up to a top speed.</li>"
        "<li>Go round the tall carved blocks by changing lane.</li>"
        "<li>Jump over fallen trunks and pits.</li>"
        "<li>Slide under the gateways. Each has a gold band along its lower edge.</li>"
        "<li>Every row leaves a way through. A line of coins often leads to it.</li>"
        "<li>A couple of seconds before a row arrives, a sign under each lane says "
        "what is in it: an arrow up to jump, an arrow down to slide, a red cross to "
        "go round. A lane with no sign is clear.</li>"
        "<li>Every jump is the same height. A jump pressed just before landing "
        "happens as soon as you land.</li>"
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
        "<li>Left or A, Right or D: change lane, one lane a press.</li>"
        "<li>Space, Up, W or Enter: jump.</li>"
        "<li>Down or S: slide, for as long as you hold it. In the air it drops you faster.</li>"
        "<li>A left click on the left or right third of the play changes lane; in the "
        "middle it jumps. Holding the right mouse button slides.</li>"
        "<li>P, or the Pause button: pause, and press again to carry on.</li>"
        "<li>When a run ends, Space or a click starts the next.</li>"
        "<li>New Game button, or Ctrl+N: start again.</li>"
        "<li>If you leave for another game, this one pauses until you come back.</li>"
        "</ul>");
}
