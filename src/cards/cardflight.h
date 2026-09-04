#pragma once

#include "cards/card.h"

#include <QPointF>

#include <vector>

// One card travelling from where it was to where it now is.
//
// Motion is information: it answers *what just changed and where did it go*,
// which a redraw of the finished position cannot, because by the time you look
// the change is over. That is GHUB-0065, and it matters most for a card that
// leaves on its own -- a double-click sending one home, a completed Spider run
// harvesting itself -- where the player never touched the destination and has
// nothing to look at.
//
// Presentation only. A rules core has no business knowing that a card takes
// time to arrive, so this sits in GAME_VIEW_SOURCES even though it needs
// nothing from QtWidgets -- the same reasoning legibility.cpp carries.
//
// Shared rather than written a fourth time, because Canasta already paid for
// the traps and CLAUDE.md records them. Two are handled here. suppressAt()
// consumes ONE flight per answer, so two identical cards arriving together
// suppress two destination copies rather than one twice -- routine rather than
// exotic wherever a game shuffles more than one pack. And a flight carries a
// destination captured when the card left, so anything that moves the layout
// must clear the flights or a card lands where its target used to be; that one
// cannot be fixed here and is the caller's to honour.
namespace cardflight {

struct Flight {
    Card card;
    QPointF from;
    QPointF to;
    double progress = 0.0;   // 0 at `from`, 1 at `to`
    double delay = 0.0;      // seconds still to wait before setting off
    double speed = 3.0;      // progress per second
    int destination = -1;    // whatever key the game identifies a pile by
};

// How long a card should linger before setting off when several leave at once,
// so a run reads as a sequence rather than a single smear.
inline constexpr double kStagger = 0.055;

// Moves every flight on by dt seconds and drops the ones that have arrived.
// Returns true while anything is still in the air.
bool advance(std::vector<Flight>& flights, double dt);

// Where a flight is now. Eased at both ends: a card that starts and stops
// abruptly reads as a jump rather than a journey, which is the whole point.
QPointF positionOf(const Flight& flight);

// True if `card` is still on its way to `destination`, in which case the
// destination must not draw it yet -- otherwise the card is on screen twice and
// the eye sees it arrive before it has.
//
// `consumed` is per-repaint scratch. Size it to the flights and clear it at the
// top of every paintEvent; it is what makes the match one-per-flight.
bool suppressAt(const std::vector<Flight>& flights, std::vector<char>& consumed,
                int destination, const Card& card);

} // namespace cardflight
