# Changelog

Notable changes to the Games hub. Newest first, in the shape
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) sets out.

Started 2026-08-11, so it does not reach back to the first fourteen games —
`ROADMAP.md` and the git log carry that history.

## [Unreleased]

### Added

- **A change that would make older saved games unreadable now fails the tests instead of reaching a player.** (GHUB-0075)
  A save each game wrote is committed under tests/saves/ and restored by
  the suite. A refused old save is not a crash -- the app runs and the
  half-finished game is simply gone -- so nothing used to catch it.

- **--turns plays a game forward before the screenshot is taken.** (GHUB-0166)
  Reaches anything that only exists once a hand has been played, such as a
  frozen pack or a finished canasta. A game that has no notion of a turn
  refuses rather than photographing the deal.

- **--seed pins the shuffle, so two screenshots of one card game can be compared.** (GHUB-0093)
  Reaches every deal, board and computer seat. Without it the deal is random
  per launch, so a before-and-after pixel count measured the shuffle.

### Changed

- **Sudoku opens faster, and resuming a saved puzzle much faster.** (GHUB-0160)
  It was building a whole puzzle it then threw away -- two to open the game
  and three to restore a save.

- **Pyramid says "Redeal" on the stock instead of a symbol.** (GHUB-0160)
  The symbol was the only place on the table that said a redeal was left,
  and a symbol the computer has no font for draws as nothing at all -- so
  the cue could simply disappear. The word is sized to fit the slot.

- **Hard and Expert keep a rank back as the key to a pile worth taking.** (GHUB-0129)
  They used to prune their opening to hold a rank in hand and then lay the
  same cards on the same turn. How long they wait now depends on how many of
  that rank are still unaccounted for. Easy and Medium play as before.

- **SECURITY.md now says which half of the project it is describing.**
  Its no-network and no-personal-data claims are about the desktop
  game. The phone score book talks to a hosted database, and has its
  own section and its own reporting advice.

### Fixed

- **Notes for developers that described how the project used to work.** (GHUB-0145)
  The chess engine has not run on the drawing thread since it was moved to a
  background one, but two comments still said so, and both used it to justify
  a limit on how hard the computer thinks. The build spec also described the
  release checks incompletely enough that copying them would have reproduced
  a bug they exist to catch. Nothing a player sees changes.

- **Everything the third code analyser could see is now dealt with, and it is switched on for good.** (GHUB-0170)
  Two of them mattered to a saved game: Canasta accepted a corrupted card
  after having decided to reject it, and taking a move back in Draughts or
  Reversi read a value nobody had set. The rest were tidying that makes the
  next real problem easier to spot. Draughts also stops copying the whole
  board at every step while it looks for a jump.

- **Small defects across card drawing, sound, the donation prompt and the checking scripts.** (GHUB-0162)
  Card faces could be drawn from the wrong cache entry on a screen set to
  a fractional scale such as 150%. Both card caches now have a memory
  ceiling rather than only an entry count, and are emptied before the
  application shuts down. A sound that will not load now says so instead
  of playing silence, the donation prompt is no longer skipped when
  --game names a game that does not exist, and the three checking scripts
  no longer accept spellings of the source they cannot actually read.

- **Taking back the move that reached 2048 takes the win back with it.** (GHUB-0160)
  The game went on believing the target had been reached, on a board with no
  2048 tile on it.

- **Restarting a solved Sudoku puts the clock back on.** (GHUB-0160)
  The clock had been stopped when the puzzle was solved and Restart never
  started it again, so it sat at zero while the board played on.

- **Delete in Sudoku's pencil mode now clears that cell's marks.** (GHUB-0160)
  It did nothing at all, while still making the sound of something happening.

- **Sudoku remembers the difficulty you chose.** (GHUB-0160)
  Minesweeper already did. Sudoku started every session on Easy whatever you
  had been playing.

- **Taking back a Draughts move no longer leaves the gold marker on it.** (GHUB-0160)
  Undo restored the board but left the highlight pointing at a move that had
  just been taken back.

- **Putting a FreeCell run back where it came from no longer costs you a move.** (GHUB-0160)
  Moves are the score in FreeCell, so picking a run up and setting it
  straight back down was spending it for nothing.

