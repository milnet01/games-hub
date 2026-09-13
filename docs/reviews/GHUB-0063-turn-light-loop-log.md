# GHUB-0063 — turn light: review loop log

The review history of `docs/specs/GHUB-0063-turn-light.md`, kept here per
`spec-format.md` § 6 so the spec itself carries only a pointer.
`review-contract` writes one row per loop, as the loop closes.

## Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| 1 | 2026-09-13 | 3, cold — genre pinned `spec` | 4 | 4 | 2 | 3 | **Thirteen findings after merging: twelve verified and fixed, one dismissed.** All three lanes found five: Hearts' trick pause was invisible to `refresh()`, which runs before `m_awaitingCollect` is set; the nobody's-turn list left out Hearts' Passing and HandOver; `--shot` never consults `hasPendingAnimation()`; INV-5's per-view crop could not fail, because the board moves with the switch; the board-game band sat on the frame and Chess's file letters. Two lanes: Canasta's `advanceForShot` level was reset by its final `refresh()`; the caption-overlap check photographed an unlit band. One lane each: Hearts' light areas sat under opaque cards; INV-3 passed before the feature existed; Canasta's square hung off the table. Orchestrator-found from lane open questions: § 4.4 contradicted `GameView::hasPendingAnimation`'s own contract (a resumable animation answers false); § 4.3's step divided Canasta's seconds by milliseconds. Dismissed (Q1): § 15's fade-length sentence is false against Chess's 260 ms think delay, but no line of the build changes. Two open questions resolved clean: `aiHalfTurn` does call `refresh()`; the `kFrameWidth` values. The post-fix sweep found no collateral. Loop 2 dispatched. |
