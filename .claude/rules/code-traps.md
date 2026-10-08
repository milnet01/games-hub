---
paths:
  - "src/**"
  - "tests/**"
  - "tools/**"
  - "assets/**"
  - "CMakeLists.txt"
---

## Code traps

Moved from `CLAUDE.md` § Traps worth knowing.

**`QStringLiteral` takes a UTF-16 literal, so UTF-8 escape bytes in one build
the wrong string.** `QStringLiteral("\xf0\x9f\x94\x8a")` is not the speaker
emoji; it is four separate UTF-16 code units, and it compiles and compares
clean against nothing. In a test that is worse than a wrong answer: a search
for it matches no line, the loop asserts over an empty set, and the check
passes over the very defect it was written for. Paste the character itself, or
key on something that is not text at all -- an object name, as
`theHubHasNamesToReadOut` does after being caught this way.

**`slots` is a Qt keyword macro and expands to nothing.** A local named
`slots` compiles as `const int = ...` and the error points at the `=`, which
reads as a parser bug rather than a name collision. `signals` and `emit` are
the same. Canasta's meld layout hit this.

**`windows-2022` under `QT_QPA_PLATFORM=offscreen` has no font environment at
all, and this is the number to remember: `QFontDatabase::families()` returns
EMPTY and the default face measures digits at 0.997 of an em** — the full em
box, which is a headless stub rather than any real typeface. Anything derived
from font metrics therefore degrades to its floor on that runner and must be
allowed to. It is not evidence about what a Windows *player* sees: a real
desktop has Segoe UI and behaves like this machine.

**The rule the three red runs taught: a test may ASSERT what the code does, and
must only REPORT what the platform happens to provide.** Each rewrite of Sudoku's pencil-mark test put
a fresh environment constant into an assertion — a tuned ratio, then a growth
multiple, then a minimum font count — and each passed locally and failed on
`windows-2022`. If a number describes the machine rather than the change, print
it and assert something else.

**A UI check that clicks a Canasta card has to wait twice** — once for the turn
to come round to the human, and again for the cards to land, since the board
ignores clicks while anything is animating. Testing only the first produced a
suite that failed about one run in three.

**A seed does not mean the same deal on two compilers, so `shuffleCards` is
a hand-written Fisher-Yates.** The standard pins down what `std::mt19937`
emits but not how `std::shuffle` consumes it, and `std::uniform_int_distribution`
is unspecified the same way — so libstdc++ and MSVC's library produce
different permutations from identical state. Every seeded check here would
then play a different game on Windows. That is not theory: Canasta's AI
strength ladder passed on this machine and failed on the Windows runner with
no difference in the engine at all, which reads as a broken AI rather than a
broken shuffle. `card.cpp` now draws its index from `rng()` directly.
**`sudokugrid.cpp` still calls `std::shuffle`** — nothing asserts its
sequence across platforms today, but a new test that seeds it needs the same
treatment first. `minefield.cpp` got it in GHUB-0105, after a seeded self-test
check passed here and failed on the Windows runner.

**`QSettings` has no file on Windows.** It writes to the registry there, so
`QFile::exists(QSettings().fileName())` is false however well saving works —
a persistence check has to construct a fresh `QSettings` and read the value
back instead. That assertion cost a red Windows run for a feature that was
working.

**`M_PI` does not exist on MSVC, so this codebase uses `std::numbers::pi`.**
MSVC's `<cmath>` defines `M_PI` only if `_USE_MATH_DEFINES` was defined
before it was included, so the seven uses that lived here compiled on GCC
and would have failed the Windows build. `<numbers>` is C++20, which the
project already targets, and it needs no build flag to be load-bearing for a
maths constant. A new painter reaching for `M_PI` is caught only by the
Windows CI job, minutes later.

**`qt_add_resources(<target> <file>.qrc)` silently compiles an EMPTY resource.**
That signature expects a resource name plus a `FILES` list, so handing it a
`.qrc` gives no error, no warning and no content — every sound lookup then
fails at runtime. Sounds are embedded by listing `sounds.qrc` as a target
source with `CMAKE_AUTORCC ON`. **`tests/uitest.cpp` asserting every effect is
present and non-trivial in size is the check.** `assets/sounds/` is 17 files,
and the compiled resource object matches it. **No byte figure is kept here.**
The binary's own total grows with every game added, so it never said anything
on its own — and a stale one is worse than none, because the sentence reads as
a comparison somebody can make: two runs of this gate spent a finding on
exactly that. Measure the directory when you need the number.

**QPainter's `drawRect` fills with the current brush as well as outlining it.**
A brush set for one thing leaks into everything drawn after it: the flag's red
brush turned every dug Minesweeper square red, and only from the first flag
onward, because painting runs row by row. Set the brush — or `Qt::NoBrush` —
immediately before a shape call rather than trusting what came before.

**Sounds are generated, never sampled.** `tools/make_sounds.py` synthesises all
17 effects from a fixed seed, so re-running it is byte-identical. That is a
licensing decision rather than a stylistic one — see `ROADMAP.md` § Standing
rules before adding any asset.
