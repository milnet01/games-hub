#!/usr/bin/env python3
"""Check that the repository's markdown names files and headings that exist
(GHUB-0208).

Two kinds of reference, both resolved against the commit being checked:

    links   [text](path) and [text](path#heading), in every tracked .md file.
            The file must exist, and a heading must be one the file has,
            spelled as GitHub spells its anchors.
    paths   a repository path written in backticks, such as
            `scripts/local-ci.sh` or `src/main.cpp:58`, in every LIVING
            document. The path must be a tracked file or directory.

This is the documentation check a docs-only push runs. Before it, such a push
ran the workflow linters alone, and nothing read the prose at all
(~/.claude/standards/local-gate.md section 6 asks for the cheapest real one).
**Links alone would have checked almost nothing**: these documents link
rarely and name files in backticks instead, so the backticked paths are where
a renamed or deleted file leaves a document pointing at nothing.

**Records are not path-checked.** A changelog entry, a roadmap item or a
dated review describes the tree as it stood then, and a path that has since
gone is true history, not a defect -- rewriting it to pass would falsify the
record. RECORDS lists them. Their links are still checked: a link is a
promise that something can be clicked through to now.

**Web links are not fetched.** A check that depends on somebody else's server
goes red for reasons no commit here caused, and a gate that reddens at random
teaches people to bypass it. The result depends on nothing but the commit.

**A link outside the checkout is reported, not followed** -- `~/.claude/...`
or an absolute path names this machine, and on any other clone, and on CI,
it points at nothing. A backticked path outside the checkout is not a
repository path, so it is not checked.

Exit status is 0 only when every reference resolves.
"""

from __future__ import annotations

import fnmatch
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Documents describing the tree as it was: their backticked paths are history.
RECORDS = ("CHANGELOG.md", "ROADMAP.md", "docs/reviews/*",
           "docs/claude-md-review-*.md", ".claude/rules/claude-md-review.md")

# A path a living document names BECAUSE it does not exist. Each entry is the
# document and the path, and the reason is the sentence it sits in.
ABSENT_ON_PURPOSE = {
    # "No ADRs yet, and `docs/decisions/` does not exist."
    ("docs/design.md", "docs/decisions"),
}

FENCE = re.compile(r"^\s*(```|~~~)")
INLINE_CODE = re.compile(r"(`+).*?\1")
CODE_SPAN = re.compile(r"`([^`\s]+)`")
# [text](target) and [text](target "title"); the target cannot hold a space.
INLINE_LINK = re.compile(
    r"\[(?:[^\[\]]|\[[^\]]*\])*\]\(\s*<?([^)\s>]+)>?(?:\s+\"[^\"]*\")?\s*\)")
REF_DEF = re.compile(r"^\s{0,3}\[[^\]]+\]:\s*<?(\S+?)>?(?:\s|$)")
HEADING = re.compile(r"^\s{0,3}(#{1,6})\s+(.*?)\s*#*\s*$")
HTML_ANCHOR = re.compile(r"<a\s+(?:name|id)=\"([^\"]+)\"", re.IGNORECASE)
SCHEME = re.compile(r"^[A-Za-z][A-Za-z0-9+.-]*:")


def git_files() -> list[str]:
    out = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT,
                         capture_output=True, check=True).stdout
    return [p for p in out.decode().split("\0") if p]


def prose_lines(text: str):
    """Yield (line number, line with code spans blanked, raw line) for each
    line outside fenced code. Blanking the spans keeps a backticked example
    from being read as a link; the raw line is where the paths are."""
    in_fence = None
    for n, line in enumerate(text.splitlines(), 1):
        m = FENCE.match(line)
        if m:
            if in_fence is None:
                in_fence = m.group(1)
            elif m.group(1) == in_fence:
                in_fence = None
            continue
        if in_fence is None:
            yield n, INLINE_CODE.sub("", line), line


def slug(heading: str) -> str:
    """GitHub's anchor for a heading: the rendered text, lowercased, with
    punctuation dropped and each space a hyphen. The markup is stripped first,
    because GitHub slugs the rendered text, not the source."""
    text = re.sub(r"`([^`]*)`", r"\1", heading)
    text = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"<[^>]+>", "", text)
    text = text.replace("*", "")
    # An underscore is a word character and survives -- unless it is emphasis.
    text = re.sub(r"(?<!\w)_+|_+(?!\w)", "", text)
    text = re.sub(r"[^\w\- ]", "", text.lower())
    return text.replace(" ", "-")


