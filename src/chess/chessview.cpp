#include "chessview.h"

#include "chessart.h"
#include "legibility.h"
#include "scores.h"
#include "sound.h"
#include "theme.h"

#include <QActionGroup>
#include <QDataStream>
#include <QIODevice>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadialGradient>
#include <QTimer>
#include <QtConcurrent>

#include <algorithm>

using namespace chess;

namespace {
constexpr int kFrameWidth = 18;
constexpr int kThinkDelayMs = 260;

// A function rather than a file-scope constant, which is the shape scores.h
// already uses for its own keys. A QString built at static-initialisation time
// can in principle throw where nothing can catch it; QStringLiteral's data is
// static, so returning a copy costs nothing.
QString winsKey() { return QStringLiteral("chess/wins"); }

// The toolbar's name for a strength. One list, because a resumed game has to
// tick the level it was saved at and two lists would drift.
QString levelName(Level level)
{
    switch (level) {
    case Level::Easy: return QStringLiteral("Easy");
    case Level::Hard: return QStringLiteral("Hard");
    default:          return QStringLiteral("Medium");
    }
}

QString pieceName(PieceType type)
{
    switch (type) {
    case PieceType::Queen:  return QStringLiteral("Queen");
    case PieceType::Rook:   return QStringLiteral("Rook");
    case PieceType::Bishop: return QStringLiteral("Bishop");
    case PieceType::Knight: return QStringLiteral("Knight");
    default:                return QStringLiteral("Pawn");
    }
}

QString drawText(DrawReason reason)
{
    switch (reason) {
    case DrawReason::Stalemate:
        return QStringLiteral("Stalemate — the side to move has no legal move but is not in check.");
    case DrawReason::FiftyMove:
        return QStringLiteral("Fifty moves have passed with no capture and no pawn moved.");
    case DrawReason::Repetition:
        return QStringLiteral("The same position has appeared three times.");
    case DrawReason::InsufficientMaterial:
        return QStringLiteral("Neither side has enough material to force mate.");
    case DrawReason::None:
        break;
    }
    return QStringLiteral("A draw.");
}
}

ChessView::ChessView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(ChessView::minimumSizeHint());
    buildActions();
    newGame();
}

