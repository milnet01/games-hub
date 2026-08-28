#include "cards/cardflight.h"

#include <algorithm>

namespace cardflight {

bool advance(std::vector<Flight>& flights, double dt)
{
    for (Flight& f : flights) {
        if (f.delay > 0.0) {
            f.delay -= dt;
            // Spend what is left of dt on the journey rather than losing it, or
            // a staggered run gains a frame of lag per card.
            if (f.delay < 0.0) {
                f.progress += f.speed * -f.delay;
                f.delay = 0.0;
            }
            continue;
        }
        f.progress += f.speed * dt;
    }
    flights.erase(std::remove_if(flights.begin(), flights.end(),
                                 [](const Flight& f) { return f.progress >= 1.0; }),
                  flights.end());
    return !flights.empty();
}

QPointF positionOf(const Flight& flight)
{
    const double t = std::clamp(flight.progress, 0.0, 1.0);
    // Smoothstep. Linear motion is legible but reads as mechanical; easing both
    // ends makes the card look like it was picked up and put down.
    const double eased = t * t * (3.0 - 2.0 * t);
    return flight.from + (flight.to - flight.from) * eased;
}

bool suppressAt(const std::vector<Flight>& flights, std::vector<char>& consumed,
                int destination, const Card& card)
{
    if (consumed.size() < flights.size())
        consumed.resize(flights.size(), 0);
    for (std::size_t i = 0; i < flights.size(); ++i) {
        if (consumed[i])
            continue;
        if (flights[i].destination != destination || !(flights[i].card == card))
            continue;
        consumed[i] = 1;
        return true;
    }
    return false;
}

} // namespace cardflight
