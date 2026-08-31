#include "heartsview.h"

#include "scores.h"
#include "sound.h"
#include "cards/cardart.h"
#include "cards/cardcodec.h"
#include "theme.h"

#include <QDataStream>
#include <QMessageBox>
#include <QPushButton>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#include <algorithm>

namespace {

// Seats are drawn clockwise from the player: 0 bottom, 1 left, 2 top, 3 right.
const char* kSeatNames[HeartsEngine::kPlayers] = { "You", "West", "North", "East" };

constexpr int kAiDelayMs = 600;
constexpr int kTrickPauseMs = 900;

// How far a card chosen for the pass rises out of the hand, as a fraction of a
// card's height. Named because two places have to agree on it: the hand draws
// the lift, and the caption has to keep clear of where it lifted to.
constexpr double kPassLift = 0.18;

// An opponent's hand is drawn as a short fanned stack of backs. Named for the
// same reason: the painter lays the fan out and opponentStackRect() has to
// report the room it takes.
constexpr double kFanStepX = 3.0;
constexpr double kFanStepY = 2.0;
constexpr int kFanCards = 6;

// Where the i-th back of a seat's stack sits, relative to the seat's own rect.
//
// East fans towards the centre of the table rather than towards the wall. Its
// pile is anchored to the right edge, so fanning right ran the front card a
// pixel off the window and cut its border away (GHUB-0092) -- a card with a
// missing edge reads as a rendering fault rather than as a design. West already
// fans inwards from the left edge, so mirroring East also makes the two sides
// match, with each seat's front card facing the table.
QPointF fanOffset(int seat, int i)
{
    const double direction = (seat == 3) ? -1.0 : 1.0;
    return { direction * i * kFanStepX, i * kFanStepY };
}

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

    // nextHand() had exactly one caller: the hand-over box's accept button. So
    // Close ended the match for good -- the phase stays HandOver, step() returns
    // at once, no click is accepted, and saveState() stores that state, so it
    // survived a restart too. The only way out was New Game, which threw away
    // every accumulated total.
    m_nextHandAction = new QAction(QStringLiteral("Next Hand"), this);
    m_nextHandAction->setEnabled(false);
    connect(m_nextHandAction, &QAction::triggered, this, &HeartsView::startNextHand);
    m_actions.append(m_nextHandAction);
}

void HeartsView::newGame()
{
    m_engine.newGame();
    Sound::instance().play(Sound::kShuffle);
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
    // Pick the hand back up. deactivate() stops the clock wherever it stood,
    // so without this the computers stay frozen mid-trick and the game is
    // stuck — a worse bug than the one stopping them fixes.
    if (m_awaitingCollect
        || (m_engine.phase() == HeartsEngine::Phase::Playing && m_engine.currentPlayer() != 0))
        m_timer->start(kAiDelayMs);
    // An announcement that came due while the hub was elsewhere.
    if (m_announcePending) {
        m_announcePending = false;
        announceHand();
    }
    refresh();
}

void HeartsView::deactivate()
{
    m_timer->stop();
}

void HeartsView::startNextHand()
{
    if (m_engine.phase() != HeartsEngine::Phase::HandOver)
        return;
    m_engine.nextHand();
    m_announced = false;
    m_selected.clear();
    update();
    refresh();
    if (m_engine.phase() == HeartsEngine::Phase::Playing)
        m_timer->start(kAiDelayMs);
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
    Sound::instance().play(Sound::kCardPlace);
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
    Sound::instance().play(Sound::kCardDeal);
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
        // deactivate() stops m_timer, and cannot stop this. Leaving Hearts
        // inside the 300 ms would otherwise open a modal box over whatever game
        // the hub moved to, and its Next Hand would then run a whole hand on a
        // page nobody is looking at -- the defect the deactivate() override
        // exists to prevent, reached by a second route. Hold it until we are
        // back instead.
        if (!isVisible()) {
            m_announced = false;
            m_announcePending = true;
            return;
        }
        m_announcePending = false;
        const bool over = m_engine.phase() == HeartsEngine::Phase::GameOver;
        // A win is recorded by the total you finished on — lower is better.
        const bool newBest = over && m_engine.winner() == 0
            && Scores::instance().recordLow(Scores::heartsBestScore(), m_engine.total(0));

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
        if (newBest)
            detail += QStringLiteral("\nA new best — your lowest winning score yet!");
        else if (over && Scores::instance().has(Scores::heartsBestScore()))
            detail += QStringLiteral("\nBest winning score: %1.")
                          .arg(Scores::instance().best(Scores::heartsBestScore()));
        box.setInformativeText(detail.trimmed());

        QAbstractButton* go = box.addButton(over ? QStringLiteral("New Game")
                                                 : QStringLiteral("Next Hand"),
                                            QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Close"), QMessageBox::RejectRole);
        box.exec();

        if (box.clickedButton() != go)
            return;

        if (over)
            newGame();
        else
            startNextHand();
    });
}