void ChessView::buildActions()
{
    auto* newAction = new QAction(QStringLiteral("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &ChessView::newGame);
    m_actions.append(newAction);

    m_undoAction = new QAction(QStringLiteral("Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &ChessView::undo);
    m_actions.append(m_undoAction);

    auto* sep = new QAction(this);
    sep->setSeparator(true);
    m_actions.append(sep);

    auto* group = new QActionGroup(this);
    group->setExclusive(true);
    for (const Level level : { Level::Easy, Level::Medium, Level::Hard }) {
        auto* a = new QAction(levelName(level), this);
        a->setCheckable(true);
        a->setChecked(level == m_level);
        group->addAction(a);
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

void ChessView::newGame()
{
    abandonSearch();
    m_game.reset();
    m_human = Colour::White;
    m_selected.reset();
    m_selectedMoves.clear();
    m_lastMove.reset();
    m_thinking = false;
    m_finished = false;
    m_undoAction->setEnabled(false);
    advance();
}

void ChessView::undo()
{
    if (!m_game.canUndo())
        return;
    // Before GHUB-0047 this refused while the engine was thinking, because the
    // window was frozen and the player could not have pressed it anyway. Now
    // they can, and waiting for a search whose board is about to be thrown away
    // would be the one thing the roadmap said not to do.
    abandonSearch();
    // Step back to the player's own turn, so one undo takes back the reply too.
    while (m_game.canUndo()) {
        m_game.undo();
        if (m_game.toMove() == m_human)
            break;
    }
    m_selected.reset();
    m_selectedMoves.clear();
    m_lastMove = m_game.history().empty() ? std::optional<Move> {} : m_game.history().back();
    m_finished = false;
    m_undoAction->setEnabled(m_game.canUndo());
    advance();
}

void ChessView::deactivate()
{
    m_paused = true;
    // An answer for a board the hub has left must not move a piece on it.
    // activate() starts the thinking again, so the game is not left stuck on
    // the computer's turn.
    abandonSearch();
}

void ChessView::activate()
{
    m_paused = false;
    // Picks the search back up if it was abandoned on the way out.
    if (!m_finished && m_game.toMove() != m_human) {
        advance();
        return;
    }
    refresh();
}

// The moves, not the position.
//
// A FEN would say where the pieces are and nothing else. Threefold repetition
// needs every position the game has passed through, and Undo needs the boards
// behind it — both of which ChessGame keeps privately and rebuilds itself from
// play(). Replaying the moves therefore restores all three from one list, with
// no second copy of the history to drift out of step with the first.
QByteArray ChessView::saveState() const
{
    // Nothing worth coming back to: a game already decided, or one nobody has
    // moved in. An empty state also clears whatever was stored before.
    if (m_game.isOver() || m_game.history().empty())
        return {};

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1) << qint32(m_level) << qint32(m_game.history().size());
    for (const chess::Move& m : m_game.history()) {
        out << qint8(m.from.row) << qint8(m.from.col) << qint8(m.to.row) << qint8(m.to.col)
            << qint8(m.promotion);
    }
    return blob;
}

bool ChessView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    qint32 level = 0;
    qint32 count = 0;
    in >> version >> level >> count;
    // 1024 plies is far beyond any real game; a count from a corrupt file must
    // not be trusted into a loop.
    if (version != 1 || in.status() != QDataStream::Ok || count < 0 || count > 1024)
        return false;

    // Played into a game of its own, so a stream that turns out to be nonsense
    // leaves the board already on screen alone.
    chess::ChessGame game;
    for (qint32 i = 0; i < count; ++i) {
        qint8 fromRow = 0;
        qint8 fromCol = 0;
        qint8 toRow = 0;
        qint8 toCol = 0;
        qint8 promotion = 0;
        in >> fromRow >> fromCol >> toRow >> toCol >> promotion;
        if (in.status() != QDataStream::Ok)
            return false;

        chess::Move wanted;
        wanted.from = { int(fromRow), int(fromCol) };
        wanted.to = { int(toRow), int(toCol) };
        wanted.promotion = chess::PieceType(promotion);
        // Matched against the legal moves rather than trusted: it proves the
        // saved game is one this build would play, and the castling and
        // en-passant flags come back set by the generator rather than by the
        // file.
        const std::vector<chess::Move> legal = game.legalMoves();
        const auto found = std::find(legal.begin(), legal.end(), wanted);
        if (found == legal.end())
            return false;
        game.play(*found);
    }
    if (game.isOver())
        return false; // decided since it was saved; nothing to resume into

    m_game = game;
    abandonSearch();
    m_level = chess::Level(std::clamp<int>(level, 0, 2));
    for (QAction* a : m_actions) {
        if (a->isCheckable())
            a->setChecked(a->text() == levelName(m_level));
    }

    m_selected.reset();
    m_selectedMoves.clear();
    m_lastMove = m_game.history().empty() ? std::optional<chess::Move> {}
                                          : std::optional<chess::Move> { m_game.history().back() };
    m_thinking = false;
    m_finished = false;
    m_undoAction->setEnabled(m_game.canUndo());
    // Not refresh(): if it is the engine's move, the game has to carry on from
    // where it stopped rather than sit waiting for a click that does nothing.
    advance();
    update();
    return true;
}

void ChessView::advance(const QString& message)
{
    if (m_game.isOver()) {
        m_finished = true;
        refresh();
        announceResult();
        return;
    }

    refresh(message);

    if (m_game.toMove() != m_human) {
        m_thinking = true;
        refresh();
        QTimer::singleShot(kThinkDelayMs, this, &ChessView::playEngineMove);
    }
}

void ChessView::playEngineMove()
{
    // A scheduled search can outlive the position it was scheduled for. New
    // Game, Undo and a level change all call abandonSearch(), which discards an
    // answer already in flight -- but none of them can cancel a singleShot that
    // has not fired yet, and when it does fire startSearch() captures the CURRENT
    // board and the CURRENT generation, so engineMoveReady's generation test
    // passes and the engine plays the human's move on a board it was never
    // launched for. m_thinking is the flag to read: advance() is the only thing
    // that sets it and every abandon path clears it, so it says whether this
    // timer is still the live one.
    if (m_paused || m_finished || !m_thinking || m_game.toMove() == m_human)
        return;
    startSearch();
}

void ChessView::startSearch()
{
    if (m_search == nullptr) {
        m_search = new QFutureWatcher<SearchResult>(this);
        connect(m_search, &QFutureWatcherBase::finished, this, [this] {
            // isCanceled() guards the case where the watcher was pointed at a
            // new search before this one reported: there is nothing to read.
            if (!m_search->isCanceled() && m_search->future().resultCount() > 0)
                engineMoveReady(m_search->result());
        });
    }

    // Everything the search needs, BY VALUE. The worker must not reach back
    // into the game: Board is copied per node by design and nothing is shared,
    // which is exactly what makes this safe to move off the GUI thread.
    const Board position = m_game.board();
    const Level level = m_level;
    const quint64 generation = m_generation;
    m_search->setFuture(QtConcurrent::run([position, level, generation] {
        SearchResult result;
        result.generation = generation;
        result.found = chooseMove(position, level, result.move);
        return result;
    }));
}

void ChessView::abandonSearch()
{
    // The answer in flight is now about a board that no longer exists. Bumping
    // the generation is what discards it; the worker runs on and harms nothing.
    ++m_generation;
    m_thinking = false;
}

void ChessView::engineMoveReady(const SearchResult& result)
{
    // The game moved on while this was being worked out -- a new game, an undo,
    // a change of level, or the hub leaving the page. Drop it.
    if (result.generation != m_generation)
        return;

    // Defence in depth for the same hazard playEngineMove() guards: a result
    // whose turn is no longer the engine's is not ours to play.
    if (m_finished || m_game.toMove() == m_human)
        return;

    m_thinking = false;
    const Move move = result.move;
    if (!result.found) {
        advance();
        return;
    }

    const bool capture = !m_game.board().at(move.to).empty() || move.enPassant;
    const QString text = QString::fromStdString(m_game.board().notation(move));
    m_game.play(move);
    m_lastMove = move;
    Sound::instance().play(capture ? Sound::kDiscFlip : Sound::kDiscPlace);

    // Through advance(), not around it. This used to re-implement advance()'s
    // over-check, refresh and announce inline -- a second path through the
    // function CLAUDE.md calls the single point that moves the game on. The two
    // agreed, but that second path is the structural reason the stale-timer
    // defect was reachable at all, and closing it is what stops the next one.
    advance(m_game.board().inCheck() ? QStringLiteral("Computer played %1 — check!").arg(text)
                                     : QStringLiteral("Computer played %1.").arg(text));
}

bool ChessView::choosePromotion(PieceType& out)
{
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Promotion"));
    box.setText(QStringLiteral("Your pawn reaches the last rank."));
    box.setInformativeText(QStringLiteral("What should it become?"));

    const PieceType offered[4] = { PieceType::Queen, PieceType::Rook, PieceType::Bishop,
                                   PieceType::Knight };
    QAbstractButton* buttons[4] {};
    for (int i = 0; i < 4; ++i)
        buttons[i] = box.addButton(pieceName(offered[i]), QMessageBox::AcceptRole);
    box.setDefaultButton(qobject_cast<QPushButton*>(buttons[0]));
    box.exec();

    for (int i = 0; i < 4; ++i) {
        if (box.clickedButton() == buttons[i]) {
            out = offered[i];
            return true;
        }
    }
    // Closing the dialog without choosing still has to produce a legal move,
    // and a queen is what all but a handful of promotions want.
    out = PieceType::Queen;
    return true;
}

void ChessView::announceResult()
{
    const Result result = m_game.result();
    const bool playerWon = result == (m_human == Colour::White ? Result::WhiteWins
                                                               : Result::BlackWins);
    const bool drawn = result == Result::Draw;
    Sound::instance().play(playerWon ? Sound::kWin : Sound::kLose);

    int wins = Scores::instance().best(winsKey());
    if (playerWon) {
        ++wins;
        Scores::instance().recordHigh(winsKey(), wins);
    }

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Game over"));
    if (drawn)
        box.setText(QStringLiteral("Drawn."));
    else
        box.setText(playerWon ? QStringLiteral("Checkmate — you win!")
                              : QStringLiteral("Checkmate — the computer wins."));
    box.setInformativeText(drawn ? QStringLiteral("%1\nGames won: %2.")
                                       .arg(drawText(m_game.drawReason()))
                                       .arg(wins)
                                 : QStringLiteral("Moves played: %1.\nGames won: %2.")
                                       .arg(m_game.history().size())
                                       .arg(wins));
    QAbstractButton* again = box.addButton(QStringLiteral("Play Again"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("Close"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == again)
        newGame();
}

void ChessView::refresh(const QString& message)
{
    update();

    QString state;
    if (!message.isEmpty())
        state = message;
    else if (m_finished)
        state = QStringLiteral("Game over.");
    else if (m_thinking)
        state = QStringLiteral("Computer thinking…");
    else if (m_game.toMove() == m_human)
        state = m_game.board().inCheck() ? QStringLiteral("You are in check.")
                                         : QStringLiteral("Your move.");
    else
        state = QStringLiteral("Computer to move.");

    m_caption = state;
    Q_EMIT statusChanged(QStringLiteral("%1   Material %2 — %3   Won %4")
                             .arg(state)
                             .arg(m_game.board().material(m_human))
                             .arg(m_game.board().material(other(m_human)))
                             .arg(Scores::instance().best(winsKey())));
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

QRect ChessView::boardRect() const
{
    // Under the legibility switch the board gives up a strip at the bottom for
    // the caption, and moves up by it, so the sentence never covers a piece.
    const int band = int(captionBand(QRectF(rect())));
    const int available = std::min(width(), height() - band) - 2 * (kFrameWidth + 4);
    const int side = std::max(kFiles, (available / kFiles) * kFiles);
    return { (width() - side) / 2, (height() - band - side) / 2, side, side };
}

std::optional<Square> ChessView::squareAt(QPointF pos) const
{
    const QRect r = boardRect();
    if (!r.contains(pos.toPoint()))
        return std::nullopt;
    const double cell = r.width() / double(kFiles);
    const Square s { int((pos.y() - r.y()) / cell), int((pos.x() - r.x()) / cell) };
    return s.valid() ? std::optional<Square>(s) : std::nullopt;
}

std::vector<Move> ChessView::movesFrom(Square s) const
{
    std::vector<Move> out;
    for (const Move& m : m_game.legalMoves())
        if (m.from == s)
            out.push_back(m);
    return out;
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void ChessView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().window());

    const QRect r = boardRect();
    if (r.width() <= 0)
        return;
    const double cell = r.width() / double(kFiles);

    Theme::paintWoodFrame(p, r, kFrameWidth, 7);

    for (int row = 0; row < kRanks; ++row)
        for (int col = 0; col < kFiles; ++col)
            p.fillRect(QRectF(r.x() + col * cell, r.y() + row * cell, cell, cell),
                       (row + col) % 2 ? ChessArt::kDarkSquare : ChessArt::kLightSquare);

    // Files and ranks, engraved on the frame — the notation in the status line
    // is unreadable without them.
    QFont labels = font();
    // The frame is only as thick as kFrameWidth, so the label cannot simply be
    // scaled up — Qt::TextDontClip below is what hands over the gap between the
    // glyph's ink and its line box, the same trick Sudoku's pencil marks need.
    labels.setPointSizeF(std::max(6.0, cell * (Legibility::instance().enabled() ? 0.28 : 0.20)));
    labels.setBold(true);
    p.setFont(labels);
    p.setPen(Theme::kGold);
    for (int i = 0; i < kFiles; ++i) {
        p.drawText(QRectF(r.x() + i * cell, r.bottom(), cell, kFrameWidth),
                   Qt::AlignCenter | Qt::TextDontClip, QString(QChar('a' + i)));
        p.drawText(QRectF(r.x() - kFrameWidth, r.y() + i * cell, kFrameWidth, cell),
                   Qt::AlignCenter | Qt::TextDontClip, QString::number(kRanks - i));
    }

    const auto squareRect = [&](Square s) {
        return QRectF(r.x() + s.col * cell, r.y() + s.row * cell, cell, cell);
    };

    // The move just played, so the computer's reply is easy to follow.
    if (m_lastMove) {
        p.setPen(Qt::NoPen);
        // Faint enough to be missed at 70; the switch is exactly the player who
        // would miss it, so it goes to a wash you cannot look past.
        p.setBrush(QColor(0xff, 0xd5, 0x4f, Legibility::instance().enabled() ? 150 : 70));
        for (const Square& s : { m_lastMove->from, m_lastMove->to })
            p.drawRect(squareRect(s));
    }

    // A king in check has to be impossible to miss.
    if (!m_finished && m_game.board().inCheck()) {
        const Square king = m_game.board().kingSquare(m_game.toMove());
        if (king.valid()) {
            QRadialGradient glow(squareRect(king).center(), cell * 0.6);
            glow.setColorAt(0.0, QColor(0xe0, 0x3b, 0x2f, 190));
            glow.setColorAt(1.0, QColor(0xe0, 0x3b, 0x2f, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(glow);
            p.drawRect(squareRect(king));
        }
    }

    // Where the selected piece can go: a dot on an empty square, a ring around
    // a piece it can take.
    p.setPen(Qt::NoPen);
    for (const Move& m : m_selectedMoves) {
        const QRectF box = squareRect(m.to);
        const bool capture = !m_game.board().at(m.to).empty() || m.enPassant;
        if (capture) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0x66, 0xe0, 0x8a, 200), std::max(2.0, cell * 0.07)));
            p.drawEllipse(box.center(), cell * 0.42, cell * 0.42);
            p.setPen(Qt::NoPen);
        } else {
            p.setBrush(QColor(0x66, 0xe0, 0x8a, 150));
            p.drawEllipse(box.center(), cell * 0.14, cell * 0.14);
        }
    }

    if (m_selected) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0xff, 0xd5, 0x4f), std::max(2.0, cell * 0.06)));
        p.drawRect(squareRect(*m_selected));
    }

    for (int row = 0; row < kRanks; ++row) {
        for (int col = 0; col < kFiles; ++col) {
            const Piece piece = m_game.board().at(row, col);
            if (piece.empty())
                continue;
            ChessArt::paintPiece(p, squareRect({ row, col }).adjusted(cell * 0.06, cell * 0.04,
                                                                     -cell * 0.06, -cell * 0.02),
                                 piece.type, piece.colour);
        }
    }

    paintStatusCaption(p, QRectF(rect()));
}