- **Hearts no longer plays a card sound when no card was played.** (GHUB-0160)
  A computer play that the rules refused still made the noise, so the table
  sounded as though it had moved when it had not.

- **A tied game of Hearts is no longer announced as your win.** (GHUB-0160)
  Finishing level with a computer player was reported as "You win!" and
  written into your best scores. It now says who tied and on what, and
  records nothing.

- **A Spider column that has taken every dealt row now stays on the table.** (GHUB-0160)
  Cards were sized for a shorter column than the game actually deals, so a
  full one ran off the bottom and under the caption.

- **Clicking the bottom of Spider's last column no longer deals a row.** (GHUB-0160)
  The stock sits over that column's tail and was being tested first.

- **Double-clicking a buried Solitaire card no longer plays a different card.** (GHUB-0160)
  It sent the top of that column instead, which is a move you did not ask
  for. A click that does not land on the top card now does nothing.

- **Solitaire and Spider no longer lock up if you leave mid-drag.** (GHUB-0160)
  A run picked up and not put down had been lifted off its pile. Leaving
  the game, dealing again or undoing left those cards nowhere, and no card
  could be picked up afterwards.

- **A crash no longer loses the game you were in, and two copies of the app stop overwriting each other.** (GHUB-0067)
  Progress reached the disk only on the way out, so a crash, a kill or a
  power cut lost the one game being played -- and two copies settled by
  whichever exited last rather than whichever moved last. The open game and
  its window size are now banked on a one-second tick, so at most a second
  of play is at risk and the most recent move wins.

- **A frozen Canasta pack keeps showing its sideways card however deep it is buried.** (GHUB-0172)
  The pile draws only its top few cards, so the card lying across it
  vanished once that many discards landed on top -- while the pack stayed
  frozen. It is now laid at the deepest position still drawn, under the
  stack, so its arm shows for as long as the freeze lasts.

- **A ball wedged beside a flipper no longer bounces there for ever.** (GHUB-0173)
  A slingshot was kicking from its back as readily as its face, so a ball
  in the inlane was fired at the wall opposite and returned, endlessly. The
  kick now fires on the playfield-facing side only, as a real sling's rubber
  does, and gravity takes the ball down to the flipper.

- **Canasta AI: several judgement terms rest on figures that are not what they claim.** (GHUB-0149)
  The pack count follows the deck count, the run-it-dead judgement measures
  both sides the same way, the pair-keeping open works on more than one hand
  shape, and the freeze budget survives saving and reopening a game.

- **The legibility threshold check could never run on Windows, and reddened master.** (GHUB-0171)
  The check shelled out to a grep pipeline, which cmd.exe cannot run.
  It is a Python scan now, so it behaves the same on both platforms.

- **The Hearts sentence no longer sits on top of the card you just played.** (GHUB-0147)
  With large play on, the plate carrying what the game is telling you
  grew upward out of the gap it was given and covered your own card in
  the trick. It now moves into the empty felt beside the trick when
  that gap is too small, so the sentence is still there and the card is
  still visible.

- **The score book's instructions now describe a setup that works.** (GHUB-0143)
  It said to open the page as a file, which cannot install the offline
  shell or the home-screen icon -- both need the folder to be served.
  It also now says to bump the cache name after an edit, and is honest
  about which numbers the drift check compares.

- **Sharing the score book is safer to join and to leave.** (GHUB-0141)
  A shared book whose names arrived in an unexpected form could freeze
  a watching phone. Sharing now checks a code is free before claiming
  it, instead of possibly landing on a game already in progress.
  Taking over scoring stands the previous scorer down rather than
  leaving two. And a connection that fails says so instead of leaving
  the button looking like it did nothing.

- **The score book's offline cache no longer swallows the live database traffic.** (GHUB-0142)
  On a network that will not carry a live connection, the database
  falls back to ordinary web requests -- and those were being cached
  and replayed for ever, so sharing stopped working on that phone with
  no way back short of clearing its site data. Only the score book's
  own files and the pinned Firebase library are cached now.