QString HeartsView::captionText() const
{
    switch (m_engine.phase()) {
    case HeartsEngine::Phase::Passing:
        return QStringLiteral("Choose 3 cards to pass %1 — %2 chosen.")
            .arg(directionName(m_engine.passDirection()))
            .arg(m_selected.size());
    case HeartsEngine::Phase::HandOver:
        return QStringLiteral("Hand over.");
    case HeartsEngine::Phase::GameOver:
        return QStringLiteral("Game over.");
    case HeartsEngine::Phase::Playing:
        break;
    }

    const std::vector<std::pair<int, Card>>& trick = m_engine.trick();
    const QString turn = m_engine.currentPlayer() == 0
        ? QStringLiteral("Your turn.")
        : QStringLiteral("%1 is thinking…").arg(QString::fromUtf8(kSeatNames[m_engine.currentPlayer()]));
    if (trick.empty())
        return m_engine.currentPlayer() == 0 ? QStringLiteral("Your lead.") : turn;

    // Spelled out rather than a ♥ glyph: the suit led is the one fact that
    // decides what you may play, and a symbol at sentence size is what the
    // legibility switch exists to stop the player squinting at.
    return QStringLiteral("%1 led %2.  %3")
        .arg(QString::fromUtf8(kSeatNames[trick.front().first]))
        .arg(suitName(trick.front().second.suit))
        .arg(turn);
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
    if (m_nextHandAction != nullptr)
        m_nextHandAction->setEnabled(m_engine.phase() == HeartsEngine::Phase::HandOver);

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
    return { x, height() - h - 16 - (lifted ? h * kPassLift : 0.0), w, h };
}

// cardWidth() is capped at 92, so past a certain width the trick stops moving
// down the window while the hand stays anchored to the bottom -- and the plate
// grows UPWARD from the bottom of whatever area it is given, so a gap too small
// to hold it does not shrink it, it puts it over the card you just played. That
// is GHUB-0084, and clamping the area's height to zero did not close it because
// Theme::layoutCaption bails on zero WIDTH, not zero height. Measure the plate
// and raise the trick by the shortfall instead.
double HeartsView::trickLift() const
{
    const QRectF surface(rect());
    const QString text = captionText();
    if (text.isEmpty())
        return 0.0;
    const double lift = m_selected.empty() ? 0.0 : cardHeight() * kPassLift;
    const double bottom = height() - cardHeight() - 16 - lift;
    const double plate =
        Theme::captionRect(surface, text, captionFont(surface)).height();
    const double w = cardWidth() * 0.9;
    const double trickBottom = height() / 2.0 - cardHeight() * 0.25 + w * 1.4 * 0.55 * 0.4
        + w * 1.4;
    const double shortfall = plate - (bottom - trickBottom);
    return std::max(0.0, shortfall);
}

QRectF HeartsView::trickCardRect(int seat) const
{
    const double w = cardWidth() * 0.9;
    const double h = w * 1.4;
    const QPointF c(width() / 2.0, height() / 2.0 - cardHeight() * 0.25 - trickLift());
    const double dx = w * 0.85;
    const double dy = h * 0.55;

    switch (seat) {
    case 0: return { c.x() - w / 2, c.y() + dy * 0.4, w, h };
    case 1: return { c.x() - dx - w / 2, c.y() - h / 2, w, h };
    case 2: return { c.x() - w / 2, c.y() - dy - h * 0.6, w, h };
    default: return { c.x() + dx - w / 2, c.y() - h / 2, w, h };
    }
}

