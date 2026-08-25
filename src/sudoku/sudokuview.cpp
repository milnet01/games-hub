#include "sudokuview.h"

#include "legibility.h"
#include "scores.h"
#include "sound.h"
#include "theme.h"

#include <QActionGroup>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QDataStream>
#include <QPainter>
#include <QPushButton>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {
constexpr QColor kPaper { 0xf6, 0xf3, 0xe8 };
constexpr QColor kPaperAlt { 0xe9, 0xe4, 0xd4 };
constexpr QColor kClueInk { 0x22, 0x26, 0x2b };
constexpr QColor kOwnInk { 0x1f, 0x6f, 0xb2 };
constexpr QColor kErrorInk { 0xc2, 0x39, 0x39 };
// Pencil marks. Named rather than written inline at the call site so
// scripts/legibility-check.py can read the value it measures out of the source.
constexpr QColor kPencilInk { 0x6d, 0x6a, 0x5e };
constexpr QColor kGridLine { 0x9a, 0x93, 0x82 };
constexpr QColor kGridHeavy { 0x3c, 0x38, 0x30 };
constexpr int kFrameWidth = 9;

// Nine pencil marks sit in a fixed 3x3 pattern inside one cell — 5 is always
// the middle — so each gets a third of the cell and the pattern is how you
// know which digit without reading it. Both ratios are of the cell, matching
// the 0.52 a real digit uses.
//
// kMarkRatio is about as large as the font's LINE box can be and still fit a
// cell third, which is why it stopped there. The legible size goes deliberately
// past it: a digit's ink is far shorter than the line box it is measured in, so
// the marks have room the line box says they do not. Qt::TextDontClip at the
// drawText below is what hands that room over — without it the glyph is clipped
// to its third and the larger font draws a worse mark, not a bigger one.
//
// **There is no legible RATIO, and that is the point.** A constant cannot be
// right on two platforms, because the ceiling depends on how tall the font
// draws a digit AND on how its rasteriser rounds that at the sizes a mark is
// actually drawn at — 7 to 11 points, where hinting moves the ink by a whole
// pixel at a time. A tuned 0.29 passed here and failed the Windows CI leg.
//
// Do not reason about this from typographic ratios alone. Measured at em 100
// the faces are close (this font 0.742 bold; on the owner's Windows box Segoe
// UI 0.728, Arial 0.731, Tahoma 0.760 unbold via GDI+), and the SAME font here
// measures about 0.685 at mark sizes — the gap is quantisation, not the
// typeface. That is precisely why the solve below measures at the size it will
// draw rather than scaling a ratio: the number that matters is the rounded one.
//
// So markFont() measures the font in hand and takes the largest size whose ink
// really fits, floored at kMarkRatio, which has always fitted.
constexpr double kMarkRatio = 0.20;
// How much of its third of the cell a mark's ink may fill. The remainder is the
// paper that keeps nine marks reading as a 3x3 pattern rather than a block.
constexpr double kMarkInkShare = 0.85;

// The drawn height of the digits, which is shorter than the line box they are
// measured in and is the only number the fit actually depends on.
double markInkHeight(const QFont& f)
{
    return QFontMetricsF(f).tightBoundingRect(QStringLiteral("0123456789")).height();
}

QString levelKey(SudokuGrid::Level level)
{
    switch (level) {
    case SudokuGrid::Level::Easy:   return QStringLiteral("sudoku/best_time_easy");
    case SudokuGrid::Level::Medium: return QStringLiteral("sudoku/best_time_medium");
    case SudokuGrid::Level::Hard:   return QStringLiteral("sudoku/best_time_hard");
    }
    return {};
}
}

SudokuView::SudokuView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(minimumSizeHint());
    setFocusPolicy(Qt::StrongFocus);

    m_tick = new QTimer(this);
    m_tick->setInterval(500);
    connect(m_tick, &QTimer::timeout, this, [this] { refresh(); });

    buildActions();
    newGame(m_level);
}

