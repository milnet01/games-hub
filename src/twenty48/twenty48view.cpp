#include "twenty48view.h"

#include "legibility.h"
#include "scores.h"
#include "sound.h"
#include "theme.h"

#include <QDataStream>

#include <QKeyEvent>
#include <QMessageBox>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {
const QString kBestKey = QStringLiteral("twenty48/best_score");

// Above this relative luminance a tile is "light" and takes the dark ink. It
// sits well below the darkest light tile (64, at L = 0.279) and well above the
// dark one (the default: arm, at 0.042), so no tile is near the boundary.
constexpr double kLightTileLuminance = 0.20;

constexpr QColor kDarkInk { 0x26, 0x23, 0x1d };
constexpr QColor kLightInk { 0xf9, 0xf6, 0xf2 };
}

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

double relativeLuminance(const QColor& c)
{
    auto channel = [](double v) {
        return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(c.redF()) + 0.7152 * channel(c.greenF())
         + 0.0722 * channel(c.blueF());
}

double contrastRatio(const QColor& a, const QColor& b)
{
    const double la = relativeLuminance(a);
    const double lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QColor inkFor(int value)
{
    // Which ink reads on a tile is a property of the TILE's brightness, not of
    // its number. Keying on the value meant every tile from 8 up got near-white
    // ink over a mid-tone colour, at half the contrast a reader needs — and no
    // cut-off on the value can fix that, because the tiles are not ordered by
    // brightness: 64 is the darkest at L = 0.279 while 128 jumps back to 0.639.
    return relativeLuminance(tileColour(value)) > kLightTileLuminance ? kDarkInk : kLightInk;
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
    m_board.newGame();
    m_finished = false;
    m_undoAction->setEnabled(false);
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
    if (!m_board.canUndo())
        return;
    m_board.undo();
    m_finished = false;
    m_undoAction->setEnabled(false);
    update();
    refresh();
}

// One push: slide, and only if something actually moved does a new tile
// appear. Spawning on a dead key would hand the player a tile for nothing,
// which is why slide() reports whether it moved anything.
void Twenty48View::push(Direction direction)
{
    if (!m_board.slide(direction)) {
        // refresh() emits the sentence; the on-board caption is only redrawn by
        // a repaint, and nothing else schedules one on this path. Without the
        // update the play surface -- the one surface read during play -- keeps
        // showing the previous sentence while the status bar changes.
        refresh(QStringLiteral("Nothing moves that way."));
        update();
        return;
    }

    m_board.spawn();
    m_undoAction->setEnabled(m_board.canUndo());
    Sound::instance().play(Sound::kCardPlace);
    update();
    refresh();
    checkEnd();
}

void Twenty48View::checkEnd()
{
    if (m_board.canMove() || m_finished)
        return;

    m_finished = true;
    Sound::instance().play(Sound::kLose);
    const bool newBest = Scores::instance().recordHigh(kBestKey, m_board.score());
    refresh();

    QTimer::singleShot(200, this, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("No moves left"));
        box.setText(QStringLiteral("The board is stuck."));
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

void Twenty48View::refresh(const QString& message)
{
    int highest = 0;
    for (int v : m_board.cells())
        highest = std::max(highest, v);

    QString line = message.isEmpty()
        ? QStringLiteral("%1   Score %2   Highest %3   Best %4")
              .arg(m_finished ? QStringLiteral("Stuck")
                              : m_board.reachedTarget()
                          ? QStringLiteral("Target reached — keep going!")
                          : QStringLiteral("Slide with the arrow keys"))
              .arg(m_board.score())
              .arg(highest)
              .arg(std::max(Scores::instance().best(kBestKey), m_board.score()))
        : message;
    Q_EMIT statusChanged(line);
}

void Twenty48View::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().window());

    // The caption's strip comes off the height first, so the sentence sits
    // under the board rather than over the bottom row of tiles.
    const double band = captionBand(QRectF(rect()));
    const double side = std::min(double(width()), height() - band) - 32.0;
    const QRectF board((width() - side) / 2, (height() - band - side) / 2, side, side);

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
            p.setFont(tileFont(v, cell));
            p.setPen(inkFor(v));
            p.drawText(box, Qt::AlignCenter, QString::number(v));
        }
    }

    paintStatusCaption(p, QRectF(rect()));
}

QFont Twenty48View::tileFont(int value, double cell) const
{
    const QString text = QString::number(value);
    QFont f = font();
    f.setBold(true);
    // Long numbers get a smaller face so 1024 still fits its tile.
    const double base = cell * (text.size() <= 2 ? 0.40 : text.size() == 3 ? 0.32 : 0.26);
    if (!Legibility::instance().enabled()) {
        f.setPointSizeF(base);
        return f;
    }

    // How much of the tile the ink may take. Both are the whole answer to
    // "how big can this go": beyond them the number touches its own tile edge.
    const double room = cell * 0.82;
    const double tall = cell * 0.62;

    QFont probe = f;
    probe.setPointSizeF(100.0);
    const QFontMetricsF pm(probe);
    const double perPointWide = pm.horizontalAdvance(text) / 100.0;
    const double perPointTall = pm.tightBoundingRect(text).height() / 100.0;
    if (perPointWide <= 0.0 || perPointTall <= 0.0) {
        f.setPointSizeF(base);
        return f;
    }

    // The analytic answer, then a few steps down for the rounding analysis
    // cannot see — the metric is asked again at the size it will really be
    // drawn at, which is what a tuned ratio gets wrong on another platform.
    double points = std::min(room / perPointWide, tall / perPointTall);
    for (int step = 0; step < 8 && points > base; ++step) {
        f.setPointSizeF(points);
        const QFontMetricsF fm(f);
        if (fm.horizontalAdvance(text) <= room && fm.tightBoundingRect(text).height() <= tall)
            return f;
        points *= 0.96;
    }
    f.setPointSizeF(base);
    return f;
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

    push(direction);
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
    if (m_finished || (m_board.score() == 0 && !m_board.canUndo()))
        return {};

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1) << qint32(m_board.score()) << m_board.reachedTarget();
    for (int value : m_board.cells())
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

    // Read into an array of its own and hand it to the core, which is what
    // decides whether it is a board this game could have produced -- every tile
    // a power of two, because nothing else comes out of a merge. A blob that
    // turns out to be nonsense leaves the game already on screen alone.
    std::array<int, kCells> cells {};
    for (int& cell : cells) {
        qint32 value = 0;
        in >> value;
        cell = value;
    }
    if (in.status() != QDataStream::Ok)
        return false;
    if (!m_board.restore(cells, score, reachedTarget))
        return false;

    m_finished = false;
    m_undoAction->setEnabled(false);
    update();
    refresh();
    return true;
}
