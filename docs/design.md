# Games Hub — Design

> **Purpose — so the shape is decided once, and anyone can tell where a
> new piece of work belongs and what it is allowed to touch.**

**This document is a gate.** Work is not broken into items until it is
agreed — `~/.claude/workflow.md` § 2. It passes when someone can take any
item off the queue and say which part it belongs in and what it may
touch.

**Status:** in force. These notes lived in `CLAUDE.md` until GHUB-0180 moved
them here on 2026-09-10. `CLAUDE.md` keeps what every session needs on every
turn: building, testing, the local gate, committing, releasing and the core
rules.

## The parts

One line each: what the part owns, then its files.

- **The hub** — the tile grid, one page per game, window sizes and saved
  games. `src/hubwindow.*` and `src/main.cpp`.
- **The game contract** — what every game offers the hub. `src/gameview.*`.
- **App-wide services** — legibility (`src/legibility.*`,
  `src/legiblefont.h`), sound (`src/sound.*`), best scores (`src/scores.*`),
  the donate prompt (`src/donate.*`, `src/donatedialog.*`,
  `src/funding.h.in`) and shared colours and materials (`src/theme.*`).
- **The deal seed** — `src/dealseed.*`, the one core file no game owns.
- **Shared card code** — `src/cards/`. § Cards says what each file does.
- **Each game** — a rules core and a view, in its own directory under
  `src/`. Its section under § The games names both.

### The game contract

`src/gameview.h` — `GameView`, the contract between hub and game: a QWidget
that offers `gameActions()` for the toolbar and emits `statusChanged`. It also
declares `applyLegibility(bool)`, called when the hub's legibility switch
moves; the base constructor is what makes that connection, so **every**
constructed game hears about it and not just the one on screen. `gameview.cpp`
holds that constructor, the caption helpers below, and nothing else — a
Q_OBJECT class needs a matching source file in the build or AUTOMOC generates
no metaobject for it.

Four members are what a new game's legibility pass is built from, and using
them is cheaper than inventing anything. `captionText()` defaults to the last
string the game emitted through `statusChanged`, which the base remembers, so
no game keeps a second copy of a sentence it has already composed; override it
only where the surface needs something the status bar does not carry.
`paintStatusCaption()` draws that sentence on a plate and does nothing at all
while the switch is off, so a pass is three lines at the end of `paintEvent`.
`captionBand()` is the strip to keep clear for it — subtract it from the
height the game lays out in. It is a **fixed** two-line strip, capped, and
deliberately not the height of the current sentence; under the cap a caption
may overlap the board slightly, and that is the accepted outcome rather than
a bug. § Legibility's caption-band paragraph owns both reasons. `smallestCardWidth()` is the
narrowest card the game draws, **at the smallest scale it draws one at**.
**Every game overrides it** — a card game with that width, a game with no
cards with `0.0`. The two values are different answers, not one: `0.0` is a
game saying it draws no cards, and the inherited `-1.0` is nobody having
said. `cardsKeepTheirFaces` FAILS on a `-1.0` rather than skipping past it,
so a fifteenth game that leaves the default reddens the suite instead of
vanishing from it.

**`applyLegibility` is never called at construction** — `gameview.h` says so
— and games are built lazily, so a game opened for the first time while the
switch is already on gets no callback at all. Every game here is safe from
that because the switch is read **live**, at the point it is used — in the
game's own `paintEvent`, font accessor or `minimumSizeHint`, or on its behalf
by the inherited `paintStatusCaption()` and `captionBand()`, which read it
live inside `gameview.cpp`. **Three games never name `Legibility` at all** —
Hearts, Pyramid and Snake — and are correct as they stand; inheriting the read
is compliance, not a gap to paper over with a redundant call.
**That is the rule, not an accident.** A game that instead caches anything
derived from the switch must read it in its own constructor as well, and no
test catches the omission — `everyGameAnswersTheSwitch` builds every game
with the switch off, so a game that ignores its initial state looks perfect.

`deactivate()` is not optional for a game with a clock or an animation, and
`tests/uitest.cpp` asserts it over all fourteen: no game may still be moving
once the hub has left it. Pinball had no override at all and its ball kept
rolling — and draining — on a table nobody was looking at (GHUB-0073).
**What that assertion can catch is a game that is actually in motion when the
hub leaves**, and that is a weaker net than it sounds: every clock here except
Pinball's is idle on a freshly opened board, so deleting its `stop()` reddens
nothing. Measured, by deleting Sudoku's and watching the suite stay green.
**That is how Snake and Hearts kept running through every green run** — both
owned a timer and neither overrode `deactivate()`, so leaving Snake mid-game
drove the snake into a wall and leaving Hearts finished the hand without you.
Both were fixed. **The rule to apply is structural, not observed: a game that
schedules ANY timer work overrides `deactivate()`** — a member `QTimer` or a
bare `QTimer::singleShot` — whether or not a test can see it running.

**The single-shot is the easier half to miss, and it cannot be stopped once
posted.** So what it needs is not a timer to stop but a guard the callback
reads when it arrives — and **`deactivate()` is what invalidates that guard**,
which is the whole reason a game with no member timer still overrides it.

