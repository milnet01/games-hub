# Gate log

Genre: record

One row per run that mints a run id — not reviews alone, and a run that
dispatched nobody still owes one. A commit citing a gate cites a run id
here, so the file exists from the first commit.

**Nothing checks that id, and nothing ever has.** This file first said a
gate-record hook checked it. That was false from the start: the hook was
never installed here (stage 4 of A-20260924-8459, below, was refused),
`.githooks/` has never held a `commit-msg`, and `git log -S gate-record`
finds only the commit that wrote the claim (4246132). The hook itself
only ever existed in `~/.claude`'s v2 draft; it is readable there as
`git show 1b5847a:draft/v2/hooks/gate-record`. See CFG-0596.
Corrected 2026-09-25, reported by claude-72.

The columns are fixed by `standards/documents.md`. Per-run detail goes
in `docs/reviews/<id>.md`, never in a new column here. `Lanes` is how
many read the subject, and zero is a value — it is what tells a gate
from a self-read.

| Run | Date | Subject | Lanes | Outcome |
|---|---|---|---|---|
| A-20260924-8459 | 2026-09-24 | v2 adoption of Games_Hub | 0 | stopped at stage 4: hook install refused by permissions; stages 1-3 in 4246132 |
