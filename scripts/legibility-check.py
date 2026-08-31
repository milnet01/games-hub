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
import subprocess
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


TRIPLE = r"\{\s*(0x[0-9A-Fa-f]{1,2}|\d{1,3})\s*,\s*(0x[0-9A-Fa-f]{1,2}|\d{1,3})\s*,\s*(0x[0-9A-Fa-f]{1,2}|\d{1,3})\s*\}"
CALL = r"\(\s*(0x[0-9A-Fa-f]{1,2}|\d{1,3})\s*,\s*(0x[0-9A-Fa-f]{1,2}|\d{1,3})\s*,\s*(0x[0-9A-Fa-f]{1,2}|\d{1,3})\s*\)"


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
    for pattern in (rf"QColor\s+{re.escape(name)}\s*{TRIPLE}",
                    rf"QColor\s+{re.escape(name)}\s*{CALL}"):
        found = re.search(pattern, text)
        if found:
            return _triple(found)
    raise NotFound(f"no definition of {name} in src/{relative}")


def array_colour(relative: str, name: str, index: int) -> tuple[int, int, int]:
    """One element of `const QColor kName[N] = { QColor(...), ... };`."""
    text = read_source(relative)
    opening = re.search(rf"QColor\s+{re.escape(name)}\s*\[[^\]]*\]\s*=\s*\{{", text)
    if not opening:
        raise NotFound(f"no definition of {name}[] in src/{relative}")
    body = text[opening.end():]
    body = body[:body.index("};")]
    # Every element, including the four-argument ones. Matching only the
    # three-argument form silently SKIPS them and shifts every later index by
    # one — kNumberColours[0] is `QColor(0, 0, 0, 0)`, so a three-only regex
    # measured the wrong colour and still printed a plausible number.
    elements = re.findall(r"QColor\s*\(([^)]*)\)", body)
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
    found = re.search(rf"{re.escape(label)}\s*return\s+QColor{CALL}", body.group(1))
    if not found:
        raise NotFound(f"tileColour() has no `{label}` arm")
    return _triple(found)


def ink_for(value: int | None) -> tuple[int, int, int]:
    """2048's inkFor(), evaluated the way the product evaluates it.

    The rule is read from the source too — both inks and the luminance cut-off
    — so a change to any of the three is measured rather than assumed.
    """
    text = read_source(TWENTY48)
    threshold = re.search(r"kLightTileLuminance\s*=\s*([0-9.]+)", text)
    if not threshold:
        raise NotFound(f"no kLightTileLuminance in src/{TWENTY48}")
    if "relativeLuminance(tileColour(value)) > kLightTileLuminance" not in text:
        raise NotFound("inkFor() is no longer a luminance test — reread §4.7")
    dark = named_colour(TWENTY48, "kDarkInk")
    light = named_colour(TWENTY48, "kLightInk")
    return dark if relative_luminance(tile_colour(value)) > float(threshold.group(1)) else light


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

# Every tile 2048 can paint, its own ink against its own colour — the post-fix
# pairs, not §2.3's pre-fix measurement. `None` is the `default:` arm, which a
# list of the enumerated cases alone never reaches.
for _value in [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, None]:
    PAIRS.append((
        f"2048 ink on tile {'default (v > 2048)' if _value is None else _value}",
        (lambda v: lambda: ink_for(v))(_value),
        (lambda v: lambda: tile_colour(v))(_value),
        3.0,
    ))


def check_contrast() -> int:
    failures = 0
    width = max(len(name) for name, *_ in PAIRS)
    for name, ink, background, required in PAIRS:
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
    print(f"{len(PAIRS)} pairs, {failures} failing." if failures
          else f"{len(PAIRS)} pairs, all meet their threshold.")
    return 1 if failures else 0


# INV-4: the 46-pixel threshold has exactly one definition, in cardart.h.
#
# The three exclusions each earn their place. The leading character class
# rejects hex digits and a decimal point, so `0x46` and `h * 0.46` do not match
# (there are five of the latter in src/); the kFaceMinWidth filter drops the
# definition line itself; the last drops comment lines, two of which
# legitimately discuss the number in canastaview.cpp.
#
# It deliberately does NOT filter with `grep -v 0x`, which drops the WHOLE line
# and so hides `if (w < 46.0 && c == 0xff)` — a test a violation can hide from
# is not a test.
THRESHOLD_GREP = (
    r"grep -rnE '(^|[^0-9A-Fa-fx.])46(\.0)?([^0-9A-Fa-f]|$)' src "
    r"--include=*.cpp --include=*.h "
    r"| grep -v kFaceMinWidth "
    r"| grep -vE '^[^:]+:[0-9]+: *//'"
)


def check_thresholds() -> int:
    result = subprocess.run(THRESHOLD_GREP, shell=True, cwd=ROOT,
                            capture_output=True, text=True)
    # grep exits 1 for "no matches", which is the pass. Anything else means the
    # pipeline could not run -- no grep on PATH, src/ renamed, a bad -E pattern
    # -- and that produces empty stdout, exactly like a clean tree. INV-4 is the
    # only thing that catches a second hardcoded 46, so a pass it did not earn
    # is a dead invariant announcing itself as green.
    if result.returncode not in (0, 1) or result.stderr.strip():
        print("FAIL  the threshold grep could not run:")
        for line in result.stderr.splitlines() or [f"exit status {result.returncode}"]:
            print(f"      {line}")
        return 1
    hits = [line for line in result.stdout.splitlines() if line.strip()]
    if hits:
        print("FAIL  the 46-pixel threshold is stated outside cardart.h:")
        for line in hits:
            print(f"      {line}")
        return 1
    print("pass  kFaceMinWidth in src/cards/cardart.h is the only "
          "statement of the 46-pixel threshold")
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
