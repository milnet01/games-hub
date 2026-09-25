#!/usr/bin/env python3
"""Build packaging/gameshub-wordmark.svg: the app icon beside "Games Hub".

The lettering is drawn here, not set in a font: each letter is a handful of
round-capped strokes of one width. ROADMAP.md § Standing rules forbids a
third-party font in this repository, and a font's glyphs converted to
outlines are still that font. Being paths, the logo needs no font installed.
The icon is packaging/gameshub.svg, embedded as it is.

Made for the projects hub website's landing-page card, which wants a
wordmark that reads on a dark panel. Re-running this is byte-identical.

Run from the repository root:
    python3 tools/make_wordmark.py
"""

import re
from pathlib import Path

ICON = Path("packaging/gameshub.svg")
OUT = Path("packaging/gameshub-wordmark.svg")

HEIGHT = 200      # the whole logo; the icon fills it
ICON_SIZE = 200
GAP = 26          # icon to lettering
CAP = 66          # cap height, and the ascender of b
XH = 48           # x-height
W = 11            # stroke width
H = W / 2         # a stroke's centre sits this far inside the letter's box
TRACK = 10        # space between letters
# Two lines, so the logo comes out near 3:1 rather than 5:1. Caps run
# 22-88 and 112-178: a 24-unit gap, centred on the icon.
LINES = [("Games", "#f5f3ec", 88), ("Hub", "#f2b13c", 178)]  # the icon's cream and gold

R = (XH - W) / 2  # radius of every lowercase bowl, measured to the stroke centre
BX = H + R        # x of a bowl's centre
BY = -XH / 2      # y of a bowl's centre; y runs down from the baseline at 0


def n(v):
    """A coordinate, printed short and identically on every run."""
    return f"{v:.2f}".rstrip("0").rstrip(".")


def pt(x, y):
    return f"{n(x)},{n(y)}"


def arc(r, large, sweep, x, y):
    return f"A{n(r)},{n(r)} 0 {large} {sweep} {pt(x, y)}"


def bowl():
    return f'<circle cx="{n(BX)}" cy="{n(BY)}" r="{n(R)}"/>'


def letter_G():
    r = (CAP - W) / 2
    cx, cy = H + r, -CAP / 2
    k = r * 0.7071  # the opening's upper corner, at 45 degrees
    d = (f"M{pt(cx + k, cy - k)} {arc(r, 1, 0, cx + r, cy)} "
         f"H{n(cx + 3)}")
    return f'<path d="{d}"/>', 2 * r + W


def letter_a():
    stem = BX + R
    return (bowl() + f'<path d="M{pt(stem, -XH + H)} V{n(-H)}"/>',
            2 * R + W)


def letter_m():
    r = 13
    x1, x2, x3 = H, H + 2 * r, H + 4 * r
    top = -XH + H + r  # arch centres, so the arches meet the x-height
    d = (f"M{pt(x1, -XH + H)} V{n(-H)} "
         f"M{pt(x1, top)} {arc(r, 0, 1, x2, top)} V{n(-H)} "
         f"M{pt(x2, top)} {arc(r, 0, 1, x3, top)} V{n(-H)}")
    return f'<path d="{d}"/>', 4 * r + W


def letter_e():
    k = R * 0.7071
    d = (f"M{pt(H, BY)} H{n(BX + R)} "
         f"{arc(R, 1, 0, BX + k, BY + k)}")
    return f'<path d="{d}"/>', 2 * R + W


def letter_s():
    # Two half-ovals, one over the other, meeting at the middle.
    rx, ry = R - 1, (XH - W) / 4
    cx = BX - 1
    top, low = -XH + H + ry, -H - ry
    d = (f"M{pt(cx + rx * 0.866, top - ry * 0.5)} "
         f"A{n(rx)},{n(ry)} 0 1 0 {pt(cx, BY)} "
         f"A{n(rx)},{n(ry)} 0 1 1 {pt(cx - rx * 0.866, low + ry * 0.5)}")
    return f'<path d="{d}"/>', 2 * R + W - 2


def letter_H():
    x1, x2 = H, H + 44
    d = (f"M{pt(x1, -CAP + H)} V{n(-H)} M{pt(x2, -CAP + H)} V{n(-H)} "
         f"M{pt(x1, -CAP / 2)} H{n(x2)}")
    return f'<path d="{d}"/>', 44 + W


def letter_u():
    d = (f"M{pt(H, -XH + H)} V{n(BY)} {arc(R, 0, 0, BX + R, BY)} "
         f"M{pt(BX + R, -XH + H)} V{n(-H)}")
    return f'<path d="{d}"/>', 2 * R + W


def letter_b():
    return (bowl() + f'<path d="M{pt(H, -CAP + H)} V{n(-H)}"/>',
            2 * R + W)


LETTERS = {"G": letter_G, "a": letter_a, "m": letter_m, "e": letter_e,
           "s": letter_s, "H": letter_H, "u": letter_u, "b": letter_b}


def line(text, colour, baseline, x0):
    """One line of lettering as an SVG group, and the x where it ends."""
    parts = []
    x = x0
    for ch in text:
        shape, width = LETTERS[ch]()
        parts.append(f'<g transform="translate({pt(x, baseline)})">{shape}</g>')
        x += width + TRACK
    group = (f'  <g fill="none" stroke="{colour}" stroke-width="{n(W)}" '
             f'stroke-linecap="round" stroke-linejoin="round">'
             + "".join(parts) + "</g>")
    return group, x - TRACK


def main():
    x0 = ICON_SIZE + GAP
    groups = []
    right = x0
    for text, colour, baseline in LINES:
        group, end = line(text, colour, baseline, x0)
        groups.append(group)
        right = max(right, end)
    width = round(right + 4)

    icon = ICON.read_text()
    inner = re.search(r"<svg[^>]*>(.*)</svg>", icon, re.S).group(1)

    OUT.write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {HEIGHT}" '
        f'width="{width}" height="{HEIGHT}">\n'
        "  <title>Games Hub</title>\n"
        # A group, not a nested <svg>: SVG Tiny readers such as Qt's skip those.
        f'  <g transform="scale({ICON_SIZE / 128:g})">{inner}</g>\n'
        + "\n".join(groups)
        + "\n</svg>\n"
    )
    print(f"{OUT} {width}x{HEIGHT}")


if __name__ == "__main__":
    main()
