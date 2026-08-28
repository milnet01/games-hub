#include "draughtsview.h"

#include "legibility.h"
#include "scores.h"
#include "sound.h"
#include "theme.h"

#include <QActionGroup>
#include <QDataStream>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadialGradient>
#include <QTimer>
#include <QtConcurrent>

#include <algorithm>

namespace {
constexpr QColor kDarkSquare { 0x6b, 0x46, 0x2c };
constexpr QColor kLightSquare { 0xd9, 0xbe, 0x96 };
constexpr QColor kRedPiece { 0xb5, 0x3a, 0x30 };
constexpr QColor kWhitePiece { 0xef, 0xe9, 0xdd };
constexpr int kFrameWidth = 10;
constexpr int kThinkDelayMs = 340;

const QString kWinsKey = QStringLiteral("draughts/wins");
}

DraughtsView::DraughtsView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(minimumSizeHint());
    buildActions();
    newGame();
}

void DraughtsView::buildActions()
{
    auto* newAction = new QAction(QStringLiteral("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &DraughtsView::newGame);
    m_actions.append(newAction);

    m_undoAction = new QAction(QStringLiteral("Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &DraughtsView::undo);
    m_actions.append(m_undoAction);

    auto* sep = new QAction(this);
    sep->setSeparator(true);
    m_actions.append(sep);

    m_levelGroup = new QActionGroup(this);
    m_levelGroup->setExclusive(true);
    const struct { const char* name; DraughtsLevel level; } kLevels[] = {
        { "Easy", DraughtsLevel::Easy },
        { "Medium", DraughtsLevel::Medium },
        { "Hard", DraughtsLevel::Hard },
    };
    for (const auto& entry : kLevels) {
        auto* a = new QAction(QString::fromUtf8(entry.name), this);
        a->setCheckable(true);
        a->setChecked(entry.level == m_level);
        m_levelGroup->addAction(a);
        const DraughtsLevel level = entry.level;
        connect(a, &QAction::triggered, this, [this, level] {
            // A search already running is answering at the old strength.
            const bool wasThinking = m_thinking;
            abandonSearch();
            m_level = level;
            if (wasThinking)
                advance();
        });
        m_actions.append(a);
    }
}

void DraughtsView::newGame()
{
    abandonSearch();
    m_board.reset();
    m_toMove = Side::Red;
    m_human = Side::Red;
    m_selected.reset();
    m_selectedMoves.clear();
    m_lastMove.reset();
    m_history.clear();
    m_thinking = false;
    m_finished = false;
    m_undoAction->setEnabled(false);
    advance();
}

void DraughtsView::undo()
{
    if (m_history.empty())
        return;
    // Before GHUB-0047 this refused while the engine was thinking, because the
    // window was frozen and the player could not have pressed it. Now they can.
    abandonSearch();
    // Step back to the player's own turn, so one undo takes back the reply too.
    while (!m_history.empty()) {
        const Snapshot s = m_history.back();
        m_history.pop_back();
        m_board = s.board;
        m_toMove = s.toMove;
        if (m_toMove == m_human)
            break;
    }
    m_selected.reset();
    m_selectedMoves.clear();
    m_finished = false;
    m_undoAction->setEnabled(!m_history.empty());
    advance();
}

void DraughtsView::deactivate()
{
    // An answer for a board the hub has left must not move a piece on it.
    m_paused = true;
    abandonSearch();
}

void DraughtsView::activate()
{
    m_paused = false;
    // Picks the thinking back up if it was abandoned on the way out.
    if (!m_finished && m_toMove != m_human) {
        advance();
        return;
    }
    refresh();
}

void DraughtsView::advance()
{
    if (m_board.legalMoves(m_toMove).empty()) {
        // A side with no move has lost — trapped counts the same as wiped out.
        m_finished = true;
        refresh();
        announceResult(other(m_toMove));
        return;
    }

    refresh();

    if (m_toMove != m_human) {
        m_thinking = true;
        refresh();
        QTimer::singleShot(kThinkDelayMs, this, &DraughtsView::playEngineMove);
    }
}

void DraughtsView::playEngineMove()
{
    if (m_paused)
        return;
    startSearch();
}

void DraughtsView::startSearch()
{
    if (m_search == nullptr) {
        m_search = new QFutureWatcher<SearchResult>(this);
        connect(m_search, &QFutureWatcherBase::finished, this, [this] {
            if (!m_search->isCanceled() && m_search->future().resultCount() > 0)
                engineMoveReady(m_search->result());
        });
    }

    // By value, so the worker cannot reach back into the game.
    const DraughtsBoard position = m_board;
    const Side toMove = m_toMove;
    const DraughtsLevel level = m_level;
    const quint64 generation = m_generation;
    m_search->setFuture(QtConcurrent::run([position, toMove, level, generation] {
        SearchResult result;
        result.generation = generation;
        result.found = chooseDraughtsMove(position, toMove, level, result.move);
        return result;
    }));
}

void DraughtsView::abandonSearch()
{
    ++m_generation;
    m_thinking = false;
}

void DraughtsView::engineMoveReady(const SearchResult& result)
{
    if (result.generation != m_generation)
        return;

    const DraughtsMove move = result.move;
    m_thinking = false;
    if (!result.found) {
        advance();
        return;
    }

    m_board.apply(move);
    m_lastMove = move;
    Sound::instance().play(move.isCapture() ? Sound::kDiscFlip : Sound::kDiscPlace);
    m_toMove = other(m_toMove);
    advance();
}

void DraughtsView::announceResult(Side winner)
{
    const bool playerWon = winner == m_human;
    Sound::instance().play(playerWon ? Sound::kWin : Sound::kLose);

    int wins = Scores::instance().best(kWinsKey);
    if (playerWon) {
        ++wins;
        Scores::instance().recordHigh(kWinsKey, wins);
    }

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Game over"));
    box.setText(playerWon ? QStringLiteral("You win!") : QStringLiteral("The computer wins."));
    box.setInformativeText(QStringLiteral("Pieces left — you %1, computer %2.\nGames won: %3.")
                               .arg(m_board.count(m_human))
                               .arg(m_board.count(other(m_human)))
                               .arg(wins));
    QAbstractButton* again = box.addButton(QStringLiteral("Play Again"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("Close"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == again)
        newGame();
}

void DraughtsView::refresh(const QString& message)
{
    update();

    const bool humanTurn = !m_finished && !m_thinking && m_toMove == m_human;
    QString state;
    if (!message.isEmpty())
        state = message;
    else if (m_finished)
        state = QStringLiteral("Game over.");
    else if (humanTurn)
        state = m_board.legalMoves(m_human).front().isCapture()
            ? QStringLiteral("Your turn — you must take.")
            : QStringLiteral("Your turn.");
    else
        state = QStringLiteral("Computer thinking…");

    // State and the piece counts, but not the win tally: a running total of
    // past games is worth a glance in the status bar and not a line of board.
    m_caption = QStringLiteral("%1   You %2 — %3 Computer")
                    .arg(state)
                    .arg(m_board.count(m_human))
                    .arg(m_board.count(other(m_human)));
    Q_EMIT statusChanged(QStringLiteral("%1   You %2 — %3 Computer   Won %4")
                             .arg(state)
                             .arg(m_board.count(m_human))
                             .arg(m_board.count(other(m_human)))
                             .arg(Scores::instance().best(kWinsKey)));
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

QRect DraughtsView::boardRect() const
{
    // Under the legibility switch the board gives up a strip at the bottom for
    // the caption, and moves up by it, so the sentence never covers a piece.
    const int band = int(captionBand(QRectF(rect())));
    const int available = std::min(width(), height() - band) - 2 * (kFrameWidth + 4);
    const int side = std::max(kBoardSize, (available / kBoardSize) * kBoardSize);
    return { (width() - side) / 2, (height() - band - side) / 2, side, side };
}

std::optional<Square> DraughtsView::squareAt(QPointF pos) const
{
    const QRect r = boardRect();
    if (!r.contains(pos.toPoint()))
        return std::nullopt;
    const double cell = r.width() / double(kBoardSize);
    const int col = int((pos.x() - r.x()) / cell);
    const int row = int((pos.y() - r.y()) / cell);
    if (!DraughtsBoard::inBounds(row, col))
        return std::nullopt;
    return Square { row, col };
}

std::vector<DraughtsMove> DraughtsView::movesFrom(Square s) const
{
    std::vector<DraughtsMove> out;
    for (const DraughtsMove& m : m_board.legalMoves(m_human))
        if (m.from == s)
            out.push_back(m);
    return out;
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void DraughtsView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().window());

    const QRect r = boardRect();
    if (r.width() <= 0)
        return;
    const double cell = r.width() / double(kBoardSize);

    Theme::paintWoodFrame(p, r, kFrameWidth, 7);

    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const QRectF square(r.x() + col * cell, r.y() + row * cell, cell, cell);
            p.fillRect(square, DraughtsBoard::isPlayable(row, col) ? kDarkSquare : kLightSquare);
        }
    }

    // The move just played, so the computer's reply is easy to follow.
    if (m_lastMove) {
        // 60 is a hint you can look past, which is the wrong thing to be for the
        // player the switch is for.
        p.setBrush(QColor(0xff, 0xd5, 0x4f, Legibility::instance().enabled() ? 150 : 60));
        p.setPen(Qt::NoPen);
        for (const Square& s : { m_lastMove->from, m_lastMove->destination() })
            p.drawRect(QRectF(r.x() + s.col * cell, r.y() + s.row * cell, cell, cell));
    }

    // Where the selected piece can go.
    p.setPen(Qt::NoPen);
    for (const DraughtsMove& m : m_selectedMoves) {
        const Square d = m.destination();
        p.setBrush(QColor(0x66, 0xe0, 0x8a, m.isCapture() ? 150 : 110));
        p.drawEllipse(QPointF(r.x() + (d.col + 0.5) * cell, r.y() + (d.row + 0.5) * cell),
                      cell * 0.16, cell * 0.16);
    }

    if (m_selected) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0xff, 0xd5, 0x4f), std::max(2.0, cell * 0.06)));
        p.drawRect(QRectF(r.x() + m_selected->col * cell, r.y() + m_selected->row * cell,
                          cell, cell));
    }

    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const Piece piece = m_board.at(row, col);
            if (piece == Piece::Empty)
                continue;

            const QPointF centre(r.x() + (col + 0.5) * cell, r.y() + (row + 0.5) * cell);
            const double radius = cell * 0.38;
            const QColor face = belongsTo(piece, Side::Red) ? kRedPiece : kWhitePiece;

            Theme::paintContactShadow(p, centre, radius);

            QRadialGradient g(centre - QPointF(radius * 0.3, radius * 0.35), radius * 1.7);
            g.setColorAt(0.0, face.lighter(135));
            g.setColorAt(1.0, face.darker(130));
            p.setBrush(g);
            p.setPen(QPen(QColor(0, 0, 0, 70), 1));
            p.drawEllipse(centre, radius, radius);

            // A ridge, so a draught reads as a stacked counter.
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0, 0, 0, 45), std::max(1.0, cell * 0.02)));
            p.drawEllipse(centre, radius * 0.78, radius * 0.78);

            if (isKing(piece)) {
                // A crown for a king: a simple gold band and points.
                p.setPen(QPen(Theme::kGold, std::max(1.5, cell * 0.05)));
                const double cr = radius * 0.44;
                QPolygonF crown;
                crown << centre + QPointF(-cr, cr * 0.5) << centre + QPointF(-cr, -cr * 0.3)
                      << centre + QPointF(-cr * 0.5, cr * 0.1) << centre + QPointF(0, -cr * 0.7)
                      << centre + QPointF(cr * 0.5, cr * 0.1) << centre + QPointF(cr, -cr * 0.3)
                      << centre + QPointF(cr, cr * 0.5);
                p.drawPolyline(crown);
            }
        }
    }

    paintStatusCaption(p, QRectF(rect()));
}

