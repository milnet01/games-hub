# Gate log

Genre: record

One row per run that mints a run id — not reviews alone, and a run that
dispatched nobody still owes one. A commit citing a gate cites a run id
here, and the gate-record hook checks the id is in this file, so the
file exists from the first commit.

The columns are fixed by `standards/documents.md`. Per-run detail goes
in `docs/reviews/<id>.md`, never in a new column here. `Lanes` is how
many read the subject, and zero is a value — it is what tells a gate
from a self-read.

| Run | Date | Subject | Lanes | Outcome |
|---|---|---|---|---|