**Chess, Draughts and Reversi each carry BOTH halves, and they cooperate.**
`m_generation` is stamped before the work is posted and bumped on teardown,
so a callback arriving late can see that it is stale; `m_thinking` says a
reply is still owed. Neither alone is enough — a bare `!m_thinking` cannot
tell a callback posted before `deactivate()` from one posted by a think
started after it, and a generation stamp alone does not stop a callback
firing during a live but paused think. Copy both. A single-shot that opens a
DIALOG is guarded instead by `GameView::announceLater()`, whose header says
why and which games use it; `everyDelayedDialogIsGuarded` in
`tests/uitest.cpp` fails a bare one (GHUB-0179).

**`deactivate()` freezes a game; it does not settle it, and the two are
different.** A board frozen mid-deal is static and will pass any stillness
probe while still holding state the next settings change consumes — Canasta's
cards in flight carry a destination captured when they left, so
`applyLegibility` lands them, correctly and irreversibly.
`hasPendingAnimation()` is how a game says so, and Canasta is the only one
that answers true. **Asking pixels instead does not work**: a staggered deal
has lulls where every remaining card is counting down its delay, so two
matching renders mean nothing. That mistake passed here every time and
reddened both CI legs.

### The hub

`src/hubwindow.*` — the tile grid and one page per game in a `QStackedWidget`.
Games are constructed lazily on first open. Each tile paints its own
miniature; `openGameNamed()` backs the `--game` flag. It also owns two things
every game inherits: window size and position, kept **per page** so each game
reopens the size it was left, and saved games — a game that overrides
`GameView::saveState()`/`restoreState()` is stored and restored the next time
it is opened, with no save dialog anywhere. An empty state means
"nothing worth keeping" and clears the stored one, which is how a finished
game avoids resuming onto its own final scores. `geometryKey()` and `saveKey()`
build those keys from the game's id, `Entry::name`, never from the label the
tile shows: a label is translated, and a key built from one would move with the
language (GHUB-0161). Renaming an id orphans its saved position and its window
size, silently and with no migration. Changing an id is a decision, not a
tidy-up.

**`saveState()` is called on a one-second tick while the game is on screen,
not only on the way out**, so it has to stay cheap — the dearest today is
Canasta at 0.008 ms. That tick is what bounds a crash, a kill or a power cut
to a second of play, and what stops two copies of the app settling by
whichever exits last. **The tick only ever writes; it never clears.** A game
can be momentarily empty mid-play — a deal still arriving, a hand finished
and not yet re-dealt — and clearing there would throw away a position still
being played, so only the exit paths clear. `writeIfChanged()` is what makes
the tick nearly free: an unchanged value costs a comparison and no write.

Best scores are not part of this and never were: `scores.cpp` writes and
syncs on every change already.

**A `QStackedWidget` takes the largest minimum size of every page it has
built, so one page can decide how small every other one can be made.** The tile
grid was doing exactly that: fourteen 190-pixel tiles are five rows deep, which
put a floor of about 1170 pixels on Chess as well as on the grid — taller than a
1080p screen, and the opposite of the README's opening promise. The grid now
lives in a `QScrollArea` and asks for nothing. `HubWindow::kFitsBesideYourWork`
(960x1000) is that promise written as a number — half a 1920x1080 desktop
across, its height less a panel and a title bar — and **any** game whose minimum size
exceeds it fails the check in `tests/uitest.cpp` — in either legibility state,
so a per-game legibility pass that raises a minimum is bound by it too.
Canasta already sits at 908 wide against a 960 bar, which is 52 pixels of
headroom rather than a comfortable margin. **That check gives each
game its own hub**, because measured through one window every game reports the
worst one's floor and thirteen innocent games go red together.

**A widget that resizes its own window after lowering its minimum must let the
layout catch up first.** `setMinimumSize()` lowers *that widget's* floor, but
the hub's minimum is computed from its central widget **through a
`QStackedWidget`**, and that chain is recalculated lazily — so a `resize()`
issued in the same breath is clamped straight back up by the stale figure.
Canasta's legibility switch was one-way inside the hub for this reason
(GHUB-0072), and **its own reversibility check passed the whole time**: that
check uses a bare `CanastaView`, where `window()` is the view itself and the
stale chain does not exist. A widget test can be green about a bug that only
exists in the real window. Walk up to the window activating each layout before
resizing.

### Legibility

`src/legibility.*` — `Legibility`, the app-wide legibility preference
(`display/legibility`, default off). It is no longer the only thing this app
stores outside a game's own group: `audio/muted` (`sound.cpp`), `donate/ask`
and `donate/launches` are app-wide too, and `window/geometry/<page>` and
`saved/<game>` are written per page and per game by `hubwindow.cpp`'s
`geometryKey()` and `saveKey()`. **Anything sweeping stored state — a settings
reset, a migration — has every one of those families to handle, PLUS every
per-game group**: `scores.cpp` writes `<game>/best_*` keys and Canasta keeps its
House set under `canasta/house/` and its target under `canasta/target`, so a
reset that clears only the app-wide families leaves a rule set and a score table standing, and getting it wrong is quiet: clearing the donate
switch but leaving the counter at 149 fires the prompt on the very next
start, and leaving `saved/` behind resumes games a "reset" was meant to
forget.
A singleton like `Sound`, but
broadcasting: games are built lazily and live for the session, so one built
before the switch moved would never learn without the signal.
`docs/specs/GHUB-0017-legibility-switch.md` is the contract. **All fourteen
per-game passes have shipped** — Canasta (GHUB-0038) and Sudoku (GHUB-0039)
first, both described in § Canasta and § Sudoku, and the other twelve as GHUB-0071.
**A pass does not have to be a caption, and three of the fourteen are not.**
Four shapes, covering all fourteen. **Ten reserve a band and draw a caption.**
**Hearts draws a caption but reserves nothing** — it puts the sentence in the
gap that already exists between the trick and your hand, because its hand is
anchored to the bottom and a band would take space off the cards instead.
**Pinball grows what it already says** rather than adding a caption: the
backglass and both labels on it, together. And **Canasta and Sudoku predate
the caption entirely** — Canasta's pass is a raised minimum size and Sudoku's
is bigger pencil marks, both in their own sections. What is required is that the
game answer the switch, not that it answer in one particular shape.

