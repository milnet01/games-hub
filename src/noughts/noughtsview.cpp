#include "noughtsview.h"

#include "dealseed.h"
#include "legibility.h"
#include "sound.h"
#include "theme.h"

#include <QActionGroup>
#include <QDataStream>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>

#include <algorithm>

using noughts::Mark;

namespace {

// Warm and cool, both bright against the dark felt, so the two marks differ in
// colour as well as shape.
constexpr QColor kCrossInk { 0xff, 0x8a, 0x75 };
constexpr QColor kNoughtInk { 0x8f, 0xd8, 0xff };
constexpr QColor kGridInk { 0xe8, 0xf0, 0xe0 };

constexpr int kFrameWidth = 10;
constexpr quint32 kBlobVersion = 1;

// Nearly a second, so the computer's mark is seen arriving rather than found
// already there.
constexpr int kThinkDelayMs = 800;
constexpr int kTickMs = 16;
constexpr double kMarkDrawMs = 220.0;
constexpr double kLineDrawMs = 450.0;

const QString& opponentKey()
{
    static const QString key = QStringLiteral("noughts/opponent"); // untranslated: settings key
    return key;
}

} // namespace

NoughtsView::NoughtsView(QWidget* parent)
    : GameView(parent)
    , m_rng(dealSeed())
{
    setMinimumSize(NoughtsView::minimumSizeHint());
    // GHUB-0168: without this the board takes no keys at all.
    setFocusPolicy(Qt::StrongFocus);
    m_timer = new QTimer(this);
    m_timer->setInterval(kTickMs);
    connect(m_timer, &QTimer::timeout, this, &NoughtsView::step);

    // Read before the toolbar is built, so the remembered choice is the one
    // ticked.
    const int stored = QSettings().value(opponentKey(), 0).toInt();
    if (stored >= int(Opponent::Easy) && stored <= int(Opponent::TwoPlayers))
        m_opponent = Opponent(stored);
    buildActions();
    newGame();
}

