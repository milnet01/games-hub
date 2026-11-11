# GHUB-0239 — four-player Mahjong: review loop log

Genre: record

The review history of `docs/specs/GHUB-0239-four-player-mahjong.md`, kept here
per `spec-format.md` § 6 so the spec itself carries only a pointer.
`review-contract` writes one row per loop, as the loop closes.

## Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| — | 2026-11-11 | none — not a review | 0 | 0 | 0 | 0 | **No gate: rule 14's trigger judged not met.** The save binds only this game and a wrong shape refuses to load; the rule-set interface binds only in-repo code; doubt goes to no review. Canasta and Mahjong Solitaire shipped saves without a spec. The owner may ask for a review. |
| impl | 2026-11-11 | none — not a review; implementation | 0 | 0 | 0 | 0 | **Fold-back from the build, no reviewer dispatched.** Four details proved false, no design premise moved: `paintTileFace` draws the markings only and takes the caller's font (§ 4.2); `Draw` is not an action, draws are the wall's (§ 4.3); `RuleSet` gains `fanList()` so the rules text has one home for the values (§ 4.6); the keys are the number keys and Return or Space (§ 4.7). § 4.1 names the files. INV-12 measured: five seeded `--shot` pairs of Mahjong Solitaire byte-identical before and after the art moved, and a different seed differs. |