**Canasta, Sudoku and Hearts reserve no band, and neither Canasta nor Hearts
may ever be given one**: Canasta's melds clear `kFaceMinWidth` by 0.4 px and
Hearts' trick by 1.2, and a band comes off the height each of them solves its
card width from. Sudoku draws no cards, so it has no such floor.

What holds that true is not the count: `tests/uitest.cpp` walks the games the
hub can open, renders each at its own smallest size with the switch off and
on, and asserts the picture changed and then went back. **A fifteenth game
added without a pass reddens that block**, which is the point of it — it
lands in `silent` if it holds still, and fails the `deactivate()` assertion
if it does not. **Sudoku is excluded by name**, because its pass grows pencil
marks and a freshly generated board has none, so all three renders match at
any window size; its own block puts a mark in every cell that will take one
before asserting.

**The caption band is a FIXED height — two lines of the caption font plus its
plate, `fm.height() * 3.1` — and it is capped, and both are load bearing.** A
band sized to the current sentence would resize the board every time the
sentence changed length, and a board that jumps between moves is
worse than a slightly smaller one. The cap — 22% of the surface's height — is the
Windows lesson again in a new place: the band comes off the height a card game
solves its card width from, and `fm.height()` is a property of the platform's
font, not of this code. `windows-2022` under the offscreen platform has no font
environment at all, so an uncapped band would be far wider there than here and
could drive a card below `CardArt::kFaceMinWidth` on a runner and nowhere else.
A capped band can be narrower than the sentence needs, and then the caption
just overlaps a little; a faceless card cannot be recovered from.

### The donate prompt

`src/donate.*` and `src/donatedialog.*` — the one place the app asks for
money, reached from Help → Support this project and from the every-150th-
launch prompt. **A `--game` launch advances the count and shows nothing** —
a startup prompt is an interruption, but one landing on your turn is a
different thing — so a launch that skips the prompt is deliberate rather
than an off-by-one. `donate::launchOwesPrompt` is pure so the off-by-one everyone
remembers is checkable without touching stored settings; the counter is per
process, written as it is read, so a killed process still counted. The links
themselves are generated, never written — see below.

**The three donate URLs are generated from `.github/FUNDING.yml` at configure
time and must never be typed into a C++ file.** That YAML is already GitHub's
sponsor button, so it is the copy that stays maintained; a list in a .cpp is a
second copy that drifts the day one link changes, with nothing to catch it.
`CMakeLists.txt` writes `funding.h` from it and **stops the build on a funding
key it has no rule for** — a dropped link otherwise looks exactly like a working
build. A uitest check counts the keys back out of the YAML as a second guard.

**A new platform's rule goes in `gameshub_funding_entry()`, not under
`custom:`.** The handle-to-URL stems (`https://github.com/sponsors/`,
`https://www.patreon.com/`) do live in `cmake/funding.cmake`, because
FUNDING.yml stores account names rather than addresses for the platforms GitHub
knows — so the ban above is on C++, and that function is where a rule for
`ko_fi:` belongs. Routing it through `custom:` does not work either: the rule
reads one URL from a `custom:` line, though GitHub's `custom:` key takes a list,
so a two-entry list stops the build whether it is quoted or not. The `funding`
ctest case locks that. Widening the rule is the fix if a second custom link is
ever wanted.

## What may depend on what

`CLAUDE.md` § Core rules owns the rule that a rules core never includes a
widget, and says what else keeps a file out of the core. This section records
how that split came to hold, and the edges between the other parts.

**The split now holds for all fourteen.** `GAME_CORE_SOURCES` in `CMakeLists.txt` is
the list of record; `gameshub_core` is built from it and `gameshub_selftest`
links that — so a rules check for any game can be written straight into the
self-test. Six games held their rules
inside the widget until GHUB-0066 closed on 2026-08-25 (Klondike, Spider,
FreeCell, Pyramid, Snake and 2048); the split found two shipped bugs that
nothing could have caught while it did not hold, GHUB-0125 and GHUB-0126.

**No game includes `hubwindow.h`.** Only `src/main.cpp` and the hub itself do,
so a game reaches the hub only through what `GameView` declares.

## What every part does the same way

Each of these has one home:

- **Teardown when the hub leaves a game** — § The game contract.
- **Answering the legibility switch** — § The game contract, for how a game
  hears it; § Legibility, for what a pass may look like.
- **Randomness** — `dealSeed()`, as a member initialiser. `CLAUDE.md`
  § Commands owns that rule, beside `--seed`.
