#include "draughtsview.h"

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

    auto* group = new QActionGroup(this);
    group->setExclusive(true);
    const struct { const char* name; DraughtsLevel level; } kLevels[] = {
        { "Easy", DraughtsLevel::Easy },
        { "Medium", DraughtsLevel::Medium },
        { "Hard", DraughtsLevel::Hard },
    };
    for (const auto& entry : kLevels) {
        auto* a = new QAction(QString::fromUtf8(entry.name), this);
        a->setCheckable(true);
        a->setChecked(entry.level == m_level);
        group->addAction(a);
        const DraughtsLevel level = entry.level;
        connect(a, &QAction::triggered, this, [this, level] { m_level = level; });
        m_actions.append(a);
    }
}

void DraughtsView::newGame()
{
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
    if (m_history.empty() || m_thinking)
        return;
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

void DraughtsView::activate()
{
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
    DraughtsMove move;
    m_thinking = false;
    if (!chooseDraughtsMove(m_board, m_toMove, m_level, move)) {
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
    const int available = std::min(width(), height()) - 2 * (kFrameWidth + 4);
    const int side = std::max(kBoardSize, (available / kBoardSize) * kBoardSize);
    return { (width() - side) / 2, (height() - side) / 2, side, side };
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
        p.setBrush(QColor(0xff, 0xd5, 0x4f, 60));
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
