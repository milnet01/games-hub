#include "reversiview.h"

#include "legibility.h"
#include "scores.h"
#include "sound.h"
#include "theme.h"

#include <QActionGroup>
#include <QDataStream>
#include <QLinearGradient>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QTimer>
#include <QtConcurrent>

#include <algorithm>

namespace {
constexpr QColor kFeltTop { 0x23, 0x84, 0x55 };
constexpr QColor kFeltBottom { 0x16, 0x60, 0x3c };
constexpr QColor kFrame { 0x2b, 0x1d, 0x14 };
constexpr QColor kFrameEdge { 0x4a, 0x33, 0x22 };
constexpr QColor kGrid { 0x0d, 0x3d, 0x27 };
constexpr QColor kBlackDisc { 0x22, 0x24, 0x26 };
constexpr QColor kWhiteDisc { 0xf4, 0xf3, 0xee };

constexpr int kFrameWidth = 10;

// A short pause before the computer answers — an instant reply feels jarring.
constexpr int kThinkDelayMs = 320;

QString playerName(Player p)
{
    return p == Player::Black ? QStringLiteral("Black") : QStringLiteral("White");
}
}

ReversiView::ReversiView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(minimumSizeHint());
    buildActions();
    newGame();
}

void ReversiView::buildActions()
{
    auto* newAction = new QAction(QStringLiteral("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &ReversiView::newGame);
    m_actions.append(newAction);

    m_undoAction = new QAction(QStringLiteral("Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &ReversiView::undo);
    m_actions.append(m_undoAction);

    auto* separator = new QAction(this);
    separator->setSeparator(true);
    m_actions.append(separator);

    m_levelGroup = new QActionGroup(this);
    m_levelGroup->setExclusive(true);
    const struct { const char* name; Difficulty value; } kLevels[] = {
        { "Easy", Difficulty::Easy },
        { "Medium", Difficulty::Medium },
        { "Hard", Difficulty::Hard },
    };
    for (const auto& entry : kLevels) {
        auto* a = new QAction(QString::fromUtf8(entry.name), this);
        a->setCheckable(true);
        a->setChecked(entry.value == m_difficulty);
        m_levelGroup->addAction(a);
        const Difficulty value = entry.value;
        connect(a, &QAction::triggered, this, [this, value] {
            // A search already running is answering at the old strength.
            const bool wasThinking = m_thinking;
            abandonSearch();
            m_difficulty = value;
            if (wasThinking)
                advance();
        });
        m_actions.append(a);
    }

    auto* hints = new QAction(QStringLiteral("Hints"), this);
    hints->setCheckable(true);
    hints->setChecked(true);
    connect(hints, &QAction::toggled, this, [this](bool on) {
        m_showHints = on;
        update();
    });
    m_actions.append(hints);
}

void ReversiView::deactivate()
{
    // An answer for a board the hub has left must not place a disc on it.
    m_paused = true;
    abandonSearch();
}

void ReversiView::activate()
{
    m_paused = false;
    // Picks the thinking back up if it was abandoned on the way out.
    if (!m_finished && m_toMove != m_human) {
        advance();
        return;
    }
    refresh();
}

void ReversiView::newGame()
{
    m_resumed = false;
    abandonSearch();
    m_board.reset();
    m_passNotice.clear();
    m_toMove = Player::Black;
    m_human = Player::Black;
    m_lastMove.reset();
    m_history.clear();
    m_thinking = false;
    m_finished = false;
    m_undoAction->setEnabled(false);
    advance();
}

void ReversiView::undo()
{
    if (m_history.empty())
        return;
    // Before GHUB-0047 this refused while the engine was thinking, because the
    // window was frozen and the player could not have pressed it. Now they can.
    abandonSearch();

    // Step back to the last position where it was the player's turn, so one
    // undo rewinds both their move and the computer's reply.
    while (!m_history.empty()) {
        const Snapshot s = m_history.back();
        m_history.pop_back();
        m_board = s.board;
        m_toMove = s.toMove;
        m_lastMove = s.lastMove;
        if (m_toMove == m_human)
            break;
    }

    m_finished = false;
    m_undoAction->setEnabled(!m_history.empty());
    advance();
}

void ReversiView::advance()
{
    // The pass branch below schedules advance() on a timer that nothing can
    // cancel, so it can fire inside announceResult's modal exec() and declare
    // the game over a second time -- a second dialog, and a second write to the
    // stored best score. Once finished, this is not ours to re-enter.
    if (m_finished || m_paused)
        return;

    if (m_board.gameOver()) {
        m_finished = true;
        refresh();
        announceResult();
        return;
    }

    if (m_board.legalMoves(m_toMove).empty()) {
        const QString who = (m_toMove == m_human) ? QStringLiteral("You have")
                                                  : QStringLiteral("The computer has");
        m_toMove = opponent(m_toMove);
        // Kept rather than emitted once: advance() runs again on the timer below
        // and its refresh() replaced this sentence after 320ms, which is not
        // long enough to read and is the only sign a pass happened at all.
        m_passNotice = who + QStringLiteral(" no legal move — turn passes.");
        refresh();
        QTimer::singleShot(kThinkDelayMs, this, &ReversiView::advance);
        return;
    }

    refresh();

    if (m_toMove != m_human) {
        m_thinking = true;
        refresh();
        QTimer::singleShot(kThinkDelayMs, this, &ReversiView::playEngineMove);
    }
}

void ReversiView::playEngineMove()
{
    // A scheduled search can outlive the position it was scheduled for. New
    // Game, Undo, a level change and activate() all leave a singleShot in
    // flight that abandonSearch() cannot cancel -- and when it fires,
    // startSearch() captures the CURRENT board and the CURRENT generation, so
    // engineMoveReady's generation test passes and the engine plays the human's
    // move. m_thinking is the flag to read: advance() is the only thing that
    // sets it and every abandon path clears it.
    if (m_paused || m_finished || !m_thinking || m_toMove == m_human)
        return;
    startSearch();
}

void ReversiView::startSearch()
{
    if (m_search == nullptr) {
        m_search = new QFutureWatcher<SearchResult>(this);
        connect(m_search, &QFutureWatcherBase::finished, this, [this] {
            if (!m_search->isCanceled() && m_search->future().resultCount() > 0)
                engineMoveReady(m_search->result());
        });
    }

    // By value: Board is a value type and the search relies on that, which is
    // what makes it safe to hand to another thread.
    const Board position = m_board;
    const Player toMove = m_toMove;
    const Difficulty difficulty = m_difficulty;
    const quint64 generation = m_generation;
    m_search->setFuture(QtConcurrent::run([position, toMove, difficulty, generation] {
        SearchResult result;
        result.generation = generation;
        result.move = chooseMove(position, toMove, difficulty);
        return result;
    }));
}

void ReversiView::abandonSearch()
{
    ++m_generation;
    m_thinking = false;
}

void ReversiView::engineMoveReady(const SearchResult& result)
{
    if (result.generation != m_generation)
        return;

    // Defence in depth for the same hazard playEngineMove() guards.
    if (m_finished || m_toMove == m_human)
        return;

    const std::optional<Move> m = result.move;
    m_thinking = false;

    if (!m) {
        advance();
        return;
    }

    m_passNotice.clear();
    m_board.play(m_toMove, *m);
    Sound::instance().play(Sound::kDiscPlace);
    m_lastMove = m;
    m_toMove = opponent(m_toMove);
    advance();
}

void ReversiView::refresh(const QString& message)
{
    m_hints = m_board.legalMoves(m_toMove);
    update();

    const bool humanTurn = !m_finished && !m_thinking && m_toMove == m_human;
    const QString score = QStringLiteral("You %1 — %2 Computer")
                              .arg(m_board.count(m_human))
                              .arg(m_board.count(opponent(m_human)));

    QString state;
    if (!message.isEmpty())
        state = message;
    else if (m_finished)
        state = QStringLiteral("Game over.");
    else if (humanTurn)
        state = QStringLiteral("Your turn (%1).").arg(playerName(m_human));
    else
        state = QStringLiteral("Computer thinking…");

    // In front of whatever comes next, so it survives until a disc is actually
    // placed rather than being replaced by the following turn's line.
    if (!m_passNotice.isEmpty())
        state = m_passNotice + QStringLiteral("   ") + state;

    Q_EMIT statusChanged(state + QStringLiteral("   ") + score);
}

void ReversiView::announceResult()
{
    const int mine = m_board.count(m_human);
    const int theirs = m_board.count(opponent(m_human));

    QString headline;
    if (mine > theirs)
        headline = QStringLiteral("You win!");
    else if (theirs > mine)
        headline = QStringLiteral("The computer wins.");
    else
        headline = QStringLiteral("A draw.");

    Sound::instance().play(mine > theirs ? Sound::kWin : Sound::kLose);

    // Only a win counts towards the record, and the record is how many discs
    // you finished with — a 40-24 win beats a 33-31 one.
    bool newBest = false;
    if (mine > theirs)
        newBest = Scores::instance().recordHigh(Scores::reversiBest(int(m_difficulty)), mine);

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Game over"));
    box.setText(headline);
    const int record = Scores::instance().best(Scores::reversiBest(int(m_difficulty)));
    QString detail = QStringLiteral("Final score %1 – %2.").arg(mine).arg(theirs);
    if (newBest)
        detail += QStringLiteral("\nA new best win at this level!");
    else if (record > 0)
        detail += QStringLiteral("\nYour best win here: %1 discs.").arg(record);
    box.setInformativeText(detail);
    QAbstractButton* again = box.addButton(QStringLiteral("Play Again"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("Close"), QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == again)
        newGame();
}

QRect ReversiView::boardRect() const
{
    // Keep cells on whole pixels so the grid lines stay crisp, and leave room
    // for the frame drawn around the playing surface.
    // Under the legibility switch the board gives up a strip at the bottom for
    // the caption, and moves up by it, so the sentence never covers a piece.
    const int band = int(captionBand(QRectF(rect())));
    const int available = std::min(width(), height() - band) - 2 * (kFrameWidth + 4);
    const int side = std::max(kSize, (available / kSize) * kSize);
    return { (width() - side) / 2, (height() - band - side) / 2, side, side };
}

std::optional<Move> ReversiView::cellAt(QPointF pos) const
{
    const QRect r = boardRect();
    if (r.width() <= 0 || !r.contains(pos.toPoint()))
        return std::nullopt;

    const double cell = r.width() / double(kSize);
    const int col = int((pos.x() - r.x()) / cell);
    const int row = int((pos.y() - r.y()) / cell);
    if (!Board::inBounds(row, col))
        return std::nullopt;
    return Move { row, col };
}

void ReversiView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().window());

    const QRect r = boardRect();
    if (r.width() <= 0)
        return;

    const double cell = r.width() / double(kSize);

    // Wooden frame, then the felt playing surface inside it.
    Theme::paintWoodFrame(p, r, kFrameWidth, 7);
    Theme::paintFelt(p, r, Theme::kFeltGreenTop, Theme::kFeltGreenBottom);

    p.setPen(QPen(kGrid, 1));
    for (int i = 1; i < kSize; ++i) {
        const double x = r.x() + i * cell;
        const double y = r.y() + i * cell;
        p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
    }

    // The four traditional guide dots at the 2/6 intersections.
    p.setBrush(kGrid);
    p.setPen(Qt::NoPen);
    for (int gr : { 2, 6 })
        for (int gc : { 2, 6 })
            p.drawEllipse(QPointF(r.x() + gc * cell, r.y() + gr * cell), cell * 0.06, cell * 0.06);

    const bool humanTurn = !m_finished && !m_thinking && m_toMove == m_human;

    for (int row = 0; row < kSize; ++row) {
        for (int col = 0; col < kSize; ++col) {
            const QPointF centre(r.x() + (col + 0.5) * cell, r.y() + (row + 0.5) * cell);
            const Cell c = m_board.at(row, col);

            if (c == Cell::Empty) {
                const bool isHint = m_showHints && humanTurn
                    && std::find(m_hints.begin(), m_hints.end(), Move { row, col }) != m_hints.end();
                if (isHint) {
                    // These are the whole affordance in Reversi -- where you may
                    // play is not otherwise visible on the board -- and they
                    // ignored the switch. Draughts already rejected an alpha of
                    // 60 as "a hint you can look past, which is the wrong thing
                    // to be for the player the switch is for"; 70 was no better,
                    // and a dot of 0.12 of a square is small on top of that.
                    const bool large = Legibility::instance().enabled();
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(255, 255, 255, large ? 190 : 70));
                    const double r = cell * (large ? 0.18 : 0.12);
                    p.drawEllipse(centre, r, r);
                }
                continue;
            }

            const QColor face = (c == Cell::Black) ? kBlackDisc : kWhiteDisc;
            const double radius = cell * 0.40;

            // Contact shadow — it is what makes the counter sit on the board
            // rather than float above it.
            Theme::paintContactShadow(p, centre, radius);

            // A soft radial fill reads as a physical counter rather than a dot.
            QRadialGradient g(centre - QPointF(radius * 0.3, radius * 0.35), radius * 1.6);
            g.setColorAt(0.0, face.lighter(c == Cell::Black ? 260 : 108));
            g.setColorAt(1.0, face.darker(c == Cell::Black ? 130 : 118));
            p.setBrush(g);
            p.setPen(QPen(QColor(0, 0, 0, 60), 1));
            p.drawEllipse(centre, radius, radius);
        }
    }

    if (m_lastMove) {
        const QPointF centre(r.x() + (m_lastMove->col + 0.5) * cell,
                             r.y() + (m_lastMove->row + 0.5) * cell);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0xff, 0xd5, 0x4f), std::max(2.0, cell * 0.05)));
        p.drawEllipse(centre, cell * 0.45, cell * 0.45);
    }

    // Reversi's status line is already the right sentence for the board: whose
    // turn it is AND the score, which is the number a player checks most and
    // the one the corner counters make you add up by eye.
    paintStatusCaption(p, QRectF(rect()));
}

