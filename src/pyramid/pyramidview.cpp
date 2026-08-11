#include "pyramidview.h"

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
    std::vector<Card> deck = makeDeck(1, 4);
    shuffleCards(deck, m_rng);
    Sound::instance().play(Sound::kShuffle);

    m_pyramid.clear();
    constexpr int kPyramidCards = kRows * (kRows + 1) / 2; // 28
    for (int i = 0; i < kPyramidCards; ++i) {
        Card c = deck[std::size_t(i)];
        c.faceUp = true;
        m_pyramid.push_back({ c, false });
    }

    m_stock.assign(deck.begin() + kPyramidCards, deck.end());
    for (Card& c : m_stock)
        c.faceUp = false;

    m_waste.clear();
    m_history.clear();
    clearSelection();
    m_pairs = 0;
    m_redeals = 0;
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

void PyramidView::pushUndo()
{
    m_history.push_back({ m_pyramid, m_stock, m_waste, m_pairs, m_redeals });
    if (m_history.size() > 200)
        m_history.erase(m_history.begin());
    m_undoAction->setEnabled(true);
}

void PyramidView::undo()
{
    if (m_history.empty())
        return;
    const Snapshot s = m_history.back();
    m_history.pop_back();
    m_pyramid = s.pyramid;
    m_stock = s.stock;
    m_waste = s.waste;
    m_pairs = s.pairs;
    m_redeals = s.redeals;
    m_won = false;
    m_announced = false;
    clearSelection();
    m_undoAction->setEnabled(!m_history.empty());
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
    if (m_won || m_history.empty())
        return {};

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1) << qint32(m_pairs) << qint32(m_redeals) << qint32(m_pyramid.size());
    // A taken pyramid card keeps its slot and is simply marked gone, because the
    // slot above it still needs to know both its supports have been cleared.
    for (const Slot& s : m_pyramid) {
        cardcodec::writeCard(out, s.card);
        out << qint8(s.removed ? 1 : 0);
    }
    cardcodec::writePile(out, m_stock);
    cardcodec::writePile(out, m_waste);
    return blob;
}

bool PyramidView::restoreState(const QByteArray& blob)
{
    constexpr int kPyramidCards = kRows * (kRows + 1) / 2; // 28

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
        || redeals > kMaxRedeals || slotCount != kPyramidCards)
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

    // Pyramid takes matched pairs out of play, so the pack cannot come back
    // whole. What it can promise is that nothing here was ever outside it, and
    // that no card has turned up twice.
    std::vector<Card> all;
    for (const Slot& s : pyramid)
        all.push_back(s.card);
    cardcodec::gather(all, stock);
    cardcodec::gather(all, waste);
    if (!cardcodec::fitsPack(all, 1, 4))
        return false;

    m_pyramid = pyramid;
    m_stock = stock;
    m_waste = waste;
    m_pairs = int(pairs);
    m_redeals = int(redeals);
    m_history.clear();
    m_won = false;
    m_announced = false;
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
    const double byHeight = (height() - 40.0) / (1.4 + (kRows - 1) * 0.52 + 1.6);
    return std::max(30.0, std::min(byWidth * 1.6, byHeight));
}

QRectF PyramidView::pyramidRect(int row, int index) const
{
    const double w = cardWidth();
    const double h = cardHeight();
    const double stepX = w * 0.56;
    const double stepY = h * 0.46;
    const double rowWidth = stepX * row + w;
    const double left = (width() - rowWidth) / 2 + index * stepX;
    return { left, 16 + row * stepY, w, h };
}

QRectF PyramidView::stockRect() const
{
    const double w = cardWidth();
    return { width() / 2.0 - w * 1.25, height() - cardHeight() - 16, w, cardHeight() };
}

QRectF PyramidView::wasteRect() const
{
    const double w = cardWidth();
    return { width() / 2.0 + w * 0.25, height() - cardHeight() - 16, w, cardHeight() };
}

bool PyramidView::isExposed(int row, int index) const
{
    if (m_pyramid[std::size_t(slotIndex(row, index))].removed)
        return false;
    if (row == kRows - 1)
        return true;
    return m_pyramid[std::size_t(slotIndex(row + 1, index))].removed
        && m_pyramid[std::size_t(slotIndex(row + 1, index + 1))].removed;
}