void NoughtsView::buildActions()
{
    auto* newAction = new QAction(tr("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &NoughtsView::newGame);
    m_actions.append(newAction);

    m_undoAction = new QAction(tr("Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &NoughtsView::undo);
    m_actions.append(m_undoAction);

    auto* separator = new QAction(this);
    separator->setSeparator(true);
    m_actions.append(separator);

    m_opponentGroup = new QActionGroup(this);
    m_opponentGroup->setExclusive(true);
    const struct { const char* name; Opponent value; } kOpponents[] = {
        { QT_TRANSLATE_NOOP("NoughtsView", "Easy"), Opponent::Easy },
        { QT_TRANSLATE_NOOP("NoughtsView", "Hard"), Opponent::Hard },
        { QT_TRANSLATE_NOOP("NoughtsView", "Two players"), Opponent::TwoPlayers },
    };
    for (const auto& entry : kOpponents) {
        auto* a = new QAction(tr(entry.name), this);
        a->setCheckable(true);
        a->setChecked(entry.value == m_opponent);
        m_opponentGroup->addAction(a);
        const Opponent value = entry.value;
        connect(a, &QAction::triggered, this, [this, value] {
            // The one point every explicit choice passes through, so it is
            // where the choice is remembered. restoreState() does not come
            // here: resuming a game must not re-point the preference.
            QSettings().setValue(opponentKey(), int(value));
            setOpponent(value);
        });
        m_actions.append(a);
    }
}

// GHUB-0016. The rules as noughts::Board enforces them and the controls as
// mousePressEvent, keyPressEvent and buildActions take them.
QString NoughtsView::rulesText() const
{
    return tr(
        "<h3>The aim</h3>"
        "<p>Get three of your marks in a row: across, up and down, or corner to "
        "corner.</p>"

        "<h3>How it plays</h3>"
        "<p>The board is 3 by 3 squares. One side plays crosses (X) and the other "
        "plays noughts (O). Against the computer you are always the crosses.</p>"
        "<p>The two sides take turns to put one mark on an empty square. Who goes "
        "first swaps every game.</p>"
        "<p>The first to make three in a row wins, and a line is drawn through "
        "them. If the board fills up with no three in a row, the game is a "
        "draw.</p>"
        "<p>On <b>Easy</b> the computer makes mistakes you can win from. On "
        "<b>Hard</b> it plays perfectly and never loses, so the best you can do "
        "is a draw. <b>Two players</b> is for two people taking turns at the "
        "same screen.</p>"
        "<p>The score for this session is in the status line: wins on each side "
        "and draws. It starts again when you change the opponent.</p>"

        "<h3>Controls</h3>"
        "<p><b>With the mouse:</b></p>"
        "<ul>"
        "<li>Click an empty square to put your mark there.</li>"
        "<li>When a game is over, click the board to start the next one.</li>"
        "</ul>"
        "<p><b>With the keyboard:</b></p>"
        "<ul>"
        "<li>The arrow keys move a frame from square to square. The frame shows "
        "once you press a key, and hides again when you click.</li>"
        "<li>Space or Enter puts your mark in the square inside the frame, or "
        "starts the next game once one is over.</li>"
        "</ul>"
        "<p><b>On the toolbar:</b></p>"
        "<ul>"
        "<li><b>New Game</b> (Ctrl+N) clears the board.</li>"
        "<li><b>Undo</b> (Ctrl+Z) takes back your last move, and the computer's "
        "reply to it. With two players it takes back one move.</li>"
        "<li><b>Easy</b>, <b>Hard</b> and <b>Two players</b> choose who you play "
        "against. The choice is remembered.</li>"
        "</ul>");
}

void NoughtsView::activate()
{
    m_paused = false;
    m_turnFades = true;
    if (m_turn.moving())
        m_timer->start();
    // Picks the computer's reply back up if leaving the game abandoned it.
    advance();
}

void NoughtsView::deactivate()
{
    m_paused = true;
    abandonThinking();
    // The cross-fade freezes; a mark or line being drawn is finished, since
    // there is nothing to come back to half-way through one.
    m_turnFades = false;
    m_markDrawn = 1.0;
    m_lineDrawn = 1.0;
    m_timer->stop();
    update();
}

bool NoughtsView::hasPendingAnimation() const
{
    return m_markDrawn < 1.0 || m_lineDrawn < 1.0;
}

void NoughtsView::abandonThinking()
{
    ++m_generation;
    m_thinking = false;
}

void NoughtsView::newGame()
{
    abandonThinking();
    m_board = noughts::Board();
    m_starter = m_nextStarter;
    m_nextStarter = noughts::other(m_nextStarter);
    m_history.clear();
    m_finished = false;
    m_resumed = false;
    m_newestCell = -1;
    m_markDrawn = 1.0;
    m_lineDrawn = 1.0;
    m_undoAction->setEnabled(false);
    advance();
}

void NoughtsView::setOpponent(Opponent opponent)
{
    if (opponent == m_opponent)
        return;
    abandonThinking();
    m_opponent = opponent;
    // A different contest, so a different score.
    m_crossWins = m_noughtWins = m_draws = 0;
    // The board stays: a game in progress carries on against whoever is now
    // playing the noughts.
    advance();
}

void NoughtsView::undo()
{
    if (m_history.empty() || m_finished)
        return;
    abandonThinking();
    // Back to the last position where a person was to move, so one Undo takes
    // back your move and the computer's reply together.
    do {
        m_board = m_history.back();
        m_history.pop_back();
    } while (!m_history.empty() && againstComputer() && toMove() != Mark::Cross);
    m_newestCell = -1;
    m_markDrawn = 1.0;
    m_undoAction->setEnabled(!m_history.empty());
    advance();
}

void NoughtsView::advance()
{
    if (m_finished) {
        refresh();
        return;
    }
    if (m_board.over()) {
        finish();
        return;
    }
    if (computerToMove() && !m_paused && !m_thinking) {
        m_thinking = true;
        const quint64 generation = m_generation;
        QTimer::singleShot(kThinkDelayMs, this, [this, generation] {
            if (generation != m_generation || !m_thinking || m_paused)
                return;
            playComputer();
        });
    }
    refresh();
}

void NoughtsView::playComputer()
{
    m_thinking = false;
    if (!computerToMove())
        return;
    const noughts::Level level =
        m_opponent == Opponent::Hard ? noughts::Level::Hard : noughts::Level::Easy;
    const int cell = noughts::chooseMove(m_board, Mark::Nought, level, m_rng);
    if (cell < 0)
        return;
    place(cell);
    advance();
}

void NoughtsView::playAt(int cell)
{
    if (m_finished) {
        newGame();
        return;
    }
    if (m_thinking || computerToMove() || !noughts::Board::inBounds(cell)
        || m_board.at(cell) != Mark::Empty)
        return;
    place(cell);
    advance();
}

void NoughtsView::place(int cell)
{
    m_history.push_back(m_board);
    m_board.play(cell, toMove());
    m_undoAction->setEnabled(true);
    Sound::instance().play(Sound::kDiscPlace);
    m_newestCell = cell;
    m_markDrawn = 0.0;
    m_lineDrawn = m_board.winner() != Mark::Empty ? 0.0 : 1.0;
    m_timer->start();
}

bool NoughtsView::advanceForShot(int turns)
{
    abandonThinking();
    for (int i = 0; i < turns && !m_board.over(); ++i) {
        const Mark side = toMove();
        m_history.push_back(m_board);
        m_board.play(noughts::chooseMove(m_board, side, noughts::Level::Easy, m_rng), side);
    }
    m_newestCell = -1;
    m_markDrawn = 1.0;
    m_lineDrawn = 1.0;
    m_timer->stop();
    m_undoAction->setEnabled(!m_history.empty());
    if (m_board.over())
        finish(false);
    else
        refresh();
    m_turn.land();
    return true;
}

void NoughtsView::finish(bool loud)
{
    m_finished = true;
    m_undoAction->setEnabled(false);
    const Mark winner = m_board.winner();
    if (winner == Mark::Cross)
        ++m_crossWins;
    else if (winner == Mark::Nought)
        ++m_noughtWins;
    else
        ++m_draws;
    if (loud && winner != Mark::Empty)
        Sound::instance().play(againstComputer() && winner == Mark::Nought ? Sound::kLose
                                                                            : Sound::kWin);
    refresh();
}

QString NoughtsView::boardSentence() const
{
    const Mark winner = m_board.winner();
    if (m_finished) {
        QString result;
        if (winner == Mark::Empty)
            result = tr("A draw.");
        else if (againstComputer())
            result = winner == Mark::Cross ? tr("You win!") : tr("The computer wins.");
        else
            result = winner == Mark::Cross ? tr("Crosses win!") : tr("Noughts win!");
        return tr("%1 Click for the next game.").arg(result);
    }
    if (againstComputer())
        return toMove() == Mark::Cross ? tr("Your turn. You are the crosses.")
                                       : tr("The computer is thinking…");
    return toMove() == Mark::Cross ? tr("Crosses to play.") : tr("Noughts to play.");
}

void NoughtsView::refresh()
{
    m_turn.setSeat(m_finished ? -1 : (toMove() == Mark::Cross ? 0 : 1), m_turnFades);
    if (m_turnFades && m_turn.moving())
        m_timer->start();
    update();

    m_score = againstComputer()
        ? tr("You %1, computer %2, drawn %3").arg(m_crossWins).arg(m_noughtWins).arg(m_draws)
        : tr("Crosses %1, noughts %2, drawn %3").arg(m_crossWins).arg(m_noughtWins).arg(m_draws);
    Q_EMIT statusChanged(boardSentence() + QStringLiteral("   ") + m_score);
}

void NoughtsView::step()
{
    const bool lightMoving = m_turnFades && m_turn.step(kTickMs);
    if (m_markDrawn < 1.0)
        m_markDrawn = std::min(1.0, m_markDrawn + kTickMs / kMarkDrawMs);
    else if (m_lineDrawn < 1.0)
        m_lineDrawn = std::min(1.0, m_lineDrawn + kTickMs / kLineDrawMs);
    if (!lightMoving && !hasPendingAnimation())
        m_timer->stop();
    update();
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

namespace {
// The strip under the board that carries the sentence, as a share of the room.
// Larger under the legibility switch, like everything else on the board.
int sentenceHeight(int room)
{
    const double share = Legibility::instance().enabled() ? 0.11 : 0.08;
    return std::clamp(int(room * share), 22, 72);
}
} // namespace

QRect NoughtsView::boardRect() const
{
    // From the bottom: the caption band, the sentence, the two turn bands
    // either side of the frame, then the board, kept on whole pixels per cell
    // so the grid lines stay crisp.
    const int room = height() - int(captionBand(QRectF(rect())));
    const int depth = Theme::turnBandDepth(room);
    const int sentence = sentenceHeight(room);
    const int available =
        std::min(width(), room - 2 * depth - sentence) - 2 * (kFrameWidth + 4);
    const int side = std::max(noughts::kSize, (available / noughts::kSize) * noughts::kSize);
    return { (width() - side) / 2, (room - sentence - side) / 2, side, side };
}

QRectF NoughtsView::turnBand(int seat) const
{
    const QRect r = boardRect();
    if (r.width() <= 0)
        return {};
    const QRectF frame =
        QRectF(r).adjusted(-kFrameWidth, -kFrameWidth, kFrameWidth, kFrameWidth);
    const double depth = Theme::turnBandDepth(height() - int(captionBand(QRectF(rect()))));
    // As Reversi's: three times the depth, two thirds of it hidden behind the
    // board. Crosses below the board, where you sit; noughts above.
    if (seat == 0)
        return { frame.left() - depth, frame.bottom() - 2 * depth, frame.width() + 2 * depth,
                 depth * 3 };
    return { frame.left() - depth, frame.top() - depth, frame.width() + 2 * depth, depth * 3 };
}

QRectF NoughtsView::sentenceRect() const
{
    const QRect r = boardRect();
    const int room = height() - int(captionBand(QRectF(rect())));
    const double top = r.bottom() + kFrameWidth + Theme::turnBandDepth(room) + 2;
    return { 0.0, top, double(width()), double(sentenceHeight(room)) };
}

QRectF NoughtsView::cellRect(int cell) const
{
    const QRect r = boardRect();
    const double size = r.width() / double(noughts::kSize);
    const int row = cell / noughts::kSize;
    const int col = cell % noughts::kSize;
    return { r.x() + col * size, r.y() + row * size, size, size };
}

int NoughtsView::cellAt(QPointF pos) const
{
    const QRect r = boardRect();
    if (r.width() <= 0 || !r.contains(pos.toPoint()))
        return -1;
    const double size = r.width() / double(noughts::kSize);
    const int col = std::clamp(int((pos.x() - r.x()) / size), 0, noughts::kSize - 1);
    const int row = std::clamp(int((pos.y() - r.y()) / size), 0, noughts::kSize - 1);
    return row * noughts::kSize + col;
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void NoughtsView::paintMark(QPainter& p, const QRectF& cell, Mark mark, double drawn) const
{
    const bool legible = Legibility::instance().enabled();
    const double inset = cell.width() * 0.2;
    const QRectF box = cell.adjusted(inset, inset, -inset, -inset);
    const double pen = cell.width() * (legible ? 0.14 : 0.11);

    QPainterPath path;
    if (mark == Mark::Cross) {
        // Two strokes, the second starting once the first is down.
        const double first = std::min(1.0, drawn * 2.0);
        const double second = std::clamp(drawn * 2.0 - 1.0, 0.0, 1.0);
        path.moveTo(box.topLeft());
        path.lineTo(box.topLeft() + (box.bottomRight() - box.topLeft()) * first);
        if (second > 0.0) {
            path.moveTo(box.topRight());
            path.lineTo(box.topRight() + (box.bottomLeft() - box.topRight()) * second);
        }
    } else {
        // Round from the top, clockwise, the way a hand draws it.
        path.arcMoveTo(box, 90.0);
        path.arcTo(box, 90.0, -360.0 * drawn);
    }

    // A soft shadow under the ink lifts it off the felt, the way Reversi's
    // contact shadow does for a disc.
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0, 0, 0, 80), pen, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path.translated(pen * 0.18, pen * 0.28));
    p.setPen(QPen(mark == Mark::Cross ? kCrossInk : kNoughtInk, pen, Qt::SolidLine,
                  Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
}

void NoughtsView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().window());

    const QRect r = boardRect();
    if (r.width() <= 0)
        return;

    const bool legible = Legibility::instance().enabled();
    const TurnLight lit = m_turn.value();
    if (lit.leavingSeat >= 0)
        Theme::paintTurnLight(p, turnBand(lit.leavingSeat), lit.leavingLevel, legible);
    if (lit.seat >= 0)
        Theme::paintTurnLight(p, turnBand(lit.seat), lit.level, legible);

    Theme::paintWoodFrame(p, r, kFrameWidth, 7);
    Theme::paintFelt(p, r, Theme::kFeltGreenTop, Theme::kFeltGreenBottom);

    // The grid: two lines each way, stopping short of the frame so the board
    // reads as the familiar hash rather than nine boxes.
    const double size = r.width() / double(noughts::kSize);
    const double margin = size * 0.12;
    p.setPen(QPen(kGridInk, std::max(3.0, size * (legible ? 0.06 : 0.045)), Qt::SolidLine,
                  Qt::RoundCap));
    for (int i = 1; i < noughts::kSize; ++i) {
        const double x = r.x() + i * size;
        const double y = r.y() + i * size;
        p.drawLine(QPointF(x, r.top() + margin), QPointF(x, r.bottom() - margin));
        p.drawLine(QPointF(r.left() + margin, y), QPointF(r.right() - margin, y));
    }

    for (int cell = 0; cell < noughts::kCells; ++cell) {
        const Mark m = m_board.at(cell);
        if (m != Mark::Empty)
            paintMark(p, cellRect(cell), m, cell == m_newestCell ? m_markDrawn : 1.0);
    }

    // The winning line, drawn through the three once the last mark is down:
    // white over a dark edge, so it reads on felt and on both inks.
    const std::optional<noughts::Line> line = m_board.winningLine();
    if (line && m_markDrawn >= 1.0 && m_lineDrawn > 0.0) {
        const QPointF a = cellRect((*line)[0]).center();
        const QPointF b = cellRect((*line)[2]).center();
        const QPointF reach = (b - a) * (0.35 / 2.0);
        const QPointF from = a - reach;
        const QPointF to = from + ((b + reach) - from) * m_lineDrawn;
        const double pen = size * (legible ? 0.12 : 0.09);
        p.setPen(QPen(QColor(0x10, 0x20, 0x18, 200), pen * 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(from, to);
        p.setPen(QPen(QColor(0xff, 0xff, 0xff), pen, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(from, to);
    }

    // Only once the keys are in use (GHUB-0219), and over everything else.
    if (keyboardCursorShown())
        Theme::paintCellCursor(p, cellRect(m_cursor), legible);

    // The sentence under the board, always: the status bar is not read during
    // play, and in a game for two people it is what says whose go it is.
    const QRectF strip = sentenceRect();
    QFont f = font();
    f.setBold(true);
    f.setPixelSize(std::max(12, int(strip.height() * 0.5)));
    p.setFont(f);
    p.setPen(palette().windowText().color());
    p.drawText(strip, Qt::AlignCenter | Qt::TextWordWrap, boardSentence());

    paintStatusCaption(p, QRectF(rect()));
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void NoughtsView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_finished) {
        newGame();
        return;
    }
    const int cell = cellAt(event->position());
    if (cell < 0)
        return;
    // The cursor follows the mouse, so the two ways of playing never disagree
    // about where you are.
    m_cursor = cell;
    update();
    playAt(cell);
}

void NoughtsView::keyPressEvent(QKeyEvent* event)
{
    const int row = m_cursor / noughts::kSize;
    const int col = m_cursor % noughts::kSize;
    const int last = noughts::kSize - 1;
    switch (event->key()) {
    case Qt::Key_Left:  m_cursor = row * noughts::kSize + std::max(0, col - 1); break;
    case Qt::Key_Right: m_cursor = row * noughts::kSize + std::min(last, col + 1); break;
    case Qt::Key_Up:    m_cursor = std::max(0, row - 1) * noughts::kSize + col; break;
    case Qt::Key_Down:  m_cursor = std::min(last, row + 1) * noughts::kSize + col; break;
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        playAt(m_cursor);
        return;
    default:
        GameView::keyPressEvent(event);
        return;
    }
    update();
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

// The board, who opened it, who opens the next, the opponent and the cursor.
// The undo history and the session score are not kept: a resumed game starts
// a fresh history, and the score is the evening's.
QByteArray NoughtsView::saveState() const
{
    if (m_finished || (m_history.empty() && !m_resumed))
        return {};
    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << kBlobVersion << qint8(m_opponent) << qint8(m_starter) << qint8(m_nextStarter);
    for (Mark m : m_board.cells())
        out << qint8(m);
    out << qint8(m_cursor);
    return blob;
}

bool NoughtsView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    qint8 opponent = 0;
    qint8 starter = 0;
    qint8 nextStarter = 0;
    in >> version >> opponent >> starter >> nextStarter;
    if (version != kBlobVersion || in.status() != QDataStream::Ok)
        return false;
    if (opponent < qint8(Opponent::Easy) || opponent > qint8(Opponent::TwoPlayers))
        return false;
    const auto isSide = [](qint8 v) { return v == qint8(Mark::Cross) || v == qint8(Mark::Nought); };
    if (!isSide(starter) || !isSide(nextStarter))
        return false;

    std::array<Mark, noughts::kCells> cells {};
    for (Mark& m : cells) {
        qint8 value = 0;
        in >> value;
        m = Mark(value);
    }
    qint8 cursor = 0;
    in >> cursor;
    if (in.status() != QDataStream::Ok || !noughts::Board::inBounds(cursor))
        return false;

    // Into a board of its own, so a refused blob leaves the game on screen
    // alone.
    noughts::Board board;
    if (!board.restore(cells, Mark(starter)) || board.over())
        return false;

    abandonThinking();
    m_board = board;
    m_starter = Mark(starter);
    m_nextStarter = Mark(nextStarter);
    // The saved game's opponent, ticked, without writing the preference.
    m_opponent = Opponent(opponent);
    if (m_opponentGroup != nullptr)
        m_opponentGroup->actions().at(opponent)->setChecked(true);
    m_cursor = cursor;
    m_history.clear();
    m_finished = false;
    m_resumed = true;
    m_newestCell = -1;
    m_markDrawn = 1.0;
    m_lineDrawn = 1.0;
    m_undoAction->setEnabled(false);
    advance();
    return true;
}
