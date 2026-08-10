#include "heartsview.h"

#include "cards/cardart.h"

#include <QMessageBox>
#include <QPushButton>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#include <algorithm>

namespace {
constexpr QColor kFeltTop { 0x2a, 0x3c, 0x66 };
constexpr QColor kFeltBottom { 0x16, 0x20, 0x3c };

// Seats are drawn clockwise from the player: 0 bottom, 1 left, 2 top, 3 right.
const char* kSeatNames[HeartsEngine::kPlayers] = { "You", "West", "North", "East" };

constexpr int kAiDelayMs = 600;
constexpr int kTrickPauseMs = 900;

QString directionName(HeartsEngine::PassDirection d)
{
    switch (d) {
    case HeartsEngine::PassDirection::Left:   return QStringLiteral("left");
    case HeartsEngine::PassDirection::Right:  return QStringLiteral("right");
    case HeartsEngine::PassDirection::Across: return QStringLiteral("across");
    case HeartsEngine::PassDirection::Hold:   return QStringLiteral("nobody");
    }
    return {};
}
}

HeartsView::HeartsView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(minimumSizeHint());

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &HeartsView::step);

    buildActions();
    newGame();
}

void HeartsView::buildActions()
{
    auto* newAction = new QAction(QStringLiteral("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &HeartsView::newGame);
    m_actions.append(newAction);

    m_passAction = new QAction(QStringLiteral("Pass 3 Cards"), this);
    m_passAction->setEnabled(false);
    connect(m_passAction, &QAction::triggered, this, &HeartsView::confirmPass);
    m_actions.append(m_passAction);
}

void HeartsView::newGame()
{
    m_engine.newGame();
    m_selected.clear();
    m_awaitingCollect = false;
    m_announced = false;
    update();
    refresh();
    // A hold hand starts in the play phase and may not be the player's lead.
    if (m_engine.phase() == HeartsEngine::Phase::Playing)
        m_timer->start(kAiDelayMs);
}

void HeartsView::activate()
{
    refresh();
}

void HeartsView::confirmPass()
{
    if (m_engine.phase() != HeartsEngine::Phase::Passing || m_selected.size() != 3)
        return;

    m_engine.setPass(0, m_selected);
    for (int p = 1; p < HeartsEngine::kPlayers; ++p)
        m_engine.setPass(p, m_engine.chooseAiPass(p));

    m_engine.executePass();
    m_selected.clear();
    m_passAction->setEnabled(false);
    update();
    refresh();

    if (m_engine.currentPlayer() != 0)
        m_timer->start(kAiDelayMs);
}

void HeartsView::step()
{
    if (m_awaitingCollect) {
        finishTrick();
        return;
    }

    if (m_engine.phase() != HeartsEngine::Phase::Playing)
        return;

    if (m_engine.trickComplete()) {
        m_awaitingCollect = true;
        m_timer->start(kTrickPauseMs);
        return;
    }

    const int seat = m_engine.currentPlayer();
    if (seat == 0) {
        refresh();
        return; // the player's move
    }

    m_engine.playCard(seat, m_engine.chooseAiCard(seat));
    update();
    refresh();

    // Leave a completed trick on the table briefly before sweeping it up.
    m_timer->start(m_engine.trickComplete() ? kTrickPauseMs : kAiDelayMs);
    if (m_engine.trickComplete())
        m_awaitingCollect = true;
}

void HeartsView::finishTrick()
{
    m_awaitingCollect = false;
    m_engine.collectTrick();
    update();
    refresh();

    if (m_engine.phase() == HeartsEngine::Phase::HandOver
        || m_engine.phase() == HeartsEngine::Phase::GameOver) {
        announceHand();
        return;
    }

    if (m_engine.currentPlayer() != 0)
        m_timer->start(kAiDelayMs);
}

void HeartsView::announceHand()
{
    if (m_announced)
        return;
    m_announced = true;

    QTimer::singleShot(300, this, [this] {
        const bool over = m_engine.phase() == HeartsEngine::Phase::GameOver;

        QString detail;
        for (int p = 0; p < HeartsEngine::kPlayers; ++p) {
            detail += QStringLiteral("%1: %2 this hand, %3 total\n")
                          .arg(QString::fromUtf8(kSeatNames[p]))
                          .arg(m_engine.handPoints(p))
                          .arg(m_engine.total(p));
        }

        QMessageBox box(this);
        box.setWindowTitle(over ? QStringLiteral("Game over") : QStringLiteral("Hand over"));
        if (over) {
            const int w = m_engine.winner();
            box.setText(w == 0 ? QStringLiteral("You win!")
                               : QStringLiteral("%1 wins.").arg(QString::fromUtf8(kSeatNames[w])));
        } else {
            box.setText(QStringLiteral("Hand complete."));
        }
        box.setInformativeText(detail.trimmed());

        QAbstractButton* go = box.addButton(over ? QStringLiteral("New Game")
                                                 : QStringLiteral("Next Hand"),
                                            QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Close"), QMessageBox::RejectRole);
        box.exec();

        if (box.clickedButton() != go)
            return;

        if (over) {
            newGame();
        } else {
            m_engine.nextHand();
            m_announced = false;
            m_selected.clear();
            update();
            refresh();
            if (m_engine.phase() == HeartsEngine::Phase::Playing)
                m_timer->start(kAiDelayMs);
        }
    });
}

void HeartsView::refresh()
{
    QString state;
    switch (m_engine.phase()) {
    case HeartsEngine::Phase::Passing:
        state = QStringLiteral("Choose 3 cards to pass %1 (%2 chosen)")
                    .arg(directionName(m_engine.passDirection()))
                    .arg(m_selected.size());
        break;
    case HeartsEngine::Phase::Playing:
        state = m_engine.currentPlayer() == 0
            ? QStringLiteral("Your turn")
            : QStringLiteral("%1 is thinking…")
                  .arg(QString::fromUtf8(kSeatNames[m_engine.currentPlayer()]));
        break;
    case HeartsEngine::Phase::HandOver:
        state = QStringLiteral("Hand over");
        break;
    case HeartsEngine::Phase::GameOver:
        state = QStringLiteral("Game over");
        break;
    }

    m_passAction->setEnabled(m_engine.phase() == HeartsEngine::Phase::Passing
                             && m_selected.size() == 3);

    Q_EMIT statusChanged(QStringLiteral("%1   You %2  West %3  North %4  East %5   %6")
                             .arg(state)
                             .arg(m_engine.total(0))
                             .arg(m_engine.total(1))
                             .arg(m_engine.total(2))
                             .arg(m_engine.total(3))
                             .arg(m_engine.heartsBroken() ? QStringLiteral("hearts broken")
                                                          : QStringLiteral("hearts intact")));
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

double HeartsView::cardWidth() const
{
    // The player's 13 cards overlap; the width is set so a full hand fits with
    // room for the opponents down the sides.
    const double byWidth = (width() - 200.0) / 8.0;
    const double byHeight = height() / 5.4;
    return std::clamp(std::min(byWidth, byHeight), 40.0, 92.0);
}

QRectF HeartsView::handCardRect(int index) const
{
    const std::vector<Card>& hand = m_engine.hand(0);
    const double w = cardWidth();
    const double h = cardHeight();
    const int n = int(hand.size());
    if (n == 0)
        return {};

    const double overlap = std::min(w, (width() - 60.0) / std::max(1, n));
    const double total = overlap * (n - 1) + w;
    const double x = (width() - total) / 2 + index * overlap;

    // Selected cards lift, which is how the player sees their pass forming.
    const bool lifted = std::find(m_selected.begin(), m_selected.end(), hand[std::size_t(index)])
        != m_selected.end();
    return { x, height() - h - 16 - (lifted ? h * 0.18 : 0.0), w, h };
}

QRectF HeartsView::trickCardRect(int seat) const
{
    const double w = cardWidth() * 0.9;
    const double h = w * 1.4;
    const QPointF c(width() / 2.0, height() / 2.0 - cardHeight() * 0.25);
    const double dx = w * 0.85;
    const double dy = h * 0.55;

    switch (seat) {
    case 0: return { c.x() - w / 2, c.y() + dy * 0.4, w, h };
    case 1: return { c.x() - dx - w / 2, c.y() - h / 2, w, h };
    case 2: return { c.x() - w / 2, c.y() - dy - h * 0.6, w, h };
    default: return { c.x() + dx - w / 2, c.y() - h / 2, w, h };
    }
}

QRectF HeartsView::opponentRect(int seat) const
{
    const double w = cardWidth() * 0.7;
    const double h = w * 1.4;
    switch (seat) {
    case 1: return { 14, height() / 2.0 - h / 2, w, h };
    case 2: return { width() / 2.0 - w / 2, 14, w, h };
    default: return { width() - 14 - w, height() / 2.0 - h / 2, w, h };
    }
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void HeartsView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient felt(rect().topLeft(), rect().bottomLeft());
    felt.setColorAt(0, kFeltTop);
    felt.setColorAt(1, kFeltBottom);
    p.fillRect(rect(), felt);

    // Opponents: a fanned stack of backs plus a name and card count.
    for (int seat = 1; seat < HeartsEngine::kPlayers; ++seat) {
        const QRectF r = opponentRect(seat);
        const int n = int(m_engine.hand(seat).size());
        for (int i = 0; i < std::min(n, 6); ++i)
            CardArt::paintBack(p, r.translated(i * 3.0, i * 2.0));

        QFont f = font();
        f.setBold(true);
        p.setFont(f);
        p.setPen(m_engine.currentPlayer() == seat ? QColor(0xff, 0xd5, 0x4f)
                                                  : QColor(0xd6, 0xdd, 0xe6));

        // Anchor each label to the edge its seat sits against, or the left and
        // right names run off the window.
        const QString text = QStringLiteral("%1 (%2)")
                                 .arg(QString::fromUtf8(kSeatNames[seat]))
                                 .arg(n);
        const QRectF label(8, r.bottom() + 20, width() - 16, 20);
        const int align = (seat == 1) ? Qt::AlignLeft
            : (seat == 3)             ? Qt::AlignRight
                                      : Qt::AlignHCenter;
        p.drawText(label, align | Qt::AlignVCenter, text);
    }

    // The trick in the middle.
    for (const auto& [seat, card] : m_engine.trick())
        CardArt::paintFace(p, trickCardRect(seat), card);

    if (m_engine.trick().empty() && m_engine.phase() == HeartsEngine::Phase::Playing) {
        p.setPen(QColor(255, 255, 255, 60));
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() + 2);
        p.setFont(f);
        p.drawText(QRectF(0, height() / 2.0 - 40, width(), 40), Qt::AlignCenter,
                   m_engine.currentPlayer() == 0 ? QStringLiteral("Your lead")
                                                 : QStringLiteral("Waiting…"));
    }

    // The player's hand, left to right.
    const std::vector<Card>& hand = m_engine.hand(0);
    const std::vector<Card> legal = m_engine.legalPlays(0);
    const bool playing = m_engine.phase() == HeartsEngine::Phase::Playing
        && m_engine.currentPlayer() == 0 && !m_engine.trickComplete();

    for (int i = 0; i < int(hand.size()); ++i) {
        const QRectF r = handCardRect(i);
        CardArt::paintFace(p, r, hand[std::size_t(i)]);

        // Dim what cannot legally be played, rather than silently ignoring the
        // click later.
        const bool allowed = !playing
            || std::find(legal.begin(), legal.end(), hand[std::size_t(i)]) != legal.end();
        if (!allowed) {
            p.save();
            p.setBrush(QColor(0, 0, 0, 110));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(r, r.width() * 0.08, r.width() * 0.08);
            p.restore();
        }
        if (std::find(m_selected.begin(), m_selected.end(), hand[std::size_t(i)]) != m_selected.end())
            CardArt::paintHighlight(p, r, QColor(0xff, 0xd5, 0x4f));
    }
}

void HeartsView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    const std::vector<Card>& hand = m_engine.hand(0);

    // Topmost card first: the hand is drawn left to right, so later cards
    // overlap earlier ones.
    for (int i = int(hand.size()) - 1; i >= 0; --i) {
        if (!handCardRect(i).contains(event->position()))
            continue;

        const Card& card = hand[std::size_t(i)];

        if (m_engine.phase() == HeartsEngine::Phase::Passing) {
            auto it = std::find(m_selected.begin(), m_selected.end(), card);
            if (it != m_selected.end())
                m_selected.erase(it);
            else if (m_selected.size() < 3)
                m_selected.push_back(card);
            update();
            refresh();
            return;
        }

        if (m_engine.phase() == HeartsEngine::Phase::Playing && m_engine.currentPlayer() == 0) {
            if (!m_engine.playCard(0, card))
                return;
            update();
            refresh();
            if (m_engine.trickComplete()) {
                m_awaitingCollect = true;
                m_timer->start(kTrickPauseMs);
            } else {
                m_timer->start(kAiDelayMs);
            }
        }
        return;
    }
}
