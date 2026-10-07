#include "klondikeview.h"
#include "klondike/klondiketable.h"

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

namespace {
constexpr double kMargin = 14.0;

// A drag only starts once the pointer has actually moved, so a click that
// happens to wobble still counts as a click.
constexpr double kDragThreshold = 4.0;
}

KlondikeView::KlondikeView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(KlondikeView::minimumSizeHint());
    setMouseTracking(true);
    // Without it setFocus() does nothing and no key ever arrives (GHUB-0168).
    setFocusPolicy(Qt::StrongFocus);
    buildActions();
    newGame();
}

void KlondikeView::buildActions()
{
    auto* newAction = new QAction(tr("New Deal"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &KlondikeView::newGame);
    m_actions.append(newAction);

    m_undoAction = new QAction(tr("Undo"), this);
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
        auto* a = new QAction(tr("Draw %1").arg(n), this);
        // Object names, not labels: restoreState matches on these. A label is
        // what the player reads, so the Qt standard asks for tr() around it --
        // and adding it would break the match silently, leaving the toolbar
        // claiming a setting the resumed game is not playing. GHUB-0186, the
        // same shape GHUB-0154 fixed in Canasta.
        a->setObjectName(QStringLiteral("klondike-draw-%1").arg(n));
        a->setCheckable(true);
        a->setChecked(n == m_table.drawCount());
        group->addAction(a);
        connect(a, &QAction::triggered, this, [this, n] {
            m_table.setDrawCount(n);
            newGame();
        });
        m_actions.append(a);
    }
}

void KlondikeView::newGame()
{
    // Before the deal, not after: settling puts a held run back on the table
    // it came from, and after a deal that is a fresh table it never left.
    settleForChange();
    m_resumed = false;
    m_table.deal();
    Sound::instance().play(Sound::kShuffle);
    m_cursorCol = 0;
    m_cursorDepth = -1;
    m_drag.clear();
    m_dragging = false;
    m_won = false;
    m_undoAction->setEnabled(false);

    update();
    refresh();
}

void KlondikeView::activate()
{
    refresh();
}

void KlondikeView::undo()
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
    // A run held up banked an undo snapshot as it was lifted, but nothing has
    // moved until it lands (GHUB-0069).
    const bool touched = m_table.undoDepth() > (holdingARun() ? 1u : 0u);
    if (m_won || (!touched && !m_resumed))
        return {};

    // A run lifted in mid-drag has been erased from its pile and is held until
    // it is dropped. Closing the window at that moment must not lose those
    // cards, so they go back onto the pile they came from.
    const auto pile = [this](PileKind kind, int index) {
        std::vector<Card> cards = pileFor(kind, index);
        if ((m_dragging || m_keyHolding) && m_dragFrom.kind == kind && m_dragFrom.pile == index)
            cards.insert(cards.end(), m_drag.begin(), m_drag.end());
        return cards;
    };

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    // Version 2 appends the keyboard cursor, last, so the render of a resumed
    // deal matches the one that was saved; a version-1 save still loads, with
    // the cursor where a fresh deal puts it.
    out << quint32(2) << qint32(m_table.drawCount()) << qint32(m_table.score());
    cardcodec::writePile(out, pile(PileKind::Stock, 0));
    cardcodec::writePile(out, pile(PileKind::Waste, 0));
    for (int f = 0; f < 4; ++f)
        cardcodec::writePile(out, pile(PileKind::Foundation, f));
    for (int col = 0; col < 7; ++col)
        cardcodec::writePile(out, pile(PileKind::Tableau, col));
    out << qint8(m_cursorCol) << qint8(m_cursorDepth);
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
    if ((version != 1 && version != 2) || in.status() != QDataStream::Ok || (draw != 1 && draw != 3) || score < 0)
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
    qint8 cursorCol = 0;
    qint8 cursorDepth = -1;
    if (version >= 2) {
        in >> cursorCol >> cursorDepth;
        if (in.status() != QDataStream::Ok || cursorCol < 0 || cursorCol > 6 || cursorDepth < -1)
            return false;
    }

    // The table decides whether this is a position the rules could have
    // produced -- the whole pack back, because Klondike never takes a card out
    // of play.
    if (!m_table.restore(stock, waste, foundations, tableau, int(draw), int(score)))
        return false;

    m_drag.clear();
    m_dragging = false;
    m_pressValid = false;
    m_keyHolding = false;
    m_won = false;
    m_resumed = true;
    m_cursorCol = cursorCol;
    m_cursorDepth = cursorDepth;
    clampCursor();
    m_undoAction->setEnabled(false);
    const QString wanted = QStringLiteral("klondike-draw-%1").arg(m_table.drawCount()); // untranslated: an object name
    for (QAction* a : m_actions) {
        if (a->isCheckable() && a->objectName() == wanted)
            a->setChecked(true);
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
    //
    // What the height must hold, in card heights: the stock/waste/foundation
    // row, the gap under it, then the longest column the deal makes — six
    // face-down steps and one whole card. The figure used to be a flat 2.6,
    // which a fresh deal cleared by about six pixels at the smallest window
    // and not at all once the window went wide and short.
    constexpr double kHeightCost = 1.0 + kGapRatio * kHeaderGap / 1.4
        + (kLongestDealtColumn - 1) * kFaceDownStep + 1.0;
    const double byHeight =
        (height() - 2 * kMargin - captionBand(QRectF(rect()))) / (1.4 * kHeightCost);
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
        return { kMargin + step * pile, kMargin + h + gap() * kHeaderGap, w, h };
    }
    return {};
}

