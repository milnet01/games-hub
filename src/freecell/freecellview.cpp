#include "freecellview.h"

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
    setMinimumSize(minimumSizeHint());
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
    std::vector<Card> deck = makeDeck(1, 4);
    shuffleCards(deck, m_rng);
    Sound::instance().play(Sound::kShuffle);

    for (auto& c : m_columns)
        c.clear();
    for (auto& c : m_cells)
        c.clear();
    for (auto& f : m_foundations)
        f.clear();
    m_drag.clear();
    m_history.clear();
    m_dragging = false;
    m_pressValid = false;
    m_moves = 0;
    m_won = false;
    m_undoAction->setEnabled(false);

    // All 52 face up across eight columns: four of seven, four of six.
    for (std::size_t i = 0; i < deck.size(); ++i) {
        Card c = deck[i];
        c.faceUp = true;
        m_columns[i % kColumns].push_back(c);
    }

    update();
    refresh();
}

void FreeCellView::activate()
{
    refresh();
}

void FreeCellView::pushUndo()
{
    m_history.push_back({ m_columns, m_cells, m_foundations, m_moves });
    if (m_history.size() > 300)
        m_history.erase(m_history.begin());
    m_undoAction->setEnabled(true);
}

void FreeCellView::undo()
{
    if (m_history.empty())
        return;
    const Snapshot s = m_history.back();
    m_history.pop_back();
    m_columns = s.columns;
    m_cells = s.cells;
    m_foundations = s.foundations;
    m_moves = s.moves;
    m_won = false;
    m_undoAction->setEnabled(!m_history.empty());
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
    if (m_won || m_history.empty())
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
    out << quint32(1) << qint32(m_moves);
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

    // A cell holds one card. That is the whole point of the game, so a save
    // saying otherwise is not a FreeCell position.
    for (const std::vector<Card>& cell : cells) {
        if (cell.size() > 1)
            return false;
    }

    // FreeCell never takes a card out of play, so the whole pack must be here —
    // nothing missing, nothing doubled.
    std::vector<Card> all;
    cardcodec::gather(all, columns);
    cardcodec::gather(all, cells);
    cardcodec::gather(all, foundations);
    if (!cardcodec::matchesPack(all, 1, 4))
        return false;

    m_columns = columns;
    m_cells = cells;
    m_foundations = foundations;
    m_moves = int(moves);
    m_history.clear();
    m_drag.clear();
    m_dragging = false;
    m_pressValid = false;
    m_won = false;
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

QRectF FreeCellView::cardRect(int column, int index) const
{
    QRectF r = pileOrigin(PileKind::Column, column);
    r.moveTop(r.top() + index * fanStep());
    return r;
}

std::vector<Card>& FreeCellView::pileFor(PileKind kind, int pile)
{
    switch (kind) {
    case PileKind::Cell: return m_cells[std::size_t(pile)];
    case PileKind::Foundation: return m_foundations[std::size_t(pile)];
    case PileKind::Column: return m_columns[std::size_t(pile)];
    }
    return m_columns[0];
}

const std::vector<Card>& FreeCellView::pileFor(PileKind kind, int pile) const
{
    return const_cast<FreeCellView*>(this)->pileFor(kind, pile);
}

FreeCellView::Spot FreeCellView::hitTest(QPointF pos) const
{
    for (int col = 0; col < kColumns; ++col) {
        const std::vector<Card>& column = m_columns[std::size_t(col)];
        for (int i = int(column.size()) - 1; i >= 0; --i)
            if (cardRect(col, i).contains(pos))
                return { PileKind::Column, col, i, true };
        if (column.empty() && pileOrigin(PileKind::Column, col).contains(pos))
            return { PileKind::Column, col, -1, true };
    }
    for (int i = 0; i < kCells; ++i)
        if (pileOrigin(PileKind::Cell, i).contains(pos))
            return { PileKind::Cell, i, int(m_cells[std::size_t(i)].size()) - 1, true };
    for (int i = 0; i < 4; ++i)
        if (pileOrigin(PileKind::Foundation, i).contains(pos))
            return { PileKind::Foundation, i, int(m_foundations[std::size_t(i)].size()) - 1, true };
    return {};
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------

int FreeCellView::maxMoveSize(bool toEmptyColumn) const
{
    int freeCells = 0;
    for (const auto& c : m_cells)
        if (c.empty())
            ++freeCells;
    int emptyColumns = 0;
    for (const auto& c : m_columns)
        if (c.empty())
            ++emptyColumns;

    // Moving *into* an empty column cannot also use that column as a staging
    // area, so it does not count towards the multiplier.
    if (toEmptyColumn && emptyColumns > 0)
        --emptyColumns;

    return (freeCells + 1) * (1 << std::min(emptyColumns, 10));
}

int FreeCellView::orderedRunLength(int column) const
{
    const std::vector<Card>& col = m_columns[std::size_t(column)];
    if (col.empty())
        return 0;
    int run = 1;
    for (int i = int(col.size()) - 1; i > 0; --i) {
        const Card& lower = col[std::size_t(i)];
        const Card& upper = col[std::size_t(i - 1)];
        if (upper.rank != lower.rank + 1 || isRed(upper) == isRed(lower))
            break;
        ++run;
    }
    return run;
}

bool FreeCellView::canStack(const Card& moving, int column) const
{
    const std::vector<Card>& target = m_columns[std::size_t(column)];
    if (target.empty())
        return true;
    const Card& top = target.back();
    return isRed(top) != isRed(moving) && top.rank == moving.rank + 1;
}

bool FreeCellView::canPlaceOnFoundation(const Card& moving, int foundation) const
{
    const std::vector<Card>& target = m_foundations[std::size_t(foundation)];
    if (target.empty())
        return moving.rank == kAce;
    return target.back().suit == moving.suit && target.back().rank + 1 == moving.rank;
}

bool FreeCellView::sendToFoundation(PileKind from, int pile)
{
    std::vector<Card>& source = pileFor(from, pile);
    if (source.empty())
        return false;
    for (int f = 0; f < 4; ++f) {
        if (!canPlaceOnFoundation(source.back(), f))
            continue;
        pushUndo();
        m_foundations[std::size_t(f)].push_back(source.back());
        source.pop_back();
        ++m_moves;
        Sound::instance().play(Sound::kCardPlace);
        return true;
    }
    return false;
}

void FreeCellView::checkWin()
{
    int total = 0;
    for (const auto& f : m_foundations)
        total += int(f.size());
    if (total != 52 || m_won)
        return;

    m_won = true;
    Sound::instance().play(Sound::kWin);
    const bool newBest = Scores::instance().recordLow(
        QStringLiteral("freecell/best_moves"), m_moves);
    refresh();

    QTimer::singleShot(200, this, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("Solved"));
        box.setText(QStringLiteral("Every card home!"));
        box.setInformativeText(
            newBest ? QStringLiteral("Moves: %1 — a new best!").arg(m_moves)
                    : QStringLiteral("Moves: %1.   Best: %2.")
                          .arg(m_moves)
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
    for (const auto& f : m_foundations)
        done += int(f.size());
    int freeCells = 0;
    for (const auto& c : m_cells)
        if (c.empty())
            ++freeCells;

    QString line = message.isEmpty()
        ? QStringLiteral("%1   Home %2/52   Free cells %3   Moves %4")
              .arg(m_won ? QStringLiteral("Solved!") : QStringLiteral("FreeCell"))
              .arg(done)
              .arg(freeCells)
              .arg(m_moves)
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
        if (m_cells[std::size_t(i)].empty())
            CardArt::paintSlot(p, r);
        else
            CardArt::paintFace(p, r, m_cells[std::size_t(i)].back());
    }

    for (int i = 0; i < 4; ++i) {
        const QRectF r = pileOrigin(PileKind::Foundation, i);
        if (m_foundations[std::size_t(i)].empty())
            CardArt::paintSlot(p, r, QStringLiteral("A"));
        else
            CardArt::paintFace(p, r, m_foundations[std::size_t(i)].back());
    }

    for (int col = 0; col < kColumns; ++col) {
        const std::vector<Card>& column = m_columns[std::size_t(col)];
        if (column.empty()) {
            CardArt::paintSlot(p, pileOrigin(PileKind::Column, col));
            continue;
        }
        const int run = orderedRunLength(col);
        for (int i = 0; i < int(column.size()); ++i) {
            const QRectF r = cardRect(col, i);
            CardArt::paintFace(p, r, column[std::size_t(i)]);
            // Show where the liftable run begins, and whether it will fit.
            if (run > 1 && i == int(column.size()) - run) {
                const bool fits = run <= maxMoveSize(false);
                CardArt::paintHighlight(p, r,
                                        fits ? QColor(0xff, 0xd5, 0x4f, 170)
                                             : QColor(0xe8, 0x51, 0x4f, 130));
            }
        }
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
        const int run = orderedRunLength(s.pile);
        const int from = int(m_columns[std::size_t(s.pile)].size()) - run;
        if (s.index < from) {
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
        std::vector<Card>& pile = pileFor(m_dragFrom.kind, m_dragFrom.pile);
        m_drag.assign(pile.begin() + m_dragFrom.index, pile.end());
        pile.erase(pile.begin() + m_dragFrom.index, pile.end());
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

    if (m_drag.size() == 1) {
        for (int i = 0; i < kCells && !placed; ++i) {
            if (pileOrigin(PileKind::Cell, i).contains(drop) && m_cells[std::size_t(i)].empty()) {
                pushUndo();
                m_cells[std::size_t(i)].push_back(m_drag.front());
                ++m_moves;
                placed = true;
            }
        }
        for (int f = 0; f < 4 && !placed; ++f) {
            if (pileOrigin(PileKind::Foundation, f).contains(drop)
                && canPlaceOnFoundation(m_drag.front(), f)) {
                pushUndo();
                m_foundations[std::size_t(f)].push_back(m_drag.front());
                ++m_moves;
                placed = true;
            }
        }
    }

    for (int col = 0; col < kColumns && !placed; ++col) {
        QRectF zone = pileOrigin(PileKind::Column, col);
        const std::vector<Card>& column = m_columns[std::size_t(col)];
        if (!column.empty())
            zone = zone.united(cardRect(col, int(column.size()) - 1));
        zone.setBottom(zone.bottom() + cardHeight() * 0.5);
        if (!zone.contains(drop) || !canStack(m_drag.front(), col))
            continue;

        const int limit = maxMoveSize(column.empty());
        if (int(m_drag.size()) > limit) {
            refusal = QStringLiteral("Only %1 card%2 can move at once — free a cell or a column.")
                          .arg(limit)
                          .arg(limit == 1 ? QString() : QStringLiteral("s"));
            break;
        }
        pushUndo();
        for (const Card& c : m_drag)
            m_columns[std::size_t(col)].push_back(c);
        ++m_moves;
        placed = true;
    }

    if (placed) {
        Sound::instance().play(Sound::kCardPlace);
    } else {
        std::vector<Card>& source = pileFor(m_dragFrom.kind, m_dragFrom.pile);
        for (const Card& c : m_drag)
            source.push_back(c);
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
    if (sendToFoundation(s.kind, s.pile)) {
        update();
        refresh();
        checkWin();
    }
}
