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
// cell third, which is why it stopped there. kMarkRatioLegible is deliberately
// past that: a digit's ink is far shorter than the line box it is measured in,
// so the marks have room the line box says they do not. Qt::TextDontClip at
// the drawText below is what hands that room over — without it the glyph is
// clipped to its third and the larger font draws a worse mark, not a bigger
// one. marksFitCell() is what holds the ratio to ink that really fits.
//
// 0.29 is measured rather than chosen. The app font's digits are about 0.685
// of an em, so ink comes to roughly 2.74 x the ratio as a fraction of the cell
// third: 0.29 lands at 0.795 and 0.30 at 0.822, and at the smallest window the
// whole-pixel rounding of a 34-pixel cell pushes 0.30 to 0.882 — past what
// marksFitCell() allows. **A platform whose digits are more than about 0.73 of
// an em would fail that check rather than draw marks that touch**, which is the
// intended direction and is worth knowing: Windows has produced font and
// library differences here before, and the fix is this constant, not the test.
constexpr double kMarkRatio = 0.20;
constexpr double kMarkRatioLegible = 0.29;

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
    QFont f = font();
    f.setPointSizeF(cellSize() * (large ? kMarkRatioLegible : kMarkRatio));
    f.setBold(large);
    return f;
}

double SudokuView::markPointSize() const
{
    return markFont().pointSizeF();
}

// A mark fits when its INK — not its line box — clears the cell third it is
// centred in, with room left over so two marks in adjacent rows have visible
// paper between them rather than merely failing to overlap. tightBoundingRect
// is the ink; the 0.85 leaves a seventh of the third as that paper. Measured
// over all ten digits because they are not the same height in every font.
//
// The bar is deliberately not a restatement of kMarkRatioLegible: it is set
// where the marks stop reading as a 3x3 pattern, so it still has something to
// say if the ratio, the font or the cell arithmetic moves.
bool SudokuView::marksFitCell() const
{
    const QFontMetricsF fm(markFont());
    const double ink = fm.tightBoundingRect(QStringLiteral("0123456789")).height();
    const double third = cellSize() / 3.0;
    return ink <= third * 0.85;
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
