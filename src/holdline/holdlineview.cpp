#include "holdlineview.h"

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
#include <numbers>

namespace {
constexpr int kFrameMs = 16;
using Field = HoldLineField;

// A function for the reason scores.h's own keys are: nothing is built at
// static-initialisation time. The leaf starts "best_" so Scores::clear(),
// which matches by that shape, forgets the table with every other record.
QString tableKey() { return QStringLiteral("holdline/best_scores"); } // untranslated: settings key

// Each species' colour, by row: the top row's wisps, the middle rows' mantas
// and the bottom rows' pods.
QColor speciesColour(int row)
{
    if (row == 0)
        return { 0xff, 0x5c, 0xc8 };
    if (row <= 2)
        return { 0x3c, 0xe0, 0xe8 };
    return { 0xff, 0xb0, 0x30 };
}

constexpr QColor kShotColour { 0x9c, 0xf6, 0xff };
constexpr QColor kBombColour { 0xff, 0x55, 0x2a };
constexpr QColor kShelterColour { 0x52, 0xf0, 0x9a };
constexpr QColor kOutline { 0x0a, 0x0c, 0x1c };
constexpr QColor kPanelEdge { 0x6a, 0xd8, 0xff };
constexpr QColor kGold { 0xff, 0xd5, 0x4a };

double unit(std::mt19937& rng)
{
    return double(rng()) / double(std::mt19937::max());
}
}

HoldLineView::HoldLineView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(HoldLineView::minimumSizeHint());
    setFocusPolicy(Qt::StrongFocus);

    m_timer = new QTimer(this);
    m_timer->setInterval(kFrameMs);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &HoldLineView::tick);

    // Three layers of stars, far ones small and slow.
    for (int i = 0; i < 150; ++i)
        m_stars.push_back({ QPointF(unit(m_fx), unit(m_fx)), unit(m_fx), unit(m_fx) * 6.28 });

    m_table = highScores();
    buildActions();
    newGame();
}