- **Words a player reads** — every one goes through Qt's translation lookup,
  and anything a program reads instead carries `// untranslated: <reason>`.
  `docs/specs/GHUB-0161-translatable-text.md` owns the rules; the
  `translatable` ctest case enforces them.
- **Saves** — below.

### Saves

**A game's save is the moves that made it, not the position it reached** —
where the game keeps a history at all. Chess saves its move list and replays it
through `ChessGame::play()`, which rebuilds the board, the undo stack and the
position keys threefold repetition counts from one list; storing the position
instead would need a second copy of each, free to drift. Each move is matched
against `legalMoves()` on the way back in, so the file supplies from/to/promotion
and the generator supplies the castling and en-passant flags — a save that is
not a game this build would play is refused rather than half-loaded. Canasta
cannot do this (it has no move log, so its engine serialises directly), which is
why the two look different; prefer Chess's shape when a game offers the choice.

**A game with no move log saves the table, and then the PACK is what re-checks
it.** The four solitaires keep piles rather than moves, so there is nothing to
replay against the rules. What stands in for that is `cardcodec::matchesPack` —
Klondike and FreeCell never take a card out of play, so the whole deck must come
back, nothing missing and nothing doubled. Spider, Pyramid and Hearts do remove
cards (a harvested run, a matched pair, a collected trick), so they get
`fitsPack` plus a count of their own — Hearts' is that hands plus the cards on
the table equal fifty-two less four per trick collected, without which a blob
restores a full hand into the endgame. Without that check a corrupt blob
restores into a deal that cannot be won, and the player finds out an hour later.
Minesweeper, Reversi, Draughts, Sudoku and 2048 have no pack at all, so what
stands in is the check their `restore()`/`load()` makes, refusing a board the
game could not have reached, in a core in all five cases since GHUB-0066 split
the last of them out: the mine count must match the level and
the numbers are recomputed rather than read, a Reversi board must hold at least
the opening four discs, a draughts piece may not stand on a light square, a
Sudoku solution must be a completed grid that every clue agrees with, and
every 2048 tile must be a power of two. **A drag is the other half:** a run
lifted in mid-drag has been erased from its pile and lives in `m_drag` until it
is dropped, so each `saveState()` writes it back onto the pile it came from —
otherwise closing the window with a card in hand loses it, and the pack check
then refuses the save it just wrote.

## The stack, and what it rules out

C++20 and Qt 6 Widgets, at least Qt 6.5, built with CMake. `CMakeLists.txt`
declares all three. Multimedia plays the sounds. Concurrent runs the
computer's search in Chess, Draughts and Reversi off the main thread. No
runner-up was recorded when the project started.

**It rules out QML.** No Qt Quick module is linked, so every game paints its
own surface in a `paintEvent`.

**It includes MSVC.** CI's Windows leg builds with it, so the code has to
compile there as well as under GCC. `CLAUDE.md` § Traps worth knowing lists
what that has already cost.

## Close calls

No ADRs yet, and `docs/decisions/` does not exist. Each decision taken so far
sits with the part or game it shaped.

## The games

Each section opens with the game's rules core, which is listed in
`GAME_CORE_SOURCES`, and its view.

### Cards

Shared by the card games. Core: `card.*`, `cardcodec.*`. View: `cardart.*`,
`cardflight.*`. All in `src/cards/`.

`cards/card.*` (deck building; `makeDeck(decks, suitsUsed,
jokers)` is what gives Spider its difficulty and Canasta its 108-card pack)
and `cards/cardart.*` (shared drawing, so the card games look like one deck).
A joker is rank `kJoker` = 0, which sorts below every real rank and so never
collides with the arithmetic other games do. `Card::deck` records which pack
a card came from and decides only the colour of its back — Canasta shuffles
two packs together, so its stock shows red and blue backs mixed the way a
real table does. It is deliberately outside `operator==`: two red kings are
the same card whichever pack they came from. `cards/cardcodec.*` is the third
shared piece: piles in and out of a `QDataStream`, plus `fitsPack` /
`matchesPack`, which are what a table-based save has instead of Chess's
legal-move check — see § Saves.

**Card art is cached, and the cache key must decide the picture completely.**
`CardArt::paintFace`/`paintBack` snap a card to whole device pixels, draw it
once and keep it. Each of the following cost real time to learn. **The key is
computed from the SNAPPED size and the card must be drawn at that same snapped
size** — key on the rounded size while drawing at the exact one and two rects a
fraction of a pixel apart share an entry, so whichever drew first decides the
picture and a frame after an eviction differs from the same frame before one.
`cardArtKeyDecidesThePicture` is the guard. **Key on the card, never on the
padded pixmap**: the padded pixel size is a `std::ceil` away, and a product
landing a fraction above a whole number rounds up — so at a fractional device
pixel ratio, which is what a 150% desktop scale gives, two cards a whole device
pixel apart keyed the same. Ratios of 1 and 2 divide exactly and never show it,
which is why every check written before that one passed against it.
**The shadow padding must be a whole pixel**, or the card sits at a fractional
offset inside the pixmap and every line
antialiases differently. And **a rotated FACE is never cached** — resampling
softens it, and this game is read by pip pattern. Rotated backs are cached
because there is nothing on a back to read.

**Card corner text needs room for two characters and a descender.** A box half
a card wide clipped "10" to a stray stroke and cut the tail off "Q".

