#!/usr/bin/env python3
"""Measure the collection's ink-on-background contrast, and police the one
place the 46-pixel face threshold is allowed to live.

Two jobs, one script, because both are mechanical legibility checks
(GHUB-0017 §4.5):

    scripts/legibility-check.py               contrast ratios (§2.3's table)
    scripts/legibility-check.py --thresholds  INV-4's grep

**This script holds the pair LIST and the thresholds. It reads every colour
VALUE out of the source.** That is the whole point: §2.3's ratios were
transcribed by hand from the source, and a script carrying its own copy of the
same hex could only ever disagree with the table by arithmetic — it could never
catch a transcription error, and it would stop tracking the source the day a
colour constant changed. A named constant it cannot find is a failure, not a
skipped row.

Exit status is 0 only when every pair meets its own threshold and every named
constant was found.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

# WCAG 2.2. The C++ copy is relativeLuminance()/contrastRatio() in
# src/twenty48/twenty48view.cpp; these two are the only other copy anywhere.
# https://www.w3.org/WAI/WCAG21/Understanding/non-text-contrast.html


def relative_luminance(rgb: tuple[int, int, int]) -> float:
    def channel(v: int) -> float:
        f = v / 255.0
        return f / 12.92 if f <= 0.03928 else ((f + 0.055) / 1.055) ** 2.4

    r, g, b = rgb
    return 0.2126 * channel(r) + 0.7152 * channel(g) + 0.0722 * channel(b)


def contrast_ratio(a: tuple[int, int, int], b: tuple[int, int, int]) -> float:
    la, lb = relative_luminance(a), relative_luminance(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)


class NotFound(Exception):
    """A constant this script names is not in the source it names."""


# One pattern for both spellings. This tree writes `QColor k { r, g, b }` in
# some places and `QColor(r, g, b)` in others, and a reader that knows only one
# of them measures whatever it happens to find -- which for an ARRAY is worse
# than finding nothing, because a mixed list still parses and every index after
# the first missed element is off by one.
COMPONENT = r"(0x[0-9A-Fa-f]{1,2}|\d{1,3})"
COLOUR = rf"[({{]\s*{COMPONENT}\s*,\s*{COMPONENT}\s*,\s*{COMPONENT}\s*[)}}]"


def _triple(match: re.Match[str]) -> tuple[int, int, int]:
    return tuple(int(match.group(i), 0) for i in (1, 2, 3))  # type: ignore[return-value]


def read_source(relative: str) -> str:
    path = SRC / relative
    if not path.exists():
        raise NotFound(f"src/{relative} does not exist")
    return path.read_text(encoding="utf-8")


def named_colour(relative: str, name: str) -> tuple[int, int, int]:
    """`constexpr QColor kName { r, g, b };` or `QColor kName(r, g, b);`."""
    text = read_source(relative)
    found = re.search(rf"QColor\s+{re.escape(name)}\s*{COLOUR}", text)
    if not found:
        raise NotFound(f"no definition of {name} in src/{relative}")
    return _triple(found)


def array_colour(relative: str, name: str, index: int) -> tuple[int, int, int]:
    """One element of `const QColor kName[N] = { QColor(...), ... };`."""
    text = read_source(relative)
    opening = re.search(rf"QColor\s+{re.escape(name)}\s*\[[^\]]*\]\s*=\s*\{{", text)
    if not opening:
        raise NotFound(f"no definition of {name}[] in src/{relative}")
    body = text[opening.end():]
    body = body[:body.index("};")]
    # Every element, including the four-argument ones AND the brace-initialised
    # ones. Matching only the three-argument call form silently SKIPS them and
    # shifts every later index by one — kNumberColours[0] is `QColor(0, 0, 0,
    # 0)`, so a three-only regex measured the wrong colour and still printed a
    # plausible number. A brace-only element does the same thing for the same
    # reason, and both spellings are in use in this tree.
    elements = re.findall(r"QColor\s*[({]([^)}]*)[)}]", body)
    if index >= len(elements):
        raise NotFound(f"{name}[] in src/{relative} has no element {index}")
    parts = [p.strip() for p in elements[index].split(",")]
    if len(parts) not in (3, 4):
        raise NotFound(f"{name}[{index}] in src/{relative} is not an r,g,b colour")
    return tuple(int(c, 0) for c in parts[:3])  # type: ignore[return-value]


TWENTY48 = "twenty48/twenty48view.cpp"


def tile_colour(value: int | None) -> tuple[int, int, int]:
    """One arm of 2048's tileColour(). `None` asks for the `default:` arm."""
    text = read_source(TWENTY48)
    body = re.search(r"QColor tileColour\(int value\)\s*\{(.*?)\n\}", text, re.S)
    if not body:
        raise NotFound(f"no tileColour() in src/{TWENTY48}")
    label = "default:" if value is None else f"case {value}:"
    found = re.search(rf"{re.escape(label)}\s*return\s+QColor\s*{COLOUR}", body.group(1))
    if not found:
        raise NotFound(f"tileColour() has no `{label}` arm")
    return _triple(found)


