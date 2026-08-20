#include "klondikeview.h"

#include "legibility.h"
#include "scores.h"
#include "sound.h"
#include "cards/cardart.h"
#include "cards/cardcodec.h"
#include "theme.h"

#include <QActionGroup>
#include <QDataStream>
#include <QIODevice>
#include <QMessageBox>
#include <QPushButton>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#include <algorithm>

namespace {
constexpr double kMargin = 14.0;

// A drag only starts once the pointer has actually moved, so a click that
// happens to wobble still counts as a click.
constexpr double kDragThreshold = 4.0;
}

KlondikeView::KlondikeView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(minimumSizeHint());
    setMouseTracking(true);
    buildActions();
    newGame();
}

void KlondikeView::buildActions()
{
    auto* newAction = new QAction(QStringLiteral("New Deal"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &KlondikeView::newGame);
    m_actions.append(newAction);

    m_undoAction = new QAction(QStringLiteral("Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &KlondikeView::undo);
    m_actions.append(m_undoAction);

    auto* sep = new QAction(this);
    sep->setSeparator(true);
    m_actions.append(sep);

    auto* group = new QActionGroup(this);
    group->setExclusive(true);
    for (int n : { 1, 3 }) {
        auto* a = new QAction(QStringLiteral("Draw %1").arg(n), this);
        a->setCheckable(true);
        a->setChecked(n == m_drawCount);
        group->addAction(a);
        connect(a, &QAction::triggered, this, [this, n] {
            m_drawCount = n;
            newGame();
        });
        m_actions.append(a);
    }
}

void KlondikeView::newGame()
{
    std::vector<Card> deck = makeDeck(1, 4);
    shuffleCards(deck, m_rng);
    Sound::instance().play(Sound::kShuffle);

    for (auto& f : m_foundations)
        f.clear();
    for (auto& t : m_tableau)
        t.clear();
    m_waste.clear();
    m_stock.clear();
    m_drag.clear();
    m_history.clear();
    m_dragging = false;
    m_won = false;
    m_score = 0;
    m_undoAction->setEnabled(false);

    // Columns get 1..7 cards, only the last of each face up.
    std::size_t next = 0;
    for (int col = 0; col < 7; ++col) {
        for (int i = 0; i <= col; ++i) {
            Card c = deck[next++];
            c.faceUp = (i == col);
            m_tableau[col].push_back(c);
        }
    }
    m_stock.assign(deck.begin() + next, deck.end());
    for (Card& c : m_stock)
        c.faceUp = false;

    update();
    refresh();
}

void KlondikeView::activate()
{
    refresh();
}

void KlondikeView::pushUndo()
{
    m_history.push_back({ m_stock, m_waste, m_foundations, m_tableau, m_score });
    if (m_history.size() > 200)
        m_history.erase(m_history.begin());
    m_undoAction->setEnabled(true);
}

void KlondikeView::undo()
{
    if (m_history.empty())
        return;
    const Snapshot s = m_history.back();
    m_history.pop_back();
    m_stock = s.stock;
    m_waste = s.waste;
    m_foundations = s.foundations;
    m_tableau = s.tableau;
    m_score = s.score;
    m_won = false;
    m_undoAction->setEnabled(!m_history.empty());
    update();
    refresh();
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

// The table, not the moves that made it.
//
// Chess saves its move list because replaying it rebuilds the undo stack and the
// repetition keys as a side effect. A solitaire keeps no move log, so the piles
// themselves are what gets written, and cardcodec's pack check stands in for
// Chess's legal-move check on the way back in. The undo history is deliberately
// not saved: a resumed deal starts a fresh one.
QByteArray KlondikeView::saveState() const
{
    // Nothing worth coming back to: a deal already solved, or one nobody has
    // touched. An empty state also clears whatever was stored before.
    if (m_won || m_history.empty())
        return {};

    // A run lifted in mid-drag has been erased from its pile and is held until
    // it is dropped. Closing the window at that moment must not lose those
    // cards, so they go back onto the pile they came from.
    const auto pile = [this](PileKind kind, int index) {
        std::vector<Card> cards = pileFor(kind, index);
        if (m_dragging && m_dragFrom.kind == kind && m_dragFrom.pile == index)
            cards.insert(cards.end(), m_drag.begin(), m_drag.end());
        return cards;
    };

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1) << qint32(m_drawCount) << qint32(m_score);
    cardcodec::writePile(out, pile(PileKind::Stock, 0));
    cardcodec::writePile(out, pile(PileKind::Waste, 0));
    for (int f = 0; f < 4; ++f)
        cardcodec::writePile(out, pile(PileKind::Foundation, f));
    for (int col = 0; col < 7; ++col)
        cardcodec::writePile(out, pile(PileKind::Tableau, col));
    return blob;
}

bool KlondikeView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    qint32 draw = 0;
    qint32 score = 0;
    in >> version >> draw >> score;
    if (version != 1 || in.status() != QDataStream::Ok || (draw != 1 && draw != 3) || score < 0)
        return false;

    // Read into a table of its own, so a blob that turns out to be nonsense
    // leaves the deal already on screen alone.
    std::vector<Card> stock;
    std::vector<Card> waste;
    std::array<std::vector<Card>, 4> foundations;
    std::array<std::vector<Card>, 7> tableau;
    if (!cardcodec::readPile(in, stock) || !cardcodec::readPile(in, waste)
        || !cardcodec::readPiles(in, foundations) || !cardcodec::readPiles(in, tableau))
        return false;

    // Klondike never takes a card out of play, so the whole pack must be here —
    // nothing missing, nothing doubled.
    std::vector<Card> all;
    cardcodec::gather(all, stock);
    cardcodec::gather(all, waste);
    cardcodec::gather(all, foundations);
    cardcodec::gather(all, tableau);
    if (!cardcodec::matchesPack(all, 1, 4))
        return false;

    m_stock = stock;
    m_waste = waste;
    m_foundations = foundations;
    m_tableau = tableau;
    m_drawCount = int(draw);
    m_score = int(score);
    m_history.clear();
    m_drag.clear();
    m_dragging = false;
    m_pressValid = false;
    m_won = false;
    m_undoAction->setEnabled(false);
    for (QAction* a : m_actions) {
        if (a->isCheckable())
            a->setChecked(a->text() == QStringLiteral("Draw %1").arg(m_drawCount));
    }
    update();
    refresh();
    return true;
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

double KlondikeView::cardWidth() const
{
    // Seven columns plus the six gaps between them must fit the width. The gap
    // is itself a fraction of the card width, so the whole row costs
    // 7w + 6*(0.14w) = 7.84w — solving for that is what keeps the last column
    // and the fourth foundation on screen.
    constexpr double kRowCost = 7.0 + 6.0 * 0.14;
    const double byWidth = (width() - 2 * kMargin) / kRowCost;
    // The caption's strip comes off the height before the card is sized: the
    // piles are anchored to the top, so a smaller card is what keeps the tail
    // of a long column clear of the sentence under it.
    const double byHeight =
        (height() - 2 * kMargin - captionBand(QRectF(rect()))) / (1.4 * 2.6);
    return std::max(34.0, std::min(byWidth, byHeight));
}

QRectF KlondikeView::pileOrigin(PileKind kind, int pile) const
{
    const double w = cardWidth();
    const double h = cardHeight();
    const double step = w + gap();

    switch (kind) {
    case PileKind::Stock:
        return { kMargin, kMargin, w, h };
    case PileKind::Waste:
        return { kMargin + step, kMargin, w, h };
    case PileKind::Foundation:
        return { kMargin + step * (3 + pile), kMargin, w, h };
    case PileKind::Tableau:
        return { kMargin + step * pile, kMargin + h + gap() * 1.6, w, h };
    }
    return {};
}

// Face-down cards in a column are packed tighter than face-up ones, which is
// what lets a long column still fit on screen.
double KlondikeView::fanStep(const std::vector<Card>& pile, int index) const
{
    return pile[std::size_t(index)].faceUp ? cardHeight() * 0.28 : cardHeight() * 0.13;
}

QRectF KlondikeView::cardRect(PileKind kind, int pile, int index) const
{
    QRectF r = pileOrigin(kind, pile);
    if (kind != PileKind::Tableau)
        return r;

    const std::vector<Card>& column = pileFor(kind, pile);
    double y = r.top();
    for (int i = 0; i < index && i < int(column.size()); ++i)
        y += fanStep(column, i);
    r.moveTop(y);
    return r;
}

std::vector<Card>& KlondikeView::pileFor(PileKind kind, int pile)
{
    switch (kind) {
    case PileKind::Stock: return m_stock;
    case PileKind::Waste: return m_waste;
    case PileKind::Foundation: return m_foundations[std::size_t(pile)];
    case PileKind::Tableau: return m_tableau[std::size_t(pile)];
    }
    return m_stock;
}

const std::vector<Card>& KlondikeView::pileFor(PileKind kind, int pile) const
{
    return const_cast<KlondikeView*>(this)->pileFor(kind, pile);
}

KlondikeView::Spot KlondikeView::hitTest(QPointF pos) const
{
    // Tableau first and from the bottom of each column up, so the card drawn
    // on top is the one that gets picked.
    for (int col = 0; col < 7; ++col) {
        const std::vector<Card>& column = m_tableau[std::size_t(col)];
        for (int i = int(column.size()) - 1; i >= 0; --i) {
            if (cardRect(PileKind::Tableau, col, i).contains(pos))
                return { PileKind::Tableau, col, i, true };
        }
        if (column.empty() && pileOrigin(PileKind::Tableau, col).contains(pos))
            return { PileKind::Tableau, col, -1, true };
    }

    for (int f = 0; f < 4; ++f) {
        if (pileOrigin(PileKind::Foundation, f).contains(pos))
            return { PileKind::Foundation, f, int(m_foundations[std::size_t(f)].size()) - 1, true };
    }

    if (pileOrigin(PileKind::Waste, 0).contains(pos))
        return { PileKind::Waste, 0, int(m_waste.size()) - 1, true };

    if (pileOrigin(PileKind::Stock, 0).contains(pos))
        return { PileKind::Stock, 0, int(m_stock.size()) - 1, true };

    return {};
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------

bool KlondikeView::canStackOnTableau(const Card& moving, int column) const
{
    const std::vector<Card>& target = m_tableau[std::size_t(column)];
    if (target.empty())
        return moving.rank == kKing;
    const Card& top = target.back();
    if (!top.faceUp)
        return false;
    return isRed(top) != isRed(moving) && top.rank == moving.rank + 1;
}

bool KlondikeView::canPlaceOnFoundation(const Card& moving, int foundation) const
{
    const std::vector<Card>& target = m_foundations[std::size_t(foundation)];
    if (target.empty())
        return moving.rank == kAce;
    return target.back().suit == moving.suit && target.back().rank + 1 == moving.rank;
}

void KlondikeView::dealFromStock()
{
    pushUndo();
    if (m_stock.empty()) {
        // Recycle: the waste goes back under the stock, face down, in order.
        std::reverse(m_waste.begin(), m_waste.end());
        for (Card& c : m_waste)
            c.faceUp = false;
        m_stock = m_waste;
        m_waste.clear();
    } else {
        for (int i = 0; i < m_drawCount && !m_stock.empty(); ++i) {
            Card c = m_stock.back();
            m_stock.pop_back();
            c.faceUp = true;
            m_waste.push_back(c);
        }
    }
    Sound::instance().play(Sound::kCardDeal);
    update();
    refresh();
}

bool KlondikeView::sendToFoundation(PileKind from, int pile)
{
    std::vector<Card>& source = pileFor(from, pile);
    if (source.empty() || !source.back().faceUp)
        return false;

    for (int f = 0; f < 4; ++f) {
        if (!canPlaceOnFoundation(source.back(), f))
            continue;
        pushUndo();
        m_foundations[std::size_t(f)].push_back(source.back());
        source.pop_back();
        m_score += 10;
        if (from == PileKind::Tableau && !source.empty() && !source.back().faceUp) {
            source.back().faceUp = true;
            m_score += 5;
        }
        return true;
    }
    return false;
}

bool KlondikeView::autoFinishStep()
{
    for (int col = 0; col < 7; ++col)
        if (sendToFoundation(PileKind::Tableau, col))
            return true;
    return sendToFoundation(PileKind::Waste, 0);
}

void KlondikeView::checkWin()
{
    int total = 0;
    for (const auto& f : m_foundations)
        total += int(f.size());
    if (total != 52 || m_won)
        return;

    m_won = true;
    Sound::instance().play(Sound::kWin);
    const bool newBest = Scores::instance().recordHigh(Scores::klondikeBestScore(), m_score);
    refresh();
    QTimer::singleShot(200, this, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("Solved"));
        box.setText(QStringLiteral("You cleared the table!"));
        box.setInformativeText(
            newBest ? QStringLiteral("Score: %1 — a new best!").arg(m_score)
                    : QStringLiteral("Score: %1.   Best: %2.")
                          .arg(m_score)
                          .arg(Scores::instance().best(Scores::klondikeBestScore())));
        QAbstractButton* again = box.addButton(QStringLiteral("New Deal"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Close"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == again)
            newGame();
    });
}

void KlondikeView::refresh()
{
    int done = 0;
    for (const auto& f : m_foundations)
        done += int(f.size());

    QString line = QStringLiteral("%1   Foundations %2/52   Stock %3   Score %4")
                       .arg(m_won ? QStringLiteral("Solved!") : QStringLiteral("Klondike"))
                       .arg(done)
                       .arg(m_stock.size())
                       .arg(m_score);
    if (Scores::instance().has(Scores::klondikeBestScore()))
        line += QStringLiteral("   Best %1").arg(Scores::instance().best(Scores::klondikeBestScore()));
    Q_EMIT statusChanged(line);
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void KlondikeView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    Theme::paintFelt(p, rect(), Theme::kFeltGreenTop, Theme::kFeltGreenBottom);

    // Stock: a back if there are cards, otherwise a recycle marker.
    if (m_stock.empty())
        CardArt::paintSlot(p, pileOrigin(PileKind::Stock, 0), QStringLiteral("↻"));
    else
        CardArt::paintBack(p, pileOrigin(PileKind::Stock, 0));

    if (m_waste.empty())
        CardArt::paintSlot(p, pileOrigin(PileKind::Waste, 0));
    else
        CardArt::paintFace(p, pileOrigin(PileKind::Waste, 0), m_waste.back());

    for (int f = 0; f < 4; ++f) {
        const QRectF r = pileOrigin(PileKind::Foundation, f);
        if (m_foundations[std::size_t(f)].empty())
            CardArt::paintSlot(p, r, QStringLiteral("A"));
        else
            CardArt::paintFace(p, r, m_foundations[std::size_t(f)].back());
    }

    for (int col = 0; col < 7; ++col) {
        const std::vector<Card>& column = m_tableau[std::size_t(col)];
        if (column.empty()) {
            CardArt::paintSlot(p, pileOrigin(PileKind::Tableau, col), QStringLiteral("K"));
            continue;
        }
        for (int i = 0; i < int(column.size()); ++i) {
            const QRectF r = cardRect(PileKind::Tableau, col, i);
            if (column[std::size_t(i)].faceUp)
                CardArt::paintFace(p, r, column[std::size_t(i)]);
            else
                CardArt::paintBack(p, r);
        }
    }

    // The dragged stack rides above everything else.
    if (m_dragging && !m_drag.empty()) {
        const double w = cardWidth();
        const double h = cardHeight();
        for (int i = 0; i < int(m_drag.size()); ++i) {
            const QRectF r(m_dragPos.x() - m_dragGrab.x(),
                           m_dragPos.y() - m_dragGrab.y() + i * h * 0.28, w, h);
            p.save();
            p.setOpacity(0.96);
            CardArt::paintFace(p, r, m_drag[std::size_t(i)]);
            p.restore();
        }
    }

    paintStatusCaption(p, QRectF(rect()));
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------

void KlondikeView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    m_pressPos = event->position();
    m_pressValid = false;

    const Spot s = hitTest(event->position());
    if (!s.valid)
        return;

    if (s.kind == PileKind::Stock) {
        dealFromStock();
        return;
    }

    if (s.index < 0)
        return;

    const std::vector<Card>& pile = pileFor(s.kind, s.pile);
    const Card& card = pile[std::size_t(s.index)];
    if (!card.faceUp)
        return;

    // Only the top card can leave the waste or a foundation; a tableau column
    // gives up its whole face-up run from the grabbed card down.
    if (s.kind != PileKind::Tableau && s.index != int(pile.size()) - 1)
        return;

    m_dragFrom = s;
    m_pressValid = true;
    m_dragGrab = event->position() - cardRect(s.kind, s.pile, s.index).topLeft();
}

void KlondikeView::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_pressValid)
        return;

    if (!m_dragging) {
        const QPointF delta = event->position() - m_pressPos;
        if (std::hypot(delta.x(), delta.y()) < kDragThreshold)
            return;

        // Lift the run off its pile now that this is definitely a drag.
        std::vector<Card>& pile = pileFor(m_dragFrom.kind, m_dragFrom.pile);
        m_drag.assign(pile.begin() + m_dragFrom.index, pile.end());
        pile.erase(pile.begin() + m_dragFrom.index, pile.end());
        m_dragging = true;
    }

    m_dragPos = event->position();
    update();
}

void KlondikeView::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_dragging) {
        m_pressValid = false;
        return;
    }

    m_dragging = false;
    const QPointF drop = event->position();
    bool placed = false;

    // A single card may go to a foundation; any run may go to a tableau.
    if (m_drag.size() == 1) {
        for (int f = 0; f < 4 && !placed; ++f) {
            if (pileOrigin(PileKind::Foundation, f).contains(drop)
                && canPlaceOnFoundation(m_drag.front(), f)) {
                pushUndo();
                m_foundations[std::size_t(f)].push_back(m_drag.front());
                m_score += 10;
                placed = true;
            }
        }
    }

    for (int col = 0; col < 7 && !placed; ++col) {
        // Accept a drop anywhere in the column's vertical run, not just on the
        // top card, which is far more forgiving to aim at.
        QRectF zone = pileOrigin(PileKind::Tableau, col);
        const std::vector<Card>& column = m_tableau[std::size_t(col)];
        if (!column.empty())
            zone = zone.united(cardRect(PileKind::Tableau, col, int(column.size()) - 1));
        zone.setBottom(zone.bottom() + cardHeight() * 0.5);

        if (zone.contains(drop) && canStackOnTableau(m_drag.front(), col)) {
            pushUndo();
            for (const Card& c : m_drag)
                m_tableau[std::size_t(col)].push_back(c);
            m_score += 5;
            placed = true;
        }
    }

    if (placed) {
        Sound::instance().play(Sound::kCardPlace);
        // Turn over whatever the run was covering.
        std::vector<Card>& source = pileFor(m_dragFrom.kind, m_dragFrom.pile);
        if (m_dragFrom.kind == PileKind::Tableau && !source.empty() && !source.back().faceUp) {
            source.back().faceUp = true;
            m_score += 5;
        }
    } else {
        // Put it back exactly where it came from.
        std::vector<Card>& source = pileFor(m_dragFrom.kind, m_dragFrom.pile);
        for (const Card& c : m_drag)
            source.push_back(c);
    }

    m_drag.clear();
    m_pressValid = false;
    update();
    refresh();
    checkWin();
}

void KlondikeView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    const Spot s = hitTest(event->position());
    if (!s.valid || s.kind == PileKind::Stock || s.index < 0)
        return;

    if (sendToFoundation(s.kind, s.pile)) {
        update();
        refresh();
        checkWin();
    }
}
