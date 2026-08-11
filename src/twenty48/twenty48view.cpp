#include "twenty48view.h"

#include <QDataStream>

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
#include <cmath>

namespace {
const QString kBestKey = QStringLiteral("twenty48/best_score");

// Warm paper tones that climb towards a hot colour as the numbers grow.
QColor tileColour(int value)
{
    switch (value) {
    case 2:    return QColor(0xee, 0xe4, 0xda);
    case 4:    return QColor(0xed, 0xe0, 0xc8);
    case 8:    return QColor(0xf2, 0xb1, 0x79);
    case 16:   return QColor(0xf5, 0x95, 0x63);
    case 32:   return QColor(0xf6, 0x7c, 0x5f);
    case 64:   return QColor(0xf6, 0x5e, 0x3b);
    case 128:  return QColor(0xed, 0xcf, 0x72);
    case 256:  return QColor(0xed, 0xcc, 0x61);
    case 512:  return QColor(0xed, 0xc8, 0x50);
    case 1024: return QColor(0xed, 0xc5, 0x3f);
    case 2048: return QColor(0xed, 0xc2, 0x2e);
    default:   return QColor(0x3c, 0x3a, 0x32);
    }
}

QColor inkFor(int value)
{
    return value <= 4 ? QColor(0x77, 0x6e, 0x65) : QColor(0xf9, 0xf6, 0xf2);
}
}

Twenty48View::Twenty48View(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(minimumSizeHint());
    setFocusPolicy(Qt::StrongFocus);
    buildActions();
    newGame();
}

void Twenty48View::buildActions()
{
    auto* newAction = new QAction(QStringLiteral("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &Twenty48View::newGame);
    m_actions.append(newAction);

    m_undoAction = new QAction(QStringLiteral("Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &Twenty48View::undo);
    m_actions.append(m_undoAction);
}

void Twenty48View::newGame()
{
    m_board.fill(0);
    m_previous.fill(0);
    m_score = 0;
    m_previousScore = 0;
    m_canUndo = false;
    m_reachedTarget = false;
    m_finished = false;
    m_undoAction->setEnabled(false);
    spawn();
    spawn();
    setFocus();
    update();
    refresh();
}

void Twenty48View::activate()
{
    setFocus();
    refresh();
}

void Twenty48View::undo()
{
    if (!m_canUndo)
        return;
    m_board = m_previous;
    m_score = m_previousScore;
    m_canUndo = false;
    m_finished = false;
    m_undoAction->setEnabled(false);
    update();
    refresh();
}

void Twenty48View::spawn()
{
    std::vector<int> free;
    for (int i = 0; i < kCells; ++i)
        if (m_board[std::size_t(i)] == 0)
            free.push_back(i);
    if (free.empty())
        return;

    std::uniform_int_distribution<std::size_t> pick(0, free.size() - 1);
    // One in ten new tiles is a 4, as in the original.
    std::uniform_int_distribution<int> four(0, 9);
    m_board[std::size_t(free[pick(m_rng)])] = (four(m_rng) == 0) ? 4 : 2;
}

bool Twenty48View::slide(Direction direction)
{
    const std::array<int, kCells> before = m_board;
    const int beforeScore = m_score;

    // Every direction is the same operation over a line; only the traversal
    // order changes, so the merge logic is written once.
    auto lineOf = [&](int index, int step) {
        std::vector<int*> line;
        for (int i = 0; i < kSize; ++i) {
            const int row = (direction == Direction::Left || direction == Direction::Right)
                ? index
                : (step > 0 ? i : kSize - 1 - i);
            const int col = (direction == Direction::Left || direction == Direction::Right)
                ? (step > 0 ? i : kSize - 1 - i)
                : index;
            line.push_back(&at(row, col));
        }
        return line;
    };

    const int step = (direction == Direction::Left || direction == Direction::Up) ? 1 : -1;

    for (int index = 0; index < kSize; ++index) {
        std::vector<int*> line = lineOf(index, step);

        // Compact towards the front.
        std::vector<int> values;
        for (int* cell : line)
            if (*cell != 0)
                values.push_back(*cell);

        // Merge equal neighbours once each, front to back.
        std::vector<int> merged;
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i + 1 < values.size() && values[i] == values[i + 1]) {
                const int value = values[i] * 2;
                merged.push_back(value);
                m_score += value;
                if (value >= kTarget)
                    m_reachedTarget = true;
                ++i;
            } else {
                merged.push_back(values[i]);
            }
        }

        for (std::size_t i = 0; i < line.size(); ++i)
            *line[i] = (i < merged.size()) ? merged[i] : 0;
    }

    if (m_board == before) {
        m_score = beforeScore;
        return false;
    }

    m_previous = before;
    m_previousScore = beforeScore;
    m_canUndo = true;
    m_undoAction->setEnabled(true);
    return true;
}

bool Twenty48View::canMove() const
{
    for (int i = 0; i < kCells; ++i)
        if (m_board[std::size_t(i)] == 0)
            return true;
    for (int row = 0; row < kSize; ++row) {
        for (int col = 0; col < kSize; ++col) {
            const int v = at(row, col);
            if (col + 1 < kSize && at(row, col + 1) == v)
                return true;
            if (row + 1 < kSize && at(row + 1, col) == v)
                return true;
        }
    }
    return false;
}

void Twenty48View::checkEnd()
{
    if (canMove() || m_finished)
        return;

    m_finished = true;
    Sound::instance().play(Sound::kLose);
    const bool newBest = Scores::instance().recordHigh(kBestKey, m_score);
    refresh();

    QTimer::singleShot(200, this, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("No moves left"));
        box.setText(QStringLiteral("The board is stuck."));
        box.setInformativeText(newBest
                                   ? QStringLiteral("Score: %1 — a new best!").arg(m_score)
                                   : QStringLiteral("Score: %1.   Best: %2.")
                                         .arg(m_score)
                                         .arg(Scores::instance().best(kBestKey)));
        QAbstractButton* again = box.addButton(QStringLiteral("Play Again"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Close"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == again)
            newGame();
    });
}

void Twenty48View::refresh(const QString& message)
{
    int highest = 0;
    for (int v : m_board)
        highest = std::max(highest, v);

    QString line = message.isEmpty()
        ? QStringLiteral("%1   Score %2   Highest %3   Best %4")
              .arg(m_finished ? QStringLiteral("Stuck")
                              : m_reachedTarget ? QStringLiteral("Target reached — keep going!")
                                                : QStringLiteral("Slide with the arrow keys"))
              .arg(m_score)
              .arg(highest)
              .arg(std::max(Scores::instance().best(kBestKey), m_score))
        : message;
    Q_EMIT statusChanged(line);
}

void Twenty48View::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().window());

    const double side = std::min(width(), height()) - 32.0;
    const QRectF board((width() - side) / 2, (height() - side) / 2, side, side);

    QPainterPath boardPath;
    boardPath.addRoundedRect(board, side * 0.02, side * 0.02);
    p.fillPath(boardPath, QColor(0xbb, 0xad, 0xa0));

    const double gap = side * 0.025;
    const double cell = (side - gap * (kSize + 1)) / kSize;

    for (int row = 0; row < kSize; ++row) {
        for (int col = 0; col < kSize; ++col) {
            const QRectF box(board.left() + gap + col * (cell + gap),
                             board.top() + gap + row * (cell + gap), cell, cell);
            QPainterPath path;
            path.addRoundedRect(box, cell * 0.06, cell * 0.06);

            const int v = at(row, col);
            if (v == 0) {
                p.fillPath(path, QColor(0xcd, 0xc1, 0xb4));
                continue;
            }

            p.fillPath(path, tileColour(v));
            QFont f = font();
            f.setBold(true);
            // Long numbers get a smaller face so 1024 still fits its tile.
            const int digits = QString::number(v).size();
            f.setPointSizeF(cell * (digits <= 2 ? 0.40 : digits == 3 ? 0.32 : 0.26));
            p.setFont(f);
            p.setPen(inkFor(v));
            p.drawText(box, Qt::AlignCenter, QString::number(v));
        }
    }
}