void HoldLineView::buildActions()
{
    auto* newAction = new QAction(tr("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &HoldLineView::newGame);
    m_actions.append(newAction);

    m_pause = new QAction(tr("Pause"), this);
    m_pause->setCheckable(true);
    // P, as Minesweeper's pause is. Space fires here, so it cannot pause.
    m_pause->setShortcut(QKeySequence(Qt::Key_P));
    connect(m_pause, &QAction::toggled, this, [this] {
        m_leftHeld = m_rightHeld = m_fireHeld = false;
        m_field.setMove(0);
        runClock();
        refresh();
        update();
        setFocus();
    });
    m_actions.append(m_pause);
}

void HoldLineView::newGame()
{
    m_field.newGame();
    m_field.takeEvents();
    m_recorded = false;
    m_rank = -1;
    m_particles.clear();
    m_rings.clear();
    m_popups.clear();
    m_shake = m_flash = m_muzzle = 0.0;
    m_bannerAge = 10.0;
    m_banner.clear();
    m_leftHeld = m_rightHeld = m_fireHeld = false;
    // A new game starts unpaused, as Snake's does (GHUB-0230).
    m_pause->setChecked(false);
    runClock();
    setFocus();
    refresh();
    update();
}

void HoldLineView::activate()
{
    m_active = true;
    setFocus();
    runClock();
    refresh();
}

void HoldLineView::deactivate()
{
    m_active = false;
    // A key let go of while another game had focus never reaches this view.
    m_leftHeld = m_rightHeld = m_fireHeld = false;
    m_field.setMove(0);
    runClock();
}

void HoldLineView::runClock()
{
    // The clock runs while the page is shown and not paused, Ready included,
    // so the stars move and the wave breathes before the first shot.
    if (m_active && !m_pause->isChecked())
        m_timer->start();
    else
        m_timer->stop();
}

QList<int> HoldLineView::highScores()
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

void HoldLineView::gameOver()
{
    if (m_recorded)
        return;
    m_recorded = true;
    QList<int> table = highScores();
    const int score = m_field.score();
    m_rank = -1;
    if (score > 0) {
        int at = 0;
        while (at < table.size() && table[at] >= score)
            ++at;
        if (at < kTableSize) {
            table.insert(at, score);
            while (table.size() > kTableSize)
                table.removeLast();
            m_rank = at;
            QVariantList stored;
            for (int v : table)
                stored.append(v);
            QSettings().setValue(tableKey(), stored);
        }
    }
    m_table = highScores();
}

bool HoldLineView::advanceForShot(int turns)
{
    // One turn is one second of play. A plain pilot flies the cannon under
    // the nearest low attacker and keeps firing. Driven through the field
    // alone, so no sound plays and no effect is left in the air.
    const double frame = 1.0 / 30.0;
    for (int f = 0; f < turns * 30 && !m_field.over(); ++f) {
        double target = Field::kWidth / 2.0;
        double lowest = -1.0;
        for (const Field::Attacker& a : m_field.attackers())
            if (a.alive && (a.centre.y() > lowest + 1.0
                            || (std::abs(a.centre.y() - lowest) <= 1.0
                                && std::abs(a.centre.x() - m_field.cannonX())
                                       < std::abs(target - m_field.cannonX())))) {
                lowest = a.centre.y();
                target = a.centre.x();
            }
        const double gap = target - m_field.cannonX();
        m_field.setMove(std::abs(gap) < 4.0 ? 0 : (gap > 0 ? 1 : -1));
        m_field.fire();
        m_field.advance(frame);
    }
    m_field.setMove(0);
    m_field.takeEvents();
    m_particles.clear();
    m_rings.clear();
    m_popups.clear();
    m_shake = m_flash = m_muzzle = 0.0;
    if (m_field.over())
        gameOver();
    refresh();
    update();
    return true;
}

void HoldLineView::tick()
{
    const double dt = kFrameMs / 1000.0;
    m_time += dt;

    if (!m_field.over()) {
        m_field.setMove((m_rightHeld ? 1 : 0) - (m_leftHeld ? 1 : 0));
        // Holding the fire key fires again as soon as the last shot is spent.
        if (m_fireHeld && m_field.phase() == Field::Phase::Playing)
            m_field.fire();
        m_field.advance(dt);
    }
    readEvents();

    for (Particle& q : m_particles) {
        q.at += q.velocity * dt;
        q.velocity *= 0.985;
        q.velocity.ry() += 40.0 * dt;
        q.life -= dt;
    }
    std::erase_if(m_particles, [](const Particle& q) { return q.life <= 0.0; });
    for (Ring& r : m_rings)
        r.age += dt;
    std::erase_if(m_rings, [](const Ring& r) { return r.age >= r.maxAge; });
    for (Popup& u : m_popups) {
        u.age += dt;
        u.at.ry() -= 28.0 * dt;
    }
    std::erase_if(m_popups, [](const Popup& u) { return u.age >= 1.1; });

    m_shake = std::max(0.0, m_shake - dt * 2.0);
    m_flash = std::max(0.0, m_flash - dt * 3.0);
    m_muzzle = std::max(0.0, m_muzzle - dt * 9.0);
    m_bannerAge += dt;

    refresh();
    update();
}

void HoldLineView::readEvents()
{
    for (const Field::Event& e : m_field.takeEvents()) {
        switch (e.kind) {
        case Field::EventKind::Fired:
            Sound::instance().play(Sound::kZap);
            m_muzzle = 1.0;
            break;
        case Field::EventKind::AttackerHit: {
            Sound::instance().play(Sound::kPop);
            const QColor c = speciesColour(e.row);
            burst(e.at, c, 26, 120.0);
            burst(e.at, QColor(255, 255, 255), 8, 60.0);
            m_rings.push_back({ e.at, 0.0, 0.45, 30.0, c });
            m_popups.push_back({ e.at, QStringLiteral("+%1").arg(e.points), 0.0, c });
            break;
        }
        case Field::EventKind::CannonHit:
            Sound::instance().play(Sound::kBoom);
            burst(e.at, QColor(0xff, 0xc0, 0x60), 50, 170.0);
            burst(e.at, kShotColour, 24, 110.0);
            m_rings.push_back({ e.at, 0.0, 0.7, 70.0, QColor(0xff, 0x90, 0x40) });
            m_shake = 1.0;
            m_flash = 1.0;
            break;
        case Field::EventKind::ShelterHit:
            burst(e.at, kShelterColour, 7, 50.0);
            break;
        case Field::EventKind::BombBurst:
            burst(e.at, kBombColour, 10, 70.0);
            m_rings.push_back({ e.at, 0.0, 0.3, 14.0, kBombColour });
            break;
        case Field::EventKind::Marched:
            Sound::instance().play(Sound::kMarch);
            break;
        case Field::EventKind::WaveCleared:
            Sound::instance().play(Sound::kWin);
            m_banner = tr("Wave cleared!  Bonus %1").arg(e.points);
            m_bannerAge = 0.0;
            break;
        case Field::EventKind::WaveStarted:
            m_banner = tr("Wave %1").arg(m_field.wave());
            m_bannerAge = 0.0;
            break;
        case Field::EventKind::GameOver:
            Sound::instance().play(Sound::kLose);
            if (m_field.ending() == Field::Ending::Landed) {
                burst(e.at, QColor(0xff, 0xc0, 0x60), 50, 170.0);
                m_shake = 1.0;
                m_flash = 1.0;
            }
            gameOver();
            break;
        }
    }
}

void HoldLineView::burst(QPointF at, const QColor& colour, int count, double speed)
{
    for (int i = 0; i < count; ++i) {
        const double angle = unit(m_fx) * 2.0 * std::numbers::pi;
        const double v = speed * (0.25 + 0.75 * unit(m_fx));
        const double life = 0.35 + 0.5 * unit(m_fx);
        QColor c = colour;
        c = c.lighter(90 + int(unit(m_fx) * 60));
        m_particles.push_back({ at, QPointF(std::cos(angle) * v, std::sin(angle) * v), life, life,
                                1.2 + unit(m_fx) * 2.2, c });
    }
}

void HoldLineView::refresh()
{
    QString state;
    if (m_field.over())
        state = tr("Game over");
    else if (m_pause->isChecked())
        state = tr("Paused");
    else if (m_field.phase() == Field::Phase::Ready)
        state = tr("Press Space to begin");
    else
        state = tr("Wave %1").arg(m_field.wave());

    const QList<int>& table = m_table;
    const int best = std::max(table.isEmpty() ? 0 : table.first(), m_field.score());
    const QString status = tr("%1   Score %2   Lives %3   Best %4")
                               .arg(state)
                               .arg(m_field.score())
                               .arg(m_field.lives())
                               .arg(best);
    if (status != m_statusShown) {
        m_statusShown = status;
        Q_EMIT statusChanged(status);
    }
}

// ---- geometry ----

double HoldLineView::scale() const
{
    const double band = captionBand(QRectF(rect()));
    return std::max(0.2, std::min((width() - 16.0) / Field::kWidth,
                                  (height() - 16.0 - band) / Field::kHeight));
}

QRectF HoldLineView::fieldRect() const
{
    const double s = scale();
    const double w = Field::kWidth * s;
    const double h = Field::kHeight * s;
    const double band = captionBand(QRectF(rect()));
    return { (width() - w) / 2.0, (height() - band - h) / 2.0, w, h };
}

double HoldLineView::outlineWidth() const
{
    return Legibility::instance().enabled() ? 1.9 : 1.2;
}

QFont HoldLineView::fieldFont(double units, bool bold) const
{
    QFont f = font();
    f.setBold(bold);
    const double grow = Legibility::instance().enabled() ? 1.25 : 1.0;
    f.setPixelSize(std::max(10, int(std::lround(units * grow * scale()))));
    return f;
}

void HoldLineView::resizeEvent(QResizeEvent* event)
{
    GameView::resizeEvent(event);
    m_backdrop = QPixmap();
}

// ---- the backdrop: drawn once per size ----

void HoldLineView::buildBackdrop()
{
    const qreal dpr = devicePixelRatioF();
    m_backdrop = QPixmap(QSize(std::max(1, int(width() * dpr)), std::max(1, int(height() * dpr))));
    m_backdrop.setDevicePixelRatio(dpr);
    QPainter p(&m_backdrop);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF all(rect());
    QLinearGradient sky(all.topLeft(), all.bottomLeft());
    sky.setColorAt(0.0, QColor(0x05, 0x06, 0x14));
    sky.setColorAt(0.55, QColor(0x10, 0x0c, 0x2e));
    sky.setColorAt(0.85, QColor(0x2a, 0x12, 0x40));
    sky.setColorAt(1.0, QColor(0x3a, 0x16, 0x3a));
    p.fillRect(all, sky);

    // Nebulae: soft clouds of colour, never bright enough to hide a bomb.
    const auto cloud = [&](QPointF c, double r, QColor colour) {
        QRadialGradient g(c, r);
        colour.setAlpha(70);
        g.setColorAt(0.0, colour);
        colour.setAlpha(25);
        g.setColorAt(0.5, colour);
        colour.setAlpha(0);
        g.setColorAt(1.0, colour);
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(c, r, r);
    };
    const double w = all.width();
    const double h = all.height();
    cloud(QPointF(w * 0.18, h * 0.30), w * 0.42, QColor(0x6a, 0x2c, 0xb0));
    cloud(QPointF(w * 0.80, h * 0.55), w * 0.38, QColor(0x1c, 0x5c, 0xb8));
    cloud(QPointF(w * 0.45, h * 0.12), w * 0.30, QColor(0xb0, 0x30, 0x80));

    const QRectF f = fieldRect();
    const double s = scale();

    // A ringed planet low in the corner, far away and dim.
    {
        const QPointF c(f.right() - 70 * s, f.top() + 150 * s);
        const double r = 36 * s;
        p.save();
        p.translate(c);
        p.rotate(-18);
        QPen ring(QColor(0xd8, 0xb0, 0x90, 40), 4 * s);
        p.setPen(ring);
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(-r * 1.9, -r * 0.42, r * 3.8, r * 0.84), 180 * 16, 180 * 16);
        p.restore();
        QRadialGradient body(c - QPointF(r * 0.4, r * 0.4), r * 1.5);
        body.setColorAt(0.0, QColor(0xe0, 0xa0, 0x80, 90));
        body.setColorAt(0.6, QColor(0x8a, 0x4a, 0x60, 80));
        body.setColorAt(1.0, QColor(0x2a, 0x10, 0x30, 80));
        p.setPen(Qt::NoPen);
        p.setBrush(body);
        p.drawEllipse(c, r, r);
        p.save();
        p.translate(c);
        p.rotate(-18);
        p.setPen(ring);
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(-r * 1.9, -r * 0.42, r * 3.8, r * 0.84), 0, 180 * 16);
        p.restore();
    }

    // The city being held: a skyline along the horizon, lit windows and all.
    const double ground = f.top() + 514 * s;
    {
        // A fixed seed on purpose: the skyline is rebuilt on every resize and
        // must come back the same city, not a new one.
        // NOLINTNEXTLINE(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed)
        std::mt19937 rng(7);
        double x = all.left() - 10;
        QPainterPath skyline;
        skyline.moveTo(x, ground);
        std::vector<QRectF> towers;
        while (x < all.right() + 10) {
            const double bw = (14 + unit(rng) * 26) * s;
            const double bh = (24 + unit(rng) * 70) * s;
            towers.push_back(QRectF(x, ground - bh, bw, bh));
            x += bw + unit(rng) * 3 * s;
        }
        QLinearGradient tower(QPointF(0, ground - 100 * s), QPointF(0, ground));
        tower.setColorAt(0.0, QColor(0x1a, 0x16, 0x3a));
        tower.setColorAt(1.0, QColor(0x0c, 0x0a, 0x1e));
        p.setPen(Qt::NoPen);
        p.setBrush(tower);
        for (const QRectF& t : towers)
            p.drawRect(t);
        for (const QRectF& t : towers) {
            const double win = 2.2 * s;
            const int rows = int((t.height() - 8 * s) / (6 * s)) + 1;
            const int columns = int((t.width() - 6 * s) / (5 * s)) + 1;
            for (int row = 0; row < rows; ++row)
                for (int column = 0; column < columns; ++column)
                    if (unit(rng) < 0.28) {
                        const double wx = t.left() + 3 * s + column * 5 * s;
                        const double wy = t.top() + 4 * s + row * 6 * s;
                        const bool warm = unit(rng) < 0.7;
                        p.fillRect(QRectF(wx, wy, win, win),
                                   warm ? QColor(0xff, 0xd0, 0x80, 120) : QColor(0x90, 0xd8, 0xff, 110));
                    }
        }
        // A haze over the city, so it sits behind the play.
        QLinearGradient haze(QPointF(0, ground - 110 * s), QPointF(0, ground));
        haze.setColorAt(0.0, QColor(0x2a, 0x16, 0x40, 0));
        haze.setColorAt(1.0, QColor(0x50, 0x20, 0x50, 120));
        p.fillRect(QRectF(all.left(), ground - 110 * s, all.width(), 110 * s), haze);
    }

    // The ground the cannon rolls on, with a glowing rim.
    {
        QLinearGradient soil(QPointF(0, ground), QPointF(0, all.bottom()));
        soil.setColorAt(0.0, QColor(0x22, 0x1c, 0x3a));
        soil.setColorAt(1.0, QColor(0x08, 0x07, 0x12));
        p.fillRect(QRectF(all.left(), ground, all.width(), all.bottom() - ground), soil);
        QLinearGradient rim(QPointF(0, ground - 3 * s), QPointF(0, ground + 3 * s));
        rim.setColorAt(0.0, QColor(0x6a, 0xd8, 0xff, 0));
        rim.setColorAt(0.5, QColor(0x6a, 0xd8, 0xff, 200));
        rim.setColorAt(1.0, QColor(0x6a, 0xd8, 0xff, 0));
        p.fillRect(QRectF(all.left(), ground - 3 * s, all.width(), 6 * s), rim);
    }

    // The field's edges, as two faint light rails.
    {
        QPen rail(QColor(0x6a, 0xd8, 0xff, 50), std::max(1.0, 1.5 * s));
        p.setPen(rail);
        p.drawLine(QPointF(f.left(), f.top() + 44 * s), QPointF(f.left(), ground));
        p.drawLine(QPointF(f.right(), f.top() + 44 * s), QPointF(f.right(), ground));
    }
}

