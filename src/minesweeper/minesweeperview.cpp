#include "minesweeperview.h"

#include "scores.h"
#include "sound.h"
#include <QActionGroup>
#include <QDataStream>
#include <QMessageBox>
#include <QPushButton>
#include <QMouseEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

// Classic Minesweeper number colours — players read the board by colour as
// much as by digit, so these are worth matching.
const QColor kNumberColours[9] = {
    QColor(0, 0, 0, 0),
    QColor(0x5c, 0x9d, 0xff), QColor(0x5c, 0xc9, 0x60), QColor(0xf2, 0x6b, 0x6b),
    QColor(0x9b, 0x79, 0xdb), QColor(0xff, 0xa7, 0x2b), QColor(0x35, 0xd0, 0xe0),
    QColor(0xff, 0x6f, 0xa5), QColor(0xd0, 0xd6, 0xdc),
};

constexpr QColor kCovered { 0x6b, 0x74, 0x7d };
constexpr QColor kCoveredEdge { 0x92, 0x9d, 0xa7 };
constexpr QColor kDug { 0x33, 0x38, 0x3d };
constexpr QColor kBezel { 0x22, 0x26, 0x2a };

const MinesweeperView::Level kLevels[] = {
    { "Beginner", 9, 9, 10 },
    { "Intermediate", 16, 16, 40 },
    { "Expert", 30, 16, 99 },
};

} // namespace

MinesweeperView::MinesweeperView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(minimumSizeHint());
    setMouseTracking(false);

    m_tick = new QTimer(this);
    m_tick->setInterval(500);
    connect(m_tick, &QTimer::timeout, this, &MinesweeperView::refresh);

    buildActions();
    newGame(m_level);
}

void MinesweeperView::buildActions()
{
    auto* newAction = new QAction(QStringLiteral("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this] { newGame(m_level); });
    m_actions.append(newAction);

    m_pauseAction = new QAction(QStringLiteral("Pause"), this);
    m_pauseAction->setCheckable(true);
    m_pauseAction->setShortcut(Qt::Key_P);
    connect(m_pauseAction, &QAction::toggled, this, [this](bool on) {
        if (on == m_paused)
            return;
        // Bank what the clock has run so far, then stop reading it.
        if (on) {
            m_elapsedMs = elapsedMs();
            m_tick->stop();
        } else if (m_started) {
            m_clock.restart();
            m_tick->start();
        }
        m_paused = on;
        update();
        refresh();
    });
    m_actions.append(m_pauseAction);

    auto* separator = new QAction(this);
    separator->setSeparator(true);
    m_actions.append(separator);

    m_levelGroup = new QActionGroup(this);
    m_levelGroup->setExclusive(true);
    for (int i = 0; i < int(std::size(kLevels)); ++i) {
        auto* a = new QAction(QString::fromUtf8(kLevels[i].name), this);
        a->setCheckable(true);
        a->setChecked(i == m_level);
        m_levelGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, i] { newGame(i); });
        m_actions.append(a);
    }
}

qint64 MinesweeperView::elapsedMs() const
{
    if (!m_started)
        return 0;
    return m_elapsedMs + ((m_paused || m_suspended) ? 0 : m_clock.elapsed());
}

void MinesweeperView::newGame(int levelIndex)
{
    m_level = std::clamp(levelIndex, 0, int(std::size(kLevels)) - 1);
    const Level& l = kLevels[m_level];
    m_field = std::make_unique<Minefield>(l.width, l.height, l.mines);
    m_started = false;
    m_announced = false;
    m_paused = false;
    m_suspended = false;
    m_elapsedMs = 0;
    if (m_pauseAction != nullptr)
        m_pauseAction->setChecked(false);
    m_tick->stop();
    update();
    refresh();
}

void MinesweeperView::activate()
{
    // Pick the clock up where it was left rather than where it would have got
    // to on its own while another game was on screen.
    if (m_suspended) {
        m_suspended = false;
        m_clock.restart();
        if (m_started && !m_paused)
            m_tick->start();
    }
    refresh();
}

void MinesweeperView::deactivate()
{
    if (m_suspended)
        return;
    m_elapsedMs = elapsedMs();
    m_suspended = true;
    m_tick->stop();
}