- **Draughts asks which way you meant to jump when two routes finish in the same place.** (GHUB-0131)
  Where a piece can reach one square by two different jump routes,
  taking different men, the board used to play whichever it happened to
  find first -- and drew the same dot twice, so nothing told you a
  choice existed. That square is now ringed, and clicking it moves the
  dots to the squares the two routes differ at so you can say which you
  meant.

- **A Canasta hand-size change now applies at the next hand, as the game says.** (GHUB-0150)
  Changing the pack size, the jokers or the hand size mid-game waited
  for a whole new game rather than the next hand, so correcting a house
  rule meant abandoning the game you were in.

- **A Canasta take can no longer leave you with nothing you are allowed to do.** (GHUB-0148)
  Where the deal had turned a red three into the pack, taking it could
  leave you on one card with no canasta -- unable to go out, and so
  unable to throw your last card either. The turn simply could not be
  finished. The same miscount could also let a side go out without the
  canasta the rules require.

- **The game tiles tell a screen reader which game they open.** (GHUB-0132)
  Each tile paints a picture and carries no text, so a screen reader
  found fourteen unnamed buttons on the first page of the app. They
  now carry a name and a description, and show their blurb on hover.

- **The Support dialog no longer comes up microscopic on some systems.** (GHUB-0134)
  Where the system font size is set in pixels rather than points, the
  dialog shrank its own text to about 2pt -- worst of all with large
  play turned on, the setting meant to make it bigger. The web
  addresses in it are also no longer greyed down.

- **Large play now makes Reversi's move dots bigger and brighter.** (GHUB-0133)
  They ignored the switch, and because the caption takes height off
  the board they actually came out SMALLER with large play on -- on
  the only marks that show where you may put a disc. Reversi also
  keeps the "turn passes" sentence on screen until a disc is actually
  played; it used to vanish after a third of a second, and a pass is
  the one move that leaves the board looking unchanged.

- **Closing the window stops the game you were on.** (GHUB-0164)
  Every other way of leaving a game stopped its clock and closing the
  window did not, so a pinball could still drain -- and take a score
  with it -- after you had shut the app.

- **Canasta says why when you click a pack that has nothing in it.** (GHUB-0164)
  It did nothing at all, which reads as the board being stuck rather
  than as a rule. The Lay Down button also stops keeping its highlight
  after the pointer has left the window.

- **Canasta marks all four black threes as the finish they are.** (GHUB-0164)
  Laying all four together is the one way to finish without keeping a
  card back to throw, and the hand never lit them up, so the move was
  invisible unless you already knew it was there.

- **Opening one game no longer stops every other game's window being made small.** (GHUB-0135)
  Canasta needs a wide window, and once you had opened it no other
  game's window could be shrunk back down for the rest of the session
  -- and the enlarged size was then remembered. Only the game you are
  looking at decides how small the window can be.

- **A mistyped option prints an error and exits instead of opening a dialog on Windows.** (GHUB-0158)
  --help and any bad option went through Qt's own reporting, which on
  Windows is a message box -- so the app waited for someone to close
  a dialog rather than exiting. --size is also bounded now, instead of
  attempting an enormous image for a silly number.

- **Taking a screenshot no longer changes your settings.** (GHUB-0136)
  --shot wrote the large-play setting and the remembered window size
  as a side effect of photographing a game, so answering a question
  about the app changed what you had chosen. It now stores nothing.

- **The toolbar's large-play button follows the switch however it was moved.** (GHUB-0157)
  Turning large play on any other way -- the --legible option does it
  before the window exists -- left the button reading "Normal" and
  unchecked, next to large play plainly on.

- **The app says so when it cannot save your settings.** (GHUB-0156)
  A full disk or a read-only settings folder lost every saved game and
  remembered window size in silence. The status line now says once
  that games and window sizes will not be remembered. A window size
  that cannot be read also falls back to the normal opening size
  rather than leaving the window unsized.

- **A damaged best score no longer locks you out of ever beating it.** (GHUB-0156)
  A stored best that was not a number read back as zero. In the games
  where smaller is better -- Minesweeper and Sudoku times, Spider and
  FreeCell moves, the Hearts total -- zero is a score nobody can beat,
  so every later result was refused. It now reads as no record at all,
  and the next result simply sets it.

