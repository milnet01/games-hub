# GHUB-0043 — self-update: review loop log

Genre: record

The review history of `docs/specs/GHUB-0043-self-update.md`, kept here
per `spec-format.md` § 6 so the spec itself carries only a pointer.
`review-contract` writes one row per loop, as the loop closes.

## Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| 1 | 2026-11-04 | 2, cold — genre pinned `spec`; both lanes held every question; dispatched through `neutral-lane`, both arrived holding only the global `CLAUDE.md`, the agent file and the brief | 3 | 3 | 1 | 1 | **Eight verified, eight fixed, one dismissed.** One was mechanical: the project's `scripts/doc-refs-check.py` rejected three backticked paths to files not yet written, which would fail the docs push gate. **Both lanes found the same three**: § 4.5's "not during a test" left INV-1's test with no seam (now: `main()` starts the updater, its network is injected); § 4.1 put `updatelogic` where every `gameshub` links it, against § 1's "no update code at all" (now its own library, linked only with the switch, namespace `update`); and § 4.3's key layout, which `release.yml` parses, had no exact form (now two `std::array` lines, both compiled). Both also found § 11 rewriting one `SECURITY.md` bullet where three go false. One lane alone found: CI's Linux leg configured once, so INV-2's `nm` check had no default build; INV-5's manual test was scheduled on the first keyed release, which no earlier build can verify; and the asset download redirects to `release-assets.githubusercontent.com` (`curl -sI`). Dismissed: `packaging/THIRD-PARTY.md` missing from § 11, which § 13 already directs. Measured clean: Ubuntu 24.04's OpenSSL 3.0.13 signs and verifies with `pkeyutl -rawin`; `Qt6::Multimedia`'s link interface carries `Qt6::Network`. Unrunnable here: the AppImage runtime and linuxdeploy's environment hooks. Loop 2 dispatched. |
