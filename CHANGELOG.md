# Changelog

Notable changes to the Games hub. Newest first, in the shape
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) sets out.

Started 2026-08-11, so it does not reach back to the first fourteen games —
`ROADMAP.md` and the git log carry that history.

## [Unreleased]

### Fixed

- **The release checks now start the downloaded build, not just ask it its version** (GHUB-0026)
  Both downloads are opened and run headless before they are published,
  and the check passes only if the game is still running twenty seconds
  later. The old check asked the file for its version number, which it
  answers before the graphics layer is even loaded — so a download that
  could not open a window would have passed. Both downloads now also
  carry the offscreen display plugin the check runs on.

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
