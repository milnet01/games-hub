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
#include <QKeyEvent>
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
    // Without it setFocus() does nothing and no key ever arrives (GHUB-0168).
    setFocusPolicy(Qt::StrongFocus);
    buildActions();
    newGame();
}

void SpiderView::buildActions()
{
    auto* newAction = new QAction(tr("New Deal"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &SpiderView::newGame);
    m_actions.append(newAction);

    m_undoAction = new QAction(tr("Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &SpiderView::undo);
    m_actions.append(m_undoAction);

    auto* deal = new QAction(tr("Deal Row"), this);
    connect(deal, &QAction::triggered, this, [this] { dealRow(); });
    m_actions.append(deal);

    auto* sep = new QAction(this);
    sep->setSeparator(true);
    m_actions.append(sep);

    auto* group = new QActionGroup(this);
    group->setExclusive(true);
    const struct { const char* name; int suits; } kModes[] = {
        { QT_TRANSLATE_NOOP("SpiderView", "1 Suit"), 1 },
        { QT_TRANSLATE_NOOP("SpiderView", "2 Suits"), 2 },
        { QT_TRANSLATE_NOOP("SpiderView", "4 Suits"), 4 },
    };
    for (const auto& mode : kModes) {
        auto* a = new QAction(tr(mode.name), this);
        // Object names, not labels: restoreState matches on these. A label is
        // what the player reads, so the Qt standard asks for tr() around it --
        // and adding it would break the match silently, leaving the toolbar
        // claiming a setting the resumed game is not playing. GHUB-0186, the
        // same shape GHUB-0154 fixed in Canasta.
        a->setObjectName(QStringLiteral("spider-suits-%1").arg(mode.suits));
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
            // deal() empties the table's hand, so a keyboard hold has gone too.
            m_keyHolding = false;
            m_cursorCol = 0;
            m_cursorDepth = 99;
            clampCursor();
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
    m_cursorCol = 0;
    m_cursorDepth = 99;
    clampCursor();
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
    clampCursor();
    m_won = false;
    m_undoAction->setEnabled(m_table.canUndo());
    update();
    refresh();
}

// The table, not the moves that made it — see KlondikeView::saveState for why
// the card games save differently from Chess.
QByteArray SpiderView::saveState() const
{
    // A run held up banked an undo snapshot as it was lifted, but nothing has
    // moved until it lands (GHUB-0069).
    const bool touched = m_table.undoDepth() > (holdingARun() ? 1u : 0u);
    if (m_won || (!touched && !m_resumed))
        return {};

    // A run lifted in mid-drag belongs to the column it came from until it is
    // dropped; closing the window while holding it must not lose the cards.
    const auto column = [this](int index) {
        std::vector<Card> cards = m_table.columns()[std::size_t(index)];
        if ((m_dragging || m_keyHolding) && m_dragFrom == index)
            cards.insert(cards.end(), m_drag.begin(), m_drag.end());
        return cards;
    };

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    // Version 2 appends the keyboard cursor, last; a version-1 save still
    // loads, with the cursor where a fresh deal puts it.
    out << quint32(2) << qint32(m_table.suits()) << qint32(m_table.completed())
        << qint32(m_table.moves());
    for (int col = 0; col < kColumns; ++col)
        cardcodec::writePile(out, column(col));
    cardcodec::writePile(out, m_table.stock());
    out << qint8(m_cursorCol) << qint8(m_cursorDepth);
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
    if ((version != 1 && version != 2) || in.status() != QDataStream::Ok
        || (suits != 1 && suits != 2 && suits != 4) || completed < 0 || completed > 8 || moves < 0)
        return false;

    std::array<std::vector<Card>, kColumns> columns;
    std::vector<Card> stock;
    if (!cardcodec::readPiles(in, columns) || !cardcodec::readPile(in, stock))
        return false;
    qint8 cursorCol = 0;
    qint8 cursorDepth = 99;
    if (version >= 2) {
        in >> cursorCol >> cursorDepth;
        if (in.status() != QDataStream::Ok || cursorCol < 0 || cursorCol > kStockStop
            || cursorDepth < -1)
            return false;
    }

    // The table decides whether this is a position the rules could have
    // produced -- Spider takes a finished run off for good, so what must come
    // back is two packs less thirteen for every run completed.
    if (!m_table.restore(columns, stock, int(suits), int(completed), int(moves)))
        return false;

    m_drag.clear();
    m_dragging = false;
    m_pressValid = false;
    m_keyHolding = false;
    m_cursorCol = cursorCol;
    m_cursorDepth = cursorDepth;
    clampCursor();
    m_won = false;
    m_undoAction->setEnabled(false);
    const QString wanted = QStringLiteral("spider-suits-%1").arg(m_table.suits()); // untranslated: an object name
    for (QAction* a : m_actions) {
        if (a->isCheckable() && a->objectName() == wanted)
            a->setChecked(true);
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
        Q_EMIT statusChanged(tr("Fill every empty column before dealing a new row."));
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

    announceLater(200, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(tr("Solved"));
        box.setText(tr("All eight runs complete!"));
        box.setInformativeText(
            newBest ? tr("Moves: %1 — a new best!").arg(m_table.moves())
                    : tr("Moves: %1.   Best: %2.")
                          .arg(m_table.moves())
                          .arg(Scores::instance().best(Scores::spiderBestMoves(m_table.suits()))));
        QAbstractButton* again = box.addButton(tr("New Deal"), QMessageBox::AcceptRole);
        box.addButton(tr("Close"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == again)
            newGame();
    });
}

void SpiderView::refresh()
{
    // The one-suit name is its own string: with no translation loaded, a %n
    // form prints its English source as written for every count.
    Q_EMIT statusChanged(tr("%1   Runs %2/8   Stock %3   Moves %4")
                             .arg(m_won                   ? tr("Solved!")
                                  : m_table.suits() == 1 ? tr("Spider (1 suit)")
                                  : tr("Spider (%n suits)", nullptr, m_table.suits()))
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

    // The keyboard's run and the cursor, over everything but the caption.
    if (!m_dragging) {
        if (m_keyHolding && !m_drag.empty()) {
            const QRectF first = heldLandingRect();
            for (int i = 0; i < int(m_drag.size()); ++i)
                CardArt::paintFace(p, first.translated(0, i * cardHeight() * 0.26),
                                   m_drag[std::size_t(i)]);
        }
        Theme::paintCellCursor(p, cursorRect(), Legibility::instance().enabled());
    }

    paintStatusCaption(p, QRectF(rect()));
}

void SpiderView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    m_pressPos = event->position();
    m_pressValid = false;

    // A run already held, by a click or by the keyboard: this press says where
    // it goes (GHUB-0069). It is the press Space makes with the cursor there,
    // so a refused column keeps the run in hand and its own column puts it
    // back. A press on bare felt puts it back too, and so does one on the
    // stock, which then deals: the stock is never a destination, and a click
    // there means "deal", whatever is in hand.
    if (m_keyHolding) {
        // A card you can see wins over the stock, as below (GHUB-0160); the
        // stock wins over a column's drop zone, which reaches under it.
        int target = -1;
        for (int col = 0; col < kColumns && target < 0; ++col) {
            for (int i = 0; i < int(m_table.columns()[std::size_t(col)].size()); ++i) {
                if (cardRect(col, i).contains(event->position())) {
                    target = col;
                    break;
                }
            }
        }
        const bool onStock = target < 0 && stockRect().contains(event->position());
        if (target < 0 && !onStock)
            target = columnAt(event->position());
        if (target >= 0) {
            m_cursorCol = target;
            m_cursorDepth = 99; // the top of the column; clamped
            pressAtCursor();
            clampCursor();
            update();
            return;
        }
        m_table.putBack();
        m_keyHolding = false;
        m_drag.clear();
        if (!onStock) {
            clampCursor();
            update();
            return;
        }
    }

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
            // The cursor follows the mouse, so the two ways of playing never
            // disagree about where you are.
            m_cursorCol = col;
            m_cursorDepth = i;
            clampCursor();
            update();
            if (!column[std::size_t(i)].faceUp || i < firstMovable)
                return; // grabbed a card that cannot move as a unit
            m_dragFrom = col;
            m_dragIndex = i;
            m_pressValid = true;
            m_dragGrab = event->position() - cardRect(col, i).topLeft();
            return;
        }
    }

    if (stockRect().contains(event->position())) {
        m_cursorCol = kStockStop;
        clampCursor();
        dealRow();
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
        // A press that never became a drag is a click, and a click on a card
        // picks it up (GHUB-0069): the press put the cursor on it, so this is
        // Space. The next click says where it goes.
        const bool click = m_pressValid;
        m_pressValid = false;
        if (click) {
            pressAtCursor();
            clampCursor();
            update();
        }
        return;
    }

    m_dragging = false;
    const int target = columnAt(event->position());

    const QPointF dragTopLeft(m_dragPos.x() - m_dragGrab.x(), m_dragPos.y() - m_dragGrab.y());
    if (dropHeldOn(target, dragTopLeft) == SpiderTable::Drop::Refused)
        m_table.putBack();
    m_undoAction->setEnabled(m_table.canUndo());

    m_drag.clear();
    m_pressValid = false;
    clampCursor();
    update();
    refresh();
    checkWin();
}

SpiderTable::Drop SpiderView::dropHeldOn(int target, QPointF runTopLeft)
{
    // What the target column holds, and where each card is sitting, BEFORE the
    // drop. A completed run is taken off inside dropOn(), so by the time it
    // reports Completed those thirteen cards no longer exist to be measured.
    std::vector<Card> preColumn;
    std::vector<QRectF> preRects;
    if (target >= 0 && target < kColumns) {
        preColumn = m_table.columns()[std::size_t(target)];
        for (int i = 0; i < int(preColumn.size()); ++i)
            preRects.push_back(cardRect(target, i));
    }
    const std::vector<Card> dragged = m_drag;

    // The view decides WHICH column the drop landed on; the table decides
    // whether the run may go there, turns over what it uncovered and takes off
    // a completed run.
    const SpiderTable::Drop result = target >= 0 && target < kColumns
        ? m_table.dropOn(target) : SpiderTable::Drop::Refused;
    if (result == SpiderTable::Drop::Refused)
        return result;

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
            rects.emplace_back(runTopLeft.x(), runTopLeft.y() + i * h * 0.26, w, h);
        }
        if (int(pile.size()) >= SpiderTable::kRunLength) {
            const std::size_t first = pile.size() - SpiderTable::kRunLength;
            launchCompletedRun({ pile.begin() + qsizetype(first), pile.end() },
                               { rects.begin() + qsizetype(first), rects.end() });
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Keyboard (GHUB-0168): the boards' scheme, as in KlondikeView. Arrows step
// between the ten columns and the stock and along the run a column can give
// up; Space lifts, Space drops, Escape puts back.
// ---------------------------------------------------------------------------

void SpiderView::clampCursor()
{
    m_cursorCol = std::clamp(m_cursorCol, 0, int(kStockStop));
    if (m_cursorCol == kStockStop) {
        m_cursorDepth = -1;
        return;
    }
    const std::vector<Card>& column = m_table.columns()[std::size_t(m_cursorCol)];
    if (column.empty()) {
        m_cursorDepth = 0;
        return;
    }
    const int last = int(column.size()) - 1;
    // Only the same-suit run at the foot of a column comes away, so that is
    // the whole range the cursor can stand on -- and while a run is held the
    // cursor points at a column, so it sits on the top.
    const int firstMovable = int(column.size()) - std::max(1, movableRunLength(m_cursorCol));
    m_cursorDepth = m_keyHolding ? last : std::clamp(m_cursorDepth, firstMovable, last);
}

int SpiderView::columnAt(QPointF pos) const
{
    for (int col = 0; col < kColumns; ++col) {
        QRectF zone = columnOrigin(col);
        const std::vector<Card>& column = m_table.columns()[std::size_t(col)];
        if (!column.empty())
            zone = zone.united(cardRect(col, int(column.size()) - 1));
        zone.setBottom(zone.bottom() + cardHeight() * 0.5);
        if (zone.contains(pos))
            return col;
    }
    return -1;
}

QRectF SpiderView::cursorRect() const
{
    if (m_keyHolding && !m_drag.empty()) {
        const QRectF first = heldLandingRect();
        return first.united(first.translated(0, (int(m_drag.size()) - 1) * cardHeight() * 0.26));
    }
    if (m_cursorCol == kStockStop)
        return stockRect();
    const std::vector<Card>& column = m_table.columns()[std::size_t(m_cursorCol)];
    return column.empty()
        ? columnOrigin(m_cursorCol)
        // The card and everything under it: what Space would lift.
        : cardRect(m_cursorCol, m_cursorDepth).united(cardRect(m_cursorCol, int(column.size()) - 1));
}

QRectF SpiderView::heldLandingRect() const
{
    QRectF r = m_cursorCol == kStockStop ? stockRect() : columnOrigin(m_cursorCol);
    if (m_cursorCol < kStockStop) {
        const std::vector<Card>& column = m_table.columns()[std::size_t(m_cursorCol)];
        if (!column.empty())
            r = cardRect(m_cursorCol, int(column.size()) - 1)
                    .translated(0, cardHeight() * 0.26 * fanScale(column));
    }
    // Raised off the column, so it reads as held rather than as played.
    return r.translated(cardWidth() * 0.10, -cardHeight() * 0.06);
}

void SpiderView::pressAtCursor()
{
    if (m_keyHolding) {
        dropAtCursor();
        return;
    }
    if (m_cursorCol == kStockStop) {
        dealRow();
        return;
    }
    const std::vector<Card>& column = m_table.columns()[std::size_t(m_cursorCol)];
    if (column.empty() || !column[std::size_t(m_cursorDepth)].faceUp)
        return;
    // Banks the undo snapshot before the cards leave, as a drag does.
    m_drag = m_table.lift(m_cursorCol, m_cursorDepth);
    if (m_drag.empty())
        return;
    m_dragFrom = m_cursorCol;
    m_dragIndex = m_cursorDepth;
    m_keyHolding = true;
}

void SpiderView::dropAtCursor()
{
    if (m_cursorCol == m_dragFrom) {
        m_table.putBack();
        m_keyHolding = false;
        m_drag.clear();
        return;
    }
    const int dropped = int(m_drag.size());
    const SpiderTable::Drop result = dropHeldOn(m_cursorCol, heldLandingRect().topLeft());
    // Not a legal home: keep holding, so the player can try another column.
    if (result == SpiderTable::Drop::Refused)
        return;

    m_keyHolding = false;
    m_drag.clear();
    // Onto the first card of the run it dropped, so Space can pick it up
    // again. A completed run has left, and the clamp finds what is there now.
    m_cursorDepth = int(m_table.columns()[std::size_t(m_cursorCol)].size()) - dropped;
    m_undoAction->setEnabled(m_table.canUndo());
    refresh();
    checkWin();
}

void SpiderView::keyPressEvent(QKeyEvent* event)
{
    if (m_dragging) {
        GameView::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Left:
        m_cursorCol = std::max(0, m_cursorCol - 1);
        m_cursorDepth = 99; // the top of the new column; clamped
        break;
    case Qt::Key_Right:
        m_cursorCol = std::min(int(kStockStop), m_cursorCol + 1);
        m_cursorDepth = 99;
        break;
    case Qt::Key_Up:
        if (!m_keyHolding && m_cursorCol != kStockStop)
            --m_cursorDepth;
        break;
    case Qt::Key_Down:
        if (!m_keyHolding && m_cursorCol != kStockStop)
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
        m_table.putBack();
        m_keyHolding = false;
        m_drag.clear();
        break;
    default:
        GameView::keyPressEvent(event);
        return;
    }

    clampCursor();
    update();
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
    m_keyHolding = false;
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