void SudokuView::buildActions()
{
    auto* newAction = new QAction(QStringLiteral("New Puzzle"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this] { newGame(m_level); });
    m_actions.append(newAction);

    auto* restart = new QAction(QStringLiteral("Restart"), this);
    connect(restart, &QAction::triggered, this, [this] {
        m_grid.restart();
        m_solved = false;
        m_announced = false;
        m_elapsedMs = 0;
        m_clock.restart();
        update();
        refresh();
    });
    m_actions.append(restart);

    m_pauseAction = new QAction(QStringLiteral("Pause"), this);
    m_pauseAction->setCheckable(true);
    connect(m_pauseAction, &QAction::toggled, this, [this](bool on) {
        if (on == m_paused)
            return;
        if (on)
            m_elapsedMs = elapsedMs(); // bank it, then stop reading the clock
        else
            m_clock.restart();
        m_paused = on;
        update();
        refresh();
    });
    m_actions.append(m_pauseAction);

    auto* pencil = new QAction(QStringLiteral("Pencil"), this);
    pencil->setCheckable(true);
    pencil->setShortcut(QKeySequence(Qt::Key_P));
    connect(pencil, &QAction::toggled, this, [this](bool on) {
        m_pencil = on;
        refresh();
    });
    m_actions.append(pencil);

    auto* errors = new QAction(QStringLiteral("Show Errors"), this);
    errors->setCheckable(true);
    errors->setChecked(true);
    connect(errors, &QAction::toggled, this, [this](bool on) {
        m_highlightErrors = on;
        update();
    });
    m_actions.append(errors);

    auto* sep = new QAction(this);
    sep->setSeparator(true);
    m_actions.append(sep);

    auto* group = new QActionGroup(this);
    group->setExclusive(true);
    const struct { const char* name; SudokuGrid::Level level; } kLevels[] = {
        { "Easy", SudokuGrid::Level::Easy },
        { "Medium", SudokuGrid::Level::Medium },
        { "Hard", SudokuGrid::Level::Hard },
    };
    for (const auto& entry : kLevels) {
        auto* a = new QAction(QString::fromUtf8(entry.name), this);
        a->setCheckable(true);
        a->setChecked(entry.level == m_level);
        group->addAction(a);
        const SudokuGrid::Level level = entry.level;
        connect(a, &QAction::triggered, this, [this, level] { newGame(level); });
        m_actions.append(a);
    }
}

qint64 SudokuView::elapsedMs() const
{
    return m_elapsedMs + ((m_paused || m_suspended) ? 0 : m_clock.elapsed());
}

void SudokuView::newGame(SudokuGrid::Level level)
{
    m_level = level;
    m_grid.generate(level);
    m_solved = false;
    m_announced = false;
    m_paused = false;
    m_suspended = false;
    m_elapsedMs = 0;
    if (m_pauseAction != nullptr)
        m_pauseAction->setChecked(false);
    m_row = 4;
    m_col = 4;
    m_clock.start();
    m_tick->start();
    setFocus();
    update();
    refresh();
}

void SudokuView::activate()
{
    // Pick the clock up where it was left, not where it would have got to on
    // its own while another game was on screen.
    if (m_suspended) {
        m_suspended = false;
        m_clock.restart();
        if (!m_solved)
            m_tick->start();
    }
    setFocus();
    refresh();
}

void SudokuView::deactivate()
{
    if (m_suspended)
        return;
    m_elapsedMs = elapsedMs();
    m_suspended = true;
    m_tick->stop();
}

double SudokuView::cellSize() const
{
    const int available = std::min(width(), height()) - 2 * (kFrameWidth + 6);
    return std::max(12.0, std::floor(available / double(SudokuGrid::kSize)));
}

QRect SudokuView::boardRect() const
{
    const int side = int(cellSize()) * SudokuGrid::kSize;
    return { (width() - side) / 2, (height() - side) / 2, side, side };
}

