#!/usr/bin/env python3
"""Draw the score book's home-screen icon.

Generated rather than drawn by hand, for the same reason tools/make_sounds.py
generates the sound effects: re-running it is byte-identical, there is no
licensing question, and the source of the artwork is the repository rather
than somebody's downloads folder.

    python3 tools/make_scorepad_icon.py

Writes scorepad/icon-192.png and scorepad/icon-512.png. No font is used —
every shape is a primitive — so it renders the same on any machine.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

FELT = (20, 83, 45)      # --felt in scorepad/index.html
PAPER = (255, 253, 248)  # --card
RULE = (150, 170, 155)
INK = (140, 47, 24)      # --accent

OUT = Path(__file__).resolve().parent.parent / "scorepad"


def draw(size: int) -> Image.Image:
    # 4x supersampling, then a single downscale: the diagonals and the rounded
    # corners come out clean without any anti-aliasing code of our own.
    s = size * 4
    img = Image.new("RGB", (s, s), FELT)
    d = ImageDraw.Draw(img)

    # A page, inset far enough that a maskable launcher can crop the corners
    # off the felt without ever reaching the page.
    m = int(s * 0.22)
    d.rounded_rectangle([m, m, s - m, s - m], radius=int(s * 0.035), fill=PAPER)

    # Ruled lines with a column rule down the right, which is what makes it
    # read as a score book rather than a blank card.
    left, right = m + int(s * 0.055), s - m - int(s * 0.055)
    col = right - int((right - left) * 0.34)
    top, bottom = m + int(s * 0.085), s - m - int(s * 0.085)
    rows = 4
    step = (bottom - top) / rows
    w = max(1, int(s * 0.011))
    for i in range(rows + 1):
        y = int(top + i * step)
        d.line([left, y, right, y], fill=RULE, width=w)
    d.line([col, top, col, bottom], fill=RULE, width=w)

    # One entry filled in, so the icon says "scores go here".
    for i in range(rows):
        y = int(top + i * step + step / 2)
        d.line([left + int(s * 0.02), y, left + int(s * 0.075), y],
               fill=INK if i == 0 else RULE, width=int(w * 1.8))

    return img.resize((size, size), Image.LANCZOS)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for size in (192, 512):
        path = OUT / f"icon-{size}.png"
        draw(size).save(path, optimize=True)
        print(f"wrote {path} ({path.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