void DraughtsView::mousePressEvent(QMouseEvent* event)
{
    if (m_finished || m_thinking || m_toMove != m_human || event->button() != Qt::LeftButton)
        return;

    const std::optional<Square> clicked = squareAt(event->position());
    if (!clicked)
        return;

    // Clicking a highlighted destination plays that move.
    if (m_selected) {
        for (const DraughtsMove& m : m_selectedMoves) {
            if (!(m.destination() == *clicked))
                continue;

            m_history.push_back({ m_board, m_toMove });
            m_undoAction->setEnabled(true);
            m_board.apply(m);
            m_lastMove = m;
            Sound::instance().play(m.isCapture() ? Sound::kDiscFlip : Sound::kDiscPlace);
            m_selected.reset();
            m_selectedMoves.clear();
            m_toMove = other(m_toMove);
            advance();
            return;
        }
    }

    // Otherwise select one of the player's own pieces that has a move.
    if (belongsTo(m_board.at(clicked->row, clicked->col), m_human)) {
        std::vector<DraughtsMove> moves = movesFrom(*clicked);
        if (moves.empty()) {
            refresh(m_board.legalMoves(m_human).front().isCapture()
                        ? QStringLiteral("A capture is available — you must take it.")
                        : QStringLiteral("That piece has no move."));
            return;
        }
        m_selected = clicked;
        m_selectedMoves = std::move(moves);
        refresh();
        return;
    }

    m_selected.reset();
    m_selectedMoves.clear();
    refresh();
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

// The board, not the moves that made it — the same shape as Reversi, for the
// same reason: there is no move log to replay, so DraughtsBoard::restore()
// stands in for a rules check on the way back in and advance() puts the game
// back in motion. The undo history is not saved; a resumed game starts a fresh
// one. The last move is, because the board marks where it went and losing that
// mark is the difference between coming back to a game and coming back to a
// puzzle.
QByteArray DraughtsView::saveState() const
{
    // Nothing worth coming back to: a finished game, or one nobody has moved in.
    // An empty state also clears whatever was stored before.
    if (m_finished || m_history.empty())
        return {};

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1) << qint8(m_toMove) << qint8(m_human) << qint8(m_level);

    const auto writeSquare = [&out](const Square& s) { out << qint8(s.row) << qint8(s.col); };
    out << qint8(m_lastMove ? 1 : 0);
    if (m_lastMove) {
        writeSquare(m_lastMove->from);
        out << quint8(m_lastMove->steps.size());
        for (const Square& s : m_lastMove->steps)
            writeSquare(s);
        out << quint8(m_lastMove->captured.size());
        for (const Square& s : m_lastMove->captured)
            writeSquare(s);
    }

    for (Piece p : m_board.cells())
        out << qint8(p);
    return blob;
}

