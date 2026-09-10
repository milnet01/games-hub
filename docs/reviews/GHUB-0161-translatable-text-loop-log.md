# GHUB-0161 — translatable text: review loop log

The review history of `docs/specs/GHUB-0161-translatable-text.md`, kept here
per `spec-format.md` § 6 so the spec itself carries only a pointer.
`review-contract` writes one row per loop, as the loop closes.

## Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| 1 | 2026-09-10 | 3, cold — genre pinned `spec`; all three lanes arrived holding the pre-split `CLAUDE.md` from session auto-load | 3 | 6 | 1 | 3 | **Thirteen findings after merging: twelve verified and fixed, one dismissed.** Fixed: § 4.4's Reversi notice could not be one sentence without changing the English or keeping a fragment (all three lanes) — now two whole sentences; § 4.5's check flagged `translate()`'s context argument (lane 1), flagged key and identity literals § 4.2 never wraps (lane 3), and let one marker hide a bare label on the same line (lanes 3 and 1) — rewritten; INV-3's `tr()` case, Snake, Pinball and the `QSettings` names were reachable by no test (lanes 1 and 3) — a marking-translator `uitest` case added; INV-4 was checked only on this item's diff (lane 1) — the check now refuses a `+` join; counted `const char*` text needs `QT_TRANSLATE_N_NOOP` (lane 2, confirmed in `qttranslation.h`); a menu mnemonic now travels with its label (lane 2's open question). Orchestrator-found: `legibility-check.py` keeps `PAIRS` as a constant, not in its header; § 8's `lrelease` reason no longer held; and two clashes this loop's own § 4.5 rewrite created (terminal-output calls unnamed; INV-1 said `exempt`). Dismissed: `uitest` does match `Undo` and `Discard`. Loop 2 dispatched. |