void ChessView::mousePressEvent(QMouseEvent* event)
{
    if (m_finished || m_thinking || m_game.toMove() != m_human
        || event->button() != Qt::LeftButton)
        return;

    const std::optional<Square> clicked = squareAt(event->position());
    if (!clicked)
        return;

    // Clicking a highlighted destination plays that move.
    if (m_selected) {
        std::vector<Move> matching;
        for (const Move& m : m_selectedMoves)
            if (m.to == *clicked)
                matching.push_back(m);

        if (!matching.empty()) {
            Move chosen = matching.front();
            if (matching.size() > 1) {
                PieceType promotion = PieceType::Queen;
                choosePromotion(promotion);
                for (const Move& m : matching)
                    if (m.promotion == promotion)
                        chosen = m;
            }

            const bool capture = !m_game.board().at(chosen.to).empty() || chosen.enPassant;
            m_game.play(chosen);
            m_lastMove = chosen;
            m_undoAction->setEnabled(true);
            Sound::instance().play(capture ? Sound::kDiscFlip : Sound::kDiscPlace);
            m_selected.reset();
            m_selectedMoves.clear();
            advance();
            return;
        }
    }

    // Otherwise select one of the player's own pieces that has a move.
    if (m_game.board().at(*clicked).is(m_human)) {
        std::vector<Move> moves = movesFrom(*clicked);
        if (moves.empty()) {
            refresh(m_game.board().inCheck()
                        ? QStringLiteral("You are in check — that piece cannot help.")
                        : QStringLiteral("That piece has no legal move."));
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