**`CardArt::paintFace` stops drawing the face below `CardArt::kFaceMinWidth`
(46) pixels wide** and leaves
only the corner index, because pips are unreadable smaller than that. That
constant in `cardart.h` is the number's only definition, and
`scripts/legibility-check.py --thresholds` fails if any other source states it
as a literal. It runs as the `legibility` ctest case rather than by hand. A game holding the threshold must hold it at the **smallest scale
it draws a card at**, not at 1.0 — Canasta's melds at 0.74 need `cardWidth()`
≥ 62.2, not 46. Anything
drawing cards at reduced scale — Canasta's melds are `kMeldScale` = 0.74, its
opponent hands 0.8 — gets a stack of
slivers rather than cards, and has to name them some other way. The melds carry
a "K ×5" badge for exactly this reason. Check the width before assuming a face.

**The five card games besides Canasta do not need a size pass, and the floors that say
they do are unreachable.** Measured, not assumed: every card view calls
`setMinimumSize(<ThisView>::minimumSizeHint())` in its constructor — qualified
by the class on purpose, since an unqualified virtual call there can only ever
reach this class's override anyway and saying so is what stops it reading as a
missed dispatch — so the smallest card each can actually
reach — at its smallest window, **with the legibility switch on** — is Klondike
67.9, FreeCell 59.4, Spider 54.2, Pyramid 49.0, Hearts 47.2 and Canasta's melds
46.4, all clear of `kFaceMinWidth`. **The band is subtracted only by the four
that reserve one**, so Hearts' and Canasta's figures carry no band cost at all
and would not survive one being added. **Hearts' figure was 52.5 until its
`smallestCardWidth()` was corrected**: it reported the hand card, and Hearts
draws the TRICK at 0.9 of one, so the published floor was overstated by that
factor. **Pyramid's was 52.1 until its height budget was corrected** to the fan
step its painter actually uses. Both are now the narrowest here after Canasta.
Klondike and Spider reserve a band and are still decided by their width at their
smallest windows, so it costs them nothing there. FreeCell joined that list on
2026-08-21 (GHUB-0083): its height budget had been assuming a column of about
two cards of fan against a deal of seven, so before the fix it was never
height-bound and the deal ran under the plate instead. `cardsKeepTheirFaces` in
`tests/uitest.cpp` prints all six every run, so these are readings rather than
history.

**A card game with animation must not let the model and the picture disagree.**
`cards/cardflight.*` is the shared way to animate a card, and Klondike, Spider
and FreeCell use it; Canasta predates it and keeps its own copy in
`CanastaView`. Each flight carries where it is going, and the destination skips
drawing that card until it lands (`cardflight::suppressAt()`, Canasta's
`suppressed()`); otherwise a card in the air is also drawn at its destination
and the eye sees it twice. **The match is CONSUMED one per flight** — the
`consumed` scratch passed to `suppressAt()`, or `CanastaView::m_consumed`,
marks a flight the moment it answers, so a second identical card in the air
finds the next unmarked flight rather than the same one. Without that, two
identical cards arriving together suppress both destination copies and one
card disappears; Canasta shuffles two packs and `Card::deck` sits outside
`operator==`, so identical cards in flight together are routine here rather
than exotic. **A caller owes three things.** Size that scratch to the flights
and zero it at the top of every `paintEvent`. Clear the flights whenever the
layout moves — a flight's destination was captured when the card left, so it
would otherwise land where its target used to be. And a game whose deal
animates answers `hasPendingAnimation()` from its flights, per § The game
contract.

### Chess

Core: `chessboard.*`, `chessai.*`. View: `chessview.*`, `chessart.*`. All in
`src/chess/`.

`chess/chessboard.*` is the rule set, wrapped in `namespace chess`
because the draughts core already owns `Side`, `Piece` and `Square` at global
scope and the self-test includes both. `Board` is the position alone —
fixed-size, heap-free, copied per search node — while `ChessGame` adds the
position history that threefold repetition needs, so the search never carries
it. `chess/chessai.*` is negamax with alpha-beta, quiescence and iterative
deepening. `chess/chessart.*` paints the pieces and is shared with the hub
tile. `ChessView::advance()` is the single point that moves the game on.

