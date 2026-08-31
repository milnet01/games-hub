#include "pyramidview.h"
#include "pyramid/pyramidtable.h"

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

namespace {
constexpr int kMaxRedeals = 2;
// The margin and the fan step are shared by the layout and by the height budget
// that sizes the card. They were two independent literals until the budget's
// 0.52 drifted from the painter's 0.46-of-a-card-height, and the stock and waste
// were drawn over the bottom row at every window size.
constexpr double kMargin = 16.0;
constexpr double kFanStep = 0.46;
const QString kBestKey = QStringLiteral("pyramid/best_pairs");
}

PyramidView::PyramidView(QWidget* parent)
    : GameView(parent)
{
    setMinimumSize(minimumSizeHint());
    buildActions();
    newGame();
}

void PyramidView::buildActions()
{
    auto* newAction = new QAction(QStringLiteral("New Deal"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &PyramidView::newGame);
    m_actions.append(newAction);

    m_undoAction = new QAction(QStringLiteral("Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &PyramidView::undo);
    m_actions.append(m_undoAction);
}

void PyramidView::newGame()
{
    m_resumed = false;
    m_table.deal();
    Sound::instance().play(Sound::kShuffle);
    clearSelection();
    m_won = false;
    m_announced = false;
    m_undoAction->setEnabled(false);

    update();
    refresh();
}

void PyramidView::activate()
{
    refresh();
}

void PyramidView::undo()
{
    if (!m_table.canUndo())
        return;
    m_table.undo();
    m_won = false;
    m_announced = false;
    clearSelection();
    m_undoAction->setEnabled(m_table.canUndo());
    update();
    refresh();
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

// The table, not the moves that made it — see KlondikeView::saveState for why
// the card games save differently from Chess. Pyramid has no drag to fold back
// in: cards are taken by clicking, so nothing is ever in mid-air.
QByteArray PyramidView::saveState() const
{
    if (m_won || (!m_table.canUndo() && !m_resumed))
        return {};

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1) << qint32(m_table.pairs()) << qint32(m_table.redeals())
        << qint32(m_table.pyramid().size());
    // A taken pyramid card keeps its slot and is simply marked gone, because the
    // slot above it still needs to know both its supports have been cleared.
    for (const Slot& s : m_table.pyramid()) {
        cardcodec::writeCard(out, s.card);
        out << qint8(s.removed ? 1 : 0);
    }
    cardcodec::writePile(out, m_table.stock());
    cardcodec::writePile(out, m_table.waste());
    return blob;
}

bool PyramidView::restoreState(const QByteArray& blob)
{
    constexpr int kPyramidCards = PyramidTable::kPyramidCards;

    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    qint32 pairs = 0;
    qint32 redeals = 0;
    // Not `slots`: that is a Qt keyword macro and expands to nothing, leaving an
    // error that points at the `=` (CLAUDE.md § Traps worth knowing).
    qint32 slotCount = 0;
    in >> version >> pairs >> redeals >> slotCount;
    if (version != 1 || in.status() != QDataStream::Ok || pairs < 0 || pairs > 52 || redeals < 0
        || redeals > PyramidTable::kMaxRedeals || slotCount != kPyramidCards)
        return false;

    std::vector<Slot> pyramid;
    pyramid.reserve(kPyramidCards);
    for (int i = 0; i < kPyramidCards; ++i) {
        Card c;
        if (!cardcodec::readCard(in, c))
            return false;
        qint8 removed = 0;
        in >> removed;
        if (in.status() != QDataStream::Ok)
            return false;
        pyramid.push_back({ c, removed != 0 });
    }

    std::vector<Card> stock;
    std::vector<Card> waste;
    if (!cardcodec::readPile(in, stock) || !cardcodec::readPile(in, waste))
        return false;

    // The table decides whether this is a position the rules could have
    // produced -- the pack check lives with the rules, not with the reader.
    if (!m_table.restore(pyramid, stock, waste, int(pairs), int(redeals)))
        return false;

    m_won = false;
    m_announced = false;
    m_resumed = true;
    clearSelection();
    m_undoAction->setEnabled(false);
    update();
    refresh();
    return true;
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

double PyramidView::cardWidth() const
{
    // The widest row is seven cards, overlapping by half a card each side.
    const double byWidth = (width() - 40.0) / (kRows * 0.62 + 0.4) / 1.6;
    // What the height must hold, in card heights: six fan steps down the
    // pyramid, the bottom row's whole card, and the stock/waste row that
    // pileTop() anchors to the bottom. kFanStep is pyramidRect()'s own step, so
    // the budget and the painter cannot drift apart again. The caption's strip
    // comes off first, because pileTop() sits above it.
    constexpr double kHeightCost = (kRows - 1) * kFanStep + 1.0 + 1.0;
    const double byHeight =
        (height() - 2 * kMargin - captionBand(QRectF(rect()))) / (1.4 * kHeightCost);
    return std::max(30.0, std::min(byWidth * 1.6, byHeight));
}

QRectF PyramidView::pyramidRect(int row, int index) const
{
    const double w = cardWidth();
    const double h = cardHeight();
    const double stepX = w * 0.56;
    const double stepY = h * kFanStep;
    const double rowWidth = stepX * row + w;
    const double left = (width() - rowWidth) / 2 + index * stepX;
    return { left, kMargin + row * stepY, w, h };
}

QRectF PyramidView::stockRect() const
{
    const double w = cardWidth();
    return { width() / 2.0 - w * 1.25, pileTop(), w, cardHeight() };
}

QRectF PyramidView::wasteRect() const
{
    const double w = cardWidth();
    return { width() / 2.0 + w * 0.25, pileTop(), w, cardHeight() };
}

// The caption's plate is opaque and painted last, so a pile anchored to the
// bottom of the WIDGET is drawn and then covered. cardWidth() already takes
// the band off the height it sizes against; the anchor has to take it off too.
double PyramidView::pileTop() const
{
    return height() - captionBand(QRectF(rect())) - cardHeight() - kMargin;
}

std::optional<int> PyramidView::pyramidAt(QPointF pos) const
{
    // Bottom row first: lower cards are drawn over the ones above them.
    for (int row = kRows - 1; row >= 0; --row) {
        for (int i = 0; i <= row; ++i) {
            const int slot = slotIndex(row, i);
            if (m_table.pyramid()[std::size_t(slot)].removed)
                continue;
            if (pyramidRect(row, i).contains(pos))
                return isExposed(row, i) ? std::optional<int>(slot) : std::nullopt;
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Play
// ---------------------------------------------------------------------------

void PyramidView::clearSelection()
{
    m_hasSelection = false;
    m_selectedIndex = -1;
}

void PyramidView::select(Source source, int index)
{
    m_hasSelection = true;
    m_selectedSource = source;
    m_selectedIndex = index;
}

void PyramidView::tryPair(Source source, int index, const Card& card)
{
    // A King is 13 on its own.
    if (card.rank == kKing) {
        if (!m_table.takeKing(source, index))
            return;
        m_undoAction->setEnabled(m_table.canUndo());
        clearSelection();
        Sound::instance().play(Sound::kCardPlace);
        update();
        refresh();
        checkEnd();
        return;
    }

    if (!m_hasSelection) {
        select(source, index);
        update();
        refresh();
        return;
    }

    // Clicking the same card again just deselects it.
    if (m_selectedSource == source && m_selectedIndex == index) {
        clearSelection();
        update();
        refresh();
        return;
    }

    const Card first = m_table.cardAt(m_selectedSource, m_selectedIndex);

    if (!m_table.takePair(source, index, m_selectedSource, m_selectedIndex)) {
        refresh(QStringLiteral("%1 and %2 make %3, not 13.")
                    .arg(rankLabel(first.rank))
                    .arg(rankLabel(card.rank))
                    .arg(first.rank + card.rank));
        select(source, index);
        update();
        return;
    }

    m_undoAction->setEnabled(m_table.canUndo());
    clearSelection();
    Sound::instance().play(Sound::kCardPlace);
    update();
    refresh();
    checkEnd();
}

void PyramidView::dealFromStock()
{
    if (!m_table.drawFromStock()) {
        refresh(QStringLiteral("No redeals left."));
        return;
    }
    m_undoAction->setEnabled(m_table.canUndo());
    clearSelection();
    Sound::instance().play(Sound::kCardDeal);
    update();
    refresh();
    checkEnd();
}

void PyramidView::checkEnd()
{
    if (!m_table.cleared() || m_won)
        return;

    m_won = true;
    Sound::instance().play(Sound::kWin);
    const bool newBest = Scores::instance().recordHigh(kBestKey, m_table.pairs());
    refresh();

    if (m_announced)
        return;
    m_announced = true;
    QTimer::singleShot(200, this, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("Cleared"));
        box.setText(QStringLiteral("The pyramid is gone!"));
        box.setInformativeText(newBest ? QStringLiteral("A new best.")
                                       : QStringLiteral("Pairs taken: %1.").arg(m_table.pairs()));
        QAbstractButton* again = box.addButton(QStringLiteral("New Deal"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Close"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == again)
            newGame();
    });
}

void PyramidView::refresh(const QString& message)
{
    int left = 0;
    for (const Slot& s : m_table.pyramid())
        if (!s.removed)
            ++left;

    QString line = message.isEmpty()
        ? QStringLiteral("%1   Pyramid %2 left   Stock %3   Redeals %4")
              .arg(m_won ? QStringLiteral("Cleared!") : QStringLiteral("Match pairs adding to 13"))
              .arg(left)
              .arg(m_table.stock().size())
              .arg(PyramidTable::kMaxRedeals - m_table.redeals())
        : message;
    Q_EMIT statusChanged(line);
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void PyramidView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    Theme::paintFelt(p, rect(), Theme::kFeltTealTop, Theme::kFeltTealBottom);

    // Top row first, so lower cards overlap the ones they cover.
    for (int row = 0; row < kRows; ++row) {
        for (int i = 0; i <= row; ++i) {
            const int slot = slotIndex(row, i);
            if (m_table.pyramid()[std::size_t(slot)].removed)
                continue;

            const QRectF r = pyramidRect(row, i);
            CardArt::paintFace(p, r, m_table.pyramid()[std::size_t(slot)].card);

            // Covered cards are dimmed: it is the quickest way to show what
            // can actually be taken.
            if (!isExposed(row, i)) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 0, 0, 105));
                p.drawRoundedRect(r, r.width() * 0.075, r.width() * 0.075);
            }
            if (m_hasSelection && m_selectedSource == Source::Pyramid && m_selectedIndex == slot)
                CardArt::paintHighlight(p, r, QColor(0xff, 0xd5, 0x4f));
        }
    }

    if (m_table.stock().empty())
        CardArt::paintSlot(p, stockRect(),
                           m_table.redealsLeft() ? QStringLiteral("↻") : QString());
    else
        CardArt::paintBack(p, stockRect());

    if (m_table.waste().empty()) {
        CardArt::paintSlot(p, wasteRect());
    } else {
        CardArt::paintFace(p, wasteRect(), m_table.waste().back());
        if (m_hasSelection && m_selectedSource == Source::Waste)
            CardArt::paintHighlight(p, wasteRect(), QColor(0xff, 0xd5, 0x4f));
    }

    paintStatusCaption(p, QRectF(rect()));
}

void PyramidView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_won)
        return;

    if (stockRect().contains(event->position())) {
        dealFromStock();
        return;
    }

    if (!m_table.waste().empty() && wasteRect().contains(event->position())) {
        tryPair(Source::Waste, int(m_table.waste().size()) - 1, m_table.waste().back());
        return;
    }

    if (const std::optional<int> slot = pyramidAt(event->position())) {
        tryPair(Source::Pyramid, *slot, m_table.pyramid()[std::size_t(*slot)].card);
        return;
    }

    clearSelection();
    update();
    refresh();
}
