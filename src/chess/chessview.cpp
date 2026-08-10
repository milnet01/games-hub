#include "chessview.h"

#include "chessart.h"
#include "scores.h"
#include "sound.h"
#include "theme.h"

#include <QActionGroup>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadialGradient>
#include <QTimer>

#include <algorithm>

using namespace chess;

namespace {
constexpr int kFrameWidth = 18;
constexpr int kThinkDelayMs = 260;

const QString kWinsKey = QStringLiteral("chess/wins");

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
    setMinimumSize(minimumSizeHint());
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
    const struct { const char* name; Level level; } kLevels[] = {
        { "Easy", Level::Easy },
        { "Medium", Level::Medium },
        { "Hard", Level::Hard },
    };
    for (const auto& entry : kLevels) {
        auto* a = new QAction(QString::fromUtf8(entry.name), this);
        a->setCheckable(true);
        a->setChecked(entry.level == m_level);
        group->addAction(a);
        const Level level = entry.level;
        connect(a, &QAction::triggered, this, [this, level] { m_level = level; });
        m_actions.append(a);
    }
}

void ChessView::newGame()
{
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
    if (!m_game.canUndo() || m_thinking)
        return;
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

void ChessView::activate()
{
    refresh();
}

void ChessView::advance()
{
    if (m_game.isOver()) {
        m_finished = true;
        refresh();
        announceResult();
        return;
    }

    refresh();

    if (m_game.toMove() != m_human) {
        m_thinking = true;
        refresh();
        QTimer::singleShot(kThinkDelayMs, this, &ChessView::playEngineMove);
    }
}

void ChessView::playEngineMove()
{
    m_thinking = false;
    Move move;
    if (!chooseMove(m_game.board(), m_level, move)) {
        advance();
        return;
    }

    const bool capture = !m_game.board().at(move.to).empty() || move.enPassant;
    const QString text = QString::fromStdString(m_game.board().notation(move));
    m_game.play(move);
    m_lastMove = move;
    Sound::instance().play(capture ? Sound::kDiscFlip : Sound::kDiscPlace);

    if (m_game.isOver()) {
        m_finished = true;
        refresh();
        announceResult();
        return;
    }
    refresh(m_game.board().inCheck() ? QStringLiteral("Computer played %1 — check!").arg(text)
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

    int wins = Scores::instance().best(kWinsKey);
    if (playerWon) {
        ++wins;
        Scores::instance().recordHigh(kWinsKey, wins);
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

    Q_EMIT statusChanged(QStringLiteral("%1   Material %2 — %3   Won %4")
                             .arg(state)
                             .arg(m_game.board().material(m_human))
                             .arg(m_game.board().material(other(m_human)))
                             .arg(Scores::instance().best(kWinsKey)));
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

QRect ChessView::boardRect() const
{
    const int available = std::min(width(), height()) - 2 * (kFrameWidth + 4);
    const int side = std::max(kFiles, (available / kFiles) * kFiles);
    return { (width() - side) / 2, (height() - side) / 2, side, side };
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
    labels.setPointSizeF(std::max(6.0, cell * 0.20));
    labels.setBold(true);
    p.setFont(labels);
    p.setPen(Theme::kGold);
    for (int i = 0; i < kFiles; ++i) {
        p.drawText(QRectF(r.x() + i * cell, r.bottom(), cell, kFrameWidth),
                   Qt::AlignCenter, QString(QChar('a' + i)));
        p.drawText(QRectF(r.x() - kFrameWidth, r.y() + i * cell, kFrameWidth, cell),
                   Qt::AlignCenter, QString::number(kRanks - i));
    }

    const auto squareRect = [&](Square s) {
        return QRectF(r.x() + s.col * cell, r.y() + s.row * cell, cell, cell);
    };

    // The move just played, so the computer's reply is easy to follow.
    if (m_lastMove) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xff, 0xd5, 0x4f, 70));
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
