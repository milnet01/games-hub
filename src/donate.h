#pragma once

// The one place that decides when the app asks for a donation.
//
// Every 150th launch shows the prompt: roughly once every few months at a game
// a day, which is the point — rare enough that it is a request rather than a
// nag. The count is per PROCESS, not per game opened, so an evening spent
// playing six games still counts as one launch.
//
// A `--game` launch advances the count and shows nothing (main.cpp decides
// that, not this file). A prompt at the tile grid is an interruption; one that
// lands on your turn is a different thing. So a skipped 150th launch is
// deliberate rather than an off-by-one.
namespace donate {

// Launches between prompts. The arithmetic below is the only thing that reads
// it, so changing the interval is changing this number and nothing else.
inline constexpr int kPromptEvery = 150;

// True when this 1-based launch number is one that owes a prompt. Pure, so the
// off-by-one everyone remembers — a prompt on every launch, or one that never
// fires — is checkable without touching stored settings.
constexpr bool launchOwesPrompt(int launchNumber)
{
    return launchNumber > 0 && launchNumber % kPromptEvery == 0;
}

// Counts this launch and returns its 1-based number. Written to disk as it is
// read, so a process that is killed rather than closed still counted.
int recordLaunch();

// Whether the player still wants to be asked. The prompt offers to turn this
// off, because a popup that cannot be silenced is one you learn to dismiss
// without reading — which loses the request as well as the goodwill.
bool asksEnabled();
void setAsksEnabled(bool on);

// Counts this launch and says whether it owes a prompt. The count advances
// either way: a player who turned the asking off should not have it start
// again at the wrong moment if they turn it back on.
bool recordLaunchAndAsk();

} // namespace donate
