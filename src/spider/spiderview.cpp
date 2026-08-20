#include "spiderview.h"

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
#include <cmath>

namespace {
constexpr double kMargin = 12.0;
constexpr double kDragThreshold = 4.0;
}

SpiderView::SpiderView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(minimumSizeHint());
    buildActions();
    newGame();
}

void SpiderView::buildActions()
{
    auto* newAction = new QAction(QStringLiteral("New Deal"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &SpiderView::newGame);
    m_actions.append(newAction);

    m_undoAction = new QAction(QStringLiteral("Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &SpiderView::undo);
    m_actions.append(m_undoAction);

    auto* deal = new QAction(QStringLiteral("Deal Row"), this);
    connect(deal, &QAction::triggered, this, [this] { dealRow(); });
    m_actions.append(deal);

    auto* sep = new QAction(this);
    sep->setSeparator(true);
    m_actions.append(sep);

    auto* group = new QActionGroup(this);
    group->setExclusive(true);
    const struct { const char* name; int suits; } kModes[] = {
        { "1 Suit", 1 }, { "2 Suits", 2 }, { "4 Suits", 4 },
    };
    for (const auto& mode : kModes) {
        auto* a = new QAction(QString::fromUtf8(mode.name), this);
        a->setCheckable(true);
        a->setChecked(mode.suits == m_suits);
        group->addAction(a);
        const int suits = mode.suits;
        connect(a, &QAction::triggered, this, [this, suits] {
            m_suits = suits;
            newGame();
        });
        m_actions.append(a);
    }
}

void SpiderView::newGame()
{
    // Spider uses two full decks; the suit count only limits which suits appear.
    std::vector<Card> deck = makeDeck(2, m_suits);
    shuffleCards(deck, m_rng);
    Sound::instance().play(Sound::kShuffle);

    for (auto& c : m_columns)
        c.clear();
    m_stock.clear();
    m_drag.clear();
    m_history.clear();
    m_dragging = false;
    m_pressValid = false;
    m_completed = 0;
    m_moves = 0;
    m_won = false;
    m_undoAction->setEnabled(false);

    // The classic deal: 54 cards out, the first four columns getting six.
    std::size_t next = 0;
    for (int col = 0; col < kColumns; ++col) {
        const int count = (col < 4) ? 6 : 5;
        for (int i = 0; i < count; ++i) {
            Card c = deck[next++];
            c.faceUp = (i == count - 1);
            m_columns[std::size_t(col)].push_back(c);
        }
    }
    m_stock.assign(deck.begin() + next, deck.end());
    for (Card& c : m_stock)
        c.faceUp = false;

    update();
    refresh();
}

void SpiderView::activate()
{
    refresh();
}

void SpiderView::pushUndo()
{
    m_history.push_back({ m_columns, m_stock, m_completed, m_moves });
    if (m_history.size() > 200)
        m_history.erase(m_history.begin());
    m_undoAction->setEnabled(true);
}

void SpiderView::undo()
{
    if (m_history.empty())
        return;
    const Snapshot s = m_history.back();
    m_history.pop_back();
    m_columns = s.columns;
    m_stock = s.stock;
    m_completed = s.completed;
    m_moves = s.moves;
    m_won = false;
    m_undoAction->setEnabled(!m_history.empty());
    update();
    refresh();
}

// The table, not the moves that made it — see KlondikeView::saveState for why
// the card games save differently from Chess.
QByteArray SpiderView::saveState() const
{
    if (m_won || m_history.empty())
        return {};

    // A run lifted in mid-drag belongs to the column it came from until it is
    // dropped; closing the window while holding it must not lose the cards.
    const auto column = [this](int index) {
        std::vector<Card> cards = m_columns[std::size_t(index)];
        if (m_dragging && m_dragFrom == index)
            cards.insert(cards.end(), m_drag.begin(), m_drag.end());
        return cards;
    };

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1) << qint32(m_suits) << qint32(m_completed) << qint32(m_moves);
    for (int col = 0; col < kColumns; ++col)
        cardcodec::writePile(out, column(col));
    cardcodec::writePile(out, m_stock);
    return blob;
}

bool SpiderView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    qint32 suits = 0;
    qint32 completed = 0;
    qint32 moves = 0;
    in >> version >> suits >> completed >> moves;
    if (version != 1 || in.status() != QDataStream::Ok
        || (suits != 1 && suits != 2 && suits != 4) || completed < 0 || completed > 8 || moves < 0)
        return false;

    std::array<std::vector<Card>, kColumns> columns;
    std::vector<Card> stock;
    if (!cardcodec::readPiles(in, columns) || !cardcodec::readPile(in, stock))
        return false;

    // Spider takes a finished run off the table for good, so it cannot ask for
    // the whole pack back: what is left is two packs less thirteen cards for
    // every run already completed, and all of it from the pack it was dealt.
    std::vector<Card> all;
    cardcodec::gather(all, columns);
    cardcodec::gather(all, stock);
    if (all.size() != std::size_t(104 - 13 * completed) || !cardcodec::fitsPack(all, 2, int(suits)))
        return false;

    m_columns = columns;
    m_stock = stock;
    m_suits = int(suits);
    m_completed = int(completed);
    m_moves = int(moves);
    m_history.clear();
    m_drag.clear();
    m_dragging = false;
    m_pressValid = false;
    m_won = false;
    m_undoAction->setEnabled(false);
    const QString mode = QStringLiteral("%1 Suit%2")
                             .arg(m_suits)
                             .arg(m_suits == 1 ? QString() : QStringLiteral("s"));
    for (QAction* a : m_actions) {
        if (a->isCheckable())
            a->setChecked(a->text() == mode);
    }
    update();
    refresh();
    return true;
}

double SpiderView::cardWidth() const
{
    const double byWidth = (width() - 2 * kMargin - (kColumns - 1) * 6.0) / kColumns;
    // The caption's strip comes off the height before the card is sized: the
    // piles are anchored to the top, so a smaller card is what keeps the tail
    // of a long column clear of the sentence under it.
    const double byHeight =
        (height() - 2 * kMargin - captionBand(QRectF(rect()))) / (1.4 * 2.2);
    return std::max(30.0, std::min(byWidth, byHeight));
}

double SpiderView::fanStep(const std::vector<Card>& column, int index) const
{
    return column[std::size_t(index)].faceUp ? cardHeight() * 0.26 : cardHeight() * 0.11;
}

QRectF SpiderView::columnOrigin(int column) const
{
    const double w = cardWidth();
    const double step = w + 6.0;
    return { kMargin + step * column, kMargin, w, cardHeight() };
}

QRectF SpiderView::cardRect(int column, int index) const
{
    QRectF r = columnOrigin(column);
    const std::vector<Card>& col = m_columns[std::size_t(column)];
    double y = r.top();
    for (int i = 0; i < index && i < int(col.size()); ++i)
        y += fanStep(col, i);
    r.moveTop(y);
    return r;
}

QRectF SpiderView::stockRect() const
{
    const double w = cardWidth();
    return { width() - kMargin - w, height() - kMargin - cardHeight(), w, cardHeight() };
}

int SpiderView::movableRunLength(int column) const
{
    const std::vector<Card>& col = m_columns[std::size_t(column)];
    if (col.empty())
        return 0;

    int run = 1;
    for (int i = int(col.size()) - 1; i > 0; --i) {
        const Card& lower = col[std::size_t(i)];
        const Card& upper = col[std::size_t(i - 1)];
        if (!upper.faceUp || upper.suit != lower.suit || upper.rank != lower.rank + 1)
            break;
        ++run;
    }
    return run;
}

bool SpiderView::canDrop(const Card& moving, int column) const
{
    const std::vector<Card>& col = m_columns[std::size_t(column)];
    if (col.empty())
        return true; // any card may start an empty column
    const Card& top = col.back();
    // Spider stacks by rank alone; only *moving* a run needs matching suits.
    return top.faceUp && top.rank == moving.rank + 1;
}

void SpiderView::dealRow()
{
    if (m_stock.empty())
        return;

    // The rule that stops a deal burying an empty column beyond recovery.
    for (const auto& col : m_columns) {
        if (col.empty()) {
            Q_EMIT statusChanged(QStringLiteral("Fill every empty column before dealing a new row."));
            return;
        }
    }

    pushUndo();
    Sound::instance().play(Sound::kCardDeal);
    for (int col = 0; col < kColumns && !m_stock.empty(); ++col) {
        Card c = m_stock.back();
        m_stock.pop_back();
        c.faceUp = true;
        m_columns[std::size_t(col)].push_back(c);
    }
    for (int col = 0; col < kColumns; ++col)
        harvest(col);

    update();
    refresh();
    checkWin();
}

void SpiderView::harvest(int column)
{
    std::vector<Card>& col = m_columns[std::size_t(column)];
    if (int(col.size()) < 13)
        return;

    // A complete run is King down to Ace, one suit, all face up.
    const int start = int(col.size()) - 13;
    for (int i = 0; i < 13; ++i) {
        const Card& c = col[std::size_t(start + i)];
        if (!c.faceUp || c.rank != kKing - i || c.suit != col[std::size_t(start)].suit)
            return;
    }

    col.erase(col.begin() + start, col.end());
    ++m_completed;
    if (!col.empty() && !col.back().faceUp)
        col.back().faceUp = true;
}

void SpiderView::checkWin()
{
    if (m_completed < 8 || m_won)
        return;
    m_won = true;
    Sound::instance().play(Sound::kWin);
    const bool newBest = Scores::instance().recordLow(Scores::spiderBestMoves(m_suits), m_moves);
    refresh();

    QTimer::singleShot(200, this, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("Solved"));
        box.setText(QStringLiteral("All eight runs complete!"));
        box.setInformativeText(
            newBest ? QStringLiteral("Moves: %1 — a new best!").arg(m_moves)
                    : QStringLiteral("Moves: %1.   Best: %2.")
                          .arg(m_moves)
                          .arg(Scores::instance().best(Scores::spiderBestMoves(m_suits))));
        QAbstractButton* again = box.addButton(QStringLiteral("New Deal"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Close"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == again)
            newGame();
    });
}

void SpiderView::refresh()
{
    Q_EMIT statusChanged(QStringLiteral("%1   Runs %2/8   Stock %3   Moves %4")
                             .arg(m_won ? QStringLiteral("Solved!")
                                        : QStringLiteral("Spider (%1 suit%2)")
                                              .arg(m_suits)
                                              .arg(m_suits == 1 ? QString() : QStringLiteral("s")))
                             .arg(m_completed)
                             .arg(m_stock.size() / kColumns)
                             .arg(m_moves));
}

void SpiderView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    Theme::paintFelt(p, rect(), Theme::kFeltTealTop, Theme::kFeltTealBottom);

    for (int col = 0; col < kColumns; ++col) {
        const std::vector<Card>& column = m_columns[std::size_t(col)];
        if (column.empty()) {
            CardArt::paintSlot(p, columnOrigin(col));
            continue;
        }
        const int movable = movableRunLength(col);
        for (int i = 0; i < int(column.size()); ++i) {
            const QRectF r = cardRect(col, i);
            if (column[std::size_t(i)].faceUp) {
                CardArt::paintFace(p, r, column[std::size_t(i)]);
                // Mark where the liftable run starts, so the player can see
                // what will come away in one piece.
                if (i == int(column.size()) - movable && movable > 1)
                    CardArt::paintHighlight(p, r, QColor(0xff, 0xd5, 0x4f, 150));
            } else {
                CardArt::paintBack(p, r);
            }
        }
    }

    // Stock, drawn as a small stack in the corner.
    if (!m_stock.empty()) {
        const QRectF s = stockRect();
        const int stacks = int(m_stock.size() / kColumns);
        for (int i = 0; i < std::min(stacks, 5); ++i)
            CardArt::paintBack(p, s.translated(-i * 5.0, -i * 2.0));
    }

    if (m_dragging && !m_drag.empty()) {
        const double w = cardWidth();
        const double h = cardHeight();
        for (int i = 0; i < int(m_drag.size()); ++i) {
            const QRectF r(m_dragPos.x() - m_dragGrab.x(),
                           m_dragPos.y() - m_dragGrab.y() + i * h * 0.26, w, h);
            p.save();
            p.setOpacity(0.96);
            CardArt::paintFace(p, r, m_drag[std::size_t(i)]);
            p.restore();
        }
    }

    paintStatusCaption(p, QRectF(rect()));
}

