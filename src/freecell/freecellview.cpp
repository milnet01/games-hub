#include "freecellview.h"
#include "freecell/freecelltable.h"

#include "legibility.h"
#include "cards/cardart.h"
#include "cards/cardcodec.h"
#include "scores.h"
#include "sound.h"
#include "theme.h"

#include <QDataStream>
#include <QIODevice>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kMargin = 12.0;
constexpr double kDragThreshold = 4.0;
}

FreeCellView::FreeCellView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(FreeCellView::minimumSizeHint());
    buildActions();
    newGame();
}

void FreeCellView::buildActions()
{
    auto* newAction = new QAction(QStringLiteral("New Deal"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &FreeCellView::newGame);
    m_actions.append(newAction);

    m_undoAction = new QAction(QStringLiteral("Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &FreeCellView::undo);
    m_actions.append(m_undoAction);
}

void FreeCellView::newGame()
{
    m_resumed = false;
    m_table.deal();
    Sound::instance().play(Sound::kShuffle);
    m_drag.clear();
    m_dragging = false;
    m_pressValid = false;
    m_won = false;
    m_undoAction->setEnabled(false);

    update();
    refresh();
}

void FreeCellView::activate()
{
    refresh();
}

void FreeCellView::undo()
{
    if (!m_table.canUndo())
        return;
    m_table.undo();
    m_won = false;
    m_undoAction->setEnabled(m_table.canUndo());
    update();
    refresh();
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

// The table, not the moves that made it — see KlondikeView::saveState for why
// the card games save differently from Chess.
QByteArray FreeCellView::saveState() const
{
    if (m_won || (!m_table.canUndo() && !m_resumed))
        return {};

    // A run lifted in mid-drag belongs to the pile it came from until it is
    // dropped; closing the window while holding it must not lose the cards.
    const auto pile = [this](PileKind kind, int index) {
        std::vector<Card> cards = pileFor(kind, index);
        if (m_dragging && m_dragFrom.kind == kind && m_dragFrom.pile == index)
            cards.insert(cards.end(), m_drag.begin(), m_drag.end());
        return cards;
    };

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1) << qint32(m_table.moves());
    for (int col = 0; col < kColumns; ++col)
        cardcodec::writePile(out, pile(PileKind::Column, col));
    for (int i = 0; i < kCells; ++i)
        cardcodec::writePile(out, pile(PileKind::Cell, i));
    for (int f = 0; f < 4; ++f)
        cardcodec::writePile(out, pile(PileKind::Foundation, f));
    return blob;
}

bool FreeCellView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    qint32 moves = 0;
    in >> version >> moves;
    if (version != 1 || in.status() != QDataStream::Ok || moves < 0)
        return false;

    // Read into a table of its own, so a blob that turns out to be nonsense
    // leaves the deal already on screen alone.
    std::array<std::vector<Card>, kColumns> columns;
    std::array<std::vector<Card>, kCells> cells;
    std::array<std::vector<Card>, 4> foundations;
    if (!cardcodec::readPiles(in, columns) || !cardcodec::readPiles(in, cells)
        || !cardcodec::readPiles(in, foundations))
        return false;

    // The table decides whether this is a position the rules could have
    // produced -- a cell holding one card at most, and the whole pack back,
    // because FreeCell never takes a card out of play.
    if (!m_table.restore(columns, cells, foundations, int(moves)))
        return false;

    m_drag.clear();
    m_dragging = false;
    m_pressValid = false;
    m_won = false;
    m_resumed = true;
    m_undoAction->setEnabled(false);
    update();
    refresh();
    return true;
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

double FreeCellView::cardWidth() const
{
    constexpr double kRowCost = kColumns + (kColumns - 1) * 0.12;
    const double byWidth = (width() - 2 * kMargin) / kRowCost;
    // The caption's strip comes off the height before the card is sized: the
    // piles are anchored to the top, so a smaller card is what keeps the tail
    // of a long column clear of the sentence under it.
    //
    // What the height must hold, in card heights: the cell/foundation row, the
    // gap under it, then the longest column the deal makes — six fan steps and
    // one whole card. The figure used to be a flat 2.5, which is a header plus
    // about two cards of fan, and the game deals seven.
    constexpr double kHeightCost =
        1.0 + kHeaderGap + (kLongestDealtColumn - 1) * kFanStep + 1.0;
    const double byHeight =
        (height() - 2 * kMargin - captionBand(QRectF(rect()))) / (1.4 * kHeightCost);
    return std::max(32.0, std::min(byWidth, byHeight));
}

QRectF FreeCellView::pileOrigin(PileKind kind, int pile) const
{
    const double w = cardWidth();
    const double h = cardHeight();
    const double step = w * 1.12;

    switch (kind) {
    case PileKind::Cell:
        return { kMargin + step * pile, kMargin, w, h };
    case PileKind::Foundation:
        return { kMargin + step * (4 + pile), kMargin, w, h };
    case PileKind::Column:
        return { kMargin + step * pile, kMargin + h + h * kHeaderGap, w, h };
    }
    return {};
}

double FreeCellView::deepestColumnBottom() const
{
    double deepest = 0.0;
    for (int col = 0; col < kColumns; ++col) {
        const std::vector<Card>& column = m_table.columns()[std::size_t(col)];
        if (!column.empty())
            deepest = std::max(deepest, cardRect(col, int(column.size()) - 1).bottom());
    }
    return deepest;
}

double FreeCellView::roomForColumns() const
{
    return height() - kMargin - captionBand(QRectF(rect()));
}

double FreeCellView::fanStep(int column) const
{
    const double full = fanStep();
    const int n = int(m_table.columns()[std::size_t(column)].size());
    if (n < 2)
        return full;
    const double top = pileOrigin(PileKind::Column, column).top();
    const double room =
        height() - kMargin - captionBand(QRectF(rect())) - top - cardHeight();
    return std::max(1.0, std::min(full, room / (n - 1)));
}

QRectF FreeCellView::cardRect(int column, int index) const
{
    QRectF r = pileOrigin(PileKind::Column, column);
    r.moveTop(r.top() + index * fanStep(column));
    return r;
}

FreeCellView::Spot FreeCellView::hitTest(QPointF pos) const
{
    for (int col = 0; col < kColumns; ++col) {
        const std::vector<Card>& column = m_table.columns()[std::size_t(col)];
        for (int i = int(column.size()) - 1; i >= 0; --i)
            if (cardRect(col, i).contains(pos))
                return { PileKind::Column, col, i, true };
        if (column.empty() && pileOrigin(PileKind::Column, col).contains(pos))
            return { PileKind::Column, col, -1, true };
    }
    for (int i = 0; i < kCells; ++i)
        if (pileOrigin(PileKind::Cell, i).contains(pos))
            return { PileKind::Cell, i, int(m_table.cells()[std::size_t(i)].size()) - 1, true };
    for (int i = 0; i < 4; ++i)
        if (pileOrigin(PileKind::Foundation, i).contains(pos))
            return { PileKind::Foundation, i, int(m_table.foundations()[std::size_t(i)].size()) - 1, true };
    return {};
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------

void FreeCellView::checkWin()
{
    if (!m_table.won() || m_won)
        return;

    m_won = true;
    Sound::instance().play(Sound::kWin);
    const bool newBest = Scores::instance().recordLow(
        QStringLiteral("freecell/best_moves"), m_table.moves());
    refresh();

    QTimer::singleShot(200, this, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("Solved"));
        box.setText(QStringLiteral("Every card home!"));
        box.setInformativeText(
            newBest ? QStringLiteral("Moves: %1 — a new best!").arg(m_table.moves())
                    : QStringLiteral("Moves: %1.   Best: %2.")
                          .arg(m_table.moves())
                          .arg(Scores::instance().best(QStringLiteral("freecell/best_moves"))));
        QAbstractButton* again = box.addButton(QStringLiteral("New Deal"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Close"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == again)
            newGame();
    });
}

void FreeCellView::refresh(const QString& message)
{
    int done = 0;
    for (const auto& f : m_table.foundations())
        done += int(f.size());
    int freeCells = 0;
    for (const auto& c : m_table.cells())
        if (c.empty())
            ++freeCells;

    QString line = message.isEmpty()
        ? QStringLiteral("%1   Home %2/52   Free cells %3   Moves %4")
              .arg(m_won ? QStringLiteral("Solved!") : QStringLiteral("FreeCell"))
              .arg(done)
              .arg(freeCells)
              .arg(m_table.moves())
        : message;
    if (Scores::instance().has(QStringLiteral("freecell/best_moves")))
        line += QStringLiteral("   Best %1")
                    .arg(Scores::instance().best(QStringLiteral("freecell/best_moves")));
    Q_EMIT statusChanged(line);
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void FreeCellView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    Theme::paintFelt(p, rect(), Theme::kFeltGreenTop, Theme::kFeltGreenBottom);

    for (int i = 0; i < kCells; ++i) {
        const QRectF r = pileOrigin(PileKind::Cell, i);
        if (m_table.cells()[std::size_t(i)].empty())
            CardArt::paintSlot(p, r);
        else
            CardArt::paintFace(p, r, m_table.cells()[std::size_t(i)].back());
    }

    // One match per flight, so two identical cards in the air do not both
    // suppress the same destination copy. Reset every repaint.
    m_flightConsumed.assign(m_flights.size(), 0);

    for (int i = 0; i < 4; ++i) {
        const QRectF r = pileOrigin(PileKind::Foundation, i);
        const std::vector<Card>& pile = m_table.foundations()[std::size_t(i)];
        // A foundation shows its top card only, so one still on its way here is
        // drawn a rank down until it lands — otherwise it is on screen twice.
        std::size_t shown = pile.size();
        if (shown > 0 && cardflight::suppressAt(m_flights, m_flightConsumed, i, pile.back()))
            --shown;
        if (shown == 0)
            CardArt::paintSlot(p, r, QStringLiteral("A"));
        else
            CardArt::paintFace(p, r, pile[shown - 1]);
    }

    for (int col = 0; col < kColumns; ++col) {
        const std::vector<Card>& column = m_table.columns()[std::size_t(col)];
        if (column.empty()) {
            CardArt::paintSlot(p, pileOrigin(PileKind::Column, col));
            continue;
        }
        const int run = m_table.orderedRunLength(col);
        for (int i = 0; i < int(column.size()); ++i) {
            const QRectF r = cardRect(col, i);
            CardArt::paintFace(p, r, column[std::size_t(i)]);
            // Show where the liftable run begins, and whether it will fit.
            if (run > 1 && i == int(column.size()) - run) {
                const bool fits = run <= m_table.maxMoveSize(false);
                CardArt::paintHighlight(p, r,
                                        fits ? QColor(0xff, 0xd5, 0x4f, 170)
                                             : QColor(0xe8, 0x51, 0x4f, 130));
            }
        }
    }

    // Cards on their way home, above the table and below the hand.
    for (const cardflight::Flight& f : m_flights) {
        const QPointF at = cardflight::positionOf(f);
        CardArt::paintFace(p, QRectF(at, QSizeF(cardWidth(), cardHeight())), f.card);
    }

    if (m_dragging && !m_drag.empty()) {
        const double w = cardWidth();
        const double h = cardHeight();
        for (int i = 0; i < int(m_drag.size()); ++i) {
            const QRectF r(m_dragPos.x() - m_dragGrab.x(),
                           m_dragPos.y() - m_dragGrab.y() + i * fanStep(), w, h);
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

void FreeCellView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    m_pressPos = event->position();
    m_pressValid = false;

    const Spot s = hitTest(event->position());
    if (!s.valid || s.index < 0)
        return;

    if (s.kind == PileKind::Column) {
        if (s.index < m_table.firstMovableIndex(s.pile)) {
            refresh(QStringLiteral("Only a run in alternating colours moves together."));
            return;
        }
    } else if (s.index != int(pileFor(s.kind, s.pile).size()) - 1) {
        return;
    }

    m_dragFrom = s;
    m_pressValid = true;
    m_dragGrab = event->position()
        - (s.kind == PileKind::Column ? cardRect(s.pile, s.index) : pileOrigin(s.kind, s.pile))
              .topLeft();
}

void FreeCellView::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_pressValid)
        return;

    if (!m_dragging) {
        const QPointF delta = event->position() - m_pressPos;
        if (std::hypot(delta.x(), delta.y()) < kDragThreshold)
            return;
        // The table lifts, and banks the undo snapshot BEFORE the cards leave
        // their pile. Doing it at drop time -- which is what this used to do --
        // snapshots a table the cards had already left, so undoing a finished
        // move lost them altogether.
        m_drag = m_table.lift(m_dragFrom.kind, m_dragFrom.pile, m_dragFrom.index);
        if (m_drag.empty())
            return;
        m_dragging = true;
    }

    m_dragPos = event->position();
    update();
}

void FreeCellView::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_dragging) {
        m_pressValid = false;
        return;
    }

    m_dragging = false;
    const QPointF drop = event->position();
    bool placed = false;
    QString refusal;

    // The view decides WHICH pile the drop landed on; the table decides
    // whether the cards may go there.
    if (m_drag.size() == 1) {
        for (int i = 0; i < kCells && !placed; ++i) {
            if (pileOrigin(PileKind::Cell, i).contains(drop))
                placed = m_table.dropOnCell(m_drag, i);
        }
        for (int f = 0; f < 4 && !placed; ++f) {
            if (pileOrigin(PileKind::Foundation, f).contains(drop))
                placed = m_table.dropOnFoundation(m_drag, f);
        }
    }

    for (int col = 0; col < kColumns && !placed; ++col) {
        QRectF zone = pileOrigin(PileKind::Column, col);
        const std::vector<Card>& column = m_table.columns()[std::size_t(col)];
        if (!column.empty())
            zone = zone.united(cardRect(col, int(column.size()) - 1));
        zone.setBottom(zone.bottom() + cardHeight() * 0.5);
        if (!zone.contains(drop))
            continue;

        int limit = 0;
        placed = m_table.dropOnColumn(m_drag, col, &limit);
        if (!placed && limit > 0) {
            refusal = QStringLiteral("Only %1 card%2 can move at once — free a cell or a column.")
                          .arg(limit)
                          .arg(limit == 1 ? QString() : QStringLiteral("s"));
            break;
        }
    }

    if (placed) {
        Sound::instance().play(Sound::kCardPlace);
        m_undoAction->setEnabled(m_table.canUndo());
    } else {
        // Nothing happened, so the table takes the cards back and drops the
        // snapshot it banked when they were lifted.
        m_table.putBack(m_dragFrom.kind, m_dragFrom.pile, m_drag);
        m_undoAction->setEnabled(m_table.canUndo());
    }

    m_drag.clear();
    m_pressValid = false;
    update();
    refresh(refusal);
    checkWin();
}

void FreeCellView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const Spot s = hitTest(event->position());
    if (!s.valid || s.index < 0 || s.kind == PileKind::Foundation)
        return;
    // Where the card stands and what the foundations hold, BEFORE the move: a
    // successful send does not report where the card went, and afterwards
    // there is nothing left at the old address to measure.
    const std::vector<Card>& source = pileFor(s.kind, s.pile);
    if (source.empty())
        return;
    const Card moving = source.back();
    const QRectF fromRect = s.kind == PileKind::Column
        ? cardRect(s.pile, int(source.size()) - 1)
        : pileOrigin(s.kind, s.pile);
    std::array<std::size_t, 4> before {};
    for (int f = 0; f < 4; ++f)
        before[std::size_t(f)] = m_table.foundations()[std::size_t(f)].size();

    if (m_table.sendToFoundation(s.kind, s.pile)) {
        Sound::instance().play(Sound::kCardPlace);
        launchToFoundation(moving, fromRect, grownFoundation(before));
        m_undoAction->setEnabled(m_table.canUndo());
        update();
        refresh();
        checkWin();
    }
}

int FreeCellView::grownFoundation(const std::array<std::size_t, 4>& before) const
{
    for (int f = 0; f < 4; ++f) {
        if (m_table.foundations()[std::size_t(f)].size() > before[std::size_t(f)])
            return f;
    }
    return -1;
}

void FreeCellView::launchToFoundation(const Card& card, QRectF fromRect, int foundation)
{
    if (foundation < 0)
        return;

    cardflight::Flight f;
    f.card = card;
    f.from = fromRect.topLeft();
    f.to = pileOrigin(PileKind::Foundation, foundation).topLeft();
    f.destination = foundation;
    m_flights.push_back(f);

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

void FreeCellView::deactivate()
{
    // A card in the air carries a destination captured when it left, and the
    // hub may resize this page while it is away.
    m_flights.clear();
    if (m_flightTimer != nullptr)
        m_flightTimer->stop();
}

void FreeCellView::applyLegibility(bool enabled)
{
    // Land them where the model already believes they are, then let the base
    // re-lay-out. Keeping them would put a card down at its old destination.
    m_flights.clear();
    if (m_flightTimer != nullptr)
        m_flightTimer->stop();
    GameView::applyLegibility(enabled);
}