std::optional<int> PyramidView::pyramidAt(QPointF pos) const
{
    // Bottom row first: lower cards are drawn over the ones above them.
    for (int row = kRows - 1; row >= 0; --row) {
        for (int i = 0; i <= row; ++i) {
            const int slot = slotIndex(row, i);
            if (m_pyramid[std::size_t(slot)].removed)
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

void PyramidView::removeFrom(Source source, int index)
{
    if (source == Source::Pyramid)
        m_pyramid[std::size_t(index)].removed = true;
    else if (source == Source::Waste && !m_waste.empty())
        m_waste.pop_back();
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
        pushUndo();
        removeFrom(source, index);
        ++m_pairs;
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

    const Card& first = (m_selectedSource == Source::Pyramid)
        ? m_pyramid[std::size_t(m_selectedIndex)].card
        : m_waste.back();

    if (first.rank + card.rank != 13) {
        refresh(QStringLiteral("%1 and %2 make %3, not 13.")
                    .arg(rankLabel(first.rank))
                    .arg(rankLabel(card.rank))
                    .arg(first.rank + card.rank));
        select(source, index);
        update();
        return;
    }

    pushUndo();
    // Remove the later index first so a pyramid index cannot shift underneath.
    removeFrom(source, index);
    removeFrom(m_selectedSource, m_selectedIndex);
    ++m_pairs;
    clearSelection();
    Sound::instance().play(Sound::kCardPlace);
    update();
    refresh();
    checkEnd();
}

void PyramidView::dealFromStock()
{
    if (m_stock.empty()) {
        if (m_redeals >= kMaxRedeals) {
            refresh(QStringLiteral("No redeals left."));
            return;
        }
        pushUndo();
        ++m_redeals;
        std::reverse(m_waste.begin(), m_waste.end());
        for (Card& c : m_waste)
            c.faceUp = false;
        m_stock = m_waste;
        m_waste.clear();
    } else {
        pushUndo();
        Card c = m_stock.back();
        m_stock.pop_back();
        c.faceUp = true;
        m_waste.push_back(c);
    }
    clearSelection();
    Sound::instance().play(Sound::kCardDeal);
    update();
    refresh();
    checkEnd();
}

void PyramidView::checkEnd()
{
    const bool cleared = std::all_of(m_pyramid.begin(), m_pyramid.end(),
                                     [](const Slot& s) { return s.removed; });
    if (!cleared || m_won)
        return;

    m_won = true;
    Sound::instance().play(Sound::kWin);
    const bool newBest = Scores::instance().recordHigh(kBestKey, m_pairs);
    refresh();

    if (m_announced)
        return;
    m_announced = true;
    QTimer::singleShot(200, this, [this, newBest] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("Cleared"));
        box.setText(QStringLiteral("The pyramid is gone!"));
        box.setInformativeText(newBest ? QStringLiteral("A new best.")
                                       : QStringLiteral("Pairs taken: %1.").arg(m_pairs));
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
    for (const Slot& s : m_pyramid)
        if (!s.removed)
            ++left;

    QString line = message.isEmpty()
        ? QStringLiteral("%1   Pyramid %2 left   Stock %3   Redeals %4")
              .arg(m_won ? QStringLiteral("Cleared!") : QStringLiteral("Match pairs adding to 13"))
              .arg(left)
              .arg(m_stock.size())
              .arg(kMaxRedeals - m_redeals)
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
            if (m_pyramid[std::size_t(slot)].removed)
                continue;

            const QRectF r = pyramidRect(row, i);
            CardArt::paintFace(p, r, m_pyramid[std::size_t(slot)].card);

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

    if (m_stock.empty())
        CardArt::paintSlot(p, stockRect(),
                           m_redeals < kMaxRedeals ? QStringLiteral("↻") : QString());
    else
        CardArt::paintBack(p, stockRect());

    if (m_waste.empty()) {
        CardArt::paintSlot(p, wasteRect());
    } else {
        CardArt::paintFace(p, wasteRect(), m_waste.back());
        if (m_hasSelection && m_selectedSource == Source::Waste)
            CardArt::paintHighlight(p, wasteRect(), QColor(0xff, 0xd5, 0x4f));
    }
}

void PyramidView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_won)
        return;

    if (stockRect().contains(event->position())) {
        dealFromStock();
        return;
    }

    if (!m_waste.empty() && wasteRect().contains(event->position())) {
        tryPair(Source::Waste, int(m_waste.size()) - 1, m_waste.back());
        return;
    }

    if (const std::optional<int> slot = pyramidAt(event->position())) {
        tryPair(Source::Pyramid, *slot, m_pyramid[std::size_t(*slot)].card);
        return;
    }

    clearSelection();
    update();
    refresh();
}
