#!/usr/bin/env python3
"""Check the score book agrees with the game about the numbers they share.

    python3 scripts/scorepad-check.py

`scorepad/index.html` is a phone score book for a real table: the players
work out each hand themselves, so it does NOT reimplement the scoring. But it
does have to SHOW the opening minimum each side needs, and that is decided by
the same bands the desktop game plays by. Those numbers therefore exist in two
places, which is exactly the drift this project refuses elsewhere — the donate
URLs are generated from .github/FUNDING.yml for the same reason.

This script holds NO copy of its own. It reads every number out of both
sources and compares them, so a house rule changed in canastaengine.h fails
here rather than quietly leaving the phone telling the table a stale figure.
A named constant it cannot find is a failure, not a skipped row.

Exit status is 0 only when every shared number matches and every constant was
found.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / "src" / "canasta" / "canastaengine.h"
SOURCE = ROOT / "src" / "canasta" / "canastaengine.cpp"
APP = ROOT / "scorepad" / "index.html"

problems: list[str] = []


def read(path: Path) -> str:
    if not path.is_file():
        problems.append(f"missing file: {path.relative_to(ROOT)}")
        return ""
    return path.read_text(encoding="utf-8")


def cpp_default(text: str, field: str) -> int | None:
    """A plain `int <field> = <n>;` default out of the Rules struct."""
    m = re.search(rf"\bint\s+{re.escape(field)}\s*=\s*(-?\d+)\s*;", text)
    if not m:
        problems.append(f"canastaengine.h: no default found for `{field}`")
        return None
    return int(m.group(1))


def cpp_bands(text: str) -> list[tuple[int | None, str]] | None:
    """The (threshold, rules-field) ladder out of openRequirementFor()."""
    m = re.search(r"int\s+openRequirementFor\s*\([^)]*\)\s*\{(.*?)\n\}", text, re.S)
    if not m:
        problems.append("canastaengine.cpp: openRequirementFor() not found")
        return None
    body = m.group(1)
    bands: list[tuple[int | None, str]] = [
        (int(t), f) for t, f in re.findall(r"if\s*\(\s*score\s*<\s*(-?\d+)\s*\)\s*return\s+rules\.(\w+)\s*;", body)
    ]
    tail = re.findall(r"return\s+rules\.(\w+)\s*;", body)
    if not bands or not tail:
        problems.append("canastaengine.cpp: openRequirementFor() did not parse into bands")
        return None
    bands.append((None, tail[-1]))
    return bands


def js_bands(text: str) -> list[tuple[int | None, int]] | None:
    m = re.search(r"const\s+OPENING_BANDS\s*=\s*\[(.*?)\]\s*;", text, re.S)
    if not m:
        problems.append("scorepad/index.html: OPENING_BANDS not found")
        return None
    out: list[tuple[int | None, int]] = []
    for below, need in re.findall(r"below\s*:\s*(null|-?\d+)\s*,\s*need\s*:\s*(-?\d+)", m.group(1)):
        out.append((None if below == "null" else int(below), int(need)))
    if not out:
        problems.append("scorepad/index.html: OPENING_BANDS did not parse")
        return None
    return out


def js_const(text: str, name: str) -> int | None:
    m = re.search(rf"const\s+{re.escape(name)}\s*=\s*(-?\d+)\s*;", text)
    if not m:
        problems.append(f"scorepad/index.html: no `{name}` found")
        return None
    return int(m.group(1))


def main() -> int:
    header, source, app = read(HEADER), read(SOURCE), read(APP)
    if problems:
        for p in problems:
            print(f"FAIL  {p}")
        return 1

    cpp = cpp_bands(source)
    js = js_bands(app)

    if cpp is not None and js is not None:
        want = [(threshold, cpp_default(header, field)) for threshold, field in cpp]
        if len(want) != len(js):
            problems.append(f"opening bands: game has {len(want)} bands, score book has {len(js)}")
        else:
            for i, ((ct, cv), (jt, jv)) in enumerate(zip(want, js)):
                if ct != jt or cv != jv:
                    problems.append(
                        f"opening band {i + 1}: game says below {ct} needs {cv}, "
                        f"score book says below {jt} needs {jv}")
                else:
                    print(f"ok    opening band {i + 1}: below {ct} needs {cv}")

    for field, name in (("targetScore", "DEFAULT_TARGET"), ("goingOutBonus", "DEFAULT_OUT_BONUS")):
        c, j = cpp_default(header, field), js_const(app, name)
        if c is not None and j is not None:
            if c != j:
                problems.append(f"{name}: game says {c}, score book says {j}")
            else:
                print(f"ok    {name} matches Rules::{field} ({c})")

    for p in problems:
        print(f"FAIL  {p}")
    if problems:
        print(f"\n{len(problems)} mismatch(es). The score book and the game disagree about "
              f"numbers they share.")
        return 1
    print("\nThe score book and the game agree on every shared number.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
