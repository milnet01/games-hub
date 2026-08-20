#include "gameview.h"

#include "legibility.h"
#include "theme.h"

#include <QFontMetricsF>

#include <algorithm>

// GameView is almost pure interface, but a Q_OBJECT class still needs a
// translation unit of its own: AUTOMOC only generates the metaobject for a
// header that has a matching source file in the build.

GameView::GameView(QWidget* parent)
    : QWidget(parent)
{
    // Connected here rather than by the hub, so a game that was opened earlier
    // and left in the background is laid out for the current setting when the
    // player returns to it. Connecting only the visible view is the bug the
    // uitest block `legibilityReachesBackgroundGames` exists to catch.
    connect(&Legibility::instance(), &Legibility::changed, this, &GameView::applyLegibility);

    // Every game already writes one sentence per move for the status bar, so
    // captionText() takes it from here rather than making twelve games keep a
    // second copy of a string they have already composed.
    connect(this, &GameView::statusChanged, this,
            [this](const QString& text) { m_lastStatus = text; });
}

QFont GameView::captionFont(const QRectF& area) const
{
    QFont f = font();
    f.setBold(true);
    // Tied to the width of the surface it is drawn on: the whole complaint
    // this answers is text that does not grow when the window does. Clamped at
    // both ends so a tiny board still gets a readable sentence and a maximised
    // one does not get a headline.
    f.setPointSizeF(std::clamp(area.width() * 0.034, 11.0, 20.0));
    return f;
}

void GameView::paintStatusCaption(QPainter& p, const QRectF& area, Qt::Alignment where) const
{
    if (!Legibility::instance().enabled())
        return;
    const QString text = captionText();
    if (text.isEmpty())
        return;
    Theme::paintCaption(p, area, text, captionFont(area), where);
}

double GameView::captionBand(const QRectF& area) const
{
    if (!Legibility::instance().enabled())
        return 0.0;
    const QFontMetricsF fm(captionFont(area));
    // Capped at a fraction of the surface, and the cap is not tidiness: the
    // band is subtracted from the height a card game solves its card width
    // from, and fm.height() is a property of the PLATFORM's font rather than
    // of this code. windows-2022 under the offscreen platform has no font
    // environment at all and measures digits at the full em box, so an
    // uncapped band would be far wider there than here and could drive a card
    // below CardArt::kFaceMinWidth on a runner and nowhere else — which is
    // the shape of defect this project has already paid three red Windows
    // legs for. A capped band can be narrower than the sentence needs, and
    // then the caption simply overlaps a little; a faceless card cannot be
    // recovered from at all.
    return std::min(fm.height() * 3.1, area.height() * 0.22);
}
