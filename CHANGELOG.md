# Changelog

Notable changes to the Games hub. Newest first, in the shape
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) sets out.

Started 2026-08-11, so it does not reach back to the first fourteen games —
`ROADMAP.md` and the git log carry that history.

## [Unreleased]

### Changed

- **Canasta explains a refused move on the table instead of in the status bar** (GHUB-0040)
  When the game will not let you do something it now says why on an amber
  panel just above your hand, and the sentence stays there until you make
  a move of your own. Every click used to wipe it — including the clicks
  you make while doing what it told you.

## [0.4.0] - 2026-08-19

A release about being able to read the board and being remembered: a
legibility switch that two games now answer, a Canasta computer that knows
when its discard is safe, and settings that survive finishing a game.

### Added

- **Canasta shows each team's opening minimum on its own score plate, and names the first round on the table**
  Both on the play area rather than the status bar. The centre strip says
  "FIRST ROUND — your throw is safe", and switches to "FIRST ROUND ENDS —
  the next seat can take" on the last seat of the round, which is the moment
  the protection stops.

- **The computer plays the first round knowing its throw cannot be taken, and both teams' opening minimums are on the table.** (GHUB-0035)
  Under house rules nobody can take the pile in the first round, so the computer stops wasting its black threes there - and each team's "points needed to open" now shows on its own score plate.

- **Canasta now answers the legibility switch — melds show a face instead of a sliver** (GHUB-0038)
  Turning on Large makes Canasta's window a little bigger and every card
  on the table with it, so the melded cards are drawn large enough to show
  their pips rather than just a letter in the corner. Turning it off puts
  the window back where it was.

- **A Canasta hand nobody goes out on can now be scored as dead.** (GHUB-0032)
  When the stock runs out and nobody has gone out, the round can now count for nothing instead of handing a big score to whoever sat on a frozen pile.

- **A legibility switch in the hub toolbar, read by Canasta and Sudoku so far** (GHUB-0037)
  Reads "🔍 Normal" or "🔍 Large", sits beside the sound switch, and
  is remembered between sessions. **Two of the fourteen games answer it
  in this release** — Canasta and Sudoku, described above. The rest
  ignore it for now and are being done one at a time, each shown to the
  owner before the next is started. 2048's unreadable tiles were fixed
  for everyone rather than behind the switch; that one is below.

### Changed

- **The House rules set now has "nobody lays down in the first round" ticked by default**
  Only affects a profile that has never saved house rules — a stored choice
  stays a choice.

- **Sudoku's pencil marks grow and embolden under the legibility switch (GHUB-0039)**
  The second of the fourteen per-game passes. Sudoku's contrast was
  never the problem — the pencil ink measures 4.88:1 — so the pass is
  about size: a mark grows from a fifth of a cell to the largest that
  still fits beside its eight neighbours, and is drawn bold. That size is
  solved against the font in use rather than fixed, so it is as large as
  each platform's digits allow. Nothing else moves, and no minimum window
  size changes.

### Fixed

- **Canasta's rule set and Minesweeper's difficulty are remembered between sessions.** (GHUB-0034)
  Both were already saved, but only inside a game still in progress —
  so finishing a hand under House rules, or winning on Expert, put you
  back on Classic and Intermediate.

- **The Canasta AI no longer feeds the meld you are one card from finishing.** (GHUB-0033)
  The computer used to throw away the card that completed your canasta. Now it works out which of your melds is furthest from finishing and throws into that one instead.

- **2048's numbers were unreadable on nine of its twelve tiles** (GHUB-0037)
  The ink was picked from the tile's NUMBER, so every tile from 8
  upward got near-white text on a mid-orange or mid-yellow square —
  as low as 1.50:1 where 3:1 is the minimum anyone can read. It is
  now picked from how bright the tile actually is, and every tile
  clears 4.9:1. Fixed for everyone rather than hidden behind the new
  switch: a tile nobody can read is a bug, not a preference.

- **The pre-push check no longer skips itself on a release (GHUB-0027)**
  Pushing a release sends the branch and its tag together, and the
  check that builds and tests everything first was reading only the
  last of the two. A tag on its own looks like no change at all, so
  the check quietly decided there was nothing to do and waved the
  release through. It now looks at everything being pushed. A new
  branch was skipped the same way and is fixed with it. There is a
  new test that pushes to a scratch copy and confirms which checks
  each kind of push runs.

## [0.3.1] - 2026-08-12

A release about the checks rather than the games: nothing you can see has
changed, but a broken download can no longer reach the releases page.

### Fixed

- **The release checks now start the downloaded build, not just ask it its version** (GHUB-0026)
  Both downloads are opened and run headless before they are published,
  and the check passes only if the game is still running afterwards with
  no error box on screen. The old check asked the file for its version
  number, which it answers before the graphics layer is even loaded — so
  a download that could not open a window would have passed. Both
  downloads now also carry the offscreen display plugin the check runs on.

## [0.3.0] - 2026-08-12

The first release. Fourteen games that had only ever run from a build
directory on one machine now download as a single file on Linux or Windows,
and eight of them remember where you left off.

### Added

- **Games downloads as one file, for Linux and for Windows** (GHUB-0025)
  No compiler, no build steps, nothing to install: grab the AppImage on
  Linux or the zip on Windows and run it. Both carry their own copy of Qt,
  so they work on a machine that has never had it, and both ship the
  licence texts that bundling Qt requires. Every push is now built and
  tested on both operating systems too, so a change that breaks Windows is
  caught by the machine rather than by whoever downloads it next.

- **Minesweeper, Reversi, Draughts and 2048 remember where you left off** (GHUB-0010)
  Close one of them mid-game and it is exactly as you left it next time you
  open it — no save dialog, nothing to press. Minesweeper keeps your clock
  and stays paused if you paused it; Draughts still shows where the computer
  last moved. A finished game leaves nothing behind, so you never resume onto
  your own final score.

- **Klondike, Spider, FreeCell and Pyramid save and resume** (GHUB-0008)
  Close a game of patience part way through and it is waiting where you
  left it next time you open it. A shared card codec does all four, and
  checks the cards coming back really are the pack that was dealt before
  it puts them on the table.

- Chess remembers a game in progress — the board, whose move it is, the moves
  behind it and the strength all come back when you reopen it (GHUB-0006).