- **The score book's A+ and A- now scale the numbers, not just the buttons.**
  Every size on the page is relative to the root, which the control
  was not changing -- so the running total, the deal and the opening
  minimum stayed fixed. Joining a shared game also no longer discards
  the book already on the phone.

- **2048 and Snake update the on-board caption when a move is refused or the game is paused.**

- **Snake keeps the second tap when you take a corner quickly.**

- **Pinball's flippers keep time, and the backglass stops covering the table.**
  The flippers were advanced once per frame but measured per physics
  step, so an identical swing gave a different kick depending on the
  timing, and a ball resting near the tip could be knocked through the
  blade toward the drain. The score strip sat over the top of the
  playfield, and large play made it cover more.

- **Sudoku's Pencil and Show Errors buttons agree with the puzzle they are above.**
  After resuming they displayed the opposite of what was in force, so
  the first press appeared to do nothing.

- **The mute switch is remembered between sessions.**

- **Hearts stops covering the card you just played, and Close no longer ends the match.**
  The caption plate could sit over your own trick card at wide, short
  windows. Declining the hand-over box left the match unplayable, and
  it survived a restart; there is a Next Hand button now.

- **Canasta remembers both of its remaining house rules, and its difficulty.**
  The freeze-card T and stacking canastas were lost on every resume
  while the toolbar still said House, and the chosen difficulty was
  never stored at all. Resuming also dropped the Expert-partner
  setting.

- **Pyramid's stock and waste no longer cover the bottom row.**
  They were drawn over the only row a fresh deal exposes, and a click
  in the overlap dealt from the stock instead of taking the card.

- **Resuming a saved game and leaving it no longer deletes it.**
  Klondike, Spider, FreeCell, Pyramid, Reversi and Draughts read a
  just-restored game as untouched and cleared the stored one, so
  opening a save and pressing Back lost it.

- **The computer no longer plays your move.**
  In Chess, Reversi and Draughts a search scheduled just before New
  Game, Undo or a level change could still fire and play a move for
  you -- after which the board sat waiting for a side that would never
  move. Reversi could also declare the game over twice and count the
  win twice.

### Security

- **Canasta bounds its saved hand size and validates the cards it reads back.**
  A saved game could claim a hand larger than the pack, which dealt
  past the end of the stock, and its card reader accepted any suit,
  rank or deck at all.

- **A crafted Pyramid save can no longer report itself won.**
  The pack check accepted a subset, so a table missing 24 cards
  loaded, and one with every card removed and no pairs taken banked a
  best score on load.

- **The release no longer builds from a moving tag.**
  linuxdeploy and its Qt plugin were fetched from a rolling tag,
  unverified, and assemble the published AppImage. Both are pinned to
  dated releases and checked against a SHA-256, and the smoke-test
  container can no longer write to the workspace.

## [0.5.0] - 2026-08-31

### Added

- **Cards you send home now travel there instead of teleporting**
  Double-click a card home in Klondike or FreeCell and you can see
  which one went and where it landed. In Spider, a completed run leaves
  its column as thirteen cards flying off one after another, rather
  than simply not being there any more. Only Canasta did this before.
  This is the first half of GHUB-0065, which stays open: the deals,
  the board games and Klondike's stock are still to come.

- **Saved games are now tested against damaged and tampered save files** (GHUB-0052)
  Loading a saved game is the only thing the app reads that it did not
  write. A new harness corrupts each game's own save thousands of ways
  and requires every game to refuse it cleanly, run under the
  compiler's memory-error and undefined-behaviour detectors. It found
  two real defects on its first two runs.

- **Sudoku comes back where you left it, clock included.** (GHUB-0009)
  A part-solved puzzle reopens with your answers, your pencil marks, the
  square you were on and the time you had spent on it. Pause already
  covered walking away for a minute; this covers closing the app. The
  clock does not quietly reset, which matters because the game records
  your best time.

- **Hearts comes back where you left it.** (GHUB-0007)
  Close the app mid-hand and Hearts reopens with the same cards, the same
  trick on the table, the same running scores and any cards you had
  already lifted for the pass still lifted. There is no save button — it
  just happens, like the other games that do this.