// Face-down cards in a column are packed tighter than face-up ones, which is
// what lets a long column still fit on screen.
double KlondikeView::fanStep(const std::vector<Card>& pile, int index) const
{
    return pile[std::size_t(index)].faceUp ? cardHeight() * kFaceUpStep
                                          : cardHeight() * kFaceDownStep;
}

double KlondikeView::deepestColumnBottom() const
{
    double deepest = 0.0;
    for (int col = 0; col < 7; ++col) {
        const std::vector<Card>& column = m_table.tableau()[std::size_t(col)];
        if (!column.empty())
            deepest = std::max(deepest,
                               cardRect(PileKind::Tableau, col, int(column.size()) - 1).bottom());
    }
    return deepest;
}

double KlondikeView::roomForColumns() const
{
    return height() - kMargin - captionBand(QRectF(rect()));
}

QRectF KlondikeView::cardRect(PileKind kind, int pile, int index) const
{
    QRectF r = pileOrigin(kind, pile);
    if (kind != PileKind::Tableau)
        return r;

    const std::vector<Card>& column = pileFor(kind, pile);
    const double scale = fanScale(column);
    double y = r.top();
    for (int i = 0; i < index && i < int(column.size()); ++i)
        y += fanStep(column, i) * scale;
    r.moveTop(y);
    return r;
}

// cardWidth() sizes the card for the DEAL -- six face-down steps and one whole
// card. Play grows a column past that: turning a face-down card more than
// doubles its step, and a king with its run onto an emptied column is routine.
// Sizing for the worst case instead would take width off every card at every
// window whether or not any column ever grew, and this game is read by pip
// pattern, so that trade is the wrong way round (GHUB-0089).
//
// So the cards keep their size and an overlong column tightens. FreeCell caps
// one step because its fan is uniform; Klondike's is not -- a face-down step
// and a face-up step differ by more than double -- so scaling the whole column
// is what keeps their RATIO, which is the thing that says at a glance how much
// of a column is still to turn.
double KlondikeView::fanScale(const std::vector<Card>& column) const
{
    if (column.size() < 2)
        return 1.0;
    double natural = 0.0;
    for (int i = 0; i < int(column.size()) - 1; ++i)
        natural += fanStep(column, i);
    if (natural <= 0.0)
        return 1.0;

    const double room =
        roomForColumns() - pileOrigin(PileKind::Tableau, 0).top() - cardHeight();
    if (natural <= room)
        return 1.0;
    // Never to nothing: a column stacked into one place is unreadable in a
    // different way, so at least a pixel per card survives. That floor can
    // still overflow at an absurd window shape, which is the same trade
    // FreeCell's own floor makes.
    return std::max(room, double(column.size() - 1)) / natural;
}

