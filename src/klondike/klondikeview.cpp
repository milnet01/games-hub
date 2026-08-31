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
    m_resumed = false;
    m_table.deal();
    Sound::instance().play(Sound::kShuffle);
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
    m_table.undo();
    m_drag.clear();
    m_dragging = false;
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
    if (m_won || (!m_table.canUndo() && !m_resumed))
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
    out << quint32(1) << qint32(m_table.drawCount()) << qint32(m_table.score());
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

    // The table decides whether this is a position the rules could have
    // produced -- the whole pack back, because Klondike never takes a card out
    // of play.
    if (!m_table.restore(stock, waste, foundations, tableau, int(draw), int(score)))
        return false;

    m_drag.clear();
    m_dragging = false;
    m_pressValid = false;
    m_won = false;
    m_resumed = true;
    m_undoAction->setEnabled(false);
    for (QAction* a : m_actions) {
        if (a->isCheckable())
            a->setChecked(a->text() == QStringLiteral("Draw %1").arg(m_table.drawCount()));
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
    QTimer::singleShot(200, this, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("Solved"));
        box.setText(QStringLiteral("You cleared the table!"));
        box.setInformativeText(
            newBest ? QStringLiteral("Score: %1 — a new best!").arg(m_table.score())
                    : QStringLiteral("Score: %1.   Best: %2.")
                          .arg(m_table.score())
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
    for (const auto& f : m_table.foundations())
        done += int(f.size());

    QString line = QStringLiteral("%1   Foundations %2/52   Stock %3   Score %4")
                       .arg(m_won ? QStringLiteral("Solved!") : QStringLiteral("Klondike"))
                       .arg(done)
                       .arg(m_table.stock().size())
                       .arg(m_table.score());
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
            CardArt::paintSlot(p, r, QStringLiteral("A"));
        else
            CardArt::paintFace(p, r, pile[shown - 1]);
    }

    for (int col = 0; col < 7; ++col) {
        const std::vector<Card>& column = m_table.tableau()[std::size_t(col)];
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
        m_pressValid = false;
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

    // Where the card is standing, and which foundations hold what, BEFORE the
    // move: a successful send does not report where the card went, and once it
    // has gone there is nothing left at the old address to measure.
    const std::vector<Card>& source = pileFor(s.kind, s.pile);
    if (source.empty())
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

void KlondikeView::deactivate()
{
    // A card in the air carries a destination captured when it left, and the
    // hub may resize this page while it is away. Landing them now is both
    // correct and what the model already believes.
    m_flights.clear();
    if (m_flightTimer != nullptr)
        m_flightTimer->stop();
}

void KlondikeView::applyLegibility(bool enabled)
{
    // Land them where the model already believes they are, then let the base
    // re-lay-out. Keeping them would put a card down at its old destination.
    m_flights.clear();
    if (m_flightTimer != nullptr)
        m_flightTimer->stop();
    GameView::applyLegibility(enabled);
}