def tile_values() -> list[int | None]:
    """Every arm tileColour() actually has, newest included.

    Read rather than listed. A hand-written list cannot cover an arm added
    after it was written, and the omission is invisible: the run reports a full
    pass over the tiles it happens to know.
    """
    text = read_source(TWENTY48)
    body = re.search(r"QColor tileColour\(int value\)\s*\{(.*?)\n\}", text, re.S)
    if not body:
        raise NotFound(f"no tileColour() in src/{TWENTY48}")
    values: list[int | None] = [int(v) for v in re.findall(r"case\s+(\d+)\s*:", body.group(1))]
    if not values:
        raise NotFound("tileColour() has no `case` arms")
    if "default:" not in body.group(1):
        raise NotFound("tileColour() has no `default:` arm")
    return values + [None]


def ink_for(value: int | None) -> tuple[int, int, int]:
    """2048's inkFor(), evaluated the way the product evaluates it.

    The rule is read from the source too — both inks and the luminance cut-off
    — so a change to any of the three is measured rather than assumed.
    """
    text = read_source(TWENTY48)
    threshold = re.search(r"kLightTileLuminance\s*=\s*([0-9.]+)", text)
    if not threshold:
        raise NotFound(f"no kLightTileLuminance in src/{TWENTY48}")
    # Both ARMS, not just the condition. Naming the two inks here and pairing
    # them by hand hardcodes the polarity: swap the arms in the C++ and the
    # condition text is unchanged, so the guard still matches and this script
    # goes on measuring the ink the product no longer uses.
    rule = re.search(r"relativeLuminance\(tileColour\(value\)\)\s*>\s*kLightTileLuminance"
                     r"\s*\?\s*(\w+)\s*:\s*(\w+)", text)
    if not rule:
        raise NotFound("inkFor() is no longer a luminance test — reread §4.7")
    above, below = rule.group(1), rule.group(2)
    bright = relative_luminance(tile_colour(value)) > float(threshold.group(1))
    return named_colour(TWENTY48, above if bright else below)


# The pair list and the thresholds. Everything at 3.0 is a graphical object or
# large text; nothing here is small body text, so nothing is required at 4.5.
# Minesweeper's 4.35 is the row to notice: required at 3.0 it passes, and
# requiring it at 4.5 would fail a game nobody has complained about, which is
# how a check gets disabled.
PAIRS: list[tuple[str, object, object, float]] = [
    ("CardArt kRed on kFaceBottom",
     lambda: named_colour("cards/cardart.cpp", "kRed"),
     lambda: named_colour("cards/cardart.cpp", "kFaceBottom"), 3.0),
    ("CardArt kBlack on kFaceBottom",
     lambda: named_colour("cards/cardart.cpp", "kBlack"),
     lambda: named_colour("cards/cardart.cpp", "kFaceBottom"), 3.0),
    ("Sudoku kPencilInk on kPaper",
     lambda: named_colour("sudoku/sudokuview.cpp", "kPencilInk"),
     lambda: named_colour("sudoku/sudokuview.cpp", "kPaper"), 3.0),
    ("Sudoku kOwnInk on kPaper",
     lambda: named_colour("sudoku/sudokuview.cpp", "kOwnInk"),
     lambda: named_colour("sudoku/sudokuview.cpp", "kPaper"), 3.0),
    ("Minesweeper kNumberColours[1] on kDug",
     lambda: array_colour("minesweeper/minesweeperview.cpp", "kNumberColours", 1),
     lambda: named_colour("minesweeper/minesweeperview.cpp", "kDug"), 3.0),
    # The one pair in this list required at 4.5, and the reason the note above
    # says "nothing HERE is small body text" rather than "nothing ever is". The
    # caption is a sentence a partially sighted player reads slowly, not a
    # glyph they spot, so WCAG's normal-text bar is the one that applies to it.
    # It is also the only pair whose two colours exist solely to be read
    # against each other, so there is no third constraint pulling either way
    # and no reason to accept less.
    ("Theme kCaptionInk on kCaptionPlate",
     lambda: named_colour("theme.h", "kCaptionInk"),
     lambda: named_colour("theme.h", "kCaptionPlate"), 4.5),
]