// ---- painting ----

void HoldLineView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_backdrop.isNull() || m_backdrop.size() != QSize(int(width() * devicePixelRatioF()),
                                                          int(height() * devicePixelRatioF())))
        buildBackdrop();

    p.save();
    if (m_shake > 0.0) {
        const double amount = 6.0 * m_shake * m_shake;
        p.translate(std::sin(m_time * 71.0) * amount, std::cos(m_time * 53.0) * amount);
    }
    p.drawPixmap(0, 0, m_backdrop);
    paintStars(p);

    const QRectF f = fieldRect();
    const double s = scale();
    p.save();
    p.translate(f.topLeft());
    p.scale(s, s);

    paintShelters(p);
    for (const Field::Attacker& a : m_field.attackers())
        if (a.alive)
            paintAttacker(p, a);
    paintShotsAndBombs(p);
    if (m_field.cannonShown())
        paintCannon(p, m_field.cannonX(), true);
    paintEffects(p);
    p.restore();
    p.restore();

    if (m_flash > 0.0)
        p.fillRect(rect(), QColor(255, 120, 60, int(90 * m_flash)));

    paintHud(p);
    paintOverlay(p);
    paintStatusCaption(p, QRectF(rect()));
}

void HoldLineView::paintStars(QPainter& p) const
{
    p.setPen(Qt::NoPen);
    const double w = width();
    const double h = height();
    for (const Star& star : m_stars) {
        // Each layer drifts down at its own speed, the near ones fastest.
        double y = star.at.y() + m_time * (0.004 + 0.02 * star.depth);
        y -= std::floor(y);
        const double twinkle = 0.55 + 0.45 * std::sin(m_time * (1.0 + star.depth * 2.0) + star.phase);
        const double r = 0.5 + star.depth * 1.3;
        QColor c = star.depth > 0.8 ? QColor(0xc8, 0xe8, 0xff) : QColor(0xff, 0xf4, 0xe0);
        c.setAlphaF(float(std::clamp((0.25 + 0.6 * star.depth) * twinkle, 0.0, 1.0)));
        p.setBrush(c);
        p.drawEllipse(QPointF(star.at.x() * w, y * h), r, r);
    }
}