KlondikeView::Spot KlondikeView::hitTest(QPointF pos) const
{
    // Tableau first and from the bottom of each column up, so the card drawn
    // on top is the one that gets picked.
    for (int col = 0; col < 7; ++col) {
        const std::vector<Card>& column = m_table.tableau()[std::size_t(col)];
        for (int i = int(column.size()) - 1; i >= 0; --i) {
            if (cardRect(PileKind::Tableau, col, i).contains(pos))
                return { PileKind::Tableau, col, i, true };
        }
        if (column.empty() && pileOrigin(PileKind::Tableau, col).contains(pos))
            return { PileKind::Tableau, col, -1, true };
    }

    for (int f = 0; f < 4; ++f) {
        if (pileOrigin(PileKind::Foundation, f).contains(pos))
            return { PileKind::Foundation, f, int(m_table.foundations()[std::size_t(f)].size()) - 1, true };
    }

    if (pileOrigin(PileKind::Waste, 0).contains(pos))
        return { PileKind::Waste, 0, int(m_table.waste().size()) - 1, true };

    if (pileOrigin(PileKind::Stock, 0).contains(pos))
        return { PileKind::Stock, 0, int(m_table.stock().size()) - 1, true };

    return {};
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------

void KlondikeView::dealFromStock()
{
    m_table.dealFromStock();
    m_undoAction->setEnabled(m_table.canUndo());
    Sound::instance().play(Sound::kCardDeal);
    update();
    refresh();
}

void KlondikeView::checkWin()
{
    int total = 0;
    for (const auto& f : m_table.foundations())
        total += int(f.size());
    if (total != 52 || m_won)
        return;

    m_won = true;
    Sound::instance().play(Sound::kWin);
    const bool newBest = Scores::instance().recordHigh(Scores::klondikeBestScore(), m_table.score());
    refresh();
    announceLater(200, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(tr("Solved"));
        box.setText(tr("You cleared the table!"));
        box.setInformativeText(
            newBest ? tr("Score: %1 — a new best!").arg(m_table.score())
                    : tr("Score: %1.   Best: %2.")
                          .arg(m_table.score())
                          .arg(Scores::instance().best(Scores::klondikeBestScore())));
        QAbstractButton* again = box.addButton(tr("New Deal"), QMessageBox::AcceptRole);
        box.addButton(tr("Close"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == again)
            newGame();
    });
}

void KlondikeView::refresh()
{
    int done = 0;
    for (const auto& f : m_table.foundations())
        done += int(f.size());

    QString line = tr("%1   Foundations %2/52   Stock %3   Score %4")
                       .arg(m_won ? tr("Solved!") : tr("Klondike"))
                       .arg(done)
                       .arg(m_table.stock().size())
                       .arg(m_table.score());
    if (Scores::instance().has(Scores::klondikeBestScore()))
        line = tr("%1   Best %2").arg(line).arg(Scores::instance().best(Scores::klondikeBestScore()));
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
    if (m_table.stock().empty())
        CardArt::paintSlot(p, pileOrigin(PileKind::Stock, 0), QStringLiteral("↻"));
    else
        CardArt::paintBack(p, pileOrigin(PileKind::Stock, 0));

    if (m_table.waste().empty())
        CardArt::paintSlot(p, pileOrigin(PileKind::Waste, 0));
    else
        CardArt::paintFace(p, pileOrigin(PileKind::Waste, 0), m_table.waste().back());

    // One match per flight, so two identical cards in the air do not both
    // suppress the same destination copy. Reset every repaint.
    m_flightConsumed.assign(m_flights.size(), 0);

    for (int f = 0; f < 4; ++f) {
        const QRectF r = pileOrigin(PileKind::Foundation, f);
        const std::vector<Card>& pile = m_table.foundations()[std::size_t(f)];
        // A foundation shows its top card only, so a card still on its way here
        // is drawn one rank down until it arrives -- otherwise it is on screen
        // twice and the journey is a lie.
        std::size_t shown = pile.size();
        if (shown > 0 && cardflight::suppressAt(m_flights, m_flightConsumed, f, pile.back()))
            --shown;
        if (shown == 0)
            CardArt::paintSlot(p, r, rankLabel(kAce));
        else
            CardArt::paintFace(p, r, pile[shown - 1]);
    }

    for (int col = 0; col < 7; ++col) {
        const std::vector<Card>& column = m_table.tableau()[std::size_t(col)];
        if (column.empty()) {
            CardArt::paintSlot(p, pileOrigin(PileKind::Tableau, col), rankLabel(kKing));
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

    // Cards on their way home, above the table and below the hand.
    for (const cardflight::Flight& f : m_flights) {
        const QPointF at = cardflight::positionOf(f);
        CardArt::paintFace(p, QRectF(at, QSizeF(cardWidth(), cardHeight())), f.card);
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

    // The keyboard's run and the cursor, over everything but the caption: the
    // cursor says where the next Space lands, so nothing may sit on top of it.
    if (!m_dragging) {
        if (m_keyHolding && !m_drag.empty()) {
            const QRectF first = heldLandingRect();
            for (int i = 0; i < int(m_drag.size()); ++i)
                CardArt::paintFace(p, first.translated(0, i * cardHeight() * kFaceUpStep),
                                   m_drag[std::size_t(i)]);
        }
        Theme::paintCellCursor(p, cursorRect(), Legibility::instance().enabled());
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

    // A run already held, by a click or by the keyboard: this press says where
    // it goes (GHUB-0069). It is the press Space makes with the cursor there,
    // so a refused pile keeps the run in hand and its own pile puts it back.
    // A press on bare felt puts it back too, and so does one on the stock,
    // which then deals: the stock is never a destination, and a click there
    // means "deal", whatever is in hand.
    if (m_keyHolding) {
        if (s.valid && s.kind != PileKind::Stock) {
            moveCursorTo(s);
            pressAtCursor();
            clampCursor();
            update();
            return;
        }
        m_table.putBack();
        m_keyHolding = false;
        m_drag.clear();
        if (!s.valid) {
            clampCursor();
            update();
            return;
        }
    }

    if (!s.valid)
        return;
    moveCursorTo(s);
    update();

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

        // The table lifts, and banks the undo snapshot BEFORE the cards leave
        // their pile. Snapshotting at drop time -- which is what this used to
        // do -- takes a picture of a table the cards have already left, so
        // undoing a finished move loses them (GHUB-0126, measured in FreeCell).
        m_drag = m_table.lift(m_dragFrom.kind, m_dragFrom.pile, m_dragFrom.index);
        if (m_drag.empty())
            return;
        m_dragging = true;
    }

    m_dragPos = event->position();
    update();
}

void KlondikeView::mouseReleaseEvent(QMouseEvent* event)
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
    const QPointF drop = event->position();
    bool placed = false;

    // A single card may go to a foundation; any run may go to a tableau.
    // The view decides WHICH pile the drop landed on; the table decides
    // whether the cards may go there, and turns over whatever they uncovered.
    if (m_drag.size() == 1) {
        for (int f = 0; f < 4 && !placed; ++f) {
            if (pileOrigin(PileKind::Foundation, f).contains(drop))
                placed = m_table.dropOnFoundation(f);
        }
    }

    for (int col = 0; col < 7 && !placed; ++col) {
        // Accept a drop anywhere in the column's vertical run, not just on the
        // top card, which is far more forgiving to aim at.
        QRectF zone = pileOrigin(PileKind::Tableau, col);
        const std::vector<Card>& column = m_table.tableau()[std::size_t(col)];
        if (!column.empty())
            zone = zone.united(cardRect(PileKind::Tableau, col, int(column.size()) - 1));
        zone.setBottom(zone.bottom() + cardHeight() * 0.5);

        if (zone.contains(drop))
            placed = m_table.dropOnTableau(col);
    }

    if (placed) {
        Sound::instance().play(Sound::kCardPlace);
    } else {
        // Nothing happened, so the table takes the cards back and drops the
        // snapshot it banked when they were lifted.
        m_table.putBack();
    }
    m_undoAction->setEnabled(m_table.canUndo());

    m_drag.clear();
    m_pressValid = false;
    clampCursor();
    update();
    refresh();
    checkWin();
}

void KlondikeView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    // The first click of the pair picked the card up (GHUB-0069). Put it back
    // before looking, or the pile's top is the card beneath it and the send
    // below plays a card nobody pointed at.
    if (m_keyHolding) {
        m_table.putBack();
        m_keyHolding = false;
        m_drag.clear();
    }

    const Spot s = hitTest(event->position());
    if (!s.valid || s.kind == PileKind::Stock || s.index < 0)
        return;

    // Where the card is standing, and which foundations hold what, BEFORE the
    // move: a successful send does not report where the card went, and once it
    // has gone there is nothing left at the old address to measure.
    const std::vector<Card>& source = pileFor(s.kind, s.pile);
    if (source.empty())
        return;
    // sendToFoundation moves the pile's TOP card, so acting on a click that
    // landed anywhere else plays a card the player did not point at
    // (GHUB-0160). A buried card is not a move; it is a miss.
    if (s.index != int(source.size()) - 1)
        return;
    const Card moving = source.back();
    const QRectF fromRect = cardRect(s.kind, s.pile, int(source.size()) - 1);
    std::array<std::size_t, 4> before {};
    for (int f = 0; f < 4; ++f)
        before[std::size_t(f)] = m_table.foundations()[std::size_t(f)].size();

    if (m_table.sendToFoundation(s.kind, s.pile)) {
        launchToFoundation(moving, fromRect, grownFoundation(before));
        update();
        refresh();
        checkWin();
    }
}

int KlondikeView::grownFoundation(const std::array<std::size_t, 4>& before) const
{
    for (int f = 0; f < 4; ++f) {
        if (m_table.foundations()[std::size_t(f)].size() > before[std::size_t(f)])
            return f;
    }
    return -1;
}

void KlondikeView::launchToFoundation(const Card& card, QRectF fromRect, int foundation)
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
        // 16ms is the frame budget the bench in gameshub_uitest measures
        // against; Klondike's full tableau costs a small fraction of it.
        m_flightTimer->setInterval(16);
        connect(m_flightTimer, &QTimer::timeout, this, [this] {
            if (!cardflight::advance(m_flights, 0.016))
                m_flightTimer->stop();
            update();
        });
    }
    m_flightTimer->start();
}