def check_contrast() -> int:
    failures = 0
    pairs = list(PAIRS)

    # Every tile 2048 can paint, its own ink against its own colour — the
    # post-fix pairs, not §2.3's pre-fix measurement. `None` is the `default:`
    # arm, which the enumerated cases never reach.
    try:
        values = tile_values()
    except NotFound as missing:
        print(f"MISSING  2048 tile list  {missing}")
        failures += 1
        values = []
    for value in values:
        pairs.append((
            f"2048 ink on tile {'default (v > 2048)' if value is None else value}",
            (lambda v: lambda: ink_for(v))(value),
            (lambda v: lambda: tile_colour(v))(value),
            3.0,
        ))

    width = max(len(name) for name, *_ in pairs)
    for name, ink, background, required in pairs:
        try:
            ratio = contrast_ratio(ink(), background())
        except NotFound as missing:
            print(f"MISSING  {name.ljust(width)}  {missing}")
            failures += 1
            continue
        ok = ratio >= required
        print(f"{'pass' if ok else 'FAIL'}     {name.ljust(width)}"
              f"  {ratio:6.2f}  required {required:.1f}")
        if not ok:
            failures += 1
    print()
    print(f"{len(pairs)} pairs, {failures} failing." if failures
          else f"{len(pairs)} pairs, all meet their threshold.")
    return 1 if failures else 0


# INV-4: the 46-pixel threshold has exactly one definition, in cardart.h.
#
# The three exclusions each earn their place. The leading character class
# rejects hex digits and a decimal point, so `0x46` and `h * 0.46` do not match
# (there are five of the latter in src/); the second drops the DEFINITION
# itself; the last drops comment lines, two of which legitimately discuss the
# number in canastaview.cpp.
#
# The definition filter matches the assignment and not the name, for the reason
# stated below about `0x`: excusing a whole line because it mentions
# kFaceMinWidth anywhere gives a violation somewhere to hide, and a test a
# violation can hide from is not a test.
#
# It deliberately does NOT drop a line merely for containing `0x`, which would
# hide `if (w < 46.0 && c == 0xff)`.
#
# Scanned in Python rather than shelled out to grep. It was a `grep | grep |
# grep` pipeline under shell=True, which on Windows is cmd.exe: no grep, and
# the pattern's `$)` read as a command. The guard below caught that and failed
# rather than reporting an unearned pass, which is what it is for -- but the
# check had never once run on that platform.
THRESHOLD_RE = re.compile(r"(^|[^0-9A-Fa-fx.])46(\.0)?([^0-9A-Fa-f]|$)")
DEFINITION_RE = re.compile(r"kFaceMinWidth\s*=\s*46(\.0)?\b")
COMMENT_RE = re.compile(r"\s*//")


def check_thresholds() -> int:
    hits = []
    scanned = 0
    for path in sorted(SRC.rglob("*")):
        if path.suffix not in (".cpp", ".h"):
            continue
        scanned += 1
        rel = path.relative_to(ROOT).as_posix()
        for number, line in enumerate(path.read_text(encoding="utf-8",
                                                     errors="replace").splitlines(), 1):
            if not THRESHOLD_RE.search(line):
                continue
            if DEFINITION_RE.search(line) or COMMENT_RE.match(line):
                continue
            hits.append(f"{rel}:{number}: {line.strip()}")
    # INV-4 is the only thing that catches a second hardcoded 46, so a pass it
    # did not earn is a dead invariant announcing itself as green. A renamed or
    # empty src/ produces no hits, exactly like a clean tree.
    if not scanned:
        print("FAIL  the threshold scan found no sources under src/")
        return 1
    if hits:
        print("FAIL  the 46-pixel threshold is stated outside cardart.h:")
        for line in hits:
            print(f"      {line}")
        return 1
    print(f"pass  kFaceMinWidth in src/cards/cardart.h is the only "
          f"statement of the 46-pixel threshold ({scanned} sources scanned)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--thresholds", action="store_true",
                        help="run INV-4's grep instead of the contrast pairs")
    args = parser.parse_args()
    return check_thresholds() if args.thresholds else check_contrast()


if __name__ == "__main__":
    sys.exit(main())