void Twenty48View::keyPressEvent(QKeyEvent* event)
{
    if (m_finished) {
        GameView::keyPressEvent(event);
        return;
    }

    Direction direction;
    switch (event->key()) {
    case Qt::Key_Left:  case Qt::Key_A: direction = Direction::Left; break;
    case Qt::Key_Right: case Qt::Key_D: direction = Direction::Right; break;
    case Qt::Key_Up:    case Qt::Key_W: direction = Direction::Up; break;
    case Qt::Key_Down:  case Qt::Key_S: direction = Direction::Down; break;
    default:
        GameView::keyPressEvent(event);
        return;
    }

    if (!slide(direction)) {
        refresh(QStringLiteral("Nothing moves that way."));
        return;
    }

    spawn();
    Sound::instance().play(Sound::kCardPlace);
    update();
    refresh();
    checkEnd();
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

// The tiles and the score, which is the whole game — 2048 keeps no move log.
// What stands in for a rules check on the way back in is that every tile has to
// be a power of two: nothing else can come out of a merge. The single undo step
// is not saved, the same way every other game here starts a resumed game with a
// fresh history.
QByteArray Twenty48View::saveState() const
{
    // Nothing worth coming back to: a board already stuck, or the opening two
    // tiles nobody has pushed yet. An empty state also clears the stored one.
    if (m_finished || (m_score == 0 && !m_canUndo))
        return {};

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1) << qint32(m_score) << m_reachedTarget;
    for (int value : m_board)
        out << qint32(value);
    return blob;
}

bool Twenty48View::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    qint32 score = 0;
    bool reachedTarget = false;
    in >> version >> score >> reachedTarget;
    if (version != 1 || in.status() != QDataStream::Ok || score < 0)
        return false;

    // Read into a board of its own, so a blob that turns out to be nonsense
    // leaves the game already on screen alone.
    std::array<int, kCells> board {};
    int tiles = 0;
    for (int& cell : board) {
        qint32 value = 0;
        in >> value;
        if (value != 0 && (value < 2 || value > (1 << 20) || (value & (value - 1)) != 0))
            return false;
        cell = value;
        if (value != 0)
            ++tiles;
    }
    if (in.status() != QDataStream::Ok || tiles == 0)
        return false;

    m_board = board;
    m_previous = board;
    m_score = score;
    m_previousScore = score;
    m_reachedTarget = reachedTarget;
    m_canUndo = false;
    m_finished = false;
    m_undoAction->setEnabled(false);
    update();
    refresh();
    return true;
}