// The one place the mark font is decided, so paintEvent and the two accessors
// below cannot disagree about what is being drawn. Bold as well as bigger:
// at this size stroke weight buys more than the extra points do.
QFont SudokuView::markFont() const
{
    const bool large = Legibility::instance().enabled();
    const double cell = cellSize();
    QFont f = font();
    f.setBold(large);
    if (!large) {
        f.setPointSizeF(cell * kMarkRatio);
        return f;
    }

    // One measurement fixes how much ink this font spends per point, which is
    // the whole of the analytic answer. The steps after it pay only for the
    // rounding that analysis cannot see — and rounding is exactly what broke a
    // tuned constant, so the loop asks the metric at the size it will really be
    // drawn at rather than trusting the arithmetic.
    const double room = cell / 3.0 * kMarkInkShare;
    const double floor = cell * kMarkRatio;
    QFont probe = f;
    probe.setPointSizeF(100.0);
    const double inkPerPoint = markInkHeight(probe) / 100.0;

    double points = inkPerPoint > 0.0 ? room / inkPerPoint : floor;
    for (int step = 0; step < 12 && points > floor; ++step) {
        f.setPointSizeF(points);
        if (markInkHeight(f) <= room)
            return f;
        points *= 0.96;
    }
    f.setPointSizeF(floor);
    return f;
}

double SudokuView::markPointSize() const
{
    return markFont().pointSizeF();
}

// Whether a mark at this size would clear the cell third it is centred in, with
// paper left over so two marks in adjacent rows read apart rather than merely
// failing to overlap. Takes a size the view did NOT choose so a test can ask
// about one — which is how "as large as it fits" gets checked rather than the
// far weaker "it fits", the only thing markFont() could fail to satisfy now
// that it solves for the answer.
bool SudokuView::marksFitAt(double pointSize) const
{
    QFont f = markFont();
    f.setPointSizeF(pointSize);
    return markInkHeight(f) <= cellSize() / 3.0 * kMarkInkShare;
}

bool SudokuView::marksFitCell() const
{
    return marksFitAt(markPointSize());
}

void SudokuView::enter(int digit)
{
    if (m_solved || m_grid.isClue(m_row, m_col))
        return;

    if (m_pencil && digit != 0) {
        m_grid.toggleMark(m_row, m_col, digit);
        Sound::instance().play(Sound::kDig);
    } else {
        m_grid.set(m_row, m_col, digit);
        Sound::instance().play(digit == 0 ? Sound::kBack : Sound::kDiscPlace);
    }

    update();
    refresh();
    checkSolved();
}