bool DraughtsView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    qint8 toMove = 0;
    qint8 human = 0;
    qint8 level = 0;
    in >> version >> toMove >> human >> level;
    if (version != 1 || in.status() != QDataStream::Ok)
        return false;
    if (toMove < qint8(Side::Red) || toMove > qint8(Side::White))
        return false;
    if (human < qint8(Side::Red) || human > qint8(Side::White))
        return false;
    if (level < qint8(DraughtsLevel::Easy) || level > qint8(DraughtsLevel::Hard))
        return false;

    const auto readSquare = [&in](Square& s) {
        qint8 row = 0;
        qint8 col = 0;
        in >> row >> col;
        s = Square { row, col };
        return DraughtsBoard::inBounds(row, col);
    };
    // A jump chain cannot be longer than the men it takes, so twelve bounds
    // both lists and stops a nonsense length reserving nonsense memory.
    const auto readSquares = [&](std::vector<Square>& out, int least) {
        quint8 count = 0;
        in >> count;
        if (count < least || count > 12)
            return false;
        out.resize(count);
        for (Square& s : out)
            if (!readSquare(s))
                return false;
        return true;
    };

    std::optional<DraughtsMove> lastMove;
    qint8 hasLast = 0;
    in >> hasLast;
    if (hasLast != 0 && hasLast != 1)
        return false;
    if (hasLast == 1) {
        DraughtsMove move;
        if (!readSquare(move.from) || !readSquares(move.steps, 1)
            || !readSquares(move.captured, 0))
            return false;
        lastMove = move;
    }

    std::vector<Piece> cells(kBoardCells);
    for (Piece& p : cells) {
        qint8 value = 0;
        in >> value;
        p = Piece(value);
    }
    if (in.status() != QDataStream::Ok)
        return false;

    // Read into a board of its own, so a blob that turns out to be nonsense
    // leaves the game already on screen alone.
    DraughtsBoard board;
    if (!board.restore(cells))
        return false;
    // A side with no move has lost, which is a finished game rather than a
    // saved one.
    if (board.gameOver(Side(toMove)))
        return false;

    m_board = board;
    m_toMove = Side(toMove);
    m_human = Side(human);
    abandonSearch();
    m_level = DraughtsLevel(level);
    m_selected.reset();
    m_selectedMoves.clear();
    m_lastMove = lastMove;
    m_history.clear();
    m_thinking = false;
    m_finished = false;
    m_undoAction->setEnabled(false);
    if (m_levelGroup != nullptr)
        m_levelGroup->actions().at(level)->setChecked(true);
    advance();
    return true;
}
