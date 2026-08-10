#include "pinballview.h"

#include "scores.h"
#include "sound.h"
#include "theme.h"

#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QLinearGradient>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kFrameMs = 16;
constexpr QColor kPlayfield { 0x1b, 0x21, 0x3d };
constexpr QColor kWallInk { 0x7d, 0x8a, 0xa8 };
constexpr QColor kAccent { 0x4f, 0xc3, 0xf7 };
}

PinballView::PinballView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(minimumSizeHint());
    setFocusPolicy(Qt::StrongFocus);

    m_timer = new QTimer(this);
    m_timer->setInterval(kFrameMs);
    connect(m_timer, &QTimer::timeout, this, &PinballView::tick);

    buildActions();
    buildLamps();
    newGame();
}

void PinballView::buildActions()
{
    auto* newAction = new QAction(QStringLiteral("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &PinballView::newGame);
    m_actions.append(newAction);

    auto* launch = new QAction(QStringLiteral("Launch"), this);
    connect(launch, &QAction::triggered, this, [this] {
        m_table.launch();
        setFocus();
    });
    m_actions.append(launch);
}

void PinballView::newGame()
{
    m_table.newGame();
    m_lastScore = 0;
    m_celebrate = 0.0;
    m_announced = false;
    m_plungerHeld = false;
    m_timer->start();
    setFocus();
    update();
    refresh();
}

void PinballView::activate()
{
    setFocus();
    if (!m_table.gameOver())
        m_timer->start();
    refresh();
}

void PinballView::tick()
{
    const double frame = kFrameMs / 1000.0;

    if (m_plungerHeld)
        m_table.chargePlunger(frame);

    m_table.advance(frame);

    if (m_table.takeBumperHits() > 0)
        Sound::instance().play(Sound::kBumper);
    if (m_table.takeSlingHits() > 0)
        Sound::instance().play(Sound::kSlingshot);

    m_animSeconds += frame;
    if (m_table.score() != m_lastScore) {
        m_lastScore = m_table.score();
        m_celebrate = 1.0;
    }
    m_celebrate = std::max(0.0, m_celebrate - frame * 2.2);

    if (m_table.takeBallLost()) {
        Sound::instance().play(Sound::kDrain);
        refresh();
    }

    if (m_table.gameOver()) {
        m_timer->stop();
        announceGameOver();
    }

    refresh();
    update();
}

void PinballView::announceGameOver()
{
    if (m_announced)
        return;
    m_announced = true;

    Sound::instance().play(Sound::kLose);
    const bool newBest = Scores::instance().recordHigh(Scores::pinballBestScore(), m_table.score());

    QTimer::singleShot(150, this, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("Game over"));
        box.setText(QStringLiteral("Out of balls."));
        box.setInformativeText(
            newBest ? QStringLiteral("Score: %1 — a new best!").arg(m_table.score())
                    : QStringLiteral("Score: %1   Best: %2")
                          .arg(m_table.score())
                          .arg(Scores::instance().best(Scores::pinballBestScore())));
        QAbstractButton* again = box.addButton(QStringLiteral("Play Again"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Close"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == again)
            newGame();
    });
}

void PinballView::refresh()
{
    QString hint;
    if (m_table.gameOver())
        hint = QStringLiteral("Game over — New Game to play again");
    else if (m_table.ballInLane())
        hint = QStringLiteral("Hold Space to charge, release to launch");
    else
        hint = QStringLiteral("Z / M or ← → for the flippers");

    Q_EMIT statusChanged(QStringLiteral("Score %1   Balls %2   Best %3   %4")
                             .arg(m_table.score())
                             .arg(std::max(0, m_table.ballsLeft()))
                             .arg(std::max(Scores::instance().best(Scores::pinballBestScore()),
                                           m_table.score()))
                             .arg(hint));
}

double PinballView::scale() const
{
    // The +24 leaves room for the wooden rails drawn outside the playfield.
    return std::min(width() / (PinballTable::kWidth + 24.0),
                    height() / (PinballTable::kHeight + 24.0));
}

QPointF PinballView::toScreen(QPointF p) const
{
    const double s = scale();
    return { (width() - PinballTable::kWidth * s) / 2 + p.x() * s,
             (height() - PinballTable::kHeight * s) / 2 + p.y() * s };
}

// A flipper is a tapered bat, not a plain bar: wide at the pivot, narrow at
// the tip, which is what makes it read as a real flipper.
void PinballView::paintFlipper(QPainter& p, const PinballTable::Flipper& f) const
{
    const double s = scale();
    const QPointF pivot = toScreen(f.pivot);
    const QPointF tip = toScreen(m_table.flipperTip(f));

    QPointF axis = tip - pivot;
    const double len = std::hypot(axis.x(), axis.y());
    if (len < 1e-6)
        return;
    axis /= len;
    const QPointF normal(-axis.y(), axis.x());

    const double wide = 8.0 * s;
    const double narrow = 4.2 * s;

    QPainterPath bat;
    bat.moveTo(pivot + normal * wide);
    bat.lineTo(tip + normal * narrow);
    bat.lineTo(tip - normal * narrow);
    bat.lineTo(pivot - normal * wide);
    bat.closeSubpath();

    QLinearGradient g(pivot + normal * wide, pivot - normal * wide);
    g.setColorAt(0.0, QColor(0xf4, 0xf8, 0xff));
    g.setColorAt(0.5, QColor(0xc2, 0xcd, 0xdd));
    g.setColorAt(1.0, QColor(0x82, 0x8f, 0xa2));

    p.setBrush(g);
    p.setPen(QPen(QColor(0x33, 0x3b, 0x4a), std::max(1.0, s)));
    p.drawPath(bat);
    // Rounded ends, drawn over the taper so the bat has no sharp corners.
    p.drawEllipse(tip, narrow, narrow);
    p.drawEllipse(pivot, wide, wide);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x4a, 0x54, 0x66));
    p.drawEllipse(pivot, 4.0 * s, 4.0 * s);
    p.setBrush(QColor(0x9d, 0xaa, 0xbd));
    p.drawEllipse(pivot, 2.0 * s, 2.0 * s);
}