// ---------------------------------------------------------------------------
// Keyboard (GHUB-0168): the same scheme as the boards. Arrows move between
// piles and up and down a column's face-up cards, Space lifts and Space drops,
// Escape puts a lifted run back.
// ---------------------------------------------------------------------------

KlondikeView::Spot KlondikeView::cursorPile() const
{
    if (m_cursorDepth < 0) {
        if (m_cursorCol == 0)
            return { PileKind::Stock, 0, int(m_table.stock().size()) - 1, true };
        if (m_cursorCol <= 2)
            return { PileKind::Waste, 0, int(m_table.waste().size()) - 1, true };
        const int f = m_cursorCol - 3;
        return { PileKind::Foundation, f, int(m_table.foundations()[std::size_t(f)].size()) - 1, true };
    }
    const std::vector<Card>& column = m_table.tableau()[std::size_t(m_cursorCol)];
    return { PileKind::Tableau, m_cursorCol, column.empty() ? -1 : m_cursorDepth, true };
}

void KlondikeView::clampCursor()
{
    m_cursorCol = std::clamp(m_cursorCol, 0, 6);
    if (m_cursorDepth < 0) {
        // The top row has no pile above the third column.
        if (m_cursorCol == 2)
            m_cursorCol = 1;
        m_cursorDepth = -1;
        return;
    }
    const std::vector<Card>& column = m_table.tableau()[std::size_t(m_cursorCol)];
    if (column.empty()) {
        m_cursorDepth = 0;
        return;
    }
    const int last = int(column.size()) - 1;
    int firstUp = last;
    while (firstUp > 0 && column[std::size_t(firstUp - 1)].faceUp)
        --firstUp;
    // While a run is held the cursor points at a PILE, so it sits on the top.
    m_cursorDepth = m_keyHolding ? last : std::clamp(m_cursorDepth, firstUp, last);
}