void HoldLineView::paintShelters(QPainter& p) const
{
    for (const Field::Shelter& shelter : m_field.shelters()) {
        // A soft glow behind the whole shelter.
        const QRectF whole(shelter.topLeft,
                           QSizeF(Field::kShelterColumns * Field::kBlock,
                                  Field::kShelterRows * Field::kBlock));
        QRadialGradient glow(whole.center(), whole.width() * 0.9);
        glow.setColorAt(0.0, QColor(0x52, 0xf0, 0x9a, 50));
        glow.setColorAt(1.0, QColor(0x52, 0xf0, 0x9a, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawEllipse(whole.center(), whole.width() * 0.9, whole.height() * 1.1);

        // The blocks overlap a hair, with antialiasing off, so the scaled grid
        // shows no seams: the shelter reads as one piece of crystal.
        const auto strengthAt = [&shelter](int r, int c) {
            if (r < 0 || c < 0 || r >= Field::kShelterRows || c >= Field::kShelterColumns)
                return 0;
            return shelter.blocks[std::size_t(r)][std::size_t(c)];
        };
        p.save();
        p.setRenderHint(QPainter::Antialiasing, false);
        for (int r = 0; r < Field::kShelterRows; ++r)
            for (int c = 0; c < Field::kShelterColumns; ++c) {
                const int strength = strengthAt(r, c);
                if (strength <= 0)
                    continue;
                const QRectF b = Field::blockRect(shelter, r, c).adjusted(-0.2, -0.2, 0.2, 0.2);
                // Full strength is bright crystal, with a facet shading
                // across it; each hit darkens it.
                const double level = double(strength) / Field::kBlockStrength;
                const double facet = ((r + c) % 3) * 0.04;
                const QColor body = QColor::fromHsvF(0.42f, float(0.70 - 0.25 * level),
                                                     float(0.30 + 0.58 * level + facet));
                p.fillRect(b, body);
            }
        // A bright rim wherever a block's top or side is open to the sky,
        // which outlines the shelter's shape however worn it is.
        for (int r = 0; r < Field::kShelterRows; ++r)
            for (int c = 0; c < Field::kShelterColumns; ++c) {
                if (strengthAt(r, c) <= 0)
                    continue;
                const QRectF b = Field::blockRect(shelter, r, c);
                const QColor rim(0xc8, 0xff, 0xe0, 60 + 60 * strengthAt(r, c));
                if (strengthAt(r - 1, c) <= 0)
                    p.fillRect(QRectF(b.left(), b.top(), b.width(), 1.1), rim);
                if (strengthAt(r, c - 1) <= 0)
                    p.fillRect(QRectF(b.left(), b.top(), 0.9, b.height()), rim.darker(115));
                if (strengthAt(r, c + 1) <= 0)
                    p.fillRect(QRectF(b.right() - 0.9, b.top(), 0.9, b.height()), QColor(0, 40, 30, 140));
                if (strengthAt(r, c) == 1)
                    p.fillRect(QRectF(b.left() + 1.0, b.top() + 1.5, b.width() - 2.0, 0.8), QColor(0, 0, 0, 110));
            }
        p.restore();
    }
}

void HoldLineView::paintAttacker(QPainter& p, const Field::Attacker& a) const
{
    const bool pose = m_field.marchCount() % 2 == 1;
    const QColor colour = speciesColour(a.row);
    const double breathe = 0.5 + 0.5 * std::sin(m_time * 3.0 + a.centre.x() * 0.05);

    p.save();
    p.translate(a.centre);

    // A glow round every attacker, so it reads against the dark at a glance.
    {
        QRadialGradient g(QPointF(0, 0), 24);
        QColor c = colour;
        c.setAlpha(int(55 + 30 * breathe));
        g.setColorAt(0.0, c);
        c.setAlpha(0);
        g.setColorAt(1.0, c);
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(QPointF(0, 0), 24, 20);
    }

    const QPen outline(kOutline, outlineWidth(), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    if (a.row == 0) {
        // The wisp: a jellyfish's bell with four trailing tendrils.
        p.setPen(QPen(colour.lighter(115), 2.0, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        for (int i = 0; i < 4; ++i) {
            const double x = -7.5 + i * 5.0;
            const double sway = (pose ? 2.5 : -2.5) * ((i % 2) ? 1 : -1);
            QPainterPath t;
            t.moveTo(x, 2);
            t.cubicTo(x + sway, 6, x - sway, 9, x + sway * 0.6, 12);
            p.drawPath(t);
        }
        QPainterPath bell;
        bell.moveTo(-13, 3);
        bell.cubicTo(-13, -14, 13, -14, 13, 3);
        // A scalloped hem.
        for (int i = 0; i < 4; ++i) {
            const double x0 = 13 - i * 6.5;
            bell.quadTo(x0 - 3.25, pose ? 7 : 5.5, x0 - 6.5, 3);
        }
        bell.closeSubpath();
        QRadialGradient fill(QPointF(-4, -7), 18);
        fill.setColorAt(0.0, colour.lighter(170));
        fill.setColorAt(0.5, colour);
        fill.setColorAt(1.0, colour.darker(200));
        p.setPen(outline);
        p.setBrush(fill);
        p.drawPath(bell);
        // A glowing heart inside the bell.
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, int(120 + 100 * breathe)));
        p.drawEllipse(QPointF(0, -5), 2.6, 2.6);
        // Eyes.
        p.setBrush(Qt::white);
        p.drawEllipse(QPointF(-5, -1), 2.4, 2.6);
        p.drawEllipse(QPointF(5, -1), 2.4, 2.6);
        p.setBrush(kOutline);
        p.drawEllipse(QPointF(-5, -0.4), 1.2, 1.4);
        p.drawEllipse(QPointF(5, -0.4), 1.2, 1.4);
    } else if (a.row <= 2) {
        // The manta: wide wings that beat with the march, and a long tail.
        const double tip = pose ? -6.0 : 3.0;
        QPainterPath tail;
        tail.moveTo(0, 6);
        tail.cubicTo(1.5, 10, pose ? 4 : -4, 12, 0, 15);
        p.setPen(QPen(colour.darker(130), 1.8, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawPath(tail);

        QPainterPath body;
        body.moveTo(0, -10);
        body.cubicTo(6, -8, 10, -3, 15, tip);
        body.cubicTo(10, 3, 5, 5, 0, 7);
        body.cubicTo(-5, 5, -10, 3, -15, tip);
        body.cubicTo(-10, -3, -6, -8, 0, -10);
        QLinearGradient fill(QPointF(0, -10), QPointF(0, 7));
        fill.setColorAt(0.0, colour.lighter(160));
        fill.setColorAt(0.6, colour);
        fill.setColorAt(1.0, colour.darker(220));
        p.setPen(outline);
        p.setBrush(fill);
        p.drawPath(body);
        // A ridge down the back, and wing veins.
        p.setPen(QPen(colour.lighter(180), 1.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(0, -8), QPointF(0, 4));
        p.setPen(QPen(colour.darker(160), 0.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(-3, -3), QPointF(-11, tip * 0.6 - 1));
        p.drawLine(QPointF(3, -3), QPointF(11, tip * 0.6 - 1));
        // A visor of an eye, glowing.
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xff, 0xf0, 0x6a, int(180 + 70 * breathe)));
        QPainterPath visor;
        visor.moveTo(-5, -4);
        visor.quadTo(0, -1, 5, -4);
        visor.quadTo(0, -2.5, -5, -4);
        p.drawPath(visor);
        p.drawEllipse(QPointF(0, -3), 2.0, 1.4);
    } else {
        // The pod: an armoured sphere on two thrusters, its one eye on you.
        const double flame = (pose ? 1.0 : 0.65) * (0.8 + 0.2 * breathe);
        for (int side : { -1, 1 }) {
            QPainterPath jet;
            jet.moveTo(side * 6 - 2.5, 8);
            jet.quadTo(side * 6, 8 + 9 * flame, side * 6 + 2.5, 8);
            QLinearGradient fire(QPointF(0, 8), QPointF(0, 8 + 9 * flame));
            fire.setColorAt(0.0, QColor(0xff, 0xf0, 0xa0));
            fire.setColorAt(1.0, QColor(0xff, 0x50, 0x20, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(fire);
            p.drawPath(jet);
            p.setPen(outline);
            p.setBrush(QColor(0x5a, 0x4a, 0x60));
            p.drawRoundedRect(QRectF(side * 6 - 3, 5, 6, 4), 1.2, 1.2);
        }
        QRadialGradient fill(QPointF(-4, -5), 16);
        fill.setColorAt(0.0, colour.lighter(160));
        fill.setColorAt(0.55, colour);
        fill.setColorAt(1.0, colour.darker(260));
        p.setPen(outline);
        p.setBrush(fill);
        p.drawEllipse(QPointF(0, -1), 12, 10);
        // Armour bands.
        p.setPen(QPen(colour.darker(200), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(-12, -11, 24, 20), 200 * 16, 140 * 16);
        p.drawArc(QRectF(-9, -11, 18, 20), 30 * 16, 120 * 16);
        // The eye, which turns toward the cannon.
        const double look = std::clamp((m_field.cannonX() - a.centre.x()) / 120.0, -1.0, 1.0);
        p.setPen(QPen(kOutline, 0.9));
        p.setBrush(QColor(0xfa, 0xf6, 0xf0));
        p.drawEllipse(QPointF(0, -1), 5.4, 4.6);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xe0, 0x20, 0x30));
        p.drawEllipse(QPointF(look * 2.2, 0.2), 2.8, 2.8);
        p.setBrush(kOutline);
        p.drawEllipse(QPointF(look * 2.4, 0.4), 1.3, 1.3);
        p.setBrush(QColor(255, 255, 255, 220));
        p.drawEllipse(QPointF(look * 2.2 - 1.0, -0.9), 0.7, 0.7);
    }
    p.restore();
}

void HoldLineView::paintCannon(QPainter& p, double x, bool live) const
{
    p.save();
    p.translate(x, Field::kCannonY);

    const QPen outline(kOutline, outlineWidth(), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    // Shadow on the ground.
    QRadialGradient shadow(QPointF(0, 14), 26);
    shadow.setColorAt(0.0, QColor(0, 0, 0, 120));
    shadow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(shadow);
    p.drawEllipse(QPointF(0, 13), 28, 5);

    // Treads, whose links run when the cannon moves.
    p.setPen(outline);
    p.setBrush(QColor(0x30, 0x34, 0x44));
    p.drawRoundedRect(QRectF(-22, 4, 44, 9), 4.5, 4.5);
    p.setPen(QPen(QColor(0x80, 0x88, 0xa0), 1.0));
    const double roll = std::fmod(m_field.cannonX() * 0.6, 5.0);
    for (int link = 0; link < 8; ++link) {
        const double t = -19 + roll + link * 5;
        if (t < 19)
            p.drawLine(QPointF(t, 5.5), QPointF(t, 11.5));
    }

    // The hull: brushed steel.
    QPainterPath hull;
    hull.moveTo(-20, 5);
    hull.lineTo(-15, -4);
    hull.lineTo(15, -4);
    hull.lineTo(20, 5);
    hull.closeSubpath();
    QLinearGradient steel(QPointF(0, -4), QPointF(0, 5));
    steel.setColorAt(0.0, QColor(0xe4, 0xec, 0xf4));
    steel.setColorAt(0.45, QColor(0x8c, 0x98, 0xb0));
    steel.setColorAt(1.0, QColor(0x46, 0x50, 0x68));
    p.setPen(outline);
    p.setBrush(steel);
    p.drawPath(hull);

    // The barrel.
    QLinearGradient barrel(QPointF(-3, 0), QPointF(3, 0));
    barrel.setColorAt(0.0, QColor(0x60, 0x6a, 0x80));
    barrel.setColorAt(0.5, QColor(0xf0, 0xf4, 0xfa));
    barrel.setColorAt(1.0, QColor(0x50, 0x58, 0x6c));
    p.setBrush(barrel);
    p.drawRect(QRectF(-2.8, -19, 5.6, 12));
    p.drawRect(QRectF(-3.8, -20, 7.6, 3));

    // The turret dome, with a glowing energy core.
    QRadialGradient dome(QPointF(-3, -9), 12);
    dome.setColorAt(0.0, QColor(0xf4, 0xf8, 0xff));
    dome.setColorAt(1.0, QColor(0x5a, 0x66, 0x80));
    p.setBrush(dome);
    p.drawChord(QRectF(-10, -12, 20, 16), 0, 180 * 16);
    const double pulse = 0.6 + 0.4 * std::sin(m_time * 5.0);
    QRadialGradient core(QPointF(0, -6), 5);
    core.setColorAt(0.0, QColor(255, 255, 255));
    core.setColorAt(0.5, QColor(kShotColour.red(), kShotColour.green(), kShotColour.blue(),
                                int(200 * pulse)));
    core.setColorAt(1.0, QColor(kShotColour.red(), kShotColour.green(), kShotColour.blue(), 0));
    p.setPen(Qt::NoPen);
    p.setBrush(core);
    p.drawEllipse(QPointF(0, -6), 5, 5);

    // A flash at the muzzle as it fires.
    if (live && m_muzzle > 0.0) {
        QRadialGradient flash(QPointF(0, -22), 12);
        flash.setColorAt(0.0, QColor(255, 255, 255, int(255 * m_muzzle)));
        flash.setColorAt(0.4, QColor(kShotColour.red(), kShotColour.green(), kShotColour.blue(),
                                     int(180 * m_muzzle)));
        flash.setColorAt(1.0, QColor(kShotColour.red(), kShotColour.green(), kShotColour.blue(), 0));
        p.setBrush(flash);
        p.drawEllipse(QPointF(0, -22), 12 * m_muzzle + 4, 12 * m_muzzle + 4);
    }
    p.restore();
}

void HoldLineView::paintShotsAndBombs(QPainter& p) const
{
    if (m_field.shotInFlight()) {
        const QPointF c = m_field.shot();
        const double half = Field::kShotHalfLength;
        // A trail, then the glow, then the white-hot core.
        QLinearGradient trail(QPointF(c.x(), c.y() + half), QPointF(c.x(), c.y() + half + 24));
        trail.setColorAt(0.0, QColor(kShotColour.red(), kShotColour.green(), kShotColour.blue(), 140));
        trail.setColorAt(1.0, QColor(kShotColour.red(), kShotColour.green(), kShotColour.blue(), 0));
        p.setPen(QPen(QBrush(trail), 3.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(c.x(), c.y() + half), QPointF(c.x(), c.y() + half + 24));
        p.setPen(QPen(QColor(kShotColour.red(), kShotColour.green(), kShotColour.blue(), 90), 11.0,
                      Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(c.x(), c.y() - half), QPointF(c.x(), c.y() + half));
        p.setPen(QPen(QColor(0xf0, 0xff, 0xff), 4.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(c.x(), c.y() - half), QPointF(c.x(), c.y() + half));
    }

    for (const Field::Bomb& b : m_field.bombs()) {
        const QPointF c = b.centre;
        const double pulse = 0.8 + 0.2 * std::sin(m_time * 18.0 + c.x());
        // A crackling tail above the bomb.
        QPainterPath tail;
        tail.moveTo(c.x(), c.y() - 6);
        for (int i = 1; i <= 4; ++i)
            tail.lineTo(c.x() + ((i % 2) ? 3.0 : -3.0) * pulse, c.y() - 6 - i * 5);
        p.setPen(QPen(QColor(0xff, 0x90, 0x40, 170), 2.2, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawPath(tail);

        QRadialGradient glow(c, 17 * pulse);
        glow.setColorAt(0.0, QColor(0xff, 0x80, 0x30, 200));
        glow.setColorAt(1.0, QColor(0xff, 0x30, 0x10, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawEllipse(c, 17 * pulse, 17 * pulse);
        QRadialGradient orb(c - QPointF(2.0, 2.5), 10);
        orb.setColorAt(0.0, QColor(0xff, 0xff, 0xd0));
        orb.setColorAt(0.45, QColor(0xff, 0xb0, 0x40));
        orb.setColorAt(1.0, kBombColour.darker(140));
        p.setPen(QPen(kOutline, outlineWidth() * 0.8));
        p.setBrush(orb);
        // Drawn a little larger than the box that hits, which errs the player's way.
        p.drawEllipse(c, Field::kBombHalfWidth + 2.0, Field::kBombHalfLength + 1.0);
    }
}

void HoldLineView::paintEffects(QPainter& p) const
{
    p.setPen(Qt::NoPen);
    for (const Particle& q : m_particles) {
        QColor c = q.colour;
        const double share = q.life / q.maxLife;
        c.setAlphaF(float(std::clamp(share, 0.0, 1.0)));
        p.setBrush(c);
        p.drawEllipse(q.at, q.size * (0.5 + 0.5 * share), q.size * (0.5 + 0.5 * share));
    }
    p.setBrush(Qt::NoBrush);
    for (const Ring& r : m_rings) {
        const double t = r.age / r.maxAge;
        QColor c = r.colour;
        c.setAlphaF(float(0.8 * (1.0 - t)));
        p.setPen(QPen(c, 3.0 * (1.0 - t) + 0.5));
        p.drawEllipse(r.at, r.radius * t, r.radius * t);
    }
    // The points an attacker was worth, rising from where it was.
    QFont f = font();
    f.setBold(true);
    f.setPixelSize(Legibility::instance().enabled() ? 16 : 12);
    p.setFont(f);
    for (const Popup& u : m_popups) {
        QColor c = u.colour.lighter(130);
        c.setAlphaF(float(std::clamp(1.2 - u.age, 0.0, 1.0)));
        const QRectF box(u.at.x() - 30, u.at.y() - 10, 60, 20);
        p.setPen(QColor(0, 0, 0, int(c.alpha() * 0.8)));
        p.drawText(box.translated(0.8, 0.8), Qt::AlignCenter, u.text);
        p.setPen(c);
        p.drawText(box, Qt::AlignCenter, u.text);
    }
}

void HoldLineView::paintHud(QPainter& p) const
{
    const QRectF f = fieldRect();
    const double s = scale();
    const QRectF strip(f.left() + 10 * s, f.top() + 6 * s, f.width() - 20 * s, 30 * s);

    const QList<int>& table = m_table;
    const int best = std::max(table.isEmpty() ? 0 : table.first(), m_field.score());

    const auto shadowed = [&](const QRectF& r, int align, const QString& text, const QColor& c) {
        p.setPen(QColor(0, 0, 0, 200));
        p.drawText(r.translated(1.5, 1.5), align, text);
        p.setPen(c);
        p.drawText(r, align, text);
    };

    p.setFont(fieldFont(17));
    shadowed(strip, Qt::AlignLeft | Qt::AlignVCenter, tr("Score %1").arg(m_field.score()),
             QColor(0xf4, 0xf8, 0xff));
    shadowed(strip, Qt::AlignHCenter | Qt::AlignVCenter, tr("Best %1").arg(best), kGold);

    // Lives as little cannons, with the wave beside them.
    const int lives = m_field.lives();
    const double icon = 24 * s;
    double x = strip.right() - icon * 0.5;
    p.save();
    for (int i = 0; i < lives; ++i) {
        p.save();
        p.translate(x, strip.center().y() + 2 * s);
        p.scale(s * 0.55, s * 0.55);
        p.translate(0, -Field::kCannonY);
        paintCannon(p, 0.0, false);
        p.restore();
        x -= icon;
    }
    p.restore();
    p.setFont(fieldFont(17));
    const QRectF waveBox(strip.left(), strip.top(), x - strip.left() - 4 * s, strip.height());
    shadowed(waveBox, Qt::AlignRight | Qt::AlignVCenter, tr("Wave %1").arg(m_field.wave()),
             QColor(0x9c, 0xf6, 0xff));
}

void HoldLineView::paintPanel(QPainter& p, const QRectF& r) const
{
    QLinearGradient fill(r.topLeft(), r.bottomLeft());
    fill.setColorAt(0.0, QColor(0x12, 0x18, 0x3a, 235));
    fill.setColorAt(1.0, QColor(0x08, 0x0a, 0x1c, 235));
    // A glow outside, then a crisp edge.
    for (int i = 3; i >= 1; --i) {
        p.setPen(QPen(QColor(kPanelEdge.red(), kPanelEdge.green(), kPanelEdge.blue(), 30), i * 4.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, 14, 14);
    }
    p.setPen(QPen(kPanelEdge, 2.0));
    p.setBrush(fill);
    p.drawRoundedRect(r, 14, 14);
}

void HoldLineView::paintOverlay(QPainter& p) const
{
    const QRectF f = fieldRect();
    const double s = scale();
    const bool legible = Legibility::instance().enabled();

    const auto line = [&](const QRectF& r, const QString& text, double size, const QColor& c,
                          bool bold = true) {
        p.setFont(fieldFont(size, bold));
        p.setPen(QColor(0, 0, 0, 220));
        p.drawText(r.translated(1.5, 1.5), Qt::AlignCenter | Qt::TextWordWrap, text);
        p.setPen(c);
        p.drawText(r, Qt::AlignCenter | Qt::TextWordWrap, text);
    };

    // A wave banner, fading in and out over the field.
    if (m_bannerAge < 2.0 && !m_banner.isEmpty() && !m_field.over()) {
        const double fade = std::clamp(std::min(m_bannerAge * 4.0, (2.0 - m_bannerAge) * 2.0), 0.0, 1.0);
        p.save();
        p.setOpacity(fade);
        const QRectF r(f.left(), f.top() + 250 * s, f.width(), 50 * s);
        line(r, m_banner, 30, kGold);
        p.restore();
    }

    const double panelW = std::min(f.width() - 30 * s, 380 * s);
    const auto panelAt = [&](double heightUnits) {
        const double h = heightUnits * s * (legible ? 1.2 : 1.0);
        return QRectF(f.center().x() - panelW / 2.0, f.top() + 300 * s - h / 2.0, panelW, h);
    };

    const QColor ink(0xe8, 0xf0, 0xff);
    if (m_field.over()) {
        const QList<int>& table = m_table;
        const QRectF panel = panelAt(130 + 26 * double(std::max<qsizetype>(1, table.size())));
        paintPanel(p, panel);
        double y = panel.top() + 14 * s;
        const double rowH = (legible ? 30 : 26) * s;
        const QString title = m_field.ending() == Field::Ending::Landed
                                  ? tr("They landed. The line is broken.")
                                  : tr("Out of cannons. The line is broken.");
        line(QRectF(panel.left() + 10 * s, y, panel.width() - 20 * s, rowH * 1.6), title, 19,
             QColor(0xff, 0x8a, 0x75));
        y += rowH * 1.7;
        line(QRectF(panel.left(), y, panel.width(), rowH),
             m_rank == 0 ? tr("Score %1 — a new best!").arg(m_field.score())
                         : tr("Score %1").arg(m_field.score()),
             18, ink);
        y += rowH * 1.2;
        for (int i = 0; i < table.size(); ++i) {
            const bool mine = i == m_rank;
            const QRectF row(panel.left() + 40 * s, y, panel.width() - 80 * s, rowH);
            if (mine) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(kGold.red(), kGold.green(), kGold.blue(), 60));
                p.drawRoundedRect(row.adjusted(-8 * s, 1, 8 * s, -1), 6, 6);
            }
            p.setFont(fieldFont(17, mine));
            p.setPen(mine ? kGold : ink);
            p.drawText(row, Qt::AlignLeft | Qt::AlignVCenter, tr("%1.").arg(i + 1));
            p.drawText(row, Qt::AlignRight | Qt::AlignVCenter, QString::number(table[i]));
            y += rowH;
        }
        y += rowH * 0.3;
        line(QRectF(panel.left(), y, panel.width(), rowH), tr("Space or a click plays again"), 15,
             QColor(0x9c, 0xf6, 0xff), false);
        return;
    }

    if (m_pause->isChecked()) {
        const QRectF panel = panelAt(90);
        paintPanel(p, panel);
        line(panel.adjusted(0, 10 * s, 0, -panel.height() / 2), tr("Paused"), 26, kGold);
        line(panel.adjusted(0, panel.height() / 2, 0, -10 * s), tr("Press P to carry on"), 16, ink,
             false);
        return;
    }

    if (m_field.phase() == Field::Phase::Ready) {
        const QRectF panel = panelAt(170);
        paintPanel(p, panel);
        const double rowH = panel.height() / 6.0;
        double y = panel.top() + rowH * 0.3;
        line(QRectF(panel.left(), y, panel.width(), rowH * 1.4), tr("Hold the Line"), 30, kGold);
        y += rowH * 1.5;
        line(QRectF(panel.left(), y, panel.width(), rowH), tr("Left and Right move the cannon"), 16,
             ink, false);
        y += rowH;
        line(QRectF(panel.left(), y, panel.width(), rowH), tr("Space fires.  P pauses."), 16, ink,
             false);
        y += rowH;
        line(QRectF(panel.left(), y, panel.width(), rowH), tr("Press Space to begin"), 18,
             QColor(0x9c, 0xf6, 0xff));
        return;
    }

    if (m_field.phase() == Field::Phase::Respawning) {
        const QRectF r(f.left(), f.top() + 280 * s, f.width(), 50 * s);
        line(r, m_field.lives() == 1 ? tr("Cannon lost. This is your last one.")
                                     : tr("Cannon lost. %1 left.").arg(m_field.lives()),
             22, QColor(0xff, 0xb0, 0x80));
    }
}

// ---- input ----

void HoldLineView::keyPressEvent(QKeyEvent* event)
{
    if (m_pause->isChecked()) {
        GameView::keyPressEvent(event);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_A:
        m_leftHeld = true;
        break;
    case Qt::Key_Right:
    case Qt::Key_D:
        m_rightHeld = true;
        break;
    case Qt::Key_Space:
    case Qt::Key_Up:
    case Qt::Key_W:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (event->isAutoRepeat())
            break;
        if (m_field.over()) {
            newGame();
            break;
        }
        m_fireHeld = true;
        m_field.fire();
        break;
    default:
        GameView::keyPressEvent(event);
        return;
    }
    m_field.setMove((m_rightHeld ? 1 : 0) - (m_leftHeld ? 1 : 0));
    readEvents();
    update();
}

void HoldLineView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) {
        GameView::keyReleaseEvent(event);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_A:
        m_leftHeld = false;
        break;
    case Qt::Key_Right:
    case Qt::Key_D:
        m_rightHeld = false;
        break;
    case Qt::Key_Space:
    case Qt::Key_Up:
    case Qt::Key_W:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        m_fireHeld = false;
        break;
    default:
        GameView::keyReleaseEvent(event);
        return;
    }
    m_field.setMove((m_rightHeld ? 1 : 0) - (m_leftHeld ? 1 : 0));
}

void HoldLineView::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    if (m_pause->isChecked() || event->button() != Qt::LeftButton)
        return;
    if (m_field.over()) {
        newGame();
        return;
    }
    m_field.fire();
    readEvents();
    update();
}

void HoldLineView::focusOutEvent(QFocusEvent* event)
{
    // A key let go of elsewhere never comes back here, so it would stick.
    m_leftHeld = m_rightHeld = m_fireHeld = false;
    m_field.setMove(0);
    GameView::focusOutEvent(event);
}

// How to play (GHUB-0016). Written for this collection from what this file
// and holdlinefield.cpp do: points are kRowPoints and kWaveBonusPer, the keys
// are keyPressEvent() and buildActions().
QString HoldLineView::rulesText() const
{
    return tr(
        "<h3>The aim</h3>"
        "<p>A wave of attackers marches down towards the city. Shoot them all "
        "before they land, and score as much as you can.</p>"

        "<h3>How it plays</h3>"
        "<ul>"
        "<li>The game waits until you fire or move.</li>"
        "<li>The wave steps sideways together. At the edge it steps down and turns back.</li>"
        "<li>It starts slowly and moves faster as fewer attackers are left.</li>"
        "<li>You have one shot in the air at a time. Fire again once it hits or leaves.</li>"
        "<li>Attackers drop bombs. Your shot can knock a bomb out of the air.</li>"
        "<li>Four shelters stand in front of you. Bombs and your own shots wear them away, "
        "and the damage lasts the whole game.</li>"
        "<li>A bomb that hits the cannon costs one of your three cannons.</li>"
        "<li>Clear a wave for a bonus of 50 points times the wave number. "
        "The next wave starts a little lower and a little faster.</li>"
        "<li>The game ends when your last cannon is hit, "
        "or when the wave reaches the ground.</li>"
        "</ul>"

        "<h3>Scoring</h3>"
        "<ul>"
        "<li>Glowing jellyfish, on the top row: 30 points.</li>"
        "<li>Winged mantas, on the next two rows: 20 points.</li>"
        "<li>Armoured pods, on the bottom two rows: 10 points.</li>"
        "<li>Your five best scores are kept, and shown when a game ends.</li>"
        "</ul>"

        "<h3>Controls</h3>"
        "<ul>"
        "<li>Left and Right arrows, or A and D: move the cannon.</li>"
        "<li>Space, Up, W or Enter: fire. Hold it down to keep firing.</li>"
        "<li>A click on the field also fires.</li>"
        "<li>P, or the Pause button: pause, and press again to carry on.</li>"
        "<li>When a game ends, Space or a click starts the next.</li>"
        "<li>New Game button, or Ctrl+N: start again.</li>"
        "<li>If you leave for another game, this one pauses until you come back.</li>"
        "</ul>");
}