void SpiderView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    m_pressPos = event->position();
    m_pressValid = false;

    if (stockRect().contains(event->position())) {
        dealRow();
        return;
    }

    for (int col = 0; col < kColumns; ++col) {
        const std::vector<Card>& column = m_columns[std::size_t(col)];
        const int movable = movableRunLength(col);
        const int firstMovable = int(column.size()) - movable;

        for (int i = int(column.size()) - 1; i >= 0; --i) {
            if (!cardRect(col, i).contains(event->position()))
                continue;
            if (!column[std::size_t(i)].faceUp || i < firstMovable)
                return; // grabbed a card that cannot move as a unit
            m_dragFrom = col;
            m_dragIndex = i;
            m_pressValid = true;
            m_dragGrab = event->position() - cardRect(col, i).topLeft();
            return;
        }
    }
}

void SpiderView::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_pressValid)
        return;

    if (!m_dragging) {
        const QPointF delta = event->position() - m_pressPos;
        if (std::hypot(delta.x(), delta.y()) < kDragThreshold)
            return;
        std::vector<Card>& column = m_columns[std::size_t(m_dragFrom)];
        m_drag.assign(column.begin() + m_dragIndex, column.end());
        column.erase(column.begin() + m_dragIndex, column.end());
        m_dragging = true;
    }

    m_dragPos = event->position();
    update();
}

