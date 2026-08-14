#include "gameview.h"

#include "legibility.h"

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
}
