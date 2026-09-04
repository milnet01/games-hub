#include "cardart.h"

#include "theme.h"

#include <QCoreApplication>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPaintDevice>
#include <QPainterPath>
#include <QPixmap>
#include <QTransform>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace {

constexpr QColor kFaceTop { 0xff, 0xff, 0xfd };
constexpr QColor kFaceBottom { 0xef, 0xed, 0xe4 };
constexpr QColor kRed { 0xc0, 0x28, 0x28 };
constexpr QColor kBlack { 0x1c, 0x1f, 0x22 };
constexpr QColor kBackInk { 0x27, 0x46, 0x86 };
constexpr QColor kBackInkDark { 0x18, 0x2c, 0x58 };
constexpr QColor kBackInkRed { 0x9c, 0x27, 0x33 };
constexpr QColor kBackInkRedDark { 0x67, 0x16, 0x22 };
constexpr QColor kBackPaper { 0xf2, 0xf4, 0xf9 };

double corner(const QRectF& r) { return std::max(2.0, r.width() * 0.075); }

struct Pip {
    double x;
    double y;
};

// The traditional pip arrangements. Columns sit at 0.28 / 0.5 / 0.72 and the
// rows are spaced so each count reads at a glance, exactly as on a real deck.
std::vector<Pip> pipLayout(int rank)
{
    // Columns sit clear of the corner indices; rows run between them.
    constexpr double L = 0.32;
    constexpr double C = 0.50;
    constexpr double R = 0.68;

    // Rows run the full height of the pip field. Ranks 2-8 use three rows,
    // 9 and 10 use four, and the odd pip sits on a line between them — the
    // arrangement a real deck uses.
    constexpr double T = 0.00;
    constexpr double M = 0.50;
    constexpr double B = 1.00;

    switch (rank) {
    case 2:  return { { C, T }, { C, B } };
    case 3:  return { { C, T }, { C, M }, { C, B } };
    case 4:  return { { L, T }, { R, T }, { L, B }, { R, B } };
    case 5:  return { { L, T }, { R, T }, { C, M }, { L, B }, { R, B } };
    case 6:  return { { L, T }, { R, T }, { L, M }, { R, M }, { L, B }, { R, B } };
    case 7:  return { { L, T }, { R, T }, { C, 0.25 }, { L, M }, { R, M },
                      { L, B }, { R, B } };
    case 8:  return { { L, T }, { R, T }, { C, 0.25 }, { L, M }, { R, M },
                      { C, 0.75 }, { L, B }, { R, B } };
    case 9:  return { { L, T }, { R, T }, { L, 0.32 }, { R, 0.32 }, { C, M },
                      { L, 0.68 }, { R, 0.68 }, { L, B }, { R, B } };
    case 10: return { { L, T }, { R, T }, { C, 0.16 }, { L, 0.32 }, { R, 0.32 },
                      { L, 0.68 }, { R, 0.68 }, { C, 0.84 }, { L, B }, { R, B } };
    default: return {};
    }
}

// Draws a suit glyph centred on `centre`. Real decks invert the lower pips,
// but an upside-down heart reads as a spade at this size, so these stay
// upright — clearer, and still reads as a proper card.
void drawPip(QPainter& p, const QPointF& centre, double size, const QString& suit)
{
    p.save();
    p.translate(centre);
    QFont f = p.font();
    f.setPointSizeF(size);
    p.setFont(f);
    const QRectF box(-size * 1.2, -size * 1.2, size * 2.4, size * 2.4);
    p.drawText(box, Qt::AlignCenter, suit);
    p.restore();
}