void ReversiView::mousePressEvent(QMouseEvent* event)
{
    if (m_finished || m_thinking || m_toMove != m_human || event->button() != Qt::LeftButton)
        return;

    const std::optional<Move> m = cellAt(event->position());
    if (!m || m_board.flipCount(m_human, *m) == 0)
        return;

    m_history.push_back({ m_board, m_toMove, m_lastMove });
    m_undoAction->setEnabled(true);
    m_passNotice.clear();

    const int flipped = m_board.play(m_human, *m);
    Sound::instance().play(Sound::kDiscPlace);
    if (flipped > 2)
        Sound::instance().play(Sound::kDiscFlip);
    m_lastMove = m;
    m_toMove = opponent(m_toMove);
    advance();
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

// The board, not the moves that made it. There is no move log to replay, so
// Board::restore() is what stands in for a rules check on the way back in, and
// advance() is what puts the game back in motion — including handing straight
// over to the engine when it was the computer's turn when you left.
//
// The undo history is deliberately not saved: a resumed game starts a fresh one.
QByteArray ReversiView::saveState() const
{
    // Nothing worth coming back to: a finished game, or one nobody has moved in.
    // An empty state also clears whatever was stored before.
    if (m_finished || (m_history.empty() && !m_resumed))
        return {};

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1) << qint8(m_toMove) << qint8(m_human) << qint8(m_difficulty)
        << qint8(m_lastMove ? 1 : 0) << qint8(m_lastMove ? m_lastMove->row : 0)
        << qint8(m_lastMove ? m_lastMove->col : 0);
    for (Cell c : m_board.cells())
        out << qint8(c);
    return blob;
}