QRectF HeartsView::opponentStackRect(int seat) const
{
    // The room a full stack takes, not the room the current one takes: a pile
    // that crept towards the edge as its owner played cards away would be worse
    // than one that simply sits where it sits.
    const QRectF base = opponentRect(seat);
    return base.united(base.translated(fanOffset(seat, kFanCards - 1)));
}

QRectF HeartsView::captionArea() const
{
    // The hand is anchored to the bottom, so the caption goes in the gap above
    // it rather than in a reserved band — a band here would come off the cards.
    // Two things live in that gap and the caption must yield to both.
    //
    // Cards chosen for the pass rise out of the hand, and the plate is bottom
    // aligned, so without this term the three cards you just chose slide under
    // it and lose the top of their gold outline (GHUB-0085).
    const double lift = m_selected.empty() ? 0.0 : cardHeight() * kPassLift;
    const double bottom = height() - cardHeight() - 16 - lift;

    // And the trick sits above. cardWidth() is capped, so past a certain width
    // the trick stops moving down the window while the plate keeps rising to
    // meet it: at 900x600 and 1400x620 the plate covered the seat-0 card, which
    // is the one YOU played, while the other three stayed visible (GHUB-0084).
    // Seat 0 is the lowest of the four, so its bottom edge is the whole trick's.
    // Reserved whether or not a card is there, so the plate does not hop up the
    // window each time a trick is swept.
    const double top = std::min(trickCardRect(0).bottom(), bottom);
    return { 0.0, top, double(width()), bottom - top };
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

    Theme::paintFelt(p, rect(), Theme::kFeltBlueTop, Theme::kFeltBlueBottom);

    // Opponents: a fanned stack of backs plus a name and card count.
    for (int seat = 1; seat < HeartsEngine::kPlayers; ++seat) {
        const QRectF r = opponentRect(seat);
        const int n = int(m_engine.hand(seat).size());
        for (int i = 0; i < std::min(n, kFanCards); ++i)
            CardArt::paintBack(p, r.translated(fanOffset(seat, i)));

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

    paintStatusCaption(p, captionArea());
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
            Sound::instance().play(Sound::kCardPlace);
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

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

QByteArray HeartsView::saveState() const
{
    // A finished game is not worth coming back to; New Game is the answer to
    // that, and keeping it would resume onto the final scores every time.
    // An empty state also clears whatever was stored before.
    if (m_engine.phase() == HeartsEngine::Phase::GameOver)
        return {};

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1);
    m_engine.save(out);
    cardcodec::writePile(out, m_selected);
    out << qint8(m_awaitingCollect ? 1 : 0) << qint8(m_announced ? 1 : 0);
    return blob;
}

bool HeartsView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    in >> version;
    if (version != 1 || in.status() != QDataStream::Ok)
        return false;

    HeartsEngine engine;
    if (!engine.load(in))
        return false;

    std::vector<Card> selected;
    qint8 awaitingCollect = 0;
    qint8 announced = 0;
    if (!cardcodec::readPile(in, selected))
        return false;
    in >> awaitingCollect >> announced;
    if (in.status() != QDataStream::Ok)
        return false;
    if (awaitingCollect != 0 && awaitingCollect != 1)
        return false;
    if (announced != 0 && announced != 1)
        return false;

    // A lift is at most three cards and only ever cards you are holding --
    // the same rule confirmPass() plays by, checked here because the blob has
    // not been through it.
    if (selected.size() > 3)
        return false;
    for (const Card& c : selected) {
        const std::vector<Card>& held = engine.hand(0);
        if (std::find(held.begin(), held.end(), c) == held.end())
            return false;
    }
    // A trick can only be waiting to be collected if there is a full one there.
    if (awaitingCollect == 1 && !engine.trickComplete())
        return false;

    m_engine = engine;
    m_selected = selected;
    m_awaitingCollect = awaitingCollect == 1;
    m_announced = announced == 1;
    // The clock is deliberately NOT started here. The hub calls activate()
    // straight after this, and that already works out whether the computers
    // owe a move -- starting it in both places runs the timer twice.
    update();
    refresh();
    return true;
}