void SpiderView::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_dragging) {
        m_pressValid = false;
        return;
    }

    m_dragging = false;
    const QPointF drop = event->position();
    int target = -1;

    for (int col = 0; col < kColumns; ++col) {
        QRectF zone = columnOrigin(col);
        const std::vector<Card>& column = m_columns[std::size_t(col)];
        if (!column.empty())
            zone = zone.united(cardRect(col, int(column.size()) - 1));
        zone.setBottom(zone.bottom() + cardHeight() * 0.5);

        if (zone.contains(drop) && canDrop(m_drag.front(), col)) {
            target = col;
            break;
        }
    }

    if (target >= 0 && target != m_dragFrom) {
        pushUndo();
        // The snapshot must show the run still on its old column, so restore
        // it before recording and then perform the move properly.
        Snapshot& snap = m_history.back();
        for (const Card& c : m_drag)
            snap.columns[std::size_t(m_dragFrom)].push_back(c);

        for (const Card& c : m_drag)
            m_columns[std::size_t(target)].push_back(c);

        std::vector<Card>& source = m_columns[std::size_t(m_dragFrom)];
        if (!source.empty() && !source.back().faceUp)
            source.back().faceUp = true;

        ++m_moves;
        Sound::instance().play(Sound::kCardPlace);
        const int before = m_completed;
        harvest(target);
        if (m_completed != before)
            Sound::instance().play(Sound::kWin);
    } else {
        std::vector<Card>& source = m_columns[std::size_t(m_dragFrom)];
        for (const Card& c : m_drag)
            source.push_back(c);
    }

    m_drag.clear();
    m_pressValid = false;
    update();
    refresh();
    checkWin();
}