// A court card gets a ruled panel rather than figure art: it reads as a face
// card at any size and never turns to mush when the cards are small.
void drawCourt(QPainter& p, const QRectF& r, const QString& rank, const QString& suit,
               const QColor& ink)
{
    const QRectF panel = r.adjusted(r.width() * 0.24, r.height() * 0.14,
                                    -r.width() * 0.24, -r.height() * 0.14);

    QColor tint = ink;
    tint.setAlpha(18);
    QPainterPath path;
    path.addRoundedRect(panel, r.width() * 0.05, r.width() * 0.05);
    p.fillPath(path, tint);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Theme::kGold, std::max(1.0, r.width() * 0.018)));
    p.drawPath(path);
    p.setPen(QPen(QColor(ink.red(), ink.green(), ink.blue(), 110),
                  std::max(0.8, r.width() * 0.012)));
    p.drawRoundedRect(panel.adjusted(r.width() * 0.028, r.height() * 0.02,
                                     -r.width() * 0.028, -r.height() * 0.02),
                      r.width() * 0.03, r.width() * 0.03);

    // Suit above and below, letter through the middle.
    const double pip = r.width() * 0.15;
    p.setPen(ink);
    drawPip(p, QPointF(panel.center().x(), panel.top() + panel.height() * 0.15), pip, suit);
    drawPip(p, QPointF(panel.center().x(), panel.bottom() - panel.height() * 0.15), pip, suit);

    QFont f = p.font();
    f.setPointSizeF(r.width() * 0.30);
    f.setBold(true);
    p.setFont(f);
    p.drawText(panel, Qt::AlignCenter, rank);
}

// A jester's cap: three lobes on a headband, each with a bell. Drawn rather
// than lettered because "JOKER" down the middle of a card this small is
// unreadable, and the silhouette survives being shrunk to a corner of a meld.
void drawJesterCap(QPainter& p, const QRectF& r, const QColor& ink)
{
    const double w = r.width();
    const QPointF c(r.center().x(), r.center().y() + r.height() * 0.06);

    const double lobe = w * 0.13;
    const double reach = w * 0.26;
    const struct { double dx; double dy; } kLobes[3] = {
        { -reach, -w * 0.16 }, { 0.0, -w * 0.30 }, { reach, -w * 0.16 },
    };

    QPainterPath cap;
    cap.moveTo(c.x() - w * 0.30, c.y());
    for (const auto& l : kLobes) {
        const QPointF tip(c.x() + l.dx, c.y() + l.dy);
        cap.quadTo(QPointF(tip.x() - lobe, tip.y()), tip);
        cap.quadTo(QPointF(tip.x() + lobe, tip.y()), QPointF(tip.x() + lobe * 0.9, c.y()));
    }
    cap.lineTo(c.x() + w * 0.30, c.y());
    cap.closeSubpath();

    QColor fill = ink;
    fill.setAlpha(190);
    p.setPen(QPen(ink, std::max(0.8, w * 0.02)));
    p.setBrush(fill);
    p.drawPath(cap);

    // Bells, and the headband under the lobes.
    p.setBrush(Theme::kGold);
    p.setPen(QPen(ink, std::max(0.6, w * 0.014)));
    for (const auto& l : kLobes)
        p.drawEllipse(QPointF(c.x() + l.dx, c.y() + l.dy - lobe * 0.2), w * 0.05, w * 0.05);

    p.setBrush(Theme::kGold);
    p.drawRoundedRect(QRectF(c.x() - w * 0.32, c.y(), w * 0.64, w * 0.10),
                      w * 0.05, w * 0.05);
    p.setBrush(Qt::NoBrush);
}

} // namespace

