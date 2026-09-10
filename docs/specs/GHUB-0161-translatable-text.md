# GHUB-0161 — Every word a player reads can be translated

**Status:** accepted (2026-09-10).
**Kind:** implement.
**Source:** ROADMAP GHUB-0161 (review-code sweep 2026-08-31; the owner's
decisions of 2026-09-02 and 2026-09-10).

**Layman:** every word the games show can later be put into another language;
for now the app still shows English, word for word, so nothing looks different.

## 1. Goal

Every string that reaches a player passes through Qt's translation lookup. A
check fails the build when a new one does not. The app still shows exactly the
English it shows today, and nothing a player's data or a script depends on
changes. Adding the first language is then a translation job plus one build
hookup, which § 9 hands to the first-language item.

## 2. Problem

1. **No code asks for a translation.** `grep -rn '\btr(' src` matches comments
   only — in `src/spider/spiderview.cpp`, `src/klondike/klondikeview.cpp`,
   `src/chess/chessview.cpp` and `src/canasta/canastaview.cpp`, each explaining
   why toolbars match on object names. Every visible word is a bare
   `QStringLiteral`. `grep -cE 'QStringLiteral\("[^"]* [^"]*"' src/*.cpp
   src/*/*.cpp` lists the literals holding a space, file by file; it is an
   overcount, because settings keys and log text match too, and an undercount,
   because a one-word label such as `"Undo"` does not.
2. **The standard already requires it.** `~/.claude/standards/languages/qt.md`
   asks for *"`tr()` around every user-visible string, from the first
   commit"*, and its own *What checks this* row says nothing catches a string
   that was never wrapped. No override is recorded here.
3. **One rules core writes sentences.** `canasta::Engine` hands error text back
   through `QString& error` out-parameters — `Engine::keepsADiscard` is one —
   and `canasta::rulesInForce()` returns sentences. `gameshub_core` links
   `Qt6::Core` alone (`CMakeLists.txt`), so a core cannot reach `QObject::tr()`
   on a widget.
4. **Game names are keys.** `HubWindow::buildEntries()` fills `Entry::name`,
   and that one string feeds `saveKey()`, `geometryKey()`, the window title
   through `hubTitle()`, and `--game` through `openGameNamed()`.
   `docs/standards/versioning-overrides.md` § 1 lists the saved game, the
   settings keys and `--game`'s names as breaking surfaces. Translating the name
   would move every player's saved game and window size.
5. **Some sentences cannot be translated as written.** `ReversiView` builds its
   pass notice as `who + QStringLiteral(" no legal move — turn passes.")`, which
   fixes the English word order. Counts use a fixed English plural, such as
   `"Time: %1 seconds.   Best: %2."` in `MinesweeperView` and `"A meld needs at
   least %1 cards; %2 has %3."` in `canasta::Engine`.
6. **Tests find controls by their English text.** `tests/uitest.cpp` compares
   `a->text() == QStringLiteral("Undo")`, `"New Game"`, `"Discard"` and more,
   and checks the hub's title against `QStringLiteral("Games ") + version`.

## 3. Scope decisions (agreed with the user)

