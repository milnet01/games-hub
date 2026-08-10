#include "reversiview.h"

#include "scores.h"
#include "sound.h"
#include "theme.h"

#include <QActionGroup>
#include <QLinearGradient>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QTimer>

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

    auto* levels = new QActionGroup(this);
    levels->setExclusive(true);
    const struct { const char* name; Difficulty value; } kLevels[] = {
        { "Easy", Difficulty::Easy },
        { "Medium", Difficulty::Medium },
        { "Hard", Difficulty::Hard },
    };
    for (const auto& entry : kLevels) {
        auto* a = new QAction(QString::fromUtf8(entry.name), this);
        a->setCheckable(true);
        a->setChecked(entry.value == m_difficulty);
        levels->addAction(a);
        const Difficulty value = entry.value;
        connect(a, &QAction::triggered, this, [this, value] { m_difficulty = value; });
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

void ReversiView::activate()
{
    refresh();
}

void ReversiView::newGame()
{
    m_board.reset();
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
    if (m_history.empty() || m_thinking)
        return;

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
        refresh(who + QStringLiteral(" no legal move — turn passes."));
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
    const std::optional<Move> m = chooseMove(m_board, m_toMove, m_difficulty);
    m_thinking = false;

    if (!m) {
        advance();
        return;
    }

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
    const int available = std::min(width(), height()) - 2 * (kFrameWidth + 4);
    const int side = std::max(kSize, (available / kSize) * kSize);
    return { (width() - side) / 2, (height() - side) / 2, side, side };
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
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(255, 255, 255, 70));
                    p.drawEllipse(centre, cell * 0.12, cell * 0.12);
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

    const int flipped = m_board.play(m_human, *m);
    Sound::instance().play(Sound::kDiscPlace);
    if (flipped > 2)
        Sound::instance().play(Sound::kDiscFlip);
    m_lastMove = m;
    m_toMove = opponent(m_toMove);
    advance();
}