namespace CardArt {

// The picture on a face, drawn from scratch. Everything below paintBack is
// about not having to call this on every frame.
static void drawFace(QPainter& p, const QRectF& r, const Card& c)
{
    Theme::paintDropShadow(p, r, corner(r), std::max(1.5, r.width() * 0.035));

    QPainterPath path;
    path.addRoundedRect(r, corner(r), corner(r));

    QLinearGradient face(r.topLeft(), r.bottomLeft());
    face.setColorAt(0.0, kFaceTop);
    face.setColorAt(1.0, kFaceBottom);
    p.fillPath(path, face);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0, 0, 0, 60), 1));
    p.drawPath(path);

    // A joker carries no suit, so its corner index is the star alone.
    const bool joker = isJoker(c);
    const QColor ink = isRed(c) ? kRed : kBlack;
    const QString rank = rankLabel(c.rank);
    const QString suit = joker ? QString() : suitSymbol(c.suit);
    p.setPen(ink);

    // Corner index, top-left upright and bottom-right inverted, as on a real
    // card — it is what makes a fanned hand readable.
    const double pad = r.width() * 0.08;
    const double indexSize = std::max(6.0, r.width() * 0.17);

    for (int i = 0; i < 2; ++i) {
        const bool inverted = (i == 1);
        p.save();
        if (inverted) {
            p.translate(r.center());
            p.rotate(180);
            p.translate(-r.center());
        }
        QFont f = p.font();
        f.setBold(true);
        f.setPointSizeF(indexSize);
        p.setFont(f);
        // A narrow column: wide enough for "10", narrow enough to leave the
        // middle of the card free for the pips.
        const QRectF box(r.left() + pad, r.top() + pad * 0.4, r.width() * 0.24, r.height() * 0.20);
        // TextDontClip, for the reason SudokuView::markFont() gives: drawText
        // CLIPS to its rect, so a font is bounded by its LINE box rather than
        // by its ink. Two digits at this size want roughly 0.25 to 0.27 of the
        // card's width against the 0.24 here, so "10" was already tight -- and
        // on an offscreen platform with no fonts, where digits measure the full
        // em box, it wants about 0.45 and lost half of itself. Below
        // kFaceMinWidth this index IS the card, which is where it matters most.
        // With AlignLeft|AlignTop the origin does not move, so nothing else
        // about the layout changes.
        p.drawText(box, Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip, rank);

        f.setBold(false);
        f.setPointSizeF(indexSize * 0.88);
        p.setFont(f);
        p.drawText(QRectF(box.left(), box.top() + r.height() * 0.125, box.width(), r.height() * 0.18),
                   Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip, suit);
        p.restore();
    }

    // Below this width the middle of the card is too small for pips to read,
    // so the corner index carries the card on its own.
    if (r.width() < kFaceMinWidth)
        return;

    if (joker) {
        drawJesterCap(p, r, ink);
        return;
    }

    if (c.rank == kJack || c.rank == kQueen || c.rank == kKing) {
        drawCourt(p, r, rank, suit, ink);
        return;
    }

    if (c.rank == kAce) {
        QFont f = p.font();
        f.setPointSizeF(r.width() * 0.44);
        p.setFont(f);
        p.drawText(r, Qt::AlignCenter, suit);
        return;
    }

    // Pip columns are fractions of the card; the rows run between the two
    // indices rather than over them.
    const double pipSize = r.width() * 0.125;
    const double top = r.top() + r.height() * 0.145;
    const double span = r.height() * 0.71;
    for (const Pip& pip : pipLayout(c.rank))
        drawPip(p, QPointF(r.left() + r.width() * pip.x, top + span * pip.y), pipSize, suit);
}

// The picture on a back, drawn from scratch. Everything below this is about
// not having to call it.
static void drawBack(QPainter& p, const QRectF& r, int deck)
{
    const bool red = (deck % 2) != 0;
    const QColor backInk = red ? kBackInkRed : kBackInk;
    const QColor backInkDark = red ? kBackInkRedDark : kBackInkDark;

    Theme::paintDropShadow(p, r, corner(r), std::max(1.5, r.width() * 0.035));

    QPainterPath path;
    path.addRoundedRect(r, corner(r), corner(r));
    p.fillPath(path, kBackPaper);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0, 0, 0, 60), 1));
    p.drawPath(path);

    const QRectF inner = r.adjusted(r.width() * 0.09, r.height() * 0.065,
                                    -r.width() * 0.09, -r.height() * 0.065);
    QPainterPath innerPath;
    innerPath.addRoundedRect(inner, corner(r) * 0.55, corner(r) * 0.55);

    QLinearGradient ink(inner.topLeft(), inner.bottomRight());
    ink.setColorAt(0.0, backInk);
    ink.setColorAt(1.0, backInkDark);
    p.fillPath(innerPath, ink);

    // Diagonal lattice, clipped to the panel.
    p.save();
    p.setClipPath(innerPath);
    p.setPen(QPen(QColor(255, 255, 255, 42), std::max(0.7, r.width() * 0.012)));
    const double step = std::max(4.0, r.width() * 0.155);
    // Counted rather than accumulated. `x += step` in the condition drifts by a
    // rounding per iteration, so the last line of a wide lattice sits somewhere
    // slightly different from where the arithmetic says -- and on a cached card
    // that is a difference nothing would ever explain.
    const double first = inner.left() - inner.height();
    const double past = inner.right() + inner.height();
    const int lines = int(std::ceil((past - first) / step));
    for (int i = 0; i < lines; ++i) {
        const double x = first + i * step;
        p.drawLine(QPointF(x, inner.top()), QPointF(x + inner.height(), inner.bottom()));
        p.drawLine(QPointF(x + inner.height(), inner.top()), QPointF(x, inner.bottom()));
    }
    // A centre medallion stops the lattice reading as pure texture.
    if (r.width() >= 40) {
        p.setPen(QPen(QColor(255, 255, 255, 70), std::max(0.8, r.width() * 0.016)));
        p.setBrush(QColor(backInkDark.red(), backInkDark.green(), backInkDark.blue(), 200));
        p.drawEllipse(inner.center(), r.width() * 0.17, r.width() * 0.17);
    }
    p.restore();

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 90), 1));
    p.drawPath(innerPath);
}