_anchor_cache: dict[Path, set[str]] = {}


def anchors(path: Path) -> set[str]:
    if path not in _anchor_cache:
        found: set[str] = set()
        seen: dict[str, int] = {}
        for _, line, _ in prose_lines(path.read_text(encoding="utf-8")):
            found.update(HTML_ANCHOR.findall(line))
            m = HEADING.match(line)
            if not m:
                continue
            base = slug(m.group(2))
            # A repeated heading gets -1, -2, ... in document order.
            count = seen.get(base, 0)
            seen[base] = count + 1
            found.add(base if count == 0 else f"{base}-{count}")
        _anchor_cache[path] = found
    return _anchor_cache[path]


def check_link(md: Path, target: str) -> str | None:
    if SCHEME.match(target):
        return None  # http:, https:, mailto: -- not fetched, see above
    path_part, _, fragment = target.partition("#")
    if path_part.startswith(("~", "/")):
        return "names a path outside the checkout"
    dest = (md.parent / path_part).resolve() if path_part else md
    try:
        dest.relative_to(ROOT)
    except ValueError:
        return "leaves the checkout"
    if not dest.exists():
        return "no such file"
    if fragment and dest.suffix == ".md" \
            and fragment.lower() not in anchors(dest):
        return "no heading with that anchor"
    return None


class Tree:
    """What the commit tracks: its files, their directories, and the
    top-level names a backticked span must start with to count as a path."""

    def __init__(self, files: list[str]):
        self.files = set(files)
        self.dirs = {"/".join(p.split("/")[:i])
                     for p in files for i in range(1, p.count("/") + 1)}
        self.tops = {p.split("/")[0] for p in files}

    def repo_path(self, span: str) -> str | None:
        """The repository path a code span names, or None when it is not one:
        it needs a slash, must start at a tracked top-level name, and holds no
        glob or placeholder. `file:58` and `file::symbol` name the file."""
        path = span.split(":", 1)[0].rstrip("/.,;")
        if "/" not in path or path.split("/")[0] not in self.tops:
            return None
        if any(c in path for c in "*?[]{}<>$"):
            return None
        return path

    def has(self, path: str) -> bool:
        return path in self.files or path in self.dirs


def check(md: Path, rel: str, tree: Tree) -> tuple[list[str], int]:
    problems, count = [], 0
    is_record = any(fnmatch.fnmatch(rel, pat) for pat in RECORDS)
    prev_n, in_paragraph = 0, False
    for n, line, raw in prose_lines(md.read_text(encoding="utf-8")):
        targets = INLINE_LINK.findall(line)
        # A reference definition cannot interrupt a paragraph, so a wrapped
        # line that happens to open with `[Unreleased]:` is prose, not a link.
        # A skipped code block between two lines ends the paragraph too.
        if n != prev_n + 1:
            in_paragraph = False
        m = None if in_paragraph else REF_DEF.match(line)
        if m:
            targets.append(m.group(1))
        in_paragraph = bool(line.strip()) and not m
        prev_n = n

        for target in targets:
            count += 1
            why = check_link(md, target)
            if why:
                problems.append(f"{rel}:{n}: link {target} -- {why}")
        if is_record:
            continue
        for span in CODE_SPAN.findall(raw):
            path = tree.repo_path(span)
            if path is None:
                continue
            count += 1
            if not tree.has(path) and (rel, path) not in ABSENT_ON_PURPOSE:
                problems.append(f"{rel}:{n}: path `{span}` -- not tracked")
    return problems, count


def main() -> int:
    files = git_files()
    tree = Tree(files)
    docs = [p for p in files if p.endswith(".md")]
    problems, checked = [], 0
    for rel in docs:
        found, count = check(ROOT / rel, rel, tree)
        problems += found
        checked += count
    for p in problems:
        print(p)
    # A run that found nothing to check must not read as a pass.
    if not checked:
        print("no links or paths were found -- nothing was checked",
              file=sys.stderr)
        return 1
    print(f"{len(docs)} markdown files, {checked} references checked, "
          f"{len(problems)} broken")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