double MinesweeperView::cellSize() const
{
    if (!m_field)
        return 0;
    // One square size for the whole grid, so cells stay square at any window
    // shape; the grid is then centred in whatever space is left.
    const double byWidth = (width() - 24.0) / m_field->width();
    const double byHeight = (height() - 24.0) / m_field->height();
    return std::max(8.0, std::floor(std::min(byWidth, byHeight)));
}

QRect MinesweeperView::fieldRect() const
{
    if (!m_field)
        return {};
    const double cell = cellSize();
    const int w = int(cell * m_field->width());
    const int h = int(cell * m_field->height());
    return { (width() - w) / 2, (height() - h) / 2, w, h };
}

bool MinesweeperView::cellAt(QPointF pos, int& row, int& col) const
{
    if (!m_field)
        return false;
    const QRect r = fieldRect();
    if (!r.contains(pos.toPoint()))
        return false;
    const double cell = cellSize();
    col = int((pos.x() - r.x()) / cell);
    row = int((pos.y() - r.y()) / cell);
    return m_field->inBounds(row, col);
}

void MinesweeperView::refresh()
{
    if (!m_field)
        return;

    const int seconds = int(elapsedMs() / 1000);
    QString state;
    switch (m_field->state()) {
    case Minefield::State::Playing:
        state = m_paused ? QStringLiteral("Paused — the clock is stopped.")
            : m_started  ? QStringLiteral("Digging…")
                         : QStringLiteral("Click anywhere to start.");
        break;
    case Minefield::State::Won:
        state = QStringLiteral("Field cleared!");
        break;
    case Minefield::State::Lost:
        state = QStringLiteral("Boom.");
        break;
    }

    QString line = QStringLiteral("%1   Mines left %2   Time %3s")
                       .arg(state)
                       .arg(m_field->minesRemaining())
                       .arg(seconds);
    if (Scores::instance().has(Scores::minesweeperBestTime(m_level)))
        line += QStringLiteral("   Best %1s")
                    .arg(Scores::instance().best(Scores::minesweeperBestTime(m_level)));
    Q_EMIT statusChanged(line);

    if (m_field->state() != Minefield::State::Playing) {
        m_tick->stop();
        if (!m_announced) {
            m_announced = true;
            const bool won = m_field->state() == Minefield::State::Won;
            Sound::instance().play(won ? Sound::kWin : Sound::kBoom);
            const bool newBest =
                won && Scores::instance().recordLow(Scores::minesweeperBestTime(m_level), seconds);
            // Queued so the final board paints before the dialog covers it.
            QTimer::singleShot(won ? 250 : 600, this, [this, won, seconds, newBest] {
                QMessageBox box(this);
                box.setWindowTitle(won ? QStringLiteral("Cleared") : QStringLiteral("Boom"));
                box.setText(won ? QStringLiteral("You cleared the field!")
                                : QStringLiteral("You hit a mine."));
                if (won) {
                    const int record = Scores::instance().best(
                        Scores::minesweeperBestTime(m_level), seconds);
                    box.setInformativeText(
                        newBest ? QStringLiteral("Time: %1 seconds — a new best!").arg(seconds)
                                : QStringLiteral("Time: %1 seconds.   Best: %2.")
                                      .arg(seconds)
                                      .arg(record));
                }
                QAbstractButton* again = box.addButton(QStringLiteral("Play Again"),
                                                       QMessageBox::AcceptRole);
                box.addButton(QStringLiteral("Close"), QMessageBox::RejectRole);
                box.exec();
                if (box.clickedButton() == again)
                    newGame(m_level);
            });
        }
    }
}

