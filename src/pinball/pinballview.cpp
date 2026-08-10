#include "pinballview.h"

#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
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

    if (m_table.takeBallLost())
        refresh();

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

    QTimer::singleShot(150, this, [this] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("Game over"));
        box.setText(QStringLiteral("Out of balls."));
        box.setInformativeText(QStringLiteral("Score: %1   Best: %2")
                                   .arg(m_table.score())
                                   .arg(m_table.best()));
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
                             .arg(std::max(m_table.best(), m_table.score()))
                             .arg(hint));
}

double PinballView::scale() const
{
    return std::min(width() / PinballTable::kWidth, height() / PinballTable::kHeight);
}

QPointF PinballView::toScreen(QPointF p) const
{
    const double s = scale();
    return { (width() - PinballTable::kWidth * s) / 2 + p.x() * s,
             (height() - PinballTable::kHeight * s) / 2 + p.y() * s };
}

void PinballView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(0x0c, 0x0f, 0x1a));

    const double s = scale();
    const QRectF table(toScreen({ 0, 0 }),
                       QSizeF(PinballTable::kWidth * s, PinballTable::kHeight * s));

    QLinearGradient bg(table.topLeft(), table.bottomLeft());
    bg.setColorAt(0, QColor(0x25, 0x2e, 0x52));
    bg.setColorAt(1, kPlayfield);
    QPainterPath tablePath;
    tablePath.addRoundedRect(table, 12 * s, 12 * s);
    p.fillPath(tablePath, bg);

    p.save();
    p.setClipPath(tablePath);

    for (const PinballTable::Wall& w : m_table.walls()) {
        // The one-way gate is never drawn: as a visible line it reads as a wall
        // sealing off the launch lane.
        if (w.oneWayUp)
            continue;
        const QColor ink = w.kick > 0 ? QColor(0xff, 0x9f, 0x43) : kWallInk;
        p.setPen(QPen(w.flash > 0 ? QColor(0xff, 0xf1, 0xa8) : ink,
                      std::max(1.5, w.radius * 2 * s * 0.8), Qt::SolidLine, Qt::RoundCap));
        p.drawLine(toScreen(w.a), toScreen(w.b));
    }

    for (const PinballTable::Bumper& b : m_table.bumpers()) {
        const QPointF c = toScreen(b.centre);
        const double r = b.radius * s;
        QRadialGradient g(c - QPointF(r * 0.3, r * 0.3), r * 1.5);
        const QColor base = b.flash > 0 ? QColor(0xff, 0xf1, 0xa8) : QColor(0xe8, 0x51, 0x4f);
        g.setColorAt(0, base.lighter(150));
        g.setColorAt(1, base.darker(140));
        p.setBrush(g);
        p.setPen(QPen(QColor(0xff, 0xff, 0xff, 90), 2));
        p.drawEllipse(c, r, r);
        p.setBrush(QColor(255, 255, 255, 55));
        p.setPen(Qt::NoPen);
        p.drawEllipse(c, r * 0.42, r * 0.42);
    }

    for (const PinballTable::Flipper* f : { &m_table.leftFlipper(), &m_table.rightFlipper() }) {
        p.setPen(QPen(QColor(0xcf, 0xd9, 0xe8), 14 * s, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(toScreen(f->pivot), toScreen(m_table.flipperTip(*f)));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x8d, 0x9a, 0xb0));
        p.drawEllipse(toScreen(f->pivot), 5 * s, 5 * s);
    }

    // Plunger charge, drawn in the lane so the power is visible.
    if (m_table.ballInLane()) {
        const QRectF lane(toScreen({ 358, 680 }), QSizeF(22 * s, 34 * s));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x37, 0x40, 0x62));
        p.drawRect(lane);
        p.setBrush(kAccent);
        p.drawRect(QRectF(lane.left(), lane.bottom() - lane.height() * m_table.plunger(),
                          lane.width(), lane.height() * m_table.plunger()));
    }

    const QPointF ball = toScreen(m_table.ball());
    const double br = m_table.ballRadius() * s;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 90));
    p.drawEllipse(ball + QPointF(br * 0.2, br * 0.25), br, br);
    QRadialGradient shine(ball - QPointF(br * 0.35, br * 0.4), br * 1.8);
    shine.setColorAt(0, QColor(0xff, 0xff, 0xff));
    shine.setColorAt(1, QColor(0x76, 0x80, 0x90));
    p.setBrush(shine);
    p.drawEllipse(ball, br, br);

    p.restore();

    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(std::max(9.0, 13.0 * s));
    p.setFont(f);
    p.setPen(QColor(0xe6, 0xed, 0xf6));
    p.drawText(table.adjusted(12, 6, -12, 0), Qt::AlignLeft | Qt::AlignTop,
               QStringLiteral("SCORE %1").arg(m_table.score()));
    p.drawText(table.adjusted(12, 6, -12, 0), Qt::AlignRight | Qt::AlignTop,
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
        break;
    case Qt::Key_Right:
    case Qt::Key_M:
        m_table.setFlipper(false, true);
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

    if (m_table.ballInLane())
        m_table.launch();
    else
        m_table.setFlipper(event->position().x() < width() / 2.0, true);
}

void PinballView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_table.setFlipper(true, false);
    m_table.setFlipper(false, false);
}