- **The test suite can now measure how long a frame takes to draw.** (GHUB-0049)
  `gameshub_uitest --bench` prints what four surfaces cost to draw, so a
  change meant to speed something up can be shown to have done it. It
  reports figures rather than failing against a threshold — a frame time
  is a property of the machine, not of this code. It found the Canasta
  slowdown above within minutes of existing.

- **Canasta house rule: you win by reaching the target, and both sides reaching it is a draw** (GHUB-0123)
  On by default in House rules, off in Classic. The game is won by
  getting to the target score rather than by being ahead when somebody
  does -- so a hand that carries both sides past 5000 is a draw, however
  far apart the two totals are, and the table says so instead of naming a
  winner. An exact tie at the target is the same thing and is now a draw
  too, where before it dealt another hand.

- **Canasta: going out the way the family plays it** (GHUB-0120)
  A new House rule, on by default. The round ends when you throw
  your last card away -- laying your whole hand down and stopping is
  no longer a way out. The one exception is finishing on all four
  black threes: everything else goes onto melds, the four threes go
  down together, nothing is thrown, and they earn their 20 instead of
  being caught in your hand. Classic Canasta is unchanged and still
  lets you meld out with anything.

- **Canasta: a stacked canasta names itself by the colour of its top card** (GHUB-0097)
  Red for a canasta with no wild card in it, black for one built with a joker or a two.

- **Canasta house rule: finished canastas stack on the red threes, turned about** (GHUB-0096)
  A completed canasta leaves the meld row, squares up and lies on the team's red threes -- across, then upright, then across -- so the stack can be counted by its edges. Each one slides along far enough to keep its own name badge readable. On in House rules, off in Classic.

- **Canasta house rule: the card that freezes the pack lies as a T** (GHUB-0095)
  One end against the pile rather than squarely across its middle. On in House rules, off in Classic.

- **The app can take a picture of itself** (GHUB-0090)
  `gameshub --shot picture.png --game hearts --size 1400x620 --legible` writes a picture of a game and exits, without opening a window or needing a screen. Chiefly a development tool: until now the only way to check that a layout looked right was to open it and look.

- **A written answer to "what would make this 1.0?"** (GHUB-0077)
  `docs/standards/versioning-overrides.md` names what a change here can break —
  your saved games, your settings, the command line — and lists the six things
  that have to be true before the version stops starting with a zero.

- **Every game now answers the Large play switch** (GHUB-0071)
  Chess, Reversi, Draughts, Minesweeper, Solitaire, Spider, FreeCell,
  Pyramid, Hearts, Snake, 2048 and Pinball join Canasta and Sudoku, so
  the switch reaches the whole collection. Nine of them previously drew
  no words at all on their own board — not the score, not whose turn it
  is, not "game over" — and now say it on the play surface instead of
  only in the status bar. Hearts names the suit that was led. Minesweeper's
  neighbour counts, Chess's board coordinates, both games' last-move
  highlights, 2048's tile numbers and Pinball's score all grow with it.

- **A way to support the project, in the Help menu** (GHUB-0041)
  Help → Support this project… opens a panel that explains itself
  before it asks, with one named button per donation link and the web
  address spelled out underneath, so you can see where a button will
  take you before it takes you there. Every 150th time you start the
  game it offers the same panel unprompted, and that one carries a
  switch to stop it asking.

### Changed

- **Undo works while the computer is still thinking** (GHUB-0047)
  In Chess, Reversi and Draughts, Undo used to do nothing until the
  computer had finished. It could not be pressed then anyway, because
  the window was frozen. It now takes the move back and abandons
  whatever the computer was working out.

- **Taking a move back in the card games no longer copies the whole table** (GHUB-0056)
  Undo now takes the stored piles rather than copying them, and the
  four solitaires keep their history in a container built for it. The
  measured win is in evicting the oldest position once the history is
  full: 20x cheaper. Neither was perceptible on a single move.