- **The app is to be translatable**, as its own piece of work after the items
  that gated 1.0 — the owner, 2026-09-02 (recorded in GHUB-0161's body).
- **The language follows the computer's language.** No in-app menu and no new
  setting — the owner, 2026-09-10.
- **English only this round.** No translation file ships — the owner,
  2026-09-10.

The last two decide the scope below: with English the only language, there is
nothing to load, so loading a translation is the first-language item's work
(§ 9), and this item makes the text translatable and keeps it that way.

## 4. Design

### 4.1 What is wrapped

Every string a player can read: widget text, labels, tiles and blurbs, dialog
text and titles, toolbar and menu labels, tooltips, the status bar, the
play-surface caption, and the window title's game part.

- Inside a `QObject` subclass — every view, the hub, the dialogs — use `tr()`.
- Outside one — free functions, painters, `canasta::Engine`,
  `canasta::rulesInForce()` — use `QCoreApplication::translate(<context>,
  <text>)`. `QCoreApplication` is part of Qt Core, so `gameshub_core` still
  links `Qt6::Core` alone.
- **The context is always a string literal**, one per class or namespace:
  `"canasta::Engine"`, `"canasta::rulesInForce"`. A computed context is
  invisible to `lupdate`, so its strings would never reach a translator.
- Text kept as a `const char*` and shown later is marked where it is written
  and translated with the same context where it is shown:
  `QT_TRANSLATE_NOOP(<context>, <text>)` for plain text, and
  `QT_TRANSLATE_N_NOOP(<context>, <text>)` for a count, translated with its
  number as `n` (§ 4.4). `canasta::rulesInForce()` passes sentences such as
  `"A canasta is %1 cards."` this way.

### 4.2 What is never wrapped

These are read by a program or a script, not a player, and several are
breaking surfaces (`versioning-overrides.md` § 1):

- settings keys and group names, including `saveKey()` and `geometryKey()`;
- `objectName` values, which `restoreState()` and the tests match on;
- `Entry::name` (§ 4.3), because it is a key and `--game`'s argument;
- everything printed to a terminal — `--version`'s `Games <version>`, the
  `--shot` messages in `main.cpp`, and `qDebug`/`qInfo`/`qWarning` output;
- the window title's `Games <version>` part, because it is `--version`'s
  spelling and `uitest` checks the two against each other;
- `QCoreApplication` identity set in `main()` — `setOrganizationName`,
  `setApplicationName`, `setApplicationDisplayName` — because `QSettings`
  derives its file from the first two;
- resource paths, icon theme names, URLs and sound names.

**A keyboard shortcut is built from a key value, never from text.** Every
`setShortcut` call today takes a `QKeySequence` standard key or a `Qt::Key`
value (`grep -rn 'setShortcut' src`). A shortcut spelled as translatable text
could be translated into a different key, and shortcuts are a breaking surface.
A menu mnemonic such as the `&` in `"&Help"` is not a shortcut in this sense:
it belongs to its label and is translated with it.

Every letter-bearing literal of these kinds that sits in no call § 4.5
recognises carries the marker `// untranslated: <reason>` on its line —
`saveKey()`'s `"saved/"` is one. The reason is not optional.

### 4.3 A game has a fixed id and a translated label

`HubWindow::Entry` keeps `name` exactly as today — `"Solitaire"`, `"2048"` and
the rest — as the game's id, and gains a translated `label` beside it.

```cpp
struct Entry {
    QString name;    // the id: saved/<name>, window/geometry/<name>, --game <name>
    QString label;   // what the tile and the window title show: tr(...)
    QString blurb;   // tr(...)
    ...
};
```

The tile and the window title show `label`. `saveKey()`, `geometryKey()`,
`openGameNamed()` and the name list `--game` reports keep `name`. With English
the only language, `label` reads exactly as `name` does today.

### 4.4 A sentence is one unit

- A sentence is translated whole, with its variable parts as `%1`, `%2` through
  `arg()`. It is never assembled from pieces with `+`, because another language
  orders the pieces differently. `ReversiView`'s pass notice becomes two whole
  sentences, one for each side, because its verb changes with who passed.
- A count of things uses Qt's plural form, `tr("… %n seconds …", nullptr, n)`,
  so a language with other plural rules can say it correctly. English output is
  unchanged.
- Where today's English picks its own singular — FreeCell's `"Only %1 card%2"`,
  Spider's `"Spider (%1 suit%2)"` — the singular stays a whole sentence of its
  own beside the `%n` form. With no translation loaded, a `%n` form prints its
  English source for every count, so a lone one would say "1 cards" and break
  INV-2.

### 4.5 The check

`scripts/translatable-check.py` is new, registered as the ctest case
`translatable` the way `legibility` is: inside `if(Python3_Interpreter_FOUND)`,
with a failing `translatable_needs_python` stub on Unix where no interpreter is
found, as `CLAUDE.md` requires of a tool-gated case.

It reads the `.cpp` and `.h` files under `src/` and exits 1, naming file and
line, for any string literal holding a letter that is:

- not an argument of `tr()`, `QCoreApplication::translate()`,
  `QT_TRANSLATE_NOOP()` or `QT_TRANSLATE_N_NOOP()` — context and disambiguation
  included;
- not an argument of a recognised call: `setObjectName`, the `QSettings`
  calls, the terminal-output calls (`qDebug`, `qInfo`, `qWarning`,
  `qCritical`, `std::printf`, `std::fprintf`), `setOrganizationName`,
  `setApplicationName`, `setApplicationDisplayName`, `setDesktopFileName`,
  `QIcon::fromTheme`, or an `#include`; and
- not the one literal a `// untranslated: <reason>` marker on its line
  exempts.

**A marker exempts exactly one literal.** A marked line holding a second
letter-bearing literal that is neither translated nor in a recognised call
fails, so marking a game's id cannot hide a bare label on the same line.

A qualified call counts by its last name part, so `main()`'s
`QGuiApplication::setDesktopFileName` is `setDesktopFileName`. Qt's argument
placeholders — `%1`, `%L1`, `%n` — are not letters.

It also exits 1 for a `+` or `+=` that directly joins a translation call to
anything else — § 4.4's first rule, made mechanical. `s += tr(...)` assembles a
sentence exactly as `s + tr(...)` does.

It exits 0 and prints nothing when every literal is accounted for. The
recognised calls are a named constant at the top of the script, as
`legibility-check.py` keeps `PAIRS`, and that constant is the one place to
extend them.

## 5. Invariants

- **INV-1** — Every string literal holding a letter under `src/` is
  translated, or is an argument of a call § 4.5 recognises, or carries
  `// untranslated:` with a reason.
  *Test:* `python3 scripts/translatable-check.py` exits 0 on the finished tree.
  Against a fixture line `addAction(QStringLiteral("Deal Again"))` added to a
  view, it exits 1 naming that file and line. The fixture isolates the wrap
  rule: the literal is in no recognised call and carries no marker, so nothing else
  can reject it.
  *Breaks when:* a new label lands as a bare `QStringLiteral`.

- **INV-2** — With no translation loaded, every string reads exactly as it does
  today.
  *Test:* the full ctest suite. `uitest` finds some actions by their English
  text and checks the hub's title, so a changed English string fails it
  wherever `uitest` matches that string, and passes everywhere else.
  *Breaks when:* the retrofit edits a source text while wrapping it — `"New
  Game"` becoming `"New game"` fails `uitest`'s `a->text() ==
  QStringLiteral("New Game")`.

- **INV-3** — Nothing a player's data or a script depends on changes: the
  `saved/<name>` and `window/geometry/<name>` keys, `--game`'s names,
  `--version`'s output, and the `QSettings` file location.
  *Test:* a new `uitest` case installs a `QTranslator` subclass whose
  `translate()` marks every string it is asked for and whose `isEmpty()`
  returns false. With it installed, `HubWindow::gameNames()` still equals the
  fixed list of ids and every tile's label carries the mark. Opening a game by
  its id and returning to the menu writes `window/geometry/<id>`, unmarked, and
  a game with something worth saving writes `saved/<id>`, unmarked.
  `savesFromOlderBuildsStillLoad` and the `shot` and `shot_plays_forward` cases
  hold the English names, and `release.yml`'s `^Games ` assertion holds
  `--version`. The `QSettings` identity has no test: `uitest` runs under its own
  identity on purpose, so it never touches a player's settings, and it cannot
  see `main()`'s.
  *Breaks when:* `Entry::name` is changed or wrapped in `tr()`, or a key or
  `openGameNamed()` is fed `label` instead of `name`.

- **INV-4** — A sentence is one translatable unit, and a count of things uses
  the plural form.
  *Test:* the `translatable` check refuses a `+` or `+=` joining a translation
  call to anything else. The plural half has nothing to run; it is read in
  review.
  *Breaks when:* `who + tr(" no legal move — turn passes.")` ships.

## 6. Failure modes

- **A string the check cannot see.** Text built at run time from pieces the
  check does not recognise ships untranslated. Nothing shows it in English; the
  first translation shows it as the one untranslated line.
- **A key wrapped by mistake.** It reads the same in English, and under a
  loaded translation it would move the player's saved game or break `--game`.
  INV-3's marking translator exposes a wrapped game id and a key built from
  `label`; § 10 records what it does not reach.
- **A computed context.** `lupdate` extracts nothing from it; § 4.1 forbids it.
- **Tests that match English text.** They pass while English is the only
  language and fail on a machine running a loaded translation. That is the
  first-language item's to fix, before it loads anything (§ 9).

## 7. Tests

- **`translatable`** — `scripts/translatable-check.py`, new. Locks INV-1. It is
  written first and run against today's tree, where it must exit 1 naming the
  untranslated literals; that is its red run. The retrofit then brings it to 0.
- **The existing suite** locks INV-2. It is green before the retrofit and must
  stay green after it.
- **A new `uitest` case with a marking translator** locks INV-3: ids and the
  keys written from them stay fixed while labels change. **`savesFromOlderBuildsStillLoad`,
  the `shot` and `shot_plays_forward` cases** hold the English names, and
  **`release.yml`** holds `--version`.
- **`translatable`** also locks INV-4's joining half. Its plural half has no
  automatic test.

## 8. Alternatives considered (and rejected)

- **`canasta::Engine` returns codes and the view words them.** Rejected: every
  error path in the engine would be rewritten for no change a player sees, and
  `QCoreApplication::translate` already keeps the core on Qt Core alone, which
  is all the core/view rule asks.
- **Build the loader now, following the computer's language.** Rejected: with
  English the only language there is nothing to load, so the loader would be
  code no run executes.
- **An in-app language menu.** Rejected by the owner, 2026-09-10.
- **An allowlist of translatable strings, with everything else left alone.**
  Rejected: a new string would then ship untranslated by default. The marker
  makes the untranslated case the one that has to be written down.
- **A project macro in place of `tr()`.** Rejected: `lupdate` recognises
  `tr()`, `QCoreApplication::translate()` and the `QT_TR_NOOP` family, and
  would not see a macro of this project's own.

## 9. Out of scope

- **Loading translations, and following the computer's language** — the first
  `.ts` and `.qm` files, `Qt6::LinguistTools` in `CMakeLists.txt`, the Qt tools
  in CI's `install-qt-action` step, and bundling Qt's own translations in the
  AppImage and the Windows zip — deferred; not yet queued. It comes with the
  first language, and its first task is § 6's: move the tests that match
  English text onto object names.
- **The phone score book in `scorepad/`** — deferred; not yet queued.
- **Right-to-left layout** — deferred; not yet queued.

## 10. What checks this

| Rule | What catches a breach |
|------|----------------------|
| INV-1 | the `translatable` ctest case (`scripts/translatable-check.py`) |
| INV-2 | **`Partial:`** the ctest suite — `uitest` matches some actions and the hub's title by their English text. **Nothing** catches a changed string it does not match |
| INV-3 | **`Partial:`** the marking-translator `uitest` case catches a changed or wrapped game id and a key built from `label`; `savesFromOlderBuildsStillLoad`, the `shot` cases and `release.yml` hold the English names and `--version`. **Nothing** catches a wrapped settings-key prefix such as `"saved/"`, or a change to `main()`'s `QSettings` identity — `uitest` runs under its own |
| INV-4 | **`Partial:`** the `translatable` check catches a `+` or `+=` join. **Nothing** catches `%1` beside a plural noun — code review |
| `// untranslated:` carries a reason | **`Partial:`** the check refuses the marker with no text after it; nothing judges whether the reason is true |
| A shortcut is built from a key value | **nothing** — every `setShortcut` call takes one today; a reader catches the first that does not |

## 11. Cross-doc impact

- `CLAUDE.md` § Commands — the per-check command list gains
  `python3 scripts/translatable-check.py`.
- `docs/design.md` § What every part does the same way — a translation bullet
  pointing here.
- `CHANGELOG.md` — none: nothing a player sees changes.
- `docs/standards/versioning-overrides.md` — none: no surface changes.

## 12. Cold-eyes loop log

Rows live in `../reviews/GHUB-0161-translatable-text-loop-log.md`.

## 13. Migration / compatibility

None. No settings key, save format, command-line output or shortcut changes
(INV-3).
