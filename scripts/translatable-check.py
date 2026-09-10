#!/usr/bin/env python3
"""Fail when a word a player reads cannot be translated (GHUB-0161 § 4.5).

Reads every .cpp and .h under src/ and names, by file and line:

  * a string literal holding a letter that is not an argument of a
    translation call, not an argument of a call in RECOGNISED, and not the one
    literal a `// untranslated: <reason>` marker on its line exempts;
  * a marker with no reason after it;
  * a `+` (or `+=`) directly joining a translation call to anything else,
    because a sentence assembled from pieces fixes the English word order.

Exits 0 and prints nothing when every literal is accounted for.

**RECOGNISED is the one place to extend the calls whose arguments are read by
a program rather than a player.** Anything else that is not for a player
carries the marker, with its reason, on its own line.

The scan is a tokenizer, not a compiler: it knows comments, string and
character literals, and which call a literal sits inside. The innermost
enclosing call decides, looking through the wrappers in TRANSPARENT and
through plain grouping parentheses, and stopping at a brace -- so a literal in
a lambda body is judged by its own call, not by the call the lambda was passed
to.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

# The calls whose every argument goes to lupdate. `tr` also matches a
# class-qualified `HubWindow::tr`.
TRANSLATION = {
    "tr",
    "QCoreApplication::translate",
    "QT_TRANSLATE_NOOP",
    "QT_TRANSLATE_N_NOOP",
}

# The calls whose literal arguments are read by a program, not a player.
RECOGNISED = {
    "setObjectName",
    # QSettings. `value` and `remove` are generic names, so a player-facing
    # literal handed to some other class's `value()` would pass here; nothing in
    # src/ does that today.
    "setValue", "value", "remove", "contains",
    "beginGroup", "beginReadArray", "beginWriteArray",
    # Terminal output.
    "qDebug", "qInfo", "qWarning", "qCritical",
    "std::printf", "std::fprintf",
    # QCoreApplication identity -- QSettings derives its file from the first two.
    "setOrganizationName", "setApplicationName", "setApplicationDisplayName",
    "setDesktopFileName",
    "QIcon::fromTheme",
}

# Wrappers that build a string without deciding who reads it. A literal inside
# one is judged by the call around the wrapper.
TRANSPARENT = {
    "QStringLiteral", "QLatin1String", "QLatin1StringView", "QString",
    "QStringView", "QByteArray", "QByteArrayLiteral", "QLatin1Char",
    "qPrintable", "qUtf8Printable",
}

MARKER_RE = re.compile(r"//\s*untranslated:(.*)$")
# Qt's argument placeholders are not words: %1, %L1, and the plural %n.
PLACEHOLDER_RE = re.compile(r"%L?[0-9]+|%n")
ESCAPE_RE = re.compile(r"\\(x[0-9A-Fa-f]+|u[0-9A-Fa-f]{4}|U[0-9A-Fa-f]{8}|[0-7]{1,3}|.)")
STRING_PREFIXES = {"u8", "u", "U", "L", "R", "u8R", "uR", "UR", "LR"}
NOT_CALLS = {"if", "while", "for", "switch", "return", "sizeof", "alignof",
             "decltype", "noexcept", "catch", "static_assert"}


def holds_letter(body: str) -> bool:
    text = PLACEHOLDER_RE.sub("", ESCAPE_RE.sub("", body))
    return any(ch.isalpha() for ch in text)


def is_translation(name: str | None) -> bool:
    return name is not None and (name in TRANSLATION or name.endswith("::tr"))


def is_recognised(name: str | None) -> bool:
    """`QGuiApplication::setDesktopFileName` is `setDesktopFileName`."""
    return name is not None and (name in RECOGNISED
                                 or name.rsplit("::", 1)[-1] in RECOGNISED)


class Scanner:
    """One file: tokens, the call stack, and what each line owes."""

    def __init__(self, rel: str, text: str) -> None:
        self.rel = rel
        self.text = text
        self.problems: list[str] = []
        self.markers: dict[int, str] = {}           # line -> reason
        self.bare: dict[int, list[str]] = {}         # line -> unaccounted literals
        self.stack: list[tuple[str, str | None]] = []  # (bracket, call name)
        self.prev: tuple[str, str] = ("", "")       # last significant token
        self.closed_translation = False              # prev token ended a tr(...)

    # -- literals -----------------------------------------------------------

    def enclosing_call(self) -> str | None:
        for bracket, name in reversed(self.stack):
            if bracket == "{":
                return None
            if name is None or name in TRANSPARENT:
                continue
            return name
        return None

    def literal(self, body: str, line: int) -> None:
        if not holds_letter(body):
            return
        call = self.enclosing_call()
        if is_translation(call) or is_recognised(call):
            return
        where = f"argument of {call}" if call else "in no call"
        shown = body if len(body) <= 50 else body[:47] + "..."
        self.bare.setdefault(line, []).append(f'"{shown}" ({where})')

    # -- tokens -------------------------------------------------------------

    def punct(self, ch: str, line: int) -> None:
        joins_after = self.closed_translation
        self.closed_translation = False
        if ch in "+" and joins_after:
            self.problems.append(f"{self.rel}:{line}: a translation call is "
                                 f"joined with '+'; translate the whole sentence")
        if ch in "([{":
            name = self.prev[1] if self.prev[0] == "ident" else None
            if name in NOT_CALLS:
                name = None
            self.stack.append((ch, name))
        elif ch in ")]}":
            if self.stack:
                _, name = self.stack.pop()
                self.closed_translation = ch == ")" and is_translation(name)
        self.prev = ("punct", ch)

    def ident(self, name: str, line: int) -> None:
        if self.prev == ("punct", "+") and is_translation(name):
            self.problems.append(f"{self.rel}:{line}: '+' joins something to a "
                                 f"translation call; translate the whole sentence")
        self.closed_translation = False
        self.prev = ("ident", name)

    # -- the walk -----------------------------------------------------------

    def scan(self) -> None:
        text, n = self.text, len(self.text)
        i, line = 0, 1
        at_line_start = True
        while i < n:
            ch = text[i]
            if ch == "\n":
                line += 1
                i += 1
                at_line_start = True
                continue
            if ch in " \t\r\f\v":
                i += 1
                continue
            if ch == "\\" and text.startswith("\n", i + 1):   # line splice
                i += 2
                line += 1
                continue
            if at_line_start and ch == "#":
                end = text.find("\n", i)
                end = n if end < 0 else end
                directive = text[i:end]
                if re.match(r"#\s*include\b", directive):
                    i = end
                    continue
                i += 1                     # other directives: scan the rest as code
                at_line_start = False
                continue
            at_line_start = False
            if text.startswith("//", i):
                end = text.find("\n", i)
                end = n if end < 0 else end
                marker = MARKER_RE.search(text[i:end])
                if marker:
                    self.markers[line] = marker.group(1).strip()
                i = end
                continue
            if text.startswith("/*", i):
                end = text.find("*/", i + 2)
                end = n if end < 0 else end + 2
                line += text.count("\n", i, end)
                i = end
                continue
            if ch == '"':
                i, line = self.string_at(i, line, raw=False)
                continue
            if ch == "'":
                j = i + 1
                while j < n and text[j] != "'":
                    j += 2 if text[j] == "\\" else 1
                i = j + 1
                self.prev = ("literal", "")
                self.closed_translation = False
                continue
            if ch.isdigit() or (ch == "." and i + 1 < n and text[i + 1].isdigit()):
                j = i + 1
                while j < n and (text[j].isalnum() or text[j] in "._'"
                                 or (text[j] in "+-" and text[j - 1] in "eEpP")):
                    j += 1
                i = j
                self.prev = ("number", "")
                self.closed_translation = False
                continue
            if ch.isalpha() or ch == "_":
                j = i + 1
                while j < n and (text[j].isalnum() or text[j] == "_"):
                    j += 1
                word = text[i:j]
                if word in STRING_PREFIXES and j < n and text[j] == '"':
                    i, line = self.string_at(j, line, raw="R" in word)
                    continue
                # Fold `a::b::c` into one qualified name.
                while text.startswith("::", j):
                    k = j + 2
                    while k < n and text[k] in " \t":
                        k += 1
                    m = k
                    while m < n and (text[m].isalnum() or text[m] == "_"):
                        m += 1
                    if m == k:
                        break
                    word, j = text[i:m].replace(" ", "").replace("\t", ""), m
                self.ident(word, line)
                i = j
                continue
            if text.startswith("+=", i):
                self.punct("+", line)
                i += 2
                continue
            if text.startswith("++", i):
                self.prev = ("punct", "++")
                self.closed_translation = False
                i += 2
                continue
            if text.startswith("->", i):
                self.prev = ("punct", "->")
                self.closed_translation = False
                i += 2
                continue
            self.punct(ch, line)
            i += 1

    def string_at(self, i: int, line: int, raw: bool) -> tuple[int, int]:
        """`i` is the opening quote. Returns the index after the literal."""
        text, n = self.text, len(self.text)
        start_line = line
        if raw:
            paren = text.index("(", i)
            delim = ")" + text[i + 1:paren] + '"'
            end = text.index(delim, paren)
            body = text[paren + 1:end]
            j = end + len(delim)
            line += body.count("\n")
        else:
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            body = text[i + 1:j]
            j += 1
        while j < n and (text[j].isalnum() or text[j] == "_"):   # _s, _L1
            j += 1
        self.literal(body if not raw else body.replace("\\", "\\\\"), start_line)
        self.prev = ("literal", "")
        self.closed_translation = False
        return j, line

    # -- verdict ------------------------------------------------------------

    def verdict(self) -> list[str]:
        out = list(self.problems)
        for number, reason in sorted(self.markers.items()):
            if not reason:
                out.append(f"{self.rel}:{number}: '// untranslated:' carries no reason")
        for number, literals in sorted(self.bare.items()):
            # A marker exempts exactly one literal, so it cannot hide a bare
            # label that shares the line with a key.
            owed = literals[1:] if self.markers.get(number) else literals
            for shown in owed:
                out.append(f"{self.rel}:{number}: {shown} is not translated")
        return sorted(out, key=lambda s: (s.split(":")[0], int(s.split(":")[1])))


def main() -> int:
    sources = sorted(p for p in SRC.rglob("*") if p.suffix in (".cpp", ".h"))
    # A renamed or empty src/ produces no findings, exactly like a clean tree.
    if not sources:
        print("FAIL  no .cpp or .h sources under src/")
        return 1
    findings: list[str] = []
    for path in sources:
        scanner = Scanner(path.relative_to(ROOT).as_posix(),
                          path.read_text(encoding="utf-8"))
        scanner.scan()
        findings.extend(scanner.verdict())
    for finding in findings:
        print(finding)
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
