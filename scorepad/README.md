# Canasta score book

A phone replacement for the paper score book kept at the table. Four people
play with real cards; this keeps the book.

It is **not** part of the desktop game and does not play Canasta. Open
`index.html` in a phone browser and add it to the home screen.

## What it does

Per hand it takes each side's score — worked out at the table, as with the
paper book — plus who went out and the going-out bonus, and the house
**exact-cut bonus** of 50 to the side that cut when the deal used the cut up
with no cards left over. It keeps the running totals, shows whose deal it is
and who deals next, and shows each side's opening minimum against its current
score.

Four initials are entered in **clockwise seating order**. Partners sit
opposite, so the sides are seats 1 + 3 and 2 + 4, and the setup screen shows
the pairing it worked out rather than assuming it was understood. Any hand can
be tapped and corrected or deleted.

**It deliberately does not reimplement the scoring.** That keeps the one thing
that would really drift — the whole scoring table — out of a second copy.

## Past games

Finishing a game files it rather than wiping it. **Past games** then shows a
record kept per PERSON rather than per seat — games played, games won, hands
they went out on, exact cuts their side was on — because who partners whom
changes week to week and it is the person's record that is interesting. Plus
records for the best hand, best game, biggest winning margin and longest game,
and the list of filed games with the winner marked.

Everything there is derived from the hands themselves rather than tallied as
the game goes, so correcting a hand corrects the record with it. A game filed
by mistake can be tapped to put it back in play.

## Sharing one book across four phones

One person keeps score; the others follow along and update on their own. The
scorer taps **Share with the table** and reads out the four-letter code; the
others type it into **Join a game** — or, on a phone that has never been set
up, into **Or join the table** on the opening screen. A watcher never enters
players or settings; the book arrives whole.

Joining shows a loading screen until the book lands. That wait is real: the
connection takes a few seconds to come up, and a phone shown an empty board
reading 0/0 in the meantime would look broken. A watcher that already holds
last week's book is shown that instead and it refreshes in place, because a
stale board beats a spinner.

Sharing is optional at every level. With no `databaseURL` configured, no
signal, or the SDK unreachable, the book still works on one phone and says so.

### Setting it up

1. `console.firebase.google.com` → **Add project**. Analytics can be off.
2. **Build → Realtime Database → Create Database**, nearest location,
   **start in locked mode**.
3. **Project settings → Your apps → web `</>`** → register → copy the config
   block into `firebase-config.js`.
4. Paste the rules below into **Realtime Database → Rules**.

### The rules, and what they are worth

```json
{
  "rules": {
    "rooms": {
      "$code": {
        ".read": "$code.length == 4",
        ".write": "$code.length == 4 && newData.hasChildren(['book'])"
      }
    }
  }
}
```

**The Firebase `apiKey` is not a secret and is safe in a public repository.**
It identifies the project; it authorises nothing. These rules are what protect
the data, which is why they live here rather than in a screenshot.

What they buy, stated honestly: nobody can read the whole database or write
anywhere outside a room, and a room must carry a real book. What they do
**not** buy is privacy between rooms — anyone who knows or guesses a
four-letter code can read and write that room. For a family's Canasta scores
that is the proportionate trade; do not reuse this shape for anything that
matters.

## Checks

`scripts/scorepad-check.py`, wired into `ctest`, so it runs locally, on the
pre-push hook and on both CI legs.

The opening bands, the target and the going-out default exist here **and** in
`src/canasta/canastaengine.{h,cpp}`. The script holds no copy of its own: it
reads both and fails when they disagree, so a house rule changed in the game
cannot quietly leave the phone telling the table a stale figure. Same reasoning
as the donate URLs being generated from `.github/FUNDING.yml`.

`tools/make_scorepad_icon.py` draws the home-screen icon. Generated rather than
drawn by hand, like the sound effects: re-running it is byte-identical and no
font is used, so it renders the same anywhere.
