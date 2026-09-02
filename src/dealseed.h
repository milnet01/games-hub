#pragma once

// The seed a new deal, board or computer seat starts from.
//
// Random per game normally, which is what anybody playing wants. `--seed` pins
// it so that two runs deal the same cards, which is what makes two pictures of
// one card game comparable: the deal is otherwise random per launch, and a
// before-and-after pixel count then measures the shuffle rather than the change
// (GHUB-0093 -- measured at 10633 changed pixels against a same-binary control
// of 6416, BOTH unseeded, so neither is a noise floor for a seeded run: two
// runs at one seed are byte-identical).
//
// Pinned, successive calls walk a fixed sequence rather than returning one
// number over and over. Four computer seats still differ from one another, and
// two runs still match, which they would not if every seat were seeded alike.
//
// Process-wide, deliberately: a command-line flag IS process-wide, and the
// alternative is threading a seed through eleven constructors that have no
// other reason to know about one.
unsigned dealSeed();

// Pins the sequence. Call once, before any game is built -- the seeds are
// member initialisers, so a game already constructed has taken its own.
void pinDealSeed(unsigned seed);