void SudokuView::checkSolved()
{
    if (!m_grid.solved() || m_solved)
        return;

    m_solved = true;
    m_tick->stop();
    const int seconds = int(elapsedMs() / 1000);
    Sound::instance().play(Sound::kWin);
    const bool newBest = Scores::instance().recordLow(levelKey(m_level), seconds);
    refresh();

    if (m_announced)
        return;
    m_announced = true;
    QTimer::singleShot(200, this, [this, seconds, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("Solved"));
        box.setText(QStringLiteral("Puzzle complete!"));
        box.setInformativeText(
            newBest ? QStringLiteral("Time: %1 seconds — a new best!").arg(seconds)
                    : QStringLiteral("Time: %1 seconds.   Best: %2.")
                          .arg(seconds)
                          .arg(Scores::instance().best(levelKey(m_level), seconds)));
        QAbstractButton* again = box.addButton(QStringLiteral("New Puzzle"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Close"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == again)
            newGame(m_level);
    });
}

void SudokuView::refresh(const QString& message)
{
    const int seconds = int(elapsedMs() / 1000);
    QString line = message.isEmpty()
        ? QStringLiteral("%1   Empty %2   Time %3s%4")
              .arg(m_solved ? QStringLiteral("Solved!") : QStringLiteral("Sudoku"))
              .arg(m_grid.emptyCount())
              .arg(seconds)
              .arg(m_pencil ? QStringLiteral("   Pencil mode") : QString())
        : message;
    if (Scores::instance().has(levelKey(m_level)))
        line += QStringLiteral("   Best %1s").arg(Scores::instance().best(levelKey(m_level)));
    Q_EMIT statusChanged(line);
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void SudokuView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().window());

    const QRect r = boardRect();
    const double cell = cellSize();
    Theme::paintWoodFrame(p, r, kFrameWidth, 6);
    p.fillRect(r, kPaper);

    // Paused covers the grid: a stopped clock over a puzzle you can still read
    // is not a pause, it is free thinking time.
    if (m_paused) {
        p.fillRect(r, kPaperAlt.darker(112));
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(std::max(11.0, r.width() * 0.045));
        p.setFont(f);
        p.setPen(kClueInk);
        p.drawText(r, Qt::AlignCenter, QStringLiteral("Paused\nPress Pause again to carry on"));
        return;
    }

    // Shade alternate 3x3 boxes so the structure reads without heavy lines.
    for (int br = 0; br < 3; ++br)
        for (int bc = 0; bc < 3; ++bc)
            if ((br + bc) % 2 == 0)
                p.fillRect(QRectF(r.x() + bc * 3 * cell, r.y() + br * 3 * cell, cell * 3, cell * 3),
                           kPaperAlt);

    const int selected = m_grid.value(m_row, m_col);

    for (int row = 0; row < SudokuGrid::kSize; ++row) {
        for (int col = 0; col < SudokuGrid::kSize; ++col) {
            const QRectF box(r.x() + col * cell, r.y() + row * cell, cell, cell);

            // Highlight the row, column and box of the cursor, plus every cell
            // holding the same digit — the two hints that make scanning quick.
            const bool sameLine = row == m_row || col == m_col
                || ((row / 3 == m_row / 3) && (col / 3 == m_col / 3));
            if (sameLine)
                p.fillRect(box, QColor(0x1f, 0x6f, 0xb2, 22));
            if (selected != 0 && m_grid.value(row, col) == selected)
                p.fillRect(box, QColor(0xff, 0xd5, 0x4f, 55));
        }
    }

    const QRectF cursor(r.x() + m_col * cell, r.y() + m_row * cell, cell, cell);
    p.fillRect(cursor, QColor(0x1f, 0x6f, 0xb2, 45));

    p.setPen(QPen(kGridLine, 1));
    for (int i = 1; i < SudokuGrid::kSize; ++i) {
        if (i % 3 == 0)
            continue;
        p.drawLine(QPointF(r.x() + i * cell, r.y()), QPointF(r.x() + i * cell, r.bottom()));
        p.drawLine(QPointF(r.x(), r.y() + i * cell), QPointF(r.right(), r.y() + i * cell));
    }
    p.setPen(QPen(kGridHeavy, std::max(2.0, cell * 0.06)));
    for (int i = 0; i <= SudokuGrid::kSize; i += 3) {
        p.drawLine(QPointF(r.x() + i * cell, r.y()), QPointF(r.x() + i * cell, r.bottom()));
        p.drawLine(QPointF(r.x(), r.y() + i * cell), QPointF(r.right(), r.y() + i * cell));
    }

    QFont digitFont = font();
    digitFont.setPointSizeF(cell * 0.52);
    const QFont pencilFont = markFont();

    for (int row = 0; row < SudokuGrid::kSize; ++row) {
        for (int col = 0; col < SudokuGrid::kSize; ++col) {
            const QRectF box(r.x() + col * cell, r.y() + row * cell, cell, cell);
            const int v = m_grid.value(row, col);

            if (v != 0) {
                const bool clue = m_grid.isClue(row, col);
                const bool bad = m_highlightErrors && !clue && m_grid.conflicts(row, col);
                digitFont.setBold(clue);
                p.setFont(digitFont);
                p.setPen(bad ? kErrorInk : (clue ? kClueInk : kOwnInk));
                p.drawText(box, Qt::AlignCenter, QString::number(v));
                continue;
            }

            const std::uint16_t marks = m_grid.marks(row, col);
            if (marks == 0)
                continue;
            p.setFont(pencilFont);
            p.setPen(kPencilInk);
            for (int d = 1; d <= 9; ++d) {
                if (!(marks & (1u << (d - 1))))
                    continue;
                const int mr = (d - 1) / 3;
                const int mc = (d - 1) % 3;
                // TextDontClip: the mark is centred on its third but allowed to
                // paint outside it, because the font's line box is taller than
                // the digit's ink. Without this the legible size is clipped to
                // the third and loses the top of every mark.
                p.drawText(QRectF(box.x() + mc * cell / 3, box.y() + mr * cell / 3, cell / 3, cell / 3),
                           Qt::AlignCenter | Qt::TextDontClip, QString::number(d));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void SudokuView::mousePressEvent(QMouseEvent* event)
{
    if (m_paused)
        return;
    const QRect r = boardRect();
    if (!r.contains(event->position().toPoint()))
        return;
    const double cell = cellSize();
    m_col = std::clamp(int((event->position().x() - r.x()) / cell), 0, SudokuGrid::kSize - 1);
    m_row = std::clamp(int((event->position().y() - r.y()) / cell), 0, SudokuGrid::kSize - 1);
    setFocus();
    update();
    refresh();
}

void SudokuView::keyPressEvent(QKeyEvent* event)
{
    if (m_paused) {
        QWidget::keyPressEvent(event);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Left:  m_col = std::max(0, m_col - 1); break;
    case Qt::Key_Right: m_col = std::min(SudokuGrid::kSize - 1, m_col + 1); break;
    case Qt::Key_Up:    m_row = std::max(0, m_row - 1); break;
    case Qt::Key_Down:  m_row = std::min(SudokuGrid::kSize - 1, m_row + 1); break;
    case Qt::Key_Backspace:
    case Qt::Key_Delete:
    case Qt::Key_0:
        enter(0);
        return;
    default:
        if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9) {
            enter(event->key() - Qt::Key_0);
            return;
        }
        GameView::keyPressEvent(event);
        return;
    }
    update();
    refresh();
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

QByteArray SudokuView::saveState() const
{
    // A solved puzzle is not worth coming back to; New Game is the answer to
    // that. An empty state also clears whatever was stored before.
    if (m_solved)
        return {};

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1);
    m_grid.save(out);
    out << qint8(m_level) << qint8(m_row) << qint8(m_col) << qint8(m_pencil ? 1 : 0)
        << qint8(m_highlightErrors ? 1 : 0) << qint8(m_announced ? 1 : 0)
        << qint8(m_paused ? 1 : 0);
    // elapsedMs() rather than m_elapsedMs: the banked figure is missing
    // whatever the clock has run since it was banked, so saving it would give
    // the player back time they had already spent.
    out << qint64(elapsedMs());
    return blob;
}

bool SudokuView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    in >> version;
    if (version != 1 || in.status() != QDataStream::Ok)
        return false;

    SudokuGrid grid;
    if (!grid.load(in))
        return false;

    qint8 level = 0;
    qint8 row = 0;
    qint8 col = 0;
    qint8 pencil = 0;
    qint8 highlight = 0;
    qint8 announced = 0;
    qint8 paused = 0;
    qint64 elapsed = 0;
    in >> level >> row >> col >> pencil >> highlight >> announced >> paused >> elapsed;
    if (in.status() != QDataStream::Ok)
        return false;

    if (level < qint8(SudokuGrid::Level::Easy) || level > qint8(SudokuGrid::Level::Hard))
        return false;
    if (row < 0 || row >= SudokuGrid::kSize || col < 0 || col >= SudokuGrid::kSize)
        return false;
    for (qint8 flag : { pencil, highlight, announced, paused }) {
        if (flag != 0 && flag != 1)
            return false;
    }
    // A negative time is not a time, and a puzzle nobody has spent a century on
    // is not one this game produced.
    if (elapsed < 0 || elapsed > qint64(100) * 365 * 24 * 60 * 60 * 1000)
        return false;
    // A solved grid would have been saved as "nothing worth keeping", so a blob
    // holding one did not come from saveState().
    if (grid.solved())
        return false;

    m_grid = grid;
    m_level = SudokuGrid::Level(level);
    m_row = row;
    m_col = col;
    m_pencil = pencil == 1;
    m_highlightErrors = highlight == 1;
    m_announced = announced == 1;
    m_paused = paused == 1;
    m_solved = false;
    m_elapsedMs = elapsed;

    // Suspended, with the clock banked: the hub calls activate() straight
    // after this and that is what picks it up. Starting it here as well would
    // charge the player for the moment in between.
    m_suspended = true;
    m_tick->stop();
    if (m_pauseAction != nullptr)
        m_pauseAction->setChecked(m_paused);
    update();
    refresh();
    return true;
}
