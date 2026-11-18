# GHUB-0243 — Chinese Official Mahjong: review loop log

Genre: record

The review history of `docs/specs/GHUB-0243-chinese-official-mahjong.md`,
kept here per `spec-format.md` § 6 so the spec itself carries only a pointer.
`review-contract` writes one row per loop, as the loop closes.

## Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| — | 2026-11-18 | none — not a review | 0 | 0 | 0 | 0 | **No gate: rule 14's trigger judged not met.** The spec is owed for a design choice and edge cases, not for a contract or a hard-to-reverse change: the only stored value, `RuleSetId` 2, was reserved by GHUB-0239, and the save format does not change. Doubt goes to no review. The owner may ask for one. |
| impl | 2026-11-18 | none — not a review; implementation | 0 | 0 | 0 | 0 | **Fold-back from the build, no reviewer dispatched.** One design decision fell: § 4.7's list of aims for the computer was not built, because the Chicken hand makes nearly every complete hand worth 8 and the unchanged computer wins 20 of 24 seeded hands. Details proved false: a dragon pung or a seat or round wind strikes only its own pung's Pung of ones, nines or a wind, not every one in the hand (the book's Two melded kongs example), so four † entries left § 4.4; INV-10's concealed-kong case cannot occur, since a concealed kong holds all four tiles; INV-6's second hand is 9 points, not 8; INV-2 has no near miss for the seven patterns whose sets are laid down as the pattern; § 4.3 keeps a reading no part of which holds the winning tile. Measured: self-test 4.4 to 4.6 s against 4.2 to 4.5 s before; Hong Kong's computers still win 37 of 43 seeded hands, as before the change. Red runs: skipping § 4.5's strike step, the loop rule and the flower rule each turned their own checks red and no others. |