// Painted decoration under the ball: arcs, lane guides and a shooter stripe.
// None of it is collidable — it exists so the table is not bare board.
void PinballView::paintPlayfieldArt(QPainter& p) const
{
    const double s = scale();

    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 3; ++i) {
        const double radius = (74 + i * 24) * s;
        const QPointF centre = toScreen({ 200, 300 });
        p.setPen(QPen(QColor(0x4f, 0xc3, 0xf7, 30 - i * 7), std::max(1.0, 2.0 * s)));
        p.drawArc(QRectF(centre.x() - radius, centre.y() - radius, radius * 2, radius * 2),
                  200 * 16, 140 * 16);
    }

    // Shooter-lane stripe.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xff, 0xff, 0xff, 12));
    p.drawRect(QRectF(toScreen({ 356, 210 }), QSizeF(30 * s, 480 * s)));

    // The drain: a soft glow rather than a drawn object, so it reads as a
    // warning underfoot instead of scenery on the table.
    // A radial falloff, so the warning has no edges of its own to look like
    // an object painted on the board.
    const QPointF mouth = toScreen({ 200, 712 });
    QRadialGradient fade(mouth, 96 * s);
    fade.setColorAt(0.0, QColor(0xe8, 0x51, 0x4f, 96));
    fade.setColorAt(0.6, QColor(0xe8, 0x51, 0x4f, 40));
    fade.setColorAt(1.0, QColor(0xe8, 0x51, 0x4f, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(fade);
    p.drawEllipse(mouth, 96 * s, 62 * s);
}

void PinballView::buildLamps()
{
    m_lamps.clear();

    // An arch of lamps following the dome, chasing left to right.
    for (int i = 0; i < 9; ++i) {
        const double a = M_PI + (i + 0.5) * (M_PI / 9.0);
        const double radius = PinballTable::kDomeRadius - 26.0;
        m_lamps.push_back({ QPointF(200.0 + std::cos(a) * radius,
                                    PinballTable::kDomeCentreY + std::sin(a) * radius),
                            QColor(0x4f, 0xc3, 0xf7), i, 6.5 });
    }

    // A triangle of lamps in among the bumpers.
    m_lamps.push_back({ { 200, 250 }, QColor(0xff, 0xd5, 0x4f), 0, 7.0 });
    m_lamps.push_back({ { 158, 345 }, QColor(0xff, 0xd5, 0x4f), 1, 7.0 });
    m_lamps.push_back({ { 242, 345 }, QColor(0xff, 0xd5, 0x4f), 2, 7.0 });

    // Inlane runs down to the flippers.
    for (int i = 0; i < 4; ++i) {
        const double t = i / 3.0;
        m_lamps.push_back({ QPointF(44 + t * 44, 470 + t * 120),
                            QColor(0x66, 0xe0, 0x8a), i, 5.5 });
        m_lamps.push_back({ QPointF(356 - t * 44, 470 + t * 120),
                            QColor(0x66, 0xe0, 0x8a), i, 5.5 });
    }

    // A pair either side of the shooter lane mouth.
    m_lamps.push_back({ { 300, 180 }, QColor(0xff, 0x9f, 0x43), 4, 6.0 });
    m_lamps.push_back({ { 100, 180 }, QColor(0xff, 0x9f, 0x43), 4, 6.0 });
}

void PinballView::paintLamps(QPainter& p) const
{
    const double s = scale();

    for (const Lamp& lamp : m_lamps) {
        // Idle chase, plus a full-brightness flash whenever the score moves.
        const double chase = 0.5 + 0.5 * std::sin(m_animSeconds * 3.4 - lamp.group * 0.7);
        const double level = std::clamp(0.22 + chase * 0.38 + m_celebrate * 0.75, 0.0, 1.0);

        const QPointF c = toScreen(lamp.at);
        const double r = lamp.radius * s;

        // Bloom around a bright lamp: the halo is what sells it as a light
        // rather than a coloured dot.
        if (level > 0.35) {
            QRadialGradient halo(c, r * 3.4);
            QColor glow = lamp.colour;
            glow.setAlphaF(std::min(0.55, (level - 0.35) * 0.9));
            halo.setColorAt(0.0, glow);
            glow.setAlpha(0);
            halo.setColorAt(1.0, glow);
            p.setPen(Qt::NoPen);
            p.setBrush(halo);
            p.drawEllipse(c, r * 3.4, r * 3.4);
        }

        QColor lit = lamp.colour;
        lit = QColor::fromHsvF(lit.hueF(), lit.saturationF() * (1.0 - level * 0.45),
                               std::min(1.0, 0.30 + level * 0.85));

        p.setPen(QPen(QColor(0, 0, 0, 110), std::max(0.8, s)));
        p.setBrush(lit);
        p.drawEllipse(c, r, r);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, int(60 + level * 130)));
        p.drawEllipse(c - QPointF(r * 0.28, r * 0.32), r * 0.34, r * 0.28);
    }
}