// A back is a pure function of its size and its deck colour, and it is the most
// expensive thing this file draws: the lattice alone is about fifty antialiased
// lines clipped to a rounded path, and Canasta keeps three hands of backs plus
// the stock on the table at all times. Measured with `gameshub_uitest --bench`
// before this cache existed, a resting Canasta table cost 24.5 ms a frame
// against a 16 ms timer -- so the game could not meet its own clock, which is
// the slowdown a player feels as the hand fills up (GHUB-0048).
//
// BACKS ONLY, and that is the whole of the design rather than a first step. A
// cached pixmap drawn under the rotation of a fanned hand is resampled and goes
// slightly soft. On a back there is nothing to read, so the softening costs
// nothing. On a FACE it would trade a cost the player cannot see for a blur he
// can -- this game is read by pip pattern -- so faces stay drawn live. Do not
// "finish the job" by caching them without measuring what it does to a face at
// the smallest scale the game draws one.
// How far Theme::paintDropShadow reaches outside the card. Its widest stacked
// rect spreads by depth * 3 * 0.6; the pixmap has to hold that or the cache
// clips the shadow off and cards stop sitting on the table.
static double shadowPad(double width)
{
    return std::max(1.5, width * 0.035) * 1.8 + 1.0;
}

// A bounded pixmap cache. Two bounds, because they answer different questions.
// The entry bound keeps a resize from filling the map with the intermediate
// widths it passes through. The byte bound is what actually caps memory: an
// entry's size follows the card's, so the same entry count costs four times as
// much at a device pixel ratio of 2 and more again at a large window. Without
// it the ceiling is a count and the footprint is whatever that count happens to
// weigh.
struct ArtCache {
    std::unordered_map<uint64_t, QPixmap> pixmaps;
    std::size_t bytes = 0;

    void drop()
    {
        pixmaps.clear();
        bytes = 0;
    }
};

// Generous on purpose: it is a backstop against the pathological case, not the
// working control. A full face cache of ordinary cards sits well under this at
// a device pixel ratio of 2, so normal play never reaches it -- and a bound
// that binds during play would empty the cache every frame, which costs more
// than the memory it saves.
static constexpr std::size_t kCacheByteBound = std::size_t(64) * 1024 * 1024;

static ArtCache& backCache()
{
    static ArtCache cache;
    return cache;
}

static ArtCache& faceCache()
{
    static ArtCache cache;
    return cache;
}

// A QPixmap may not be destroyed once QGuiApplication has gone, and a
// function-local static is destroyed at exit -- after main has returned and
// taken the application object with it. This empties both caches while the
// application is still alive.
static void ensureCachesAreEmptiedInTime()
{
    static const bool registered = [] {
        qAddPostRoutine([] {
            backCache().drop();
            faceCache().drop();
        });
        return true;
    }();
    (void)registered;
}

