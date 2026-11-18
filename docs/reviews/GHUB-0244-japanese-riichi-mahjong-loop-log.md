# GHUB-0244 — Japanese Riichi Mahjong: review loop log

Genre: record

The review history of `docs/specs/GHUB-0244-japanese-riichi-mahjong.md`,
kept here per `spec-format.md` § 6 so the spec itself carries only a pointer.
`review-contract` writes one row per loop, as the loop closes.

## Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| 1 | 2026-11-18 | 2 (neutral-lane; each lane held every question) | 2 | 3 | 7 | 1 | **Gate:** rule 14's contract trigger. `Action::Riichi` is a new value written into save files by `MahjongTableView::encodeSave`, permanent once Riichi saves exist; `RuleSetId` 3 was already reserved and `kBlobVersion` stays 1. Baseline: commit ceafcc0, the whole spec. 13 verified, 13 fixed, 0 dismissed: 10 lane findings (5 raised by both lanes), 2 from lane open questions, 1 found while building the packet. Q2: a yakuman reading had `qualifying` 0, so it could never win; INV-12's 120,000 against the game-end stick split; `uraIndicators` against double riichi. Q3: `waitingTiles` took a meld count, not the melds; `mostKongs` per deal; dead-wall places counted from the wall as built; `RiichiRules::openValue` and `basicPoints` named; Riichi's shapes, and Chinese Official refusing the new one; `HandResult::payments` over every winner; which furiten holds. Q1: a robbed added kong does not end ippatsu (book § 4.2.1); a win on a discard leaves the hand concealed. Q4: INV-10's declarer total. Packet about 47k tokens, over the 30k target, with the book's chapters 1 to 4 in it. Loop 2 dispatched. |
| 2 | 2026-11-18 | 2 (neutral-lane; each lane held every question) | 1 | 1 | 2 | 1 | 5 verified, 5 fixed, 0 dismissed; 4 open questions, none became a finding. Q1: `FullyConcealed`'s sentence says "laid down", which a concealed kong is; it becomes "no set claimed" for every rule set. Q2: INV-10's "sticks as they were" against "a winner takes the sticks". Q3: the rules text's shared Hong Kong sentences follow the rule set; `basicPoints` gives 8,000 for `kYakuman`. Q4: INV-9's first clause was refused by the yaku rule, not furiten. **Capped at the spec's two loops; calm cap**: 2 of 5 landed on text loop 1 wrote (INV-10's clause, `basicPoints`). No second share: the armed change is the whole spec. These 5 fixes are read by no lane; the build is the next reader. The cap's final sweep found nothing more. Status set to accepted. |