void PinballView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(0x0a, 0x0c, 0x14));

    const double s = scale();
    const QRectF table(toScreen({ 0, 0 }),
                       QSizeF(PinballTable::kWidth * s, PinballTable::kHeight * s));

    // Wooden cabinet rails around the playfield.
    Theme::paintWoodFrame(p, table, std::max(4.0, 9.0 * s), 12 * s);

    QLinearGradient bg(table.topLeft(), table.bottomLeft());
    bg.setColorAt(0.0, QColor(0x32, 0x3e, 0x6c));
    bg.setColorAt(0.55, QColor(0x22, 0x2a, 0x4c));
    bg.setColorAt(1.0, kPlayfield);
    QPainterPath tablePath;
    tablePath.addRoundedRect(table, 8 * s, 8 * s);
    p.fillPath(tablePath, bg);

    p.save();
    p.setClipPath(tablePath);

    paintPlayfieldArt(p);
    paintLamps(p);

    for (const PinballTable::Wall& w : m_table.walls()) {
        // The one-way gate is never drawn: as a visible line it reads as a wall
        // sealing off the launch lane.
        if (w.oneWayUp)
            continue;

        const QPointF a = toScreen(w.a);
        const QPointF b = toScreen(w.b);
        const double thickness = std::max(1.5, w.radius * 2 * s * 0.9);

        // A dark underside, the rail, then a highlight along the top edge —
        // three strokes give the metal relief a single one cannot.
        p.setPen(QPen(QColor(0, 0, 0, 95), thickness * 1.3, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(a + QPointF(0, thickness * 0.35), b + QPointF(0, thickness * 0.35));

        const QColor ink = w.kick > 0 ? QColor(0xff, 0x9f, 0x43) : kWallInk;
        p.setPen(QPen(w.flash > 0 ? QColor(0xff, 0xf1, 0xa8) : ink, thickness,
                      Qt::SolidLine, Qt::RoundCap));
        p.drawLine(a, b);

        p.setPen(QPen(QColor(255, 255, 255, 70), std::max(0.8, thickness * 0.28),
                      Qt::SolidLine, Qt::RoundCap));
        p.drawLine(a - QPointF(0, thickness * 0.22), b - QPointF(0, thickness * 0.22));
    }

    for (const PinballTable::Bumper& b : m_table.bumpers()) {
        const QPointF c = toScreen(b.centre);
        const double r = b.radius * s;
        const bool lit = b.flash > 0;

        // Skirt, cap, specular dot — a mushroom bumper in three passes.
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 85));
        p.drawEllipse(c + QPointF(0, r * 0.16), r * 1.18, r * 1.18);

        p.setBrush(lit ? QColor(0xff, 0xe9, 0x9a) : QColor(0x2b, 0x33, 0x52));
        p.drawEllipse(c, r * 1.15, r * 1.15);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(lit ? QColor(0xff, 0xf6, 0xd0) : QColor(0x6c, 0x7c, 0xa8),
                      std::max(1.0, 2.0 * s)));
        p.drawEllipse(c, r * 1.15, r * 1.15);

        QRadialGradient g(c - QPointF(r * 0.32, r * 0.36), r * 1.6);
        const QColor base = lit ? QColor(0xff, 0xf1, 0xa8) : QColor(0xe8, 0x51, 0x4f);
        g.setColorAt(0.0, base.lighter(155));
        g.setColorAt(1.0, base.darker(145));
        p.setBrush(g);
        p.setPen(QPen(QColor(0, 0, 0, 70), std::max(1.0, s)));
        p.drawEllipse(c, r, r);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, lit ? 160 : 75));
        p.drawEllipse(c - QPointF(r * 0.26, r * 0.30), r * 0.30, r * 0.24);
    }

    paintFlipper(p, m_table.leftFlipper());
    paintFlipper(p, m_table.rightFlipper());

    // Plunger charge, drawn in the lane so the power is visible.
    if (m_table.ballInLane()) {
        const QRectF meter(toScreen({ 359, 672 }), QSizeF(20 * s, 44 * s));
        p.setBrush(QColor(0x16, 0x1b, 0x30));
        p.setPen(QPen(QColor(255, 255, 255, 55), 1));
        p.drawRoundedRect(meter, 3 * s, 3 * s);
        const double fill = (meter.height() - 2) * m_table.plunger();
        if (fill > 0) {
            QLinearGradient charge(meter.bottomLeft(), meter.topLeft());
            charge.setColorAt(0.0, QColor(0x4f, 0xc3, 0xf7));
            charge.setColorAt(1.0, QColor(0xff, 0x9f, 0x43));
            p.setPen(Qt::NoPen);
            p.setBrush(charge);
            p.drawRoundedRect(QRectF(meter.left() + 1, meter.bottom() - 1 - fill,
                                     meter.width() - 2, fill),
                              2 * s, 2 * s);
        }
    }

    const QPointF ball = toScreen(m_table.ball());
    const double br = m_table.ballRadius() * s;
    Theme::paintContactShadow(p, ball, br);
    QRadialGradient shine(ball - QPointF(br * 0.38, br * 0.42), br * 1.9);
    shine.setColorAt(0.0, QColor(0xff, 0xff, 0xff));
    shine.setColorAt(0.45, QColor(0xc6, 0xce, 0xd8));
    shine.setColorAt(1.0, QColor(0x58, 0x62, 0x70));
    p.setPen(Qt::NoPen);
    p.setBrush(shine);
    p.drawEllipse(ball, br, br);
    p.setBrush(QColor(255, 255, 255, 190));
    p.drawEllipse(ball - QPointF(br * 0.34, br * 0.40), br * 0.26, br * 0.20);

    p.restore();

    // Backglass strip across the top of the cabinet.
    const QRectF glass(table.left(), table.top(), table.width(), std::max(20.0, 30.0 * s));
    p.setPen(Qt::NoPen);
    QLinearGradient strip(glass.topLeft(), glass.bottomLeft());
    strip.setColorAt(0.0, QColor(0x0e, 0x12, 0x22, 235));
    strip.setColorAt(1.0, QColor(0x0e, 0x12, 0x22, 110));
    p.setBrush(strip);
    p.drawRect(glass);

    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(std::max(9.0, 13.0 * s));
    p.setFont(f);
    p.setPen(Theme::kGold);
    p.drawText(glass.adjusted(12, 0, -12, 0), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("SCORE %1").arg(m_table.score()));
    p.setPen(QColor(0xe6, 0xed, 0xf6));
    p.drawText(glass.adjusted(12, 0, -12, 0), Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("BALLS %1").arg(std::max(0, m_table.ballsLeft())));
}