- **The Expert computer can now tell who threw each card, so it is harder to fish.** (GHUB-0124)
  Throwing away a card that others have already discarded is usually safe
  — they are unlikely to be holding a pair of it. A crafty player exploits
  that by feeding the same rank into the pile one card at a time, so it
  looks safe when it is really bait. The pack now remembers which player
  threw each card, and Expert counts how many DIFFERENT players let a rank
  go rather than how many cards are there. Two players discarding a king
  still reads as safe; the same player discarding two does not.

  Saved games from an older version still load. They simply do not carry
  who threw what, so the computer plays that hand without the new
  knowledge rather than refusing the save.

- **Every game's rules are now checked by the test suite.** (GHUB-0066)
  Six games — Klondike, Spider, FreeCell, Pyramid, Snake and 2048 — kept
  their rules mixed into the drawing code, which meant the automated
  checks could not reach them at all. They have been separated, and each
  now has its rules checked: cards are counted after every move, so a
  card can no longer go missing unnoticed, and each game's own rules
  (what may sit on what, what may be picked up, when a run comes off the
  table) are tested against positions set up on purpose. Doing this found
  the two bugs fixed above.

- **The Windows build no longer rides on an unmaintained third-party action.** (GHUB-0031)
  `ilammy/msvc-dev-cmd` still declares Node 20, which GitHub is retiring,
  and has had no release since January 2024 — so there was no version to
  bump to. Both workflows now run `scripts/setup-msvc.ps1`, which does the
  same job directly: find Visual Studio, run its `vcvarsall.bat`, export
  the variables it changed. One shared script, so the release cannot drift
  from what CI proved, and one less third-party action in workflows that
  publish binaries strangers download.

- **Canasta: the hardest computer now fishes** (GHUB-0103)
  The family's tactic. Holding three or more of a rank, it throws them out
  one at a time until a pair is left, hoping the player who discards to it
  reads that rank as safe and follows with it -- which hands it the pack.
  It only bothers when the pack is worth taking and when cards of that rank
  are still unaccounted for, since nobody can follow with a card that no
  longer exists. Expert only; it inverted the difficulty ladder when the
  level below was given it too.

- **Canasta: with the pick-up pile nearly gone the computer stops waiting for a frozen pack** (GHUB-0104)
  The other side of the same coin. If the hand is one worth having, it
  plays its cards out rather than holding them back for a frozen pack that
  may never come round again -- cards held that late are cards caught in
  your hand when the scores are counted.

- **Canasta: losing badly, the computer runs the pack dead instead of letting the hand score** (GHUB-0114)
  With "a hand nobody goes out on scores nothing" turned on -- the House
  default -- a hand the pick-up pile kills is void and neither side banks
  it. So when the computer is well behind late in a hand it now stops
  trying to finish: it will not go out, and it leaves the pack alone and
  draws instead, because drawing is what empties the pile and taking the
  pack is not. Under Classic rules, where such a hand is scored where it
  stands, none of this applies and it plays on as before.

- **Canasta: under the minus rule the computer treats its first canasta as insurance** (GHUB-0107)
  With "a side with no canasta counts nothing in its favour" turned on --
  the House default -- ending a hand without one does not merely lose the
  bonus: everything that side has on the table is taken OFF its score
  instead of added. So the first canasta is worth far more than the 300 it
  pays. The computer now puts cards down towards it rather than holding
  them back for the pack, and finishes the canasta nearest to done first
  even though that spends a joker where waiting might have made a
  500-point one. Under Classic rules nothing changes.

- **Canasta: the computer spends a black three where it blocks something** (GHUB-0109)
  A black three on top stops the next player taking the pack for one turn.
  The computer already saved them for a fat pack; it now also weighs whether
  the player to its left could actually take the pack at all. Thrown at a
  side that has not opened yet and has a long way to go, the block buys
  almost nothing, and the card is better kept.

- **Canasta: the computer now freezes the pack for a reason** (GHUB-0101)
  It used to freeze whenever it happened to be able to. Now it wants one of
  three reasons: their side is in and ours is not, so the pack is theirs to
  lose; we are holding back a pair of a rank we have already put down, and
  the freeze makes that pair the only key to it; or our hand is so full of
  cards that would hand them the pack that freezing is what makes it safe
  to throw at all. It freezes about a quarter less often as a result, and
  when it does there is something behind it.

