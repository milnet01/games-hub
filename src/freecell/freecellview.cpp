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
#include <QKeyEvent>
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
    // Without it setFocus() does nothing and no key ever arrives (GHUB-0168).
    setFocusPolicy(Qt::StrongFocus);
    buildActions();
    newGame();
}

void FreeCellView::buildActions()
{
    auto* newAction = new QAction(tr("New Deal"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &FreeCellView::newGame);
    m_actions.append(newAction);

    m_undoAction = new QAction(tr("Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &FreeCellView::undo);
    m_actions.append(m_undoAction);
}

void FreeCellView::newGame()
{
    // Before the deal, not after: settling puts a held run back on the table
    // it came from, and after a deal that is a fresh table it never left.
    settleForChange();
    m_resumed = false;
    m_table.deal();
    Sound::instance().play(Sound::kShuffle);
    m_cursorCol = 0;
    m_cursorDepth = 99;
    clampCursor();
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
    // A held run goes back first, which also drops the snapshot its lift
    // banked -- otherwise Undo would undo the lift and leave the run in hand.
    settleForChange();
    if (!m_table.canUndo()) {
        m_undoAction->setEnabled(false);
        update();
        return;
    }
    m_table.undo();
    clampCursor();
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
        if ((m_dragging || m_keyHolding) && m_dragFrom.kind == kind && m_dragFrom.pile == index)
            cards.insert(cards.end(), m_drag.begin(), m_drag.end());
        return cards;
    };

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    // Version 2 appends the keyboard cursor, last; a version-1 save still
    // loads, with the cursor where a fresh deal puts it.
    out << quint32(2) << qint32(m_table.moves());
    for (int col = 0; col < kColumns; ++col)
        cardcodec::writePile(out, pile(PileKind::Column, col));
    for (int i = 0; i < kCells; ++i)
        cardcodec::writePile(out, pile(PileKind::Cell, i));
    for (int f = 0; f < 4; ++f)
        cardcodec::writePile(out, pile(PileKind::Foundation, f));
    out << qint8(m_cursorCol) << qint8(m_cursorDepth);
    return blob;
}

bool FreeCellView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    qint32 moves = 0;
    in >> version >> moves;
    if ((version != 1 && version != 2) || in.status() != QDataStream::Ok || moves < 0)
        return false;

    // Read into a table of its own, so a blob that turns out to be nonsense
    // leaves the deal already on screen alone.
    std::array<std::vector<Card>, kColumns> columns;
    std::array<std::vector<Card>, kCells> cells;
    std::array<std::vector<Card>, 4> foundations;
    if (!cardcodec::readPiles(in, columns) || !cardcodec::readPiles(in, cells)
        || !cardcodec::readPiles(in, foundations))
        return false;
    qint8 cursorCol = 0;
    qint8 cursorDepth = 99;
    if (version >= 2) {
        in >> cursorCol >> cursorDepth;
        if (in.status() != QDataStream::Ok || cursorCol < 0 || cursorCol >= kColumns
            || cursorDepth < -1)
            return false;
    }

    // The table decides whether this is a position the rules could have
    // produced -- a cell holding one card at most, and the whole pack back,
    // because FreeCell never takes a card out of play.
    if (!m_table.restore(columns, cells, foundations, int(moves)))
        return false;

    m_drag.clear();
    m_dragging = false;
    m_pressValid = false;
    m_keyHolding = false;
    m_cursorCol = cursorCol;
    m_cursorDepth = cursorDepth;
    clampCursor();
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
        QStringLiteral("freecell/best_moves"), m_table.moves()); // untranslated: settings key
    refresh();

    announceLater(200, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(tr("Solved"));
        box.setText(tr("Every card home!"));
        box.setInformativeText(
            newBest ? tr("Moves: %1 — a new best!").arg(m_table.moves())
                    : tr("Moves: %1.   Best: %2.")
                          .arg(m_table.moves())
                          .arg(Scores::instance().best(
                              QStringLiteral("freecell/best_moves")))); // untranslated: settings key
        QAbstractButton* again = box.addButton(tr("New Deal"), QMessageBox::AcceptRole);
        box.addButton(tr("Close"), QMessageBox::RejectRole);
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
        ? tr("%1   Home %2/52   Free cells %3   Moves %4")
              .arg(m_won ? tr("Solved!") : tr("FreeCell"))
              .arg(done)
              .arg(freeCells)
              .arg(m_table.moves())
        : message;
    if (Scores::instance().has(QStringLiteral("freecell/best_moves"))) // untranslated: settings key
        line = tr("%1   Best %2")
                   .arg(line)
                   .arg(Scores::instance().best(
                       QStringLiteral("freecell/best_moves"))); // untranslated: settings key
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
            CardArt::paintSlot(p, r, rankLabel(kAce));
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

    // The keyboard's run and the cursor, over everything but the caption.
    if (!m_dragging) {
        QRectF cursor;
        if (m_keyHolding && !m_drag.empty()) {
            const QRectF first = heldLandingRect();
            cursor = first;
            for (int i = 0; i < int(m_drag.size()); ++i) {
                const QRectF r = first.translated(0, i * fanStep());
                CardArt::paintFace(p, r, m_drag[std::size_t(i)]);
                cursor = cursor.united(r);
            }
        } else {
            const Spot s = cursorPile();
            cursor = s.kind != PileKind::Column || s.index < 0
                ? pileOrigin(s.kind, s.pile)
                // The card and everything under it: what Space would lift.
                : cardRect(s.pile, s.index)
                      .united(cardRect(s.pile, int(pileFor(s.kind, s.pile).size()) - 1));
        }
        Theme::paintCellCursor(p, cursor, Legibility::instance().enabled());
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

    // A run held by the keyboard goes back before the mouse does anything, so
    // the two never hold cards at once.
    if (m_keyHolding) {
        m_table.putBack(m_dragFrom.kind, m_dragFrom.pile, m_drag);
        m_keyHolding = false;
        m_drag.clear();
        m_undoAction->setEnabled(m_table.canUndo());
        update();
    }

    const Spot s = hitTest(event->position());
    if (!s.valid)
        return;
    // The cursor follows the mouse, so the two ways of playing never disagree
    // about where you are.
    if (s.kind == PileKind::Column) {
        m_cursorCol = s.pile;
        m_cursorDepth = std::max(0, s.index);
    } else {
        m_cursorCol = s.kind == PileKind::Cell ? s.pile : 4 + s.pile;
        m_cursorDepth = -1;
    }
    clampCursor();
    update();
    if (s.index < 0)
        return;

    if (s.kind == PileKind::Column) {
        if (s.index < m_table.firstMovableIndex(s.pile)) {
            refresh(tr("Only a run in alternating colours moves together."));
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

    // The view decides WHICH pile the drop landed on; the table decides
    // whether the cards may go there.
    Spot target;
    for (int i = 0; i < kCells && !target.valid; ++i) {
        if (pileOrigin(PileKind::Cell, i).contains(drop))
            target = { PileKind::Cell, i, -1, true };
    }
    for (int f = 0; f < 4 && !target.valid; ++f) {
        if (pileOrigin(PileKind::Foundation, f).contains(drop))
            target = { PileKind::Foundation, f, -1, true };
    }
    for (int col = 0; col < kColumns && !target.valid; ++col) {
        QRectF zone = pileOrigin(PileKind::Column, col);
        const std::vector<Card>& column = m_table.columns()[std::size_t(col)];
        if (!column.empty())
            zone = zone.united(cardRect(col, int(column.size()) - 1));
        zone.setBottom(zone.bottom() + cardHeight() * 0.5);
        if (zone.contains(drop))
            target = { PileKind::Column, col, -1, true };
    }

    QString refusal;
    if (dropHeldOn(target, &refusal)) {
        m_undoAction->setEnabled(m_table.canUndo());
    } else {
        // Nothing happened, so the table takes the cards back and drops the
        // snapshot it banked when they were lifted.
        m_table.putBack(m_dragFrom.kind, m_dragFrom.pile, m_drag);
        m_undoAction->setEnabled(m_table.canUndo());
    }

    m_drag.clear();
    m_pressValid = false;
    clampCursor();
    update();
    refresh(refusal);
    checkWin();
}

bool FreeCellView::dropHeldOn(const Spot& target, QString* refusal)
{
    if (!target.valid || m_drag.empty())
        return false;

    bool placed = false;
    switch (target.kind) {
    case PileKind::Cell:
        placed = m_table.dropOnCell(m_drag, target.pile);
        break;
    case PileKind::Foundation:
        placed = m_table.dropOnFoundation(m_drag, target.pile);
        break;
    case PileKind::Column: {
        int limit = 0;
        placed = m_table.dropOnColumn(m_drag, target.pile, &limit);
        if (!placed && limit > 0 && refusal != nullptr) {
            // The singular is its own sentence: with no translation loaded, a
            // %n form prints its English source as written for every count.
            *refusal = limit == 1
                ? tr("Only 1 card can move at once — free a cell or a column.")
                : tr("Only %n cards can move at once — free a cell or a column.", nullptr, limit);
        }
        break;
    }
    }
    if (placed)
        Sound::instance().play(Sound::kCardPlace);
    return placed;
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

void FreeCellView::settleForChange()
{
    // Two halves. A card in the air carries a destination captured when it
    // left, and the layout may be about to move under it. And a run held by
    // the drag or the keyboard has been LIFTED off its pile, so it goes back
    // there; putBack() also drops the snapshot the lift banked, since nothing
    // actually happened.
    m_flights.clear();
    if (m_flightTimer != nullptr)
        m_flightTimer->stop();
    if ((m_dragging || m_keyHolding) && !m_drag.empty())
        m_table.putBack(m_dragFrom.kind, m_dragFrom.pile, m_drag);
    m_drag.clear();
    m_dragging = false;
    m_keyHolding = false;
    m_pressValid = false;
}

void FreeCellView::deactivate()
{
    // The hub may resize this page while it is away.
    settleForChange();
}

void FreeCellView::applyLegibility(bool enabled)
{
    // Land them where the model already believes they are, then let the base
    // re-lay-out. Keeping them would put a card down at its old destination.
    settleForChange();
    GameView::applyLegibility(enabled);
}

// ---------------------------------------------------------------------------
// Keyboard (GHUB-0168): the boards' scheme, as in KlondikeView. Arrows move
// between the eight columns and up a column's liftable run to the top row;
// Space lifts, Space drops, Escape puts back.
// ---------------------------------------------------------------------------

FreeCellView::Spot FreeCellView::cursorPile() const
{
    if (m_cursorDepth < 0) {
        if (m_cursorCol < kCells)
            return { PileKind::Cell, m_cursorCol,
                     int(m_table.cells()[std::size_t(m_cursorCol)].size()) - 1, true };
        const int f = m_cursorCol - kCells;
        return { PileKind::Foundation, f,
                 int(m_table.foundations()[std::size_t(f)].size()) - 1, true };
    }
    const std::vector<Card>& column = m_table.columns()[std::size_t(m_cursorCol)];
    return { PileKind::Column, m_cursorCol, column.empty() ? -1 : m_cursorDepth, true };
}

void FreeCellView::clampCursor()
{
    m_cursorCol = std::clamp(m_cursorCol, 0, kColumns - 1);
    if (m_cursorDepth < 0) {
        m_cursorDepth = -1;
        return;
    }
    const std::vector<Card>& column = m_table.columns()[std::size_t(m_cursorCol)];
    if (column.empty()) {
        m_cursorDepth = 0;
        return;
    }
    const int last = int(column.size()) - 1;
    // Only the alternating run at the foot of a column comes away, so that is
    // the whole range the cursor can stand on -- and while a run is held the
    // cursor points at a pile, so it sits on the top.
    m_cursorDepth = m_keyHolding
        ? last
        : std::clamp(m_cursorDepth, m_table.firstMovableIndex(m_cursorCol), last);
}

QRectF FreeCellView::heldLandingRect() const
{
    const Spot s = cursorPile();
    QRectF r = pileOrigin(s.kind, s.pile);
    if (s.kind == PileKind::Column && s.index >= 0) {
        const int last = int(pileFor(s.kind, s.pile).size()) - 1;
        r = cardRect(s.pile, last).translated(0, fanStep(s.pile));
    }
    // Raised off the pile, so it reads as held rather than as played.
    return r.translated(cardWidth() * 0.10, -cardHeight() * 0.06);
}

void FreeCellView::pressAtCursor()
{
    if (m_keyHolding) {
        dropAtCursor();
        return;
    }
    const Spot s = cursorPile();
    if (s.index < 0)
        return;
    // Banks the undo snapshot before the cards leave, as a drag does.
    m_drag = m_table.lift(s.kind, s.pile, s.index);
    if (m_drag.empty())
        return;
    m_dragFrom = s;
    m_keyHolding = true;
    update();
}

void FreeCellView::dropAtCursor()
{
    const Spot s = cursorPile();
    if (s.kind == m_dragFrom.kind && s.pile == m_dragFrom.pile) {
        m_table.putBack(m_dragFrom.kind, m_dragFrom.pile, m_drag);
        m_keyHolding = false;
        m_drag.clear();
        m_undoAction->setEnabled(m_table.canUndo());
        return;
    }

    const int dropped = int(m_drag.size());
    QString refusal;
    // Not a legal home: keep holding, so the player can try another pile.
    if (!dropHeldOn(s, &refusal)) {
        if (!refusal.isEmpty())
            refresh(refusal);
        return;
    }

    m_keyHolding = false;
    m_drag.clear();
    // Onto the first card of the run it dropped, so Space can pick it up again.
    if (s.kind == PileKind::Column)
        m_cursorDepth = int(pileFor(s.kind, s.pile).size()) - dropped;
    m_undoAction->setEnabled(m_table.canUndo());
    refresh();
    checkWin();
}

void FreeCellView::keyPressEvent(QKeyEvent* event)
{
    if (m_dragging) {
        GameView::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Left:
        m_cursorCol = std::max(0, m_cursorCol - 1);
        if (m_cursorDepth >= 0)
            m_cursorDepth = 99; // the top of the new column; clamped
        break;
    case Qt::Key_Right:
        m_cursorCol = std::min(kColumns - 1, m_cursorCol + 1);
        if (m_cursorDepth >= 0)
            m_cursorDepth = 99;
        break;
    case Qt::Key_Up: {
        if (m_cursorDepth < 0)
            break;
        const bool canClimb = !m_keyHolding
            && !m_table.columns()[std::size_t(m_cursorCol)].empty()
            && m_cursorDepth > m_table.firstMovableIndex(m_cursorCol);
        if (canClimb)
            --m_cursorDepth;
        else
            m_cursorDepth = -1;
        break;
    }
    case Qt::Key_Down:
        if (m_cursorDepth < 0)
            m_cursorDepth = 99; // the top card of the column below; clamped
        else if (!m_keyHolding)
            ++m_cursorDepth;
        break;
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        pressAtCursor();
        break;
    case Qt::Key_Escape:
        if (!m_keyHolding) {
            GameView::keyPressEvent(event);
            return;
        }
        m_table.putBack(m_dragFrom.kind, m_dragFrom.pile, m_drag);
        m_keyHolding = false;
        m_drag.clear();
        m_undoAction->setEnabled(m_table.canUndo());
        break;
    default:
        GameView::keyPressEvent(event);
        return;
    }

    clampCursor();
    update();
}
