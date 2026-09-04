#include "spiderview.h"
#include "spider/spidertable.h"

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
    setMinimumSize(SpiderView::minimumSizeHint());
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
        a->setChecked(mode.suits == m_table.suits());
        group->addAction(a);
        const int suits = mode.suits;
        connect(a, &QAction::triggered, this, [this, suits] {
            // The deal takes the suit count, so changing it and dealing is one
            // step rather than two.
            m_table.deal(suits);
            Sound::instance().play(Sound::kShuffle);
            m_drag.clear();
            m_dragging = false;
            m_pressValid = false;
            m_won = false;
            m_undoAction->setEnabled(false);
            update();
            refresh();
        });
        m_actions.append(a);
    }
}

void SpiderView::newGame()
{
    // Before the deal, not after: settling puts a held run back on the table
    // it came from, and after a deal that is a fresh table it never left.
    settleForChange();
    m_resumed = false;
    m_table.deal(m_table.suits());
    Sound::instance().play(Sound::kShuffle);
    m_drag.clear();
    m_dragging = false;
    m_pressValid = false;
    m_won = false;
    m_undoAction->setEnabled(false);

    update();
    refresh();
}

void SpiderView::activate()
{
    refresh();
}

void SpiderView::undo()
{
    if (!m_table.canUndo())
        return;
    settleForChange();
    m_table.undo();
    m_won = false;
    m_undoAction->setEnabled(m_table.canUndo());
    update();
    refresh();
}

// The table, not the moves that made it — see KlondikeView::saveState for why
// the card games save differently from Chess.
QByteArray SpiderView::saveState() const
{
    if (m_won || (!m_table.canUndo() && !m_resumed))
        return {};

    // A run lifted in mid-drag belongs to the column it came from until it is
    // dropped; closing the window while holding it must not lose the cards.
    const auto column = [this](int index) {
        std::vector<Card> cards = m_table.columns()[std::size_t(index)];
        if (m_dragging && m_dragFrom == index)
            cards.insert(cards.end(), m_drag.begin(), m_drag.end());
        return cards;
    };

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1) << qint32(m_table.suits()) << qint32(m_table.completed())
        << qint32(m_table.moves());
    for (int col = 0; col < kColumns; ++col)
        cardcodec::writePile(out, column(col));
    cardcodec::writePile(out, m_table.stock());
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

    // The table decides whether this is a position the rules could have
    // produced -- Spider takes a finished run off for good, so what must come
    // back is two packs less thirteen for every run completed.
    if (!m_table.restore(columns, stock, int(suits), int(completed), int(moves)))
        return false;

    m_drag.clear();
    m_dragging = false;
    m_pressValid = false;
    m_won = false;
    m_undoAction->setEnabled(false);
    const QString mode = QStringLiteral("%1 Suit%2")
                             .arg(m_table.suits())
                             .arg(m_table.suits() == 1 ? QString() : QStringLiteral("s"));
    for (QAction* a : m_actions) {
        if (a->isCheckable())
            a->setChecked(a->text() == mode);
    }
    m_resumed = true;
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
    // 2.85 card heights is what a column actually reaches, not a guess: the
    // deal leaves five face-down cards under one face-up, and the five dealt
    // rows add a face-up card each. 5 x 0.11 + 5 x 0.26 + 1 = 2.85. The budget
    // read 2.2, so a column that had taken every row ran a third of the
    // surface past the bottom, under the caption (GHUB-0160).
    const double byHeight =
        (height() - 2 * kMargin - captionBand(QRectF(rect()))) / (1.4 * 2.85);
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

// The height budget covers a fully DEALT table, which is what
// aFullSpiderTableStaysOnTheSurface asserts. Moving runs between columns grows
// one past any dealt length, so an overlong column tightens rather than every
// card shrinking -- the same trade Klondike and FreeCell make, and for the same
// reason: this game is read by pip pattern (GHUB-0089).
double SpiderView::fanScale(const std::vector<Card>& column) const
{
    if (column.size() < 2)
        return 1.0;
    double natural = 0.0;
    for (int i = 0; i < int(column.size()) - 1; ++i)
        natural += fanStep(column, i);
    if (natural <= 0.0)
        return 1.0;

    const double room = roomForColumns() - columnOrigin(0).top() - cardHeight();
    if (natural <= room)
        return 1.0;
    // At least a pixel per card, so a column never stacks into one place.
    return std::max(room, double(column.size() - 1)) / natural;
}

