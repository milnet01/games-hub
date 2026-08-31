#include "snakeview.h"

#include "scores.h"
#include "sound.h"
#include "theme.h"

#include <QKeyEvent>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>

#include <algorithm>

namespace {
const QString kBestKey = QStringLiteral("snake/best_score");
constexpr QColor kBoardDark { 0x14, 0x30, 0x22 };
constexpr QColor kBoardLight { 0x18, 0x38, 0x28 };
}

SnakeView::SnakeView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(minimumSizeHint());
    setFocusPolicy(Qt::StrongFocus);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SnakeView::step);

    buildActions();
    newGame();
}

void SnakeView::buildActions()
{
    auto* newAction = new QAction(QStringLiteral("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &SnakeView::newGame);
    m_actions.append(newAction);

    auto* pause = new QAction(QStringLiteral("Pause"), this);
    pause->setCheckable(true);
    pause->setShortcut(QKeySequence(Qt::Key_Space));
    connect(pause, &QAction::toggled, this, [this](bool on) {
        m_running = !on && m_started && !m_board.dead();
        if (m_running)
            m_timer->start(m_speedMs);
        else
            m_timer->stop();
        refresh();
        // The timer has just stopped, so nothing else will repaint: without
        // this the board caption still reads "Go" for the whole pause.
        update();
    });
    m_actions.append(pause);
}

void SnakeView::newGame()
{
    m_board.newGame();
    m_speedMs = 130;
    m_started = false;
    m_running = false;
    m_timer->stop();
    setFocus();
    update();
    refresh();
}

void SnakeView::activate()
{
    setFocus();
    if (m_started && !m_board.dead()) {
        m_running = true;
        m_timer->start(m_speedMs);
    }
    refresh();
}

void SnakeView::deactivate()
{
    m_running = false;
    m_timer->stop();
}

void SnakeView::step()
{
    switch (m_board.step()) {
    case SnakeBoard::Step::Died:
        gameOver();
        return;
    case SnakeBoard::Step::Ate:
        Sound::instance().play(Sound::kDig);
        // Speed up gently, with a floor so it stays playable. How fast the
        // snake moves is the view's business; the core only says it ate.
        m_speedMs = std::max(60, m_speedMs - 3);
        m_timer->setInterval(m_speedMs);
        break;
    case SnakeBoard::Step::Moved:
        break;
    }

    update();
    refresh();
}

void SnakeView::gameOver()
{
    m_running = false;
    m_timer->stop();
    Sound::instance().play(Sound::kLose);
    const bool newBest = Scores::instance().recordHigh(kBestKey, m_board.score());
    update();
    refresh();

    QTimer::singleShot(200, this, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("Game over"));
        box.setText(QStringLiteral("The snake stopped."));
        box.setInformativeText(newBest
                                   ? QStringLiteral("Score: %1 — a new best!").arg(m_board.score())
                                   : QStringLiteral("Score: %1.   Best: %2.")
                                         .arg(m_board.score())
                                         .arg(Scores::instance().best(kBestKey)));
        QAbstractButton* again = box.addButton(QStringLiteral("Play Again"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Close"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == again)
            newGame();
    });
}

void SnakeView::refresh()
{
    QString state;
    if (m_board.dead())
        state = QStringLiteral("Game over");
    else if (!m_started)
        state = QStringLiteral("Press an arrow key to start");
    else if (!m_running)
        state = QStringLiteral("Paused");
    else
        state = QStringLiteral("Go");

    Q_EMIT statusChanged(QStringLiteral("%1   Score %2   Length %3   Best %4")
                             .arg(state)
                             .arg(m_board.score())
                             .arg(m_board.snake().size())
                             .arg(std::max(Scores::instance().best(kBestKey), m_board.score())));
}

double SnakeView::cellSize() const
{
    // The caption's strip comes off the height before the cells are sized.
    return std::max(6.0, std::floor(std::min(
                             (width() - 24.0) / kGridWidth,
                             (height() - 24.0 - captionBand(QRectF(rect()))) / kGridHeight)));
}

QRect SnakeView::boardRect() const
{
    const int w = int(cellSize()) * kGridWidth;
    const int h = int(cellSize()) * kGridHeight;
    const int band = int(captionBand(QRectF(rect())));
    return { (width() - w) / 2, (height() - band - h) / 2, w, h };
}

void SnakeView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().window());

    const QRect r = boardRect();
    const double cell = cellSize();
    Theme::paintWoodFrame(p, r, 8, 6);

    // A faint chequer, which makes the snake's movement easy to read.
    for (int x = 0; x < kGridWidth; ++x)
        for (int y = 0; y < kGridHeight; ++y)
            p.fillRect(QRectF(r.x() + x * cell, r.y() + y * cell, cell, cell),
                       ((x + y) % 2) ? kBoardLight : kBoardDark);

    // Food.
    const QPoint food = m_board.food();
    const QPointF foodCentre(r.x() + (food.x() + 0.5) * cell, r.y() + (food.y() + 0.5) * cell);
    Theme::paintContactShadow(p, foodCentre, cell * 0.30);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xe8, 0x51, 0x4f));
    p.drawEllipse(foodCentre, cell * 0.30, cell * 0.30);
    p.setBrush(QColor(255, 255, 255, 110));
    p.drawEllipse(foodCentre - QPointF(cell * 0.09, cell * 0.10), cell * 0.09, cell * 0.07);

    // Body, tail to head, so the head lands on top.
    const std::deque<QPoint>& body = m_board.snake();
    for (int i = int(body.size()) - 1; i >= 0; --i) {
        const QPoint s = body[std::size_t(i)];
        const QRectF box(r.x() + s.x() * cell + cell * 0.06, r.y() + s.y() * cell + cell * 0.06,
                         cell * 0.88, cell * 0.88);
        const bool head = i == 0;
        const double t = body.empty() ? 0.0 : double(i) / double(body.size());

        QColor tint = QColor::fromHsvF(0.33, 0.55, 0.85 - t * 0.35);
        if (m_board.dead())
            tint = QColor::fromHsvF(0.0, 0.35, 0.72 - t * 0.30);

        QPainterPath path;
        path.addRoundedRect(box, cell * 0.28, cell * 0.28);
        p.setBrush(head ? tint.lighter(125) : tint);
        p.setPen(QPen(QColor(0, 0, 0, 60), 1));
        p.drawPath(path);

        if (head) {
            // Two eyes, looking the way it is travelling.
            const QPointF centre = box.center();
            const QPoint facing = m_board.direction();
            const QPointF along(facing.x() * cell * 0.16, facing.y() * cell * 0.16);
            const QPointF side(-facing.y() * cell * 0.14, facing.x() * cell * 0.14);
            p.setPen(Qt::NoPen);
            p.setBrush(Qt::white);
            p.drawEllipse(centre + along + side, cell * 0.09, cell * 0.09);
            p.drawEllipse(centre + along - side, cell * 0.09, cell * 0.09);
            p.setBrush(QColor(0x20, 0x20, 0x20));
            p.drawEllipse(centre + along * 1.3 + side, cell * 0.045, cell * 0.045);
            p.drawEllipse(centre + along * 1.3 - side, cell * 0.045, cell * 0.045);
        }
    }

    // Snake never wrote a word on its own board — not the score, and not
    // "game over". Both were in the status bar only.
    paintStatusCaption(p, QRectF(rect()));
}

void SnakeView::keyPressEvent(QKeyEvent* event)
{
    QPoint wanted;
    switch (event->key()) {
    case Qt::Key_Left:  case Qt::Key_A: wanted = { -1, 0 }; break;
    case Qt::Key_Right: case Qt::Key_D: wanted = { 1, 0 }; break;
    case Qt::Key_Up:    case Qt::Key_W: wanted = { 0, -1 }; break;
    case Qt::Key_Down:  case Qt::Key_S: wanted = { 0, 1 }; break;
    default:
        GameView::keyPressEvent(event);
        return;
    }

    // The core decides whether the turn is legal; a reversal into your own
    // neck is refused there rather than here.
    if (!m_board.turn(wanted))
        return;

    if (!m_started && !m_board.dead()) {
        m_started = true;
        m_running = true;
        m_timer->start(m_speedMs);
        refresh();
    }
}