// Draw one card through a pixmap cache. `identity` is what distinguishes this
// picture from every other at the same size -- a deck colour for a back, a rank
// and suit for a face.
//
// `cacheRotated` is the whole difference between the two callers. A cached
// pixmap under a rotation has to be resampled, and comes back very slightly
// soft. On a BACK that costs nothing: there is nothing on it to read. On a FACE
// it would trade a cost the player cannot see for a blur he can, and this game
// is read by pip pattern rather than by the corner index -- so faces pass false
// and a fanned hand keeps drawing them live. Every unrotated card, which is
// every solitaire tableau and every stock pile, takes the exact path either way.
//
// Returns false when the caller should just draw the card itself.
template <typename Draw>
static bool blitCached(QPainter& p, const QRectF& r, ArtCache& cache, size_t bound, int identity,
                       bool cacheRotated, Draw&& draw)
{
    if (r.width() <= 0.0 || r.height() <= 0.0)
        return true;   // nothing to draw, and nothing for the caller to do either

    const QTransform t = p.transform();
    const double devScale = (p.device() != nullptr ? p.device()->devicePixelRatioF() : 1.0);

    // A card that is only translated can be blitted onto whole device pixels
    // and lands exactly where the drawn one would.
    const bool plain = t.type() <= QTransform::TxTranslate;
    if (!plain && !cacheRotated)
        return false;

    // The scale the card actually lands on screen at, so the cache holds device
    // pixels rather than logical ones. Without this every card goes soft on a
    // HiDPI display, which for a partially sighted player is a straight loss
    // however fast it runs.
    const double zoom
        = plain ? 1.0 : std::max(std::hypot(t.m11(), t.m12()), std::hypot(t.m21(), t.m22()));
    const double dpr = devScale * zoom;
    if (!(dpr > 0.0) || !std::isfinite(dpr))
        return false;

    // The card is SNAPPED to whole device pixels before anything else, and the
    // key is then computed from the snapped size. This is not tidiness: the key
    // has to determine the picture completely. Quantising the key while drawing
    // at the exact size means two cards a fraction of a pixel apart share one
    // entry, and which of them filled it depends on what else was in the cache
    // -- so a frame drawn after an eviction differs from the same frame drawn
    // before one. That is exactly how FreeCell stopped going back pixel for
    // pixel across the legibility switch, and it is a bug that only appears
    // once the cache is full enough to evict.
    const double wq = std::round(r.width() * dpr) / dpr;
    const double hq = std::round(r.height() * dpr) / dpr;
    if (!(wq > 0.0) || !(hq > 0.0))
        return false;

    // A WHOLE pixel, and that is load bearing too. The card is drawn `pad` in
    // from the pixmap's edge, so a fractional pad puts it at a fractional
    // offset inside the cache and every line antialiases differently from the
    // card the game used to draw. Measured on a back: with a fractional pad,
    // 542 pixels moved by more than 8 levels, all on the lattice.
    const double pad = std::ceil(shadowPad(wq));
    const QRectF padded(r.left() - pad, r.top() - pad, wq + 2 * pad, hq + 2 * pad);

    // Rounded UP: a pixmap a rounding short of the padded rect clips the shadow
    // it was padded for.
    const int wPx = int(std::ceil(padded.width() * dpr));
    const int hPx = int(std::ceil(padded.height() * dpr));
    const int scaleQ = int(std::lround(dpr * 100.0));
    if (wPx <= 0 || hPx <= 0 || wPx > 4096 || hPx > 4096)
        return false;

    // Keyed on the snapped CARD size, never on the padded pixmap size. `pad`
    // grows in whole pixels while the card grows continuously, so at a
    // fractional device pixel ratio the padded size is not injective: at 1.5, a
    // card snapped to 83 device pixels wide and one snapped to 84 both pad out
    // to 99, and would share one entry holding whichever got there first --
    // the GHUB-0048 defect again, one step further along. Ratios of 1 and 2
    // never collide, which is why drawing at those alone cannot show it.
    const int wCard = int(std::lround(wq * dpr));
    const int hCard = int(std::lround(hq * dpr));
    const uint64_t key = uint64_t(wCard & 0xffff) | (uint64_t(hCard & 0xffff) << 16)
        | (uint64_t(identity & 0xff) << 32) | (uint64_t(scaleQ & 0xffff) << 40);

    auto it = cache.pixmaps.find(key);
    if (it == cache.pixmaps.end()) {
        // A resize walks through sizes on its way to the one it stops at, so
        // the map is bounded rather than left to grow with every intermediate
        // width. Dropping the lot is fine: the next frame refills what it
        // needs, which is one entry per distinct picture on the table.
        if (cache.pixmaps.size() > bound || cache.bytes > kCacheByteBound)
            cache.drop();

        QPixmap pix(wPx, hPx);
        pix.setDevicePixelRatio(dpr);
        pix.fill(Qt::transparent);

        QPainter into(&pix);
        into.setRenderHint(QPainter::Antialiasing, true);
        into.setRenderHint(QPainter::TextAntialiasing, true);
        // At the origin of the pixmap's own logical space: the card sits `pad`
        // in from each edge, which is the room the shadow needs.
        draw(into, QRectF(pad, pad, wq, hq));
        into.end();

        cache.bytes += std::size_t(wPx) * std::size_t(hPx) * std::size_t(pix.depth() / 8);
        it = cache.pixmaps.emplace(key, std::move(pix)).first;
    }

    if (plain) {
        // Drawn at the pixmap's own size, at a whole device pixel, with the
        // transform out of the way -- so nothing is rescaled and nothing is
        // shifted by a fraction of a pixel. Drawing into `padded` instead
        // resamples every card by the rounding in wPx, and a card back
        // measurably softens: 20% of the pixels in a stock pile moved by more
        // than 8 levels when this path did not exist.
        const QPointF where = padded.topLeft() + QPointF(t.dx(), t.dy());
        p.save();
        p.resetTransform();
        p.drawPixmap(QPointF(std::round(where.x() * devScale) / devScale,
                             std::round(where.y() * devScale) / devScale),
                     it->second);
        p.restore();
        return true;
    }

    const bool wasSmooth = p.testRenderHint(QPainter::SmoothPixmapTransform);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawPixmap(padded, it->second, QRectF(it->second.rect()));
    p.setRenderHint(QPainter::SmoothPixmapTransform, wasSmooth);
    return true;
}

