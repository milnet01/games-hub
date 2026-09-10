# GHUB-0191: FUNDING.yml `custom:` parsing

Contract for `gameshub_funding_entry()` in `cmake/funding.cmake` -- the
function that turns one line of `.github/FUNDING.yml` into a donate-menu
entry, or an empty string for a line the build has no rule for, which
`CMakeLists.txt` turns into a `FATAL_ERROR`. This document locks the
`custom:` branch only -- the `github:` and `patreon:` branches take a bare
handle rather than a URL and are unaffected by the defect this locks.

## Invariants

- **INV-1**: A `custom:` line naming two entries, written as an unquoted
  YAML flow list (`custom: [https://a, https://b]`), returns an empty
  string -- the same "no rule for this line" signal `CMakeLists.txt` turns
  into a `FATAL_ERROR` -- rather than merging the two URLs into one donate
  entry. Test: `tests/funding-test.cmake`.
- **INV-2**: A `custom:` line naming two entries, written as a quoted YAML
  flow list (`custom: ["https://a", "https://b"]`), also returns an empty
  string. Test: `tests/funding-test.cmake`.
- **INV-3**: A `custom:` line naming exactly one entry returns a clean
  donate entry for that URL -- no leftover `[`, `]` or `"` character -- in
  every form GitHub's own `custom:` key accepts: a bare URL, a quoted URL,
  an unquoted URL inside a one-element flow list, and a quoted URL inside a
  one-element flow list. Test: `tests/funding-test.cmake`.

## Rationale

`.github/FUNDING.yml`'s `custom:` key is a YAML flow sequence, and GitHub
reads it that way -- `custom: [a, b]` is two entries to GitHub. The regex in
`gameshub_funding_entry()` does not parse YAML; it pattern-matches one line
against a shape it expects to hold a single URL, with the brackets and
quotes marked optional so every accepted single-entry spelling matches the
same rule. That optionality is what breaks on a two-entry line: fed
`custom: [https://a, https://b]`, the capture group's `[^"]+` matches
greedily across the comma and the second URL, and the match still succeeds
because the trailing `\]?` is optional -- so the two entries silently become
one garbled donate link instead of the build refusing a line it cannot
represent faithfully. Fed a single bracketed *unquoted* URL
(`custom: [https://a]`), the same greediness pulls the closing `]` into the
capture, because nothing after the capture forces it back out when there is
no quote character to stop at.

Both are the class of bug this codebase treats as worse than a crash: a
donate link the app happily shows that goes nowhere useful, or a build that
reports success while quietly dropping one of two links the maintainer
configured.

## Scope

In scope: the `custom:` branch of `gameshub_funding_entry()`, for the
two-entry and single-entry forms enumerated above. Out of scope: the
`github:` and `patreon:` branches; any `custom:` spelling not enumerated
above (e.g. extra internal whitespace, three or more entries -- the two
locked forms are believed to generalise but are not separately tested); and
the parsing strategy itself (replacing the regex with real YAML parsing
would satisfy this contract too, and is not ruled out by it).

## Regression history

Filed as GHUB-0191, and fixed in the commit that added this test. Before the
fix, `cmake/funding.cmake`:

- failed INV-1 -- the unquoted two-entry list returned a merged single entry
  (`https://a, https://b]`) instead of an empty string;
- failed INV-3's unquoted-bracketed form the same way -- the entry came back
  as `https://a]`, carrying the closing bracket into the URL;
- passed INV-2 and INV-3's other three forms by accident rather than by
  design (a quote character stopped the greedy match where a comma or a bare
  `]` did not). They are locked here so the fix could not regress them.