**Alpha-beta only resolves the BEST move's score exactly.** Every other root
move comes back as an upper bound, and a bad move whose search fails low can be
reported level with the best one. Chess's Easy and Medium levels pick at random
among moves within a few centipawns of the best, so they search a full window
at the root (`rootScores`'s `exact` flag). Hard plays only the top move and
keeps the narrow window, so `rootScores` rotates the move that actually raised
alpha to the front — without that, Hard was playing `Nf3-g1` from a normal
opening because a fail-low tie sorted to the front. The
observable symptom is an engine that is strong in tactics and absurd in quiet
positions, which reads as a bad evaluation rather than a bad window.

**A chess move generator is proved by perft, not by eyeballing.** Counting
every leaf to a fixed depth and matching the published totals for a handful of
reference positions catches castling-through-check, en-passant and pin bugs
that no amount of playing will surface reliably. `chessMoveGeneration()` in the
self-test checks four positions and runs in about 20 ms.

**The engine is bounded by a node budget rather than by depth alone.**
`planFor()` in `chessai.cpp` sets one per level; Hard's worst observed
middlegame answer is about 1.2 s. Raising the depth without raising the budget
does nothing, and raising both makes the opponent slower to answer. **The
search runs on a worker** (`QtConcurrent::run`, watched by a
`QFutureWatcher`), so a long one no longer stops the window repainting —
GHUB-0047 moved it. What the budget buys now is the player's wait, not the
frame rate.

### Reversi

Core: `board.*`, `ai.*`. View: `reversiview.*`. All in `src/reversi/`.

`reversi/board.*` is the rule set, funnelled through
`Board::ray()`, which walks one of eight directions and returns how many
discs are bracketed. `reversi/ai.*` is negamax with alpha-beta over copied
`Board`s (`Board` is a value type and the search relies on that). Depth is
1/4/6 by difficulty. `ReversiView::advance()` is the single point that moves
the game on — player move, engine reply and forced pass all route back into
it. Change flow there, not by adding a second path.

### Draughts

Core: `draughtsboard.*`. View: `draughtsview.*`. Both in `src/draughts/`.

### Minesweeper

Core: `minefield.*`. View: `minesweeperview.*`. Both in `src/minesweeper/`.

Mines are laid on the *first reveal*, excluding that square and its neighbours, so the opening click is
always safe and always opens a blank area. The flood fill is iterative; a
recursive one overflows the stack on an Expert-sized blank.

### Klondike, Spider and FreeCell

Klondike is the tile named Solitaire. Cores: `src/klondike/klondiketable.*`,
`src/spider/spidertable.*`, `src/freecell/freecelltable.*`. Views:
`klondikeview.*`, `spiderview.*`, `freecellview.*`, each beside its core.

Klondike and Spider both keep piles as
`std::vector<Card>`. A drag asks the core to `lift()` the run, draws the view's
copy in `m_drag`, and hands a failed drop back with `putBack()`. Card width is solved from the row cost
(`7w + 6·gap` for Klondike) — assuming a fixed pixel gap pushed the last
column off screen.

**All three cores own the LIFT, and two of them keep the run**, which is
deliberate. Klondike and Spider turn over whatever a run was covering when it
lands, so the drop has to know where the cards came from; keeping the run in the
table (`m_held`, and `held()` to read it) means the view cannot tell it wrong.
FreeCell hands the run back because it uncovers nothing, and keeps only the
column it came from. **In all three the undo snapshot is
banked when the run is LIFTED, never when it is dropped** — the drop happens
after the cards have left their pile, so a snapshot taken there is of a table
they were never on. That is GHUB-0126, and it lost the card in two of the three.

**A card size solved against the DEAL is not solved against play, and the
answer is a tighter fan rather than a smaller card.** Klondike, Spider and
FreeCell all size their card for the board they deal; a column then collects
cards — a king and its run onto an emptied column is routine, and turning a
face-down Klondike card more than doubles its step. Sizing for the worst case
instead would take width off every card at every window whether or not any
column ever grew, which is the wrong trade for a game read by pip pattern. So
each compresses an overlong column: FreeCell caps its single step, Klondike and
Spider scale the whole column, which is what keeps the ratio between a
face-down and a face-up step — the thing that says at a glance how much of a
column is still to turn. Each has `deepestColumnBottom()` and
`roomForColumns()` so a test can ask; no rendered picture answers it, because a
card under an opaque caption plate looks like one that is not there. Their `std::max(30.0, …)` … `std::max(34.0,
…)` floors read alarming and no window can drive them there. Canasta was the
only game drawing faceless cards, and only in its melds: the opponents' hands
are drawn at 0.8 but face **down**, so the threshold never applied to them.

### Pyramid

Core: `pyramidtable.*`. View: `pyramidview.*`. Both in `src/pyramid/`.

### Sudoku

Core: `sudokugrid.*`. View: `sudokuview.*`. Both in `src/sudoku/`.

**`QPainter::drawText(rect, flags, text)` clips to the rect, so a font is
bounded by its LINE box and not by its ink.** Sudoku's pencil marks are the
case: nine sit in a fixed 3×3 pattern inside one cell, each centred in a cell
third, and at `cell * 0.20` the marks were already close to the largest whose
line box fits that third — so raising the ratio alone clips the top off every
mark and draws a *worse* mark, not a bigger one. `Qt::TextDontClip` hands over
the gap between the ink and the line box, and the legibility pass is what uses
it. The flag is set in both switch states because at 0.20 it changes nothing.
**A font ratio tuned on one machine is not portable, and this one cost two red
Windows CI legs to learn.** A mark tuned to 0.29 of the cell passed here and
failed on `windows-2022`, and no Linux run could have caught it:
`scripts/local-ci.sh` executes `ci.yml`'s own steps, but nothing on Linux
drives the Windows leg.

**Do not reason about it from typographic ratios — that was the second
mistake.** Measured at em 100 the faces are close (this font 0.742 of an em
bold; Segoe UI 0.728, Arial 0.731, Tahoma 0.760 unbold on the owner's Windows
box via GDI+), while the *same* font here measures about 0.685 at the 7-to-11
point sizes a mark is actually drawn at. That gap is hinting rounding the ink
by a whole pixel, not the typeface — so a figure taken at one size says very
little about the other, and comparing a large-size measurement on one platform
against a small-size one on another says nothing at all.

`CLAUDE.md` § Traps worth knowing holds the rest of this story: what the
`windows-2022` runner does to font metrics, and the testing rule it taught.

`SudokuView::markFont()` therefore **solves** rather than scales: one metric probe
fixes the font's ink-per-point, then it steps down until the ink measured at
the size it will really be drawn at fits. `marksFitAt(pointSize)` exists so a
test can ask about a size the view did *not* pick — with the size solved,
"it fits" is true by construction and only "it is the largest that fits" has
teeth. **The test that guards this loops over the machine's own font families**
(locally 0.49 to 0.99 of an em, bracketing anything a desktop would pick), and
putting the tuned constant back reddens it here rather than three minutes into
CI. Reach for that shape whenever a constant is really a property of the
platform's font, not of this codebase.

### Hearts

Core: `heartsengine.*`. View: `heartsview.*`. Both in `src/hearts/`.

`hearts/heartsengine.*` holds the whole rule set: passing
rotation, the forced two-of-clubs lead, following suit, no points on the
first trick, hearts breaking, and the moon shot (26 to everyone *else*).
`HeartsView` is presentation and timers only.

**The Queen of Spades breaks hearts here as well as a heart does.** That is a
variant rather than the strictest rule, and it is the one the widely-known
Windows version plays, so it is kept — but it was in the code and in no
document, which is what made it look accidental. **A tie is a tie**:
`winner()` returns the first seat on the lowest total, which is the human, so
ask `winnerIsShared()` before announcing a win or recording a best score.

### Canasta

Core: `canastaengine.*`, `canastaai.*`. View: `canastaview.*`. All in
`src/canasta/`.

`canasta/canastaengine.*` is the rule set, in `namespace
canasta` because `Meld`, `Team` and `Phase` are far too common to leave at
global scope. **Every number the game plays by lives in one `Rules` struct**
— card values, opening minimums, canasta bonuses, what freezes the pile —
because house rules are the norm with Canasta rather than the exception. The
game ships two sets: `Rules::classic()`, which is never edited, and a House
set the player edits in a dialog and which is saved via QSettings. Adding a
house variation should be a new field there, not a branch in the engine.
**A new field owes two more lines**: a plain sentence in `rulesInForce()`,
which is what the Rules in force panel shows, and a mutation in
`canastaRulesInForceNamesEveryRule` — that check flips each field on its own
and fails a field no sentence names, so it catches a wrong sentence but
cannot catch a field added to neither.
`canasta/canastaai.*` is judgement rather than search, so unlike Chess it
needs no work budget. **Its four levels are checked against each other, not
just described** — `canastaLevelsDiffer()` plays four rungs -- each level
against the one below it, plus **hard against easy**, which is not adjacent --
and that is how Hard was caught being weaker than Medium. Any change to one
level has to be re-measured against **every rung it appears in**, which for
Hard is three of the four.
`canasta/canastaview.*` is presentation and timing.

**Canasta's legibility pass raises the window's minimum size; it does not grow
the melds where they stand.** Growing them in place was tried first and cannot
work: a meld card wide enough to show a face makes a seven-card canasta about
130 px tall at the smallest window, and `bandFor()` gives it 107 — the overflow
runs into the stock and discard row above. So `minimumSizeHint()` returns
908×656 while the switch is on, the smallest window at which `cardWidth()`
reaches `CardArt::kFaceMinWidth / kMeldScale` unaided **at every shape the
window can take**, and every card on the table grows together. **The width half
is solved against the WORST inset, not the inset at that height** — `tableRect()`
insets by 2.2% of the shorter side, so once a window is taller than it is wide
the shorter side is the width and the table loses width as the window grows.
Solving it at the minimum height gave 900, and the card then clamped at
908×1000. That is GHUB-0153, and `cardsFitTable()` is asked at a spread of
shapes now rather than at the minimum alone, which is what hid it. **Floor, smallest scale and minimum size move together or
not at all**; `cardsFitTable()` is what asserts the floor never actually has to
clamp, because a clamped card is one the table has no room for. **And the switch
has to put the window back** — Qt clamps the window up to the new minimum and
`HubWindow::rememberPage()` writes that enlarged geometry over the stored one,
so `applyLegibility` keeps the pre-clamp size and restores it on the way out. It
clears `m_flights` first: `Flight::to` is a point captured when the card left,
so a card in the air would otherwise land where its destination used to be.

**Under `noMeldingFirstRound`, `discardRisk` is zero for every rank, so any AI
rule written in terms of it is dead code.** The rule bars melding for one round
and therefore bars taking the pile — `validateTake` refuses outright — which is
what makes a first-round throw safe. But it also means no team has melded yet,
and `discardRisk` returns 0 the moment `theirs.meldOfRank(rank)` is null. So
"the throw is free, dump your most dangerous card" cannot be built on
`discardRisk`: there is no dangerous card to find. What the AI does instead is
drop the whole `safety` accumulator in `chooseDiscard` — every judgement about
handing the pile over, gathered into one variable precisely so it can be
dropped in one place — leaving the hand-value terms to answer honestly. The
only safety term with real force in that window is Expert's `+50 ×
countRank(pile, rank)`, which is why the test that locks this uses Expert and a
king matching the up-card.

**`Engine::meldingAllowed()` and `Engine::discardCannotBeTaken()` answer about
DIFFERENT seats, and the fourth seat of the first round is where they part.**
The first is about the seat playing now; the second about the seat that plays
next, hence its `+1`. The last seat of the round is still barred from melding
while the turn after it — the first seat playing a second time — is not, so it
is the one seat in the round whose discard is live. An AI or a status line that
reads `meldingAllowed()` for "is my throw safe?" gets it right three times and
wrong on the fourth, which is the hardest quarter of a bug to notice.

**Canasta can reach a position with no legal move, and the engine has to refuse
the move that gets there.** Down to one card with no canasta, you may not go
out, and discarding your last card *is* going out — so nothing is legal and the
turn cannot end. The guard is in `keepsADiscard()`, which refuses a lay-down
leaving fewer than two cards unless a canasta comes with it. Without it the
self-test's full games hang rather than fail, which is a much worse symptom.

**Card order that the eye depends on belongs in the model, not the painter.**
A hand fans wild-cards-first and so does every meld, and both orders are made
by `canasta::sortsBefore` inside the engine. That is not a layering slip: the
flights index into `Meld::cards` and into the hand to work out where a card is
flying, so a display order held only in the painter would put the animation in
one place and the card in another. Sort before the flights are built.

### Snake

Core: `snakeboard.*`. View: `snakeview.*`. Both in `src/snake/`.

### 2048

Core: `twenty48board.*`. View: `twenty48view.*`. Both in `src/twenty48/`.

### Pinball

Core: `pinballtable.*`. View: `pinballview.*`. Both in `src/pinball/`.

`pinball/pinballtable.*` is the simulation in fixed table units
(400×720), scaled to the widget so resizing never changes the physics.
Everything collides as a circle against a fat line segment. `PinballView`
only draws it and feeds input.

**Pinball's launch is calibrated, not guessed.** `minimumLaunchSpeed()` derives
the weakest plunger from the dome height above the lane, so even a limp launch
reaches the play field. The original build used a hardcoded 620 that lifted the
ball less than half way; it fell back down a then-open-bottomed lane and
drained, losing a ball before the player touched a flipper. The lane is now
walled to the floor and a short launch re-parks the ball. `pinballLaunch()` in
the self-test locks this in across all plunger strengths.

**The one-way gate at the mouth of the lane is deliberately never drawn.** As a
visible line it reads as a wall sealing the launch lane — that was a reported
bug, not a theory.

## Cold-eyes loop log

> A design doc is gated (`documentation.md` § 9.1) and is owed no
> `## What checks this` table, so the log goes last. `review-contract`
> writes a row per loop as the loops happen; never back-fill one.

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| 1 | 2026-09-10 | 3, cold — genre pinned `adr`; all three lanes arrived holding the pre-split `CLAUDE.md` from session auto-load | 6 | 1 | 0 | n/a | **Seven findings after merging: five verified and fixed, two dismissed.** Fixed: § Chess credited Hard's guard to `rootScores`'s `exact` flag, which Hard never sets (three lanes); § Cards never described `cardflight.*`, the shared animation code, or its two caller duties (three lanes); § Legibility's stored-state list missed `audio/muted`; the one-home list sent legibility to § Legibility alone; § The donate prompt said a two-entry `custom:` list does not build, false for the unquoted form (run with `cmake -P`; code side filed as GHUB-0191). Dismissed: § Pinball's `minimumLaunchSpeed()` sentence is right and the header comment is the stale side; the `keepsADiscard()` sentence is true for the rule it describes and changes nothing built. Four stale source comments filed as GHUB-0192. Open questions resolved clean: `m_drag` against `m_held`, and `announceLater`'s guard. Sweep: no copy of a rewritten sentence elsewhere. Loop 2 dispatched. |
| 2 | 2026-09-10 | 3, cold — identical brief, packet rebuilt from disk; all three lanes again arrived holding the pre-split `CLAUDE.md` | 2 | 2 | 1 | n/a | **Five findings after merging: four verified and fixed, one dismissed. Two of the four landed on text loop 1 wrote.** Fixed: § Cards' caller duties said to clear the flight scratch where `cardflight.h` says size it to the flights and zero it (two lanes); the same sentence read as the whole list and left out `hasPendingAnimation()` for a game whose deal animates; § Klondike, Spider and FreeCell said the view lifts and restores a run, where the core's `lift()` and `putBack()` do (two lanes); § The hub never said `geometryKey()` and `saveKey()` build keys from the tile's name, so a rename orphans a player's save and window size (Q3). Dismissed: § Pinball again — the document is right and the header comment is the stale side, filed under GHUB-0192. GHUB-0191 and GHUB-0192 annotated with two more code-side cases. Sweep: no copy of a rewritten sentence elsewhere. Loop 3 dispatched. |
| 3 | 2026-09-10 | 3, cold — identical brief, packet rebuilt from disk; lanes again arrived holding the pre-split `CLAUDE.md` | 1 | 0 | 1 | n/a | **Two findings after merging, both dismissed on verification. Converged, on the last loop the ADR cap allows; nothing is deferred.** Dismissed: § Pinball's `minimumLaunchSpeed()` sentence (all three lanes) — the definition computes the weakest launch that clears the dome, so the header comment is the stale side (GHUB-0192); the packet never windowed `pinballtable.cpp`, which is why it came back every loop. And § The hub's rename sentence leaving out best scores — every score key is a fixed literal chosen by its game, so a rename leaves scores in place, and the `hubwindow.cpp` comment saying otherwise is stale (GHUB-0192). Open question resolved clean: the `m_drag` save path. Share on the triggering change: `2551608` created the whole document, so every verified finding of the run falls inside it by construction. |