bool ReversiView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    qint8 toMove = 0;
    qint8 human = 0;
    qint8 difficulty = 0;
    qint8 hasLast = 0;
    qint8 lastRow = 0;
    qint8 lastCol = 0;
    in >> version >> toMove >> human >> difficulty >> hasLast >> lastRow >> lastCol;
    if (version != 1 || in.status() != QDataStream::Ok)
        return false;
    if ((toMove != 1 && toMove != -1) || (human != 1 && human != -1))
        return false;
    if (difficulty < qint8(Difficulty::Easy) || difficulty > qint8(Difficulty::Hard))
        return false;
    if (hasLast != 0 && hasLast != 1)
        return false;
    if (hasLast == 1 && !Board::inBounds(lastRow, lastCol))
        return false;

    std::array<Cell, kCells> cells {};
    for (Cell& c : cells) {
        qint8 value = 0;
        in >> value;
        c = Cell(value);
    }
    if (in.status() != QDataStream::Ok)
        return false;

    // Read into a board of its own, so a blob that turns out to be nonsense
    // leaves the game already on screen alone.
    Board board;
    if (!board.restore(cells) || board.gameOver())
        return false;

    m_board = board;
    m_toMove = Player(toMove);
    m_human = Player(human);
    abandonSearch();
    m_difficulty = Difficulty(difficulty);
    m_lastMove = (hasLast == 1) ? std::optional<Move>(Move { lastRow, lastCol }) : std::nullopt;
    m_history.clear();
    m_thinking = false;
    m_finished = false;
    m_resumed = true;
    m_undoAction->setEnabled(false);
    if (m_levelGroup != nullptr)
        m_levelGroup->actions().at(difficulty)->setChecked(true);
    advance();
    return true;
}