void paintBack(QPainter& p, const QRectF& r, int deck)
{
    p.save();
    ensureCachesAreEmptiedInTime();
    // Two colourways and a handful of sizes, so the bound is generous.
    if (!blitCached(p, r, backCache(), 64, deck, true,
                    [deck](QPainter& into, const QRectF& at) { drawBack(into, at, deck); }))
        drawBack(p, r, deck);
    p.restore();
}

void paintFace(QPainter& p, const QRectF& r, const Card& c)
{
    p.save();
    ensureCachesAreEmptiedInTime();
    // FreeCell puts all fifty-two on the table at once and Pyramid twenty-eight,
    // so this one holds a whole pack at two sizes rather than a handful.
    // Rank is 0..13 and suit 0..3, so one byte names the picture. A joker's
    // suit is not drawn, but keeping it in the key only costs an entry.
    const int identity = (c.rank << 2) | (int(c.suit) & 0x3);
    if (!blitCached(p, r, faceCache(), 160, identity, false,
                    [&c](QPainter& into, const QRectF& at) { drawFace(into, at, c); }))
        drawFace(p, r, c);
    p.restore();
}

double slotLabelPointSize(const QFont& base, const QRectF& r, const QString& label)
{
    // SOLVE the size rather than scale it. A single character fits at any
    // sensible ratio; a word does not, and drawText clips to the rect, so a
    // ratio tuned on one machine's font leaves a stroke of a word on another
    // (SudokuView::markFont is the same lesson). Callers pass words on
    // purpose: a symbol the platform has no font for draws as nothing at all,
    // and on Pyramid's stock it was the only cue on the surface (GHUB-0160).
    QFont f = base;
    double pt = std::max(8.0, r.width() * 0.38);
    f.setPointSizeF(pt);
    while (pt > 7.0 && QFontMetricsF(f).horizontalAdvance(label) > r.width() * 0.86) {
        pt -= 0.5;
        f.setPointSizeF(pt);
    }
    return pt;
}

double slotLabelWidth(const QFont& base, const QRectF& r, const QString& label)
{
    QFont f = base;
    f.setPointSizeF(slotLabelPointSize(base, r, label));
    return QFontMetricsF(f).horizontalAdvance(label);
}

void paintSlot(QPainter& p, const QRectF& r, const QString& glyph)
{
    p.save();
    QPainterPath path;
    path.addRoundedRect(r, corner(r), corner(r));
    p.fillPath(path, QColor(0, 0, 0, 40));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 60), 1.4, Qt::DashLine));
    p.drawPath(path);

    if (!glyph.isEmpty()) {
        QFont f = p.font();
        f.setPointSizeF(slotLabelPointSize(p.font(), r, glyph));
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 70));
        p.drawText(r, Qt::AlignCenter, glyph);
    }
    p.restore();
}

void paintHighlight(QPainter& p, const QRectF& r, const QColor& colour)
{
    p.save();
    QPainterPath path;
    path.addRoundedRect(r.adjusted(-1.5, -1.5, 1.5, 1.5), corner(r), corner(r));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(colour, 2.5));
    p.drawPath(path);
    p.restore();
}

} // namespace CardArt
