#!/usr/bin/env python3
"""Generates every sound effect the games use.

The effects are synthesised rather than sampled so the whole project stays
original work under one licence — there is no third-party audio anywhere in
the repository. Re-run this after editing a recipe:

    python3 tools/make_sounds.py

Writes 16-bit mono WAVs into assets/sounds/, which sounds.qrc compiles into
the binary.
"""

import math
import pathlib
import random
import struct
import wave

RATE = 44100
OUT = pathlib.Path(__file__).resolve().parent.parent / "assets" / "sounds"


def envelope(i, n, attack=0.01, release=0.6, curve=2.0):
    """Percussive shape: near-instant attack, then a curved decay."""
    t = i / n
    a = min(1.0, t / attack) if attack > 0 else 1.0
    r = max(0.0, 1.0 - (t - attack) / max(1e-6, release)) if t > attack else 1.0
    return a * (r ** curve)


def sine(freq, t):
    return math.sin(2 * math.pi * freq * t)


def triangle(freq, t):
    phase = (freq * t) % 1.0
    return 4 * abs(phase - 0.5) - 1


def render(name, seconds, fn, gain=0.72):
    n = int(RATE * seconds)
    frames = bytearray()
    peak = 0.0
    samples = []
    for i in range(n):
        v = fn(i, n, i / RATE)
        samples.append(v)
        peak = max(peak, abs(v))

    # Normalise so every effect sits at a comparable level.
    scale = (gain / peak) if peak > 1e-9 else 0.0
    for v in samples:
        s = int(max(-1.0, min(1.0, v * scale)) * 32767)
        frames += struct.pack("<h", s)

    OUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT / f"{name}.wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(bytes(frames))
    print(f"  {name}.wav  {seconds*1000:.0f}ms")


rng = random.Random(7)  # fixed seed: regenerating gives byte-identical files


def noise():
    return rng.uniform(-1.0, 1.0)


# --- Interface -------------------------------------------------------------

def ui_click(i, n, t):
    return sine(1250, t) * envelope(i, n, 0.002, 0.5, 3.0) * 0.5


def ui_back(i, n, t):
    return sine(620, t) * envelope(i, n, 0.004, 0.6, 2.5) * 0.5


# --- Reversi ---------------------------------------------------------------

def disc_place(i, n, t):
    """A weighted counter meeting a felt board: a click over a low thud."""
    thud = sine(190 - 70 * t / 0.12, t) * envelope(i, n, 0.003, 0.55, 2.0)
    click = noise() * envelope(i, n, 0.0005, 0.05, 4.0) * 0.35
    return thud + click


def disc_flip(i, n, t):
    sweep = triangle(420 + 500 * (t / 0.09), t) * envelope(i, n, 0.01, 0.7, 2.0)
    return sweep * 0.5


# --- Minesweeper -----------------------------------------------------------

def dig(i, n, t):
    return (noise() * 0.5 + sine(880, t)) * envelope(i, n, 0.001, 0.35, 4.0) * 0.5


def flag_plant(i, n, t):
    return (sine(720, t) + sine(1080, t) * 0.6) * envelope(i, n, 0.002, 0.5, 3.0) * 0.5


def boom(i, n, t):
    """Low rumble plus a noise burst that decays slowly."""
    rumble = sine(60 + 30 * math.exp(-t * 6), t) * envelope(i, n, 0.002, 0.95, 1.4)
    crack = noise() * envelope(i, n, 0.001, 0.5, 2.2)
    return rumble * 0.8 + crack * 0.55


# --- Cards -----------------------------------------------------------------

def card_deal(i, n, t):
    """Card sliding off the deck: filtered noise with a quick swell."""
    body = noise() * envelope(i, n, 0.05, 0.75, 1.6)
    return body * (0.35 + 0.65 * math.sin(math.pi * min(1.0, t / 0.07))) * 0.7


def card_place(i, n, t):
    thud = sine(230, t) * envelope(i, n, 0.004, 0.5, 2.4)
    tap = noise() * envelope(i, n, 0.001, 0.08, 3.0) * 0.4
    return thud * 0.7 + tap


def shuffle(i, n, t):
    riffle = noise() * envelope(i, n, 0.08, 0.9, 1.2)
    flutter = 0.6 + 0.4 * math.sin(2 * math.pi * 26 * t)
    return riffle * flutter * 0.7


# --- Pinball ---------------------------------------------------------------

def bumper(i, n, t):
    """Bright two-tone ding with a metallic edge."""
    tone = sine(880, t) + 0.55 * sine(1320, t) + 0.3 * sine(1760, t)
    return tone * envelope(i, n, 0.001, 0.8, 2.6) * 0.5


def slingshot(i, n, t):
    return (sine(1500 - 500 * t / 0.12, t) + noise() * 0.35) * envelope(i, n, 0.001, 0.55, 3.0) * 0.5


def flipper(i, n, t):
    clack = noise() * envelope(i, n, 0.0005, 0.06, 4.0)
    thump = sine(150, t) * envelope(i, n, 0.002, 0.3, 3.0)
    return clack * 0.6 + thump * 0.5


def launch(i, n, t):
    """Plunger release: a rising sweep with a spring twang."""
    freq = 180 + 620 * (t / 0.28)
    return (triangle(freq, t) * 0.7 + sine(freq * 2, t) * 0.2) * envelope(i, n, 0.01, 0.85, 1.6)


def drain(i, n, t):
    freq = 420 * math.exp(-t * 5.0) + 70
    return sine(freq, t) * envelope(i, n, 0.01, 0.9, 1.5) * 0.7


# --- Outcomes --------------------------------------------------------------

def fanfare(i, n, t):
    """Three rising notes — used whenever a game is won."""
    notes = [523.25, 659.25, 783.99]  # C E G
    step = 1.0 / len(notes)
    idx = min(len(notes) - 1, int(t / (0.42 * step)))
    local = t - idx * 0.42 * step
    body = sine(notes[idx], local) + 0.4 * sine(notes[idx] * 2, local)
    return body * envelope(i, n, 0.01, 0.95, 1.1) * 0.55


def lose(i, n, t):
    notes = [392.0, 329.63, 261.63]  # G E C, falling
    step = 1.0 / len(notes)
    idx = min(len(notes) - 1, int(t / (0.5 * step)))
    local = t - idx * 0.5 * step
    return sine(notes[idx], local) * envelope(i, n, 0.01, 0.95, 1.2) * 0.55


RECIPES = [
    ("ui_click", 0.06, ui_click),
    ("ui_back", 0.08, ui_back),
    ("disc_place", 0.14, disc_place),
    ("disc_flip", 0.10, disc_flip),
    ("dig", 0.07, dig),
    ("flag", 0.10, flag_plant),
    ("boom", 0.70, boom),
    ("card_deal", 0.13, card_deal),
    ("card_place", 0.11, card_place),
    ("shuffle", 0.45, shuffle),
    ("bumper", 0.26, bumper),
    ("slingshot", 0.16, slingshot),
    ("flipper", 0.07, flipper),
    ("launch", 0.30, launch),
    ("drain", 0.55, drain),
    ("win", 0.50, fanfare),
    ("lose", 0.60, lose),
]

if __name__ == "__main__":
    print(f"writing {len(RECIPES)} effects to {OUT}")
    for name, seconds, fn in RECIPES:
        rng.seed(7)  # per-effect reset keeps output reproducible
        render(name, seconds, fn)
    print("done")