QRectF SpiderView::cardRect(int column, int index) const
{
    QRectF r = columnOrigin(column);
    const std::vector<Card>& col = m_table.columns()[std::size_t(column)];
    const double scale = fanScale(col);
    double y = r.top();
    for (int i = 0; i < index && i < int(col.size()); ++i)
        y += fanStep(col, i) * scale;
    r.moveTop(y);
    return r;
}

double SpiderView::deepestColumnBottom() const
{
    double deepest = 0.0;
    for (int col = 0; col < kColumns; ++col) {
        const std::vector<Card>& column = m_table.columns()[std::size_t(col)];
        if (!column.empty())
            deepest = std::max(deepest, cardRect(col, int(column.size()) - 1).bottom());
    }
    return deepest;
}

double SpiderView::roomForColumns() const
{
    return height() - kMargin - captionBand(QRectF(rect()));
}

QRectF SpiderView::stockRect() const
{
    const double w = cardWidth();
    // The caption's plate is opaque and painted last, so anchoring to the
    // bottom of the WIDGET puts the stock under it. cardWidth() already takes
    // the band off the height it sizes against; the anchor has to as well.
    const double bottom = height() - captionBand(QRectF(rect()));
    return { width() - kMargin - w, bottom - kMargin - cardHeight(), w, cardHeight() };
}

void SpiderView::dealRow()
{
    if (m_table.stock().empty())
        return;
    if (!m_table.dealRow()) {
        // The rule that stops a deal burying an empty column beyond recovery.
        Q_EMIT statusChanged(QStringLiteral("Fill every empty column before dealing a new row."));
        return;
    }
    m_undoAction->setEnabled(m_table.canUndo());
    Sound::instance().play(Sound::kCardDeal);

    update();
    refresh();
    checkWin();
}

void SpiderView::checkWin()
{
    if (!m_table.won() || m_won)
        return;
    m_won = true;
    Sound::instance().play(Sound::kWin);
    const bool newBest = Scores::instance().recordLow(Scores::spiderBestMoves(m_table.suits()), m_table.moves());
    refresh();

    QTimer::singleShot(200, this, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("Solved"));
        box.setText(QStringLiteral("All eight runs complete!"));
        box.setInformativeText(
            newBest ? QStringLiteral("Moves: %1 — a new best!").arg(m_table.moves())
                    : QStringLiteral("Moves: %1.   Best: %2.")
                          .arg(m_table.moves())
                          .arg(Scores::instance().best(Scores::spiderBestMoves(m_table.suits()))));
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
                                              .arg(m_table.suits())
                                              .arg(m_table.suits() == 1 ? QString() : QStringLiteral("s")))
                             .arg(m_table.completed())
                             .arg(m_table.stock().size() / kColumns)
                             .arg(m_table.moves()));
}