void KlondikeView::moveCursorTo(const Spot& s)
{
    if (s.kind == PileKind::Tableau) {
        m_cursorCol = s.pile;
        m_cursorDepth = std::max(0, s.index);
    } else {
        m_cursorCol = s.kind == PileKind::Stock ? 0 : s.kind == PileKind::Waste ? 1 : 3 + s.pile;
        m_cursorDepth = -1;
    }
    clampCursor();
}

QRectF KlondikeView::cursorRect() const
{
    if (m_keyHolding && !m_drag.empty()) {
        const QRectF first = heldLandingRect();
        return first.united(
            first.translated(0, (int(m_drag.size()) - 1) * cardHeight() * kFaceUpStep));
    }
    const Spot s = cursorPile();
    if (s.kind != PileKind::Tableau || s.index < 0)
        return pileOrigin(s.kind, s.pile);
    // The card and everything under it: what Space would lift.
    const int last = int(pileFor(s.kind, s.pile).size()) - 1;
    return cardRect(s.kind, s.pile, s.index).united(cardRect(s.kind, s.pile, last));
}

QRectF KlondikeView::heldLandingRect() const
{
    const Spot s = cursorPile();
    QRectF r = pileOrigin(s.kind, s.pile);
    if (s.kind == PileKind::Tableau && s.index >= 0) {
        const std::vector<Card>& column = pileFor(s.kind, s.pile);
        r = cardRect(s.kind, s.pile, int(column.size()) - 1)
                .translated(0, cardHeight() * kFaceUpStep * fanScale(column));
    }
    // Raised off the pile, so it reads as held rather than as played.
    return r.translated(cardWidth() * 0.10, -cardHeight() * 0.06);
}