void MinesweeperView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().window());

    if (!m_field)
        return;

    const QRect r = fieldRect();
    const double cell = cellSize();

    p.fillRect(r.adjusted(-6, -6, 6, 6), kBezel);

    // Paused covers the field. A stopped clock over a board you can still study
    // is not a pause, it is free thinking time.
    if (m_paused) {
        p.fillRect(r, kCovered.darker(150));
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(std::max(11.0, r.width() * 0.045));
        p.setFont(f);
        p.setPen(QColor(0xd0, 0xd6, 0xdc));
        p.drawText(r, Qt::AlignCenter, QStringLiteral("Paused\nPress Pause again to carry on"));
        return;
    }

    QFont numberFont = font();
    numberFont.setBold(true);
    numberFont.setPointSizeF(std::max(7.0, cell * 0.46));

    for (int row = 0; row < m_field->height(); ++row) {
        for (int col = 0; col < m_field->width(); ++col) {
            const Minefield::Square& s = m_field->at(row, col);
            const QRectF c(r.x() + col * cell, r.y() + row * cell, cell, cell);
            const QRectF inner = c.adjusted(1, 1, -1, -1);

            if (!s.revealed) {
                QLinearGradient tile(inner.topLeft(), inner.bottomRight());
                tile.setColorAt(0.0, kCovered.lighter(116));
                tile.setColorAt(1.0, kCovered.darker(112));
                p.fillRect(inner, tile);

                // A real bevel: bright along the top-left, dark along the
                // bottom-right, which is what makes the tile look raised.
                const double bevel = std::max(1.0, cell * 0.09);
                p.setPen(Qt::NoPen);
                p.setBrush(kCoveredEdge);
                p.drawPolygon(QPolygonF { inner.topLeft(), inner.topRight(),
                                          inner.topRight() - QPointF(bevel, -bevel),
                                          inner.topLeft() + QPointF(bevel, bevel),
                                          inner.bottomLeft() - QPointF(-bevel, bevel),
                                          inner.bottomLeft() });
                p.setBrush(QColor(0x39, 0x3f, 0x46));
                p.drawPolygon(QPolygonF { inner.bottomRight(), inner.bottomLeft(),
                                          inner.bottomLeft() + QPointF(bevel, -bevel),
                                          inner.bottomRight() - QPointF(bevel, bevel),
                                          inner.topRight() + QPointF(-bevel, bevel),
                                          inner.topRight() });

                if (s.flagged) {
                    const QPointF base(c.center().x() - cell * 0.02, c.bottom() - cell * 0.22);
                    p.setPen(QPen(QColor(0xd0, 0xd6, 0xdc), std::max(1.5, cell * 0.07)));
                    p.drawLine(base, QPointF(base.x(), c.top() + cell * 0.20));
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(0xe5, 0x3d, 0x3d));
                    const QPointF tip(base.x(), c.top() + cell * 0.20);
                    const QPolygonF flag { tip, tip + QPointF(cell * 0.30, cell * 0.13),
                                           tip + QPointF(0, cell * 0.26) };
                    p.drawPolygon(flag);
                }
                continue;
            }

            QLinearGradient hole(inner.topLeft(), inner.bottomRight());
            hole.setColorAt(0.0, kDug.darker(118));
            hole.setColorAt(0.5, kDug);
            hole.setColorAt(1.0, kDug.lighter(108));
            p.fillRect(inner, hole);
            p.setPen(QPen(QColor(0x45, 0x4b, 0x52), 1));
            // drawRect fills with the current brush as well as outlining, and
            // the flag above leaves that brush red — without this every dug
            // square painted after the first flag came out red.
            p.setBrush(Qt::NoBrush);
            p.drawRect(inner);

            if (s.mine) {
                const QPointF centre = c.center();
                const double rad = cell * 0.20;
                const bool detonated = m_field->state() == Minefield::State::Lost;
                p.setPen(QPen(detonated ? QColor(0xff, 0x8a, 0x80) : QColor(0xcf, 0xd6, 0xe0),
                              std::max(1.0, cell * 0.06)));
                for (int i = 0; i < 4; ++i) {
                    const double a = i * std::numbers::pi / 4;
                    p.drawLine(centre - QPointF(std::cos(a), std::sin(a)) * rad * 1.7,
                               centre + QPointF(std::cos(a), std::sin(a)) * rad * 1.7);
                }
                p.setPen(Qt::NoPen);
                p.setBrush(detonated ? QColor(0xe5, 0x3d, 0x3d) : QColor(0xcf, 0xd6, 0xe0));
                p.drawEllipse(centre, rad, rad);
                continue;
            }

            if (s.neighbours > 0) {
                p.setFont(numberFont);
                p.setPen(kNumberColours[std::clamp(s.neighbours, 0, 8)]);
                p.drawText(c, Qt::AlignCenter, QString::number(s.neighbours));
            }
        }
    }
}