void PinballView::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat())
        return;

    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Z:
    case Qt::Key_Shift:
        m_table.setFlipper(true, true);
        Sound::instance().play(Sound::kFlipper);
        break;
    case Qt::Key_Right:
    case Qt::Key_M:
        m_table.setFlipper(false, true);
        Sound::instance().play(Sound::kFlipper);
        break;
    case Qt::Key_Space:
        m_plungerHeld = true;
        break;
    default:
        GameView::keyPressEvent(event);
    }
}

void PinballView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat())
        return;

    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Z:
    case Qt::Key_Shift:
        m_table.setFlipper(true, false);
        break;
    case Qt::Key_Right:
    case Qt::Key_M:
        m_table.setFlipper(false, false);
        break;
    case Qt::Key_Space:
        m_plungerHeld = false;
        if (m_table.ballInLane())
            Sound::instance().play(Sound::kLaunch);
        m_table.launch();
        break;
    default:
        GameView::keyReleaseEvent(event);
    }
}

void PinballView::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    // Clicking a half of the table works the matching flipper, so the game is
    // playable without touching the keyboard.
    if (event->button() != Qt::LeftButton)
        return;

    if (m_table.ballInLane()) {
        Sound::instance().play(Sound::kLaunch);
        m_table.launch();
    } else {
        m_table.setFlipper(event->position().x() < width() / 2.0, true);
        Sound::instance().play(Sound::kFlipper);
    }
}

void PinballView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_table.setFlipper(true, false);
    m_table.setFlipper(false, false);
}