void SpiderView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    Theme::paintFelt(p, rect(), Theme::kFeltTealTop, Theme::kFeltTealBottom);

    for (int col = 0; col < kColumns; ++col) {
        const std::vector<Card>& column = m_table.columns()[std::size_t(col)];
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
    if (!m_table.stock().empty()) {
        const QRectF s = stockRect();
        const int stacks = int(m_table.stock().size() / kColumns);
        for (int i = 0; i < std::min(stacks, 5); ++i)
            CardArt::paintBack(p, s.translated(-i * 5.0, -i * 2.0));
    }

    // A completed run on its way out, above the table and below the hand.
    for (const cardflight::Flight& f : m_flights) {
        const QPointF at = cardflight::positionOf(f);
        CardArt::paintFace(p, QRectF(at, QSizeF(cardWidth(), cardHeight())), f.card);
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

    // Columns first. The stock sits at the bottom right, over the tail of the
    // last column, so testing it first deals a row when the player meant to
    // pick up a card they can see (GHUB-0160). Klondike's hitTest already
    // orders it this way.
    for (int col = 0; col < kColumns; ++col) {
        const std::vector<Card>& column = m_table.columns()[std::size_t(col)];
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

    if (stockRect().contains(event->position()))
        dealRow();
}

void SpiderView::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_pressValid)
        return;

    if (!m_dragging) {
        const QPointF delta = event->position() - m_pressPos;
        if (std::hypot(delta.x(), delta.y()) < kDragThreshold)
            return;
        // The table lifts, and banks the undo snapshot BEFORE the cards leave
        // the column -- which is what makes the old hand-patched snapshot
        // below unnecessary rather than merely correct.
        m_drag = m_table.lift(m_dragFrom, m_dragIndex);
        if (m_drag.empty())
            return;
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
        const std::vector<Card>& column = m_table.columns()[std::size_t(col)];
        if (!column.empty())
            zone = zone.united(cardRect(col, int(column.size()) - 1));
        zone.setBottom(zone.bottom() + cardHeight() * 0.5);

        if (zone.contains(drop)) {
            target = col;
            break;
        }
    }

    // What the target column holds, and where each card is sitting, BEFORE the
    // drop. A completed run is taken off inside dropOn(), so by the time it
    // reports Completed those thirteen cards no longer exist to be measured.
    std::vector<Card> preColumn;
    std::vector<QRectF> preRects;
    if (target >= 0) {
        preColumn = m_table.columns()[std::size_t(target)];
        for (int i = 0; i < int(preColumn.size()); ++i)
            preRects.push_back(cardRect(target, i));
    }
    const std::vector<Card> dragged = m_drag;
    const QPointF dragTopLeft(m_dragPos.x() - m_dragGrab.x(), m_dragPos.y() - m_dragGrab.y());

    // The view decides WHICH column the drop landed on; the table decides
    // whether the run may go there, turns over what it uncovered and takes off
    // a completed run.
    const SpiderTable::Drop result
        = target >= 0 ? m_table.dropOn(target) : SpiderTable::Drop::Refused;
    if (result == SpiderTable::Drop::Refused) {
        m_table.putBack();
    } else {
        Sound::instance().play(Sound::kCardPlace);
        if (result == SpiderTable::Drop::Completed) {
            Sound::instance().play(Sound::kWin);
            // The pile as it stood the instant before the harvest: what was in
            // the column, then the run that was just dropped on top of it. The
            // last kRunLength of that is what left.
            std::vector<Card> pile = preColumn;
            std::vector<QRectF> rects = preRects;
            const double w = cardWidth();
            const double h = cardHeight();
            for (int i = 0; i < int(dragged.size()); ++i) {
                pile.push_back(dragged[std::size_t(i)]);
                rects.emplace_back(dragTopLeft.x(), dragTopLeft.y() + i * h * 0.26, w, h);
            }
            if (int(pile.size()) >= SpiderTable::kRunLength) {
                const std::size_t first = pile.size() - SpiderTable::kRunLength;
                launchCompletedRun({ pile.begin() + qsizetype(first), pile.end() },
                                   { rects.begin() + qsizetype(first), rects.end() });
            }
        }
    }
    m_undoAction->setEnabled(m_table.canUndo());

    m_drag.clear();
    m_pressValid = false;
    update();
    refresh();
    checkWin();
}

void SpiderView::launchCompletedRun(const std::vector<Card>& run,
                                    const std::vector<QRectF>& fromRects)
{
    if (run.size() != fromRects.size() || run.empty())
        return;

    // The stock corner: the one anchor on this surface that means "put away".
    // Spider draws no completed-runs pile — the count lives in the status bar,
    // which is the one place this project knows the owner does not read. Giving
    // those runs a home of their own is a layout change and a bigger item than
    // this one; the motion at least answers where they went.
    const QPointF home = stockRect().topLeft();
    for (int i = 0; i < int(run.size()); ++i) {
        cardflight::Flight f;
        f.card = run[std::size_t(i)];
        f.from = fromRects[std::size_t(i)].topLeft();
        f.to = home;
        f.delay = i * cardflight::kStagger;
        f.speed = 2.2;
        // Nothing on this surface draws a completed run, so there is no
        // destination copy to suppress and no key to suppress it by.
        f.destination = -1;
        m_flights.push_back(f);
    }

    if (m_flightTimer == nullptr) {
        m_flightTimer = new QTimer(this);
        m_flightTimer->setInterval(16);
        connect(m_flightTimer, &QTimer::timeout, this, [this] {
            if (!cardflight::advance(m_flights, 0.016))
                m_flightTimer->stop();
            update();
        });
    }
    m_flightTimer->start();
}

void SpiderView::settleForChange()
{
    // Two halves, and both bite. A card in the air carries a destination
    // captured when it left, so it would land at an address that no longer
    // means anything. And a run in mid-drag has been LIFTED off its pile:
    // leaving it in m_drag strands those cards, because the table no longer
    // holds them while m_dragging stays true -- which stops the next press
    // lifting anything ever again (GHUB-0160). putBack() is the table's own
    // answer to the second, and it drops the snapshot the lift banked, since
    // nothing actually happened.
    m_flights.clear();
    if (m_flightTimer != nullptr)
        m_flightTimer->stop();
    if (m_table.holding())
        m_table.putBack();
    m_drag.clear();
    m_dragging = false;
    m_pressValid = false;
}

void SpiderView::deactivate()
{
    settleForChange();
}

void SpiderView::applyLegibility(bool enabled)
{
    // The band comes off the height this view solves its card size from, so
    // every rect on the surface moves.
    settleForChange();
    GameView::applyLegibility(enabled);
}