- **Canasta: two limits on how freely the computer freezes the pack** (GHUB-0113)
  It will not freeze more than twice in a hand -- each freeze throws away a
  wild card worth 20 or 50 -- and it will not freeze a pack that was
  already coming back to its own side, which a freeze locks it out of just
  as surely as everyone else.

- **Canasta: the computer opens small, keeps the pair, and freezes the pack** (GHUB-0122)
  The owner's play, and the computer now makes it. Holding four eights and
  a joker against an opening bar of 50, it lays the joker and TWO eights --
  70, over the bar -- instead of all four eights, which is only 40 and does
  not clear it at all. The other two eights stay in its hand, and it then
  freezes the pack, which from that moment only a matching pair can take.
  It also stops over-melding generally: it lays the least the opening asks
  for whether or not the pack is frozen, where before it only held back on
  a frozen one.

- **Canasta: the computer stops hoarding once its hand runs short** (GHUB-0104)
  While the pack is frozen the computer keeps matching cards back rather
  than melding them away, because a frozen pack can only be taken with a
  pair out of your hand. It now stops doing that once its hand is down to
  two-thirds of a deal: with few cards left there are too few draws to
  come to turn a single card into a pair, and the points are worth more on
  the table than caught in your hand at the end.

- **Canasta: the computer stops being careful at a side that cannot open** (GHUB-0121)
  While the other side still has to open, they can only take the pack
  by opening off it -- the meld they take with has to be worth their
  opening minimum as well. That is a high bar, and it gets higher the
  further behind they are. The computer was throwing safe cards at
  them anyway. It now grades how careful to be by the bar they face:
  barely at all at 15 or 50, noticeably at 90, heavily at 120. Only
  the Hard and Expert players read the table this way.

- **Canasta speaks the table's own words: jokers, and the pack** (GHUB-0102)
  The game called them "wild cards" and called the discard pile a
  "pile". At a real table both have different names: a joker is a
  joker whether it is the 50-point one (the big joker) or a two (the
  small joker), and the discard pile is the pack. Every message,
  caption and House rule row now says it that way, and the House
  rules window explains the two sizes once.

- **Canasta: the computer plays for the minus, and milks a pack that keeps feeding it** (GHUB-0099)
  Two judgements that pull opposite ways, added together so neither runs away with the hand (GHUB-0099, GHUB-0100). When the House minus rule is on and the other side has no canasta, every point they have on the table is worth two to us -- so the computer now presses to end the hand rather than pricing that at the flat 100 the going-out bonus is worth. And when the pack keeps coming back to it, it stays in and keeps milking instead of going out at the first legal chance. Both are worked out in points and weighed against each other, so a big minus beats a fat pack and a fat pack beats a small one. With Classic rules and an ordinary pack neither applies and the computer plays exactly as before.

- **Canasta: catching them a minus is now the House default, and the game says so** (GHUB-0098)
  A side that ends the hand with no canasta has its own melds counted against it. On by default in House rules, and named in the words the owner's family uses when it happens.

- **Canasta explains a refused move on the table instead of in the status bar** (GHUB-0040)
  When the game will not let you do something it now says why on an amber
  panel just above your hand, and the sentence stays there until you make
  a move of your own. Every click used to wipe it — including the clicks
  you make while doing what it told you.

### Fixed

- **The window no longer freezes while the computer works out its move** (GHUB-0047)
  Chess, Reversi and Draughts now think in the background. On a
  middlegame position at the hardest level the window went from being
  unresponsive for 420ms at a stretch — no repaint, no resize — to 8ms,
  which is what it gives when it is doing nothing at all.

- **A Canasta save claiming an absurd number of packs overflowed the check meant to catch it** (GHUB-0052)
  The check that a saved Canasta game still holds a whole pack
  multiplied out a pack size taken straight from the file, with no
  limit on it. Found only under the undefined-behaviour detector; an
  ordinary build passed the same file.

- **A damaged Canasta save could change your rule set and then call itself corrupt** (GHUB-0052)
  Canasta wrote parts of a save into the live game as it read them, so
  a truncated file could switch you between House and Classic rules, or
  leave the mutant's table in place, while reporting that it had kept
  your game. It now reads the whole file before changing anything.