void KlondikeView::pressAtCursor()
{
    if (m_keyHolding) {
        dropAtCursor();
        return;
    }
    const Spot s = cursorPile();
    if (s.kind == PileKind::Stock) {
        dealFromStock();
        return;
    }
    if (s.index < 0)
        return;
    const std::vector<Card>& pile = pileFor(s.kind, s.pile);
    if (!pile[std::size_t(s.index)].faceUp)
        return;
    // Banks the undo snapshot before the cards leave, as a drag does.
    m_drag = m_table.lift(s.kind, s.pile, s.index);
    if (m_drag.empty())
        return;
    m_dragFrom = s;
    m_keyHolding = true;
}

void KlondikeView::dropAtCursor()
{
    const Spot s = cursorPile();
    if (s.kind == m_dragFrom.kind && s.pile == m_dragFrom.pile) {
        m_table.putBack();
        m_keyHolding = false;
        m_drag.clear();
        return;
    }

    bool placed = false;
    if (s.kind == PileKind::Foundation && m_drag.size() == 1)
        placed = m_table.dropOnFoundation(s.pile);
    else if (s.kind == PileKind::Tableau)
        placed = m_table.dropOnTableau(s.pile);
    // Not a legal home: keep holding, so the player can try another pile.
    if (!placed)
        return;

    Sound::instance().play(Sound::kCardPlace);
    // Onto the first card of the run it dropped, so Space can pick it up again.
    if (s.kind == PileKind::Tableau)
        m_cursorDepth = int(pileFor(s.kind, s.pile).size() - m_drag.size());
    m_keyHolding = false;
    m_drag.clear();
    m_undoAction->setEnabled(m_table.canUndo());
    refresh();
    checkWin();
}

void KlondikeView::keyPressEvent(QKeyEvent* event)
{
    if (m_dragging) {
        GameView::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Left:
        m_cursorCol = std::max(0, m_cursorCol - 1);
        break;
    case Qt::Key_Right:
        m_cursorCol = std::min(6, m_cursorCol + 1);
        if (m_cursorDepth < 0 && m_cursorCol == 2)
            m_cursorCol = 3;
        break;
    case Qt::Key_Up: {
        if (m_cursorDepth < 0)
            break;
        const std::vector<Card>& column = m_table.tableau()[std::size_t(m_cursorCol)];
        const bool canClimb = !m_keyHolding && m_cursorDepth > 0 && !column.empty()
            && column[std::size_t(m_cursorDepth - 1)].faceUp;
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

void KlondikeView::settleForChange()
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

void KlondikeView::deactivate()
{
    settleForChange();
}

void KlondikeView::applyLegibility(bool enabled)
{
    // The band comes off the height this view solves its card size from, so
    // every rect on the surface moves.
    settleForChange();
    GameView::applyLegibility(enabled);
}