void MinesweeperView::mousePressEvent(QMouseEvent* event)
{
    if (!m_field || m_paused || m_field->state() != Minefield::State::Playing)
        return;

    int row = 0;
    int col = 0;
    if (!cellAt(event->position(), row, col))
        return;

    if (event->button() == Qt::RightButton) {
        m_field->toggleFlag(row, col);
        Sound::instance().play(Sound::kFlag);
    } else if (event->button() == Qt::MiddleButton) {
        m_field->chord(row, col);
    } else if (event->button() == Qt::LeftButton) {
        if (!m_started) {
            m_started = true;
            m_elapsedMs = 0;
            m_clock.start();
            m_tick->start();
        }
        // A left click on an already-open number chords, which is what most
        // players expect from a modern Minesweeper.
        if (m_field->at(row, col).revealed)
            m_field->chord(row, col);
        else
            m_field->reveal(row, col);
        if (m_field->state() != Minefield::State::Lost)
            Sound::instance().play(Sound::kDig);
    } else {
        return;
    }

    update();
    refresh();
}

void MinesweeperView::mouseReleaseEvent(QMouseEvent*)
{
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

// The field, not the clicks that dug it.
//
// There is no move log to replay, so what gets written is the board itself, and
// Minefield::restore() stands in for a rules check on the way back in: the mine
// count has to match the level, and the numbers are recomputed rather than read,
// so a tampered save cannot show a 3 next to one mine.
QByteArray MinesweeperView::saveState() const
{
    // Nothing worth coming back to: a field already won or lost, or one nobody
    // has dug into. An empty state also clears whatever was stored before.
    if (!m_field || !m_started || m_field->state() != Minefield::State::Playing)
        return {};

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    // The banked time, not the running clock: elapsedMs() is what the player has
    // actually spent, and it is what the best-time table will be judged against.
    out << quint32(1) << qint32(m_level) << qint64(elapsedMs()) << m_paused;

    const std::vector<Minefield::Square>& squares = m_field->squares();
    out << quint32(squares.size());
    for (const Minefield::Square& s : squares)
        out << quint8((s.mine ? 1 : 0) | (s.revealed ? 2 : 0) | (s.flagged ? 4 : 0));
    return blob;
}

bool MinesweeperView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    qint32 level = 0;
    qint64 elapsed = 0;
    bool paused = false;
    quint32 count = 0;
    in >> version >> level >> elapsed >> paused >> count;
    if (version != 1 || in.status() != QDataStream::Ok)
        return false;
    if (level < 0 || level >= int(std::size(kLevels)) || elapsed < 0)
        return false;

    const Level& l = kLevels[level];
    if (count != quint32(l.width * l.height))
        return false;

    std::vector<Minefield::Square> squares(count);
    for (Minefield::Square& s : squares) {
        quint8 bits = 0;
        in >> bits;
        s.mine = (bits & 1) != 0;
        s.revealed = (bits & 2) != 0;
        s.flagged = (bits & 4) != 0;
    }
    if (in.status() != QDataStream::Ok)
        return false;

    // Built to one side, so a blob that turns out to be nonsense leaves the
    // field already on screen alone.
    auto field = std::make_unique<Minefield>(l.width, l.height, l.mines);
    if (!field->restore(std::move(squares)) || field->state() != Minefield::State::Playing)
        return false;

    m_field = std::move(field);
    m_level = level;
    m_elapsedMs = elapsed;
    m_paused = paused;
    m_started = true;
    m_announced = false;
    // The clock picks up in activate(), exactly the way it does when the player
    // comes back from another game.
    m_suspended = true;
    m_tick->stop();

    // The toolbar has to agree with the board it is now sitting above.
    if (m_pauseAction != nullptr)
        m_pauseAction->setChecked(m_paused);
    if (m_levelGroup != nullptr)
        m_levelGroup->actions().at(m_level)->setChecked(true);

    update();
    refresh();
    return true;
}
