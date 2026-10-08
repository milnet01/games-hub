---
paths:
  - "src/**"
  - "tests/uitest.cpp"
---

## Photographing and timing a game

Moved from `CLAUDE.md` § Commands and § Traps worth knowing.

```bash
# Photograph a game instead of playing it. Needs no display, no compositor and
# no injection tool, so it works here under Wayland, over plain SSH, and on a
# CI runner. --legible turns large play on for the shot without writing it to
# settings; --seed pins the deal so two shots can be compared; --turns plays the
# game forward first. Use it before reasoning about a layout: this is the only
# thing in the project that can SEE one.
QT_QPA_PLATFORM=offscreen ./build/gameshub --shot /tmp/hearts.png \
      --game hearts --size 1400x620 --legible

# Unlike playing, an unknown --game REFUSES rather than falling back to the
# grid; a malformed --size refuses rather than picking another size; and --turns
# refuses any game that does not override the hook, which today is all but
# Canasta. A picture of the wrong thing is the
# one failure a screenshot cannot survive: it still gets written, and it still
# looks like an answer.
```

**A shot is taken the moment a game opens, which for a card game is a deal and
nothing else — so `--turns` plays it forward first.** Anything that only exists
once a hand has been played (Canasta's melds, its canasta stack, a frozen pack)
is otherwise invisible.

```bash
# A frozen pack, mid-hand, with the House rules in force.
XDG_CONFIG_HOME=/tmp/shot QT_QPA_PLATFORM=offscreen ./build/gameshub \
      --shot /tmp/canasta.png --game canasta --size 1400x760 --seed 5 --turns 44
```

`--turns` runs the game's own turns with every seat played by its computer,
**seat 0 included** — the board otherwise stops dead on your turn, which reads
as the flag not working.

`GameView::advanceForShot` is the hook, it returns false by default, and
**that default is the refusal**: a game refuses because it has not overridden
the hook, never because it has no turns. Chess and Hearts have turns and
computer opponents, and Snake has a clock that moves the game on without you;
all three refuse today, because Canasta is still the only override. Making a game photographable mid-play IS writing one.

Its contract is **synchronous, unanimated and silent**, which is a rule about
how the turns are DRIVEN rather than about tidying up afterwards.

**Drive them through the rules core, never through the game's own presentation
path.** That path advances the engine right enough — it is the animation it
adds that cannot survive: it launches flights, plays sounds and sets a pause
between turns, and those finish under a `QTimer` that cannot fire inside a
synchronous call. An override built on it therefore returns with cards still in
the air, and photographs them half-way to somewhere. **Then settle before
returning** — clear pending flights, selection, pauses and any celebration, and
re-sort.

Nothing catches either breach: the ctest case photographs Canasta only, and a
wrong picture still looks like an answer.

`--seed` pins the shuffle, so two runs deal the same cards and a before-and-after
pixel count measures the change rather than the deal. It reaches every deal,
board and computer seat through `src/dealseed.h`, and must be set before a game
is built — the seeds are member initialisers.

**So a new game takes its randomness from `dealSeed()`, as a member
initialiser**, and a game that reaches for `std::random_device` or
`QRandomGenerator` instead drops silently out of `--seed`'s reach. Nothing
catches that: the seeded ctest case never compares two shots, so it passes
either way.

**Point `XDG_CONFIG_HOME` at a scratch directory holding a hand-written
`GamesHub/Games.conf` for anything that depends on a stored setting**, so the
owner's real settings are never touched. Canasta's House set needs `useHouse=true`
as well as the `house\...` keys, **both under a `[canasta]` group**: the rule set
in force is a separate setting, and without it the toolbar comes up Classic and a
house-rule layout never appears. The keys are the ones `canastaview.cpp` writes,
not the `Rules` field names — `teeFreeze`, not `freezeCardMakesATee`.

This replaced a throwaway harness of five source edits that had to be rebuilt and
reverted each time. It found a four-canasta stack whose badges landed on top of
one another, and that stack hanging over the edge of its band — neither flagged
by any arithmetic in the project.

**`--bench` is the only thing here that can time a frame**, and it is what
makes a painting change provable. It prints ms/frame for Canasta mid-deal and
at rest, a full Klondike tableau, a FreeCell board and the tile grid, and
**reports rather than asserts** — a frame time is a property of the machine.
Take it before and after, never one reading in isolation.

**Comparing two shots of a card game needs `--seed`, and without it the number
you get is the shuffle.** The deal is random per launch, so two runs of one
build differ substantially — two confident wrong readings came of that before it
was spotted (GHUB-0093), and both figures that item records are UNSEEDED, so
neither is a noise floor for a seeded run. With the same seed two runs are
byte-identical, measured with `cmp`, which is what makes a pixel diff mean
anything: any difference at all is the change. A
deterministic surface like the tile grid still matches itself with no seed at
all.