- **Undo in FreeCell and Klondike no longer makes the card disappear.**
  (GHUB-0126)
  Moving a card and then pressing Undo removed it from the table
  altogether. The game could then no longer be saved: both games check
  that the whole pack is present, so closing the app after an undo threw
  the game away and dealt a fresh one next time.

- **Starting Snake with the right arrow no longer kills you instantly.** (GHUB-0125)
  The snake was drawn facing right but built back to front, so its head
  was behind its own body. Pressing the right arrow to start ran it
  straight into itself and the game ended before it began. Any other
  arrow was fine, which is why nobody had caught it.

- **The card games draw far faster, and Canasta stops slowing down as the
  hand fills up.** (GHUB-0048)
  A resting Canasta table was costing 24 milliseconds to draw against its
  own 16-millisecond clock — so the game could never keep up, and it got
  worse the more cards were on the table. It now costs 8. FreeCell was the
  slowest board in the collection at 19 milliseconds and is now 3; Pyramid
  11 to 3, Hearts 6 to 2, Spider 5 to 2, Klondike 6 to 3. Cards are drawn
  once and kept rather than redrawn from scratch sixty times a second.
  A card is only ever reused at the size and angle it was drawn for, so
  nothing goes soft: a card in a fanned hand, which sits at an angle, is
  still drawn fresh every time, and nothing you read by its pip pattern is
  ever resampled.

- **Canasta: a frozen pile no longer shows the freezing card twice, or above cards thrown after it** (GHUB-0094)
  The wild card that freezes the pack is drawn once, lying sideways at the depth it was actually thrown at, so a later discard covers it instead of sliding underneath.

- **Hearts draws East's stack of cards fully inside the window** (GHUB-0092)
  The pile on the right fanned outwards and lost the edge of its front card off the side of the window. It now fans towards the middle of the table, the way the pile on the left already did, so the two sides match.

- **Hearts keeps the cards you have chosen to pass out from under the caption** (GHUB-0085)
  A chosen card lifts out of the hand to show it is chosen, and used to lift straight under the caption plate, losing the top of its gold outline.

- **Hearts no longer prints its caption on top of the card you just played** (GHUB-0084)
  On wide, short windows the sentence explaining the trick sat on the seat-0 card -- yours -- while the other three stayed visible.

- **Captions break between their own phrases instead of mid-sentence** (GHUB-0088)
  At a small window the sentence on the board used to split wherever the width ran out -- Reversi read "You 2 - 2" on one line and "Computer" on the next. It now breaks between the phrases the sentence is already made of, and no longer leaves a line's trailing spaces pressed against the edge of its plate.

- **FreeCell and Klondike size their cards for the board they actually deal.** (GHUB-0083)
  Both solved card width against a height budget that assumed a
  shorter column than the deal makes, so FreeCell hid the bottom card
  of five of its eight columns behind the caption, and at wide, short
  windows both games ran the tail of every column off the bottom edge
  — with the legibility switch off as well as on. Covers GHUB-0086.

- **Pyramid and Spider draw their stock pile inside the caption band, and the plate hides it.** (GHUB-0082)
  With the legibility switch on, both games drew their stock (and
  Pyramid its waste) underneath the caption's opaque plate, so at
  their smallest windows the pile you draw from was not on screen at
  all. Both piles now sit above the caption.

- **Snake and Hearts kept playing after you switched away** (GHUB-0074)
  Leaving Snake mid-game ran the snake into a wall while you were in another
  game; leaving Hearts finished the hand without you. Coming back picks
  either up where you left it.

- **Pinball kept playing after you switched to another game** (GHUB-0073)
  The ball carried on rolling, and could drain while you were elsewhere.

- **Turning Large play off in Canasta left the window enlarged** (GHUB-0072)
  Only inside the hub, which is the only place you would ever see it.

- **Every game can be made small enough to sit beside your work** (GHUB-0042)
  The window could not be made shorter than about 1170 pixels — taller
  than a 1080p screen — because the grid of fourteen game tiles set the
  floor for every page, not just its own. The grid now scrolls, so each
  game asks only for the room it needs: the largest is Canasta at
  900x740 with large play on. The README's opening promise now says
  what that means, and a test holds the game to it.

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
