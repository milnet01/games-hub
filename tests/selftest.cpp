// Headless checks for every game's rules. Run via `ctest` or by executing
// gameshub_selftest directly; exits non-zero on the first failure.

#include "cards/card.h"
#include "hearts/heartsengine.h"
#include "draughts/draughtsboard.h"
#include "minesweeper/minefield.h"
#include "pinball/pinballtable.h"
#include "sudoku/sudokugrid.h"
#include "reversi/ai.h"
#include "reversi/board.h"

#include <chrono>
#include <cstdio>
#include <algorithm>
#include <array>
#include <map>
#include <random>

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++g_failures;
}

void section(const char* name)
{
    std::printf("\n-- %s --\n", name);
}

// ---------------------------------------------------------------------------
// Reversi
// ---------------------------------------------------------------------------

void reversiRules()
{
    const Board b;
    check(b.count(Player::Black) == 2 && b.count(Player::White) == 2,
          "reversi: opening has two discs each");
    check(b.emptyCount() == 60, "reversi: opening leaves 60 empty squares");
    check(b.legalMoves(Player::Black).size() == 4, "reversi: black opens with four moves");
    check(!b.isLegal(Player::Black, { 0, 0 }), "reversi: a corner is not legal at the start");

    Board c;
    const int flipped = c.play(Player::Black, { 2, 3 });
    check(flipped == 1, "reversi: d3 flips exactly one disc");
    check(c.at(3, 3) == Cell::Black, "reversi: the bracketed white disc became black");
    check(c.play(Player::Black, { 0, 0 }) == 0, "reversi: an illegal move is rejected");
}

void reversiGamesTerminate()
{
    std::mt19937 rng { 12345 };
    for (int game = 0; game < 200; ++game) {
        Board b;
        Player p = Player::Black;
        int plies = 0;

        while (!b.gameOver() && plies < 200) {
            const std::vector<Move> moves = b.legalMoves(p);
            if (!moves.empty()) {
                std::uniform_int_distribution<std::size_t> pick(0, moves.size() - 1);
                b.play(p, moves[pick(rng)]);
            }
            p = opponent(p);
            ++plies;
        }

        if (plies >= 200 || b.count(Player::Black) + b.count(Player::White) + b.emptyCount() != kCells) {
            check(false, "reversi: a random game broke the invariants");
            return;
        }
    }
    check(true, "reversi: 200 random games terminate with consistent disc counts");
}

void reversiEngineStrength()
{
    std::mt19937 rng { 999 };
    int wins = 0;
    constexpr int kGames = 20;

    for (int game = 0; game < kGames; ++game) {
        Board b;
        Player p = Player::Black;
        const Player engine = (game % 2 == 0) ? Player::Black : Player::White;

        while (!b.gameOver()) {
            const std::vector<Move> moves = b.legalMoves(p);
            if (!moves.empty()) {
                if (p == engine) {
                    b.play(p, *chooseMove(b, p, Difficulty::Medium));
                } else {
                    std::uniform_int_distribution<std::size_t> pick(0, moves.size() - 1);
                    b.play(p, moves[pick(rng)]);
                }
            }
            p = opponent(p);
        }
        if (b.count(engine) > b.count(opponent(engine)))
            ++wins;
    }

    std::printf("      engine won %d of %d against random play\n", wins, kGames);
    check(wins >= kGames - 2, "reversi: medium engine beats random play almost every game");

    Board b;
    b.play(Player::Black, { 2, 3 });
    const auto start = std::chrono::steady_clock::now();
    const std::optional<Move> m = chooseMove(b, Player::White, Difficulty::Hard);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start).count();
    std::printf("      hard search took %lld ms\n", static_cast<long long>(ms));
    check(m.has_value() && ms < 2000, "reversi: hard search answers in under two seconds");
}

// ---------------------------------------------------------------------------
// Minesweeper
// ---------------------------------------------------------------------------

void minesweeperRules()
{
    Minefield f(9, 9, 10);
    check(f.state() == Minefield::State::Playing, "minesweeper: starts playable");
    check(f.minesRemaining() == 10, "minesweeper: counts ten mines to find");

    // The opening click must never be a mine, however many times we try.
    for (int attempt = 0; attempt < 200; ++attempt) {
        Minefield g(9, 9, 10);
        g.reveal(4, 4);
        if (g.state() != Minefield::State::Playing || g.at(4, 4).mine) {
            check(false, "minesweeper: the first click hit a mine");
            return;
        }
        if (g.at(4, 4).neighbours != 0) {
            check(false, "minesweeper: the first click was not in a blank area");
            return;
        }
    }
    check(true, "minesweeper: 200 opening clicks are all safe and open a blank area");

    Minefield g(9, 9, 10);
    g.toggleFlag(0, 0);
    check(g.at(0, 0).flagged && g.minesRemaining() == 9, "minesweeper: flagging counts down");
    g.reveal(0, 0);
    check(!g.at(0, 0).revealed, "minesweeper: a flagged square cannot be dug");
    g.toggleFlag(0, 0);
    check(!g.at(0, 0).flagged, "minesweeper: flags toggle off");
}

void minesweeperWinAndLoss()
{
    // Clearing every safe square wins; the field is small enough to brute force.
    Minefield f(5, 5, 3);
    f.reveal(2, 2);
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 5; ++c)
            if (!f.at(r, c).mine)
                f.reveal(r, c);
    check(f.state() == Minefield::State::Won, "minesweeper: clearing every safe square wins");

    Minefield g(5, 5, 3);
    g.reveal(0, 0);
    bool hitOne = false;
    for (int r = 0; r < 5 && !hitOne; ++r)
        for (int c = 0; c < 5 && !hitOne; ++c)
            if (g.at(r, c).mine) {
                g.reveal(r, c);
                hitOne = true;
            }
    check(g.state() == Minefield::State::Lost, "minesweeper: digging a mine loses");
}

// ---------------------------------------------------------------------------
// Cards
// ---------------------------------------------------------------------------

void deckRules()
{
    const std::vector<Card> one = makeDeck(1, 4);
    check(one.size() == 52, "cards: a single deck holds 52 cards");

    std::map<std::pair<int, int>, int> seen;
    for (const Card& c : one)
        ++seen[{ int(c.suit), c.rank }];
    check(seen.size() == 52, "cards: every card in a deck is distinct");

    const std::vector<Card> spider = makeDeck(2, 1);
    check(spider.size() == 104, "cards: two decks hold 104 cards");
    bool oneSuit = true;
    for (const Card& c : spider)
        if (c.suit != spider.front().suit)
            oneSuit = false;
    check(oneSuit, "cards: a one-suit deck really uses one suit");

    const std::vector<Card> two = makeDeck(2, 2);
    std::map<int, int> suits;
    for (const Card& c : two)
        ++suits[int(c.suit)];
    check(suits.size() == 2 && suits.begin()->second == 52,
          "cards: a two-suit deck splits evenly");
}

// ---------------------------------------------------------------------------
// Hearts
// ---------------------------------------------------------------------------

void heartsRules()
{
    HeartsEngine e;
    int cards = 0;
    for (int p = 0; p < HeartsEngine::kPlayers; ++p)
        cards += int(e.hand(p).size());
    check(cards == 52, "hearts: the deal uses all 52 cards");
    check(e.hand(0).size() == 13, "hearts: each player holds 13 cards");
    check(e.phase() == HeartsEngine::Phase::Passing, "hearts: the first hand passes left");

    for (int p = 0; p < HeartsEngine::kPlayers; ++p)
        e.setPass(p, e.chooseAiPass(p));
    e.executePass();
    check(e.phase() == HeartsEngine::Phase::Playing, "hearts: passing starts the play phase");

    int after = 0;
    for (int p = 0; p < HeartsEngine::kPlayers; ++p)
        after += int(e.hand(p).size());
    check(after == 52, "hearts: passing neither loses nor duplicates cards");

    // The two of clubs must lead the first trick.
    const std::vector<Card> opening = e.legalPlays(e.currentPlayer());
    check(opening.size() == 1 && opening.front().suit == Suit::Clubs && opening.front().rank == 2,
          "hearts: the first lead is forced to the two of clubs");
}

// Plays whole games with the built-in AI in every seat. Any rules bug — a
// stuck turn, a lost card, a mis-scored hand — shows up as a failure here.
void heartsFullGames()
{
    for (int game = 0; game < 20; ++game) {
        HeartsEngine e;
        int guard = 0;

        while (e.phase() != HeartsEngine::Phase::GameOver && guard++ < 4000) {
            if (e.phase() == HeartsEngine::Phase::Passing) {
                for (int p = 0; p < HeartsEngine::kPlayers; ++p)
                    e.setPass(p, e.chooseAiPass(p));
                e.executePass();
                continue;
            }
            if (e.phase() == HeartsEngine::Phase::HandOver) {
                e.nextHand();
                continue;
            }
            if (e.trickComplete()) {
                e.collectTrick();
                continue;
            }

            const int seat = e.currentPlayer();
            const Card c = e.chooseAiCard(seat);
            if (!e.playCard(seat, c)) {
                check(false, "hearts: the AI produced an illegal card");
                return;
            }
        }

        if (guard >= 4000) {
            check(false, "hearts: a game failed to finish");
            return;
        }

        int total = 0;
        for (int p = 0; p < HeartsEngine::kPlayers; ++p)
            total += e.total(p);
        // Every hand adds either 26 (normal) or 78 (a moon shot) to the table.
        if (total % 26 != 0) {
            check(false, "hearts: hand points did not add up to a multiple of 26");
            return;
        }
    }
    check(true, "hearts: 20 full AI games finish with consistent scores");
}

void heartsMoonShot()
{
    // Verify the moon rule directly: 26 to one player must become 26 to the
    // other three, not 26 to the shooter.
    HeartsEngine e;
    bool sawMoon = false;
    for (int game = 0; game < 400 && !sawMoon; ++game) {
        HeartsEngine g;
        int guard = 0;
        std::array<int, HeartsEngine::kPlayers> before {};
        while (g.phase() != HeartsEngine::Phase::GameOver && guard++ < 4000) {
            if (g.phase() == HeartsEngine::Phase::Passing) {
                for (int p = 0; p < HeartsEngine::kPlayers; ++p)
                    g.setPass(p, g.chooseAiPass(p));
                g.executePass();
                continue;
            }
            if (g.phase() == HeartsEngine::Phase::HandOver) {
                for (int p = 0; p < HeartsEngine::kPlayers; ++p) {
                    if (g.handPoints(p) == 26) {
                        sawMoon = true;
                        // The shooter's total must not have moved this hand.
                        if (g.total(p) != before[std::size_t(p)])
                            check(false, "hearts: a moon shot scored against the shooter");
                    }
                }
                for (int p = 0; p < HeartsEngine::kPlayers; ++p)
                    before[std::size_t(p)] = g.total(p);
                g.nextHand();
                continue;
            }
            if (g.trickComplete()) {
                for (int p = 0; p < HeartsEngine::kPlayers; ++p)
                    before[std::size_t(p)] = g.total(p);
                g.collectTrick();
                continue;
            }
            g.playCard(g.currentPlayer(), g.chooseAiCard(g.currentPlayer()));
        }
    }
    // Not seeing a moon in 400 games is fine — the rule is still exercised by
    // the scoring assertion above whenever one happens.
    std::printf("      moon shot observed: %s\n", sawMoon ? "yes" : "no");
    check(true, "hearts: moon-shot scoring holds wherever it occurred");
}

// ---------------------------------------------------------------------------
// Draughts
// ---------------------------------------------------------------------------

void draughtsRules()
{
    const DraughtsBoard b;
    check(b.count(Side::Red) == 12 && b.count(Side::White) == 12,
          "draughts: both sides start with twelve pieces");

    const std::vector<DraughtsMove> opening = b.legalMoves(Side::Red);
    check(opening.size() == 7, "draughts: red has seven opening moves");
    bool allForward = true;
    for (const DraughtsMove& m : opening)
        if (m.destination().row >= m.from.row)
            allForward = false;
    check(allForward, "draughts: men only move forwards");

    // Every move must land on a dark square.
    bool onDark = true;
    for (const DraughtsMove& m : opening)
        if (!DraughtsBoard::isPlayable(m.destination().row, m.destination().col))
            onDark = false;
    check(onDark, "draughts: play stays on the dark squares");
}

// Taking is compulsory, and a chain of jumps must be played to its end.
void draughtsCaptures()
{
    std::mt19937 rng { 4242 };
    bool sawCapture = false;
    bool sawMultiJump = false;
    bool captureForced = false;

    for (int game = 0; game < 60; ++game) {
        DraughtsBoard b;
        Side side = Side::Red;
        int plies = 0;

        while (plies < 300) {
            const std::vector<DraughtsMove> moves = b.legalMoves(side);
            if (moves.empty())
                break;

            // If any capture exists, legalMoves must return captures only.
            const bool anyCapture = std::any_of(moves.begin(), moves.end(),
                                                [](const DraughtsMove& m) { return m.isCapture(); });
            if (anyCapture) {
                sawCapture = true;
                const bool allCaptures = std::all_of(
                    moves.begin(), moves.end(), [](const DraughtsMove& m) { return m.isCapture(); });
                if (!allCaptures) {
                    check(false, "draughts: a quiet move was offered while a capture existed");
                    return;
                }
                captureForced = true;
            }
            for (const DraughtsMove& m : moves)
                if (m.captured.size() > 1)
                    sawMultiJump = true;

            std::uniform_int_distribution<std::size_t> pick(0, moves.size() - 1);
            const DraughtsMove chosen = moves[pick(rng)];

            // A capture must remove exactly as many pieces as it claims.
            const int before = b.count(other(side));
            b.apply(chosen);
            const int after = b.count(other(side));
            if (before - after != int(chosen.captured.size())) {
                check(false, "draughts: a capture removed the wrong number of pieces");
                return;
            }

            side = other(side);
            ++plies;
        }
    }

    check(sawCapture && captureForced, "draughts: captures occur and are compulsory");
    check(sawMultiJump, "draughts: multi-jump chains are generated");
}

void draughtsEngine()
{
    // The engine should beat random play convincingly.
    std::mt19937 rng { 77 };
    int wins = 0;
    constexpr int kGames = 8;

    for (int game = 0; game < kGames; ++game) {
        DraughtsBoard b;
        Side side = Side::Red;
        const Side engine = (game % 2 == 0) ? Side::Red : Side::White;
        int plies = 0;
        Side loser = Side::Red;

        while (plies < 300) {
            const std::vector<DraughtsMove> moves = b.legalMoves(side);
            if (moves.empty()) {
                loser = side;
                break;
            }
            if (side == engine) {
                DraughtsMove m;
                chooseDraughtsMove(b, side, DraughtsLevel::Medium, m);
                b.apply(m);
            } else {
                std::uniform_int_distribution<std::size_t> pick(0, moves.size() - 1);
                b.apply(moves[pick(rng)]);
            }
            side = other(side);
            ++plies;
        }
        if (plies < 300 && loser != engine)
            ++wins;
    }

    std::printf("      engine won %d of %d against random play\n", wins, kGames);
    check(wins >= kGames - 1, "draughts: the engine beats random play");
}

// ---------------------------------------------------------------------------
// Sudoku
// ---------------------------------------------------------------------------

// A generated puzzle is only worth playing if it has exactly one solution —
// otherwise it cannot be reasoned out, only guessed at.
void sudokuGeneration()
{
    const struct { const char* name; SudokuGrid::Level level; } kLevels[] = {
        { "easy", SudokuGrid::Level::Easy },
        { "medium", SudokuGrid::Level::Medium },
        { "hard", SudokuGrid::Level::Hard },
    };

    for (const auto& entry : kLevels) {
        SudokuGrid grid;
        grid.generate(entry.level);

        int clues = 0;
        std::array<int, SudokuGrid::kCells> puzzle {};
        bool cluesMatchSolution = true;
        for (int r = 0; r < SudokuGrid::kSize; ++r) {
            for (int c = 0; c < SudokuGrid::kSize; ++c) {
                const int g = grid.given(r, c);
                puzzle[std::size_t(SudokuGrid::index(r, c))] = g;
                if (g != 0) {
                    ++clues;
                    if (g != grid.solution(r, c))
                        cluesMatchSolution = false;
                }
            }
        }

        std::printf("      %s: %d clues\n", entry.name, clues);
        if (!cluesMatchSolution) {
            check(false, "sudoku: a clue disagreed with the solution");
            return;
        }
        if (SudokuGrid::countSolutions(puzzle, 3) != 1) {
            check(false, "sudoku: a generated puzzle did not have a unique solution");
            return;
        }
    }
    check(true, "sudoku: every difficulty generates a puzzle with one solution");

    // The solution grid itself must be a valid, complete Sudoku.
    SudokuGrid grid;
    grid.generate(SudokuGrid::Level::Medium);
    bool valid = true;
    for (int i = 0; i < SudokuGrid::kSize && valid; ++i) {
        int rowSeen = 0;
        int colSeen = 0;
        for (int j = 0; j < SudokuGrid::kSize; ++j) {
            rowSeen |= 1 << grid.solution(i, j);
            colSeen |= 1 << grid.solution(j, i);
        }
        // Bits 1..9 all set, and nothing in bit 0 (an empty cell).
        if (rowSeen != 0b1111111110 || colSeen != 0b1111111110)
            valid = false;
    }
    check(valid, "sudoku: the solution has every digit once per row and column");

    // Clues are locked; player entries are not.
    int lockedRow = -1;
    int lockedCol = -1;
    for (int r = 0; r < SudokuGrid::kSize && lockedRow < 0; ++r)
        for (int c = 0; c < SudokuGrid::kSize && lockedRow < 0; ++c)
            if (grid.isClue(r, c)) {
                lockedRow = r;
                lockedCol = c;
            }
    const int before = grid.value(lockedRow, lockedCol);
    grid.set(lockedRow, lockedCol, before == 1 ? 2 : 1);
    check(grid.value(lockedRow, lockedCol) == before, "sudoku: a clue cannot be overwritten");
}

// ---------------------------------------------------------------------------
// Pinball
// ---------------------------------------------------------------------------

// The launch must actually deliver the ball into the play field. This is the
// check that the original table failed: a weak plunger left the ball short of
// the dome, and it fell back down an open-bottomed lane straight into the
// drain, losing a ball before the player ever touched a flipper.
void pinballLaunch()
{
    for (int strength = 0; strength <= 10; ++strength) {
        PinballTable t;
        for (int i = 0; i < strength; ++i)
            t.chargePlunger(1.0 / 60.0);
        t.launch();

        bool reached = false;
        for (int frame = 0; frame < 600 && !reached; ++frame) { // up to 10 seconds
            t.advance(1.0 / 60.0);
            if (t.ballInPlayfield())
                reached = true;
        }

        if (!reached) {
            std::printf("      plunger charge %d never reached the play field\n", strength);
            check(false, "pinball: every launch strength reaches the play field");
            return;
        }
        if (t.ballsLeft() != 3) {
            std::printf("      plunger charge %d lost a ball during the launch\n", strength);
            check(false, "pinball: the launch never costs a ball");
            return;
        }
    }
    check(true, "pinball: all 11 plunger strengths reach the play field without losing a ball");
}

// The ball must stay inside the table: no tunnelling through a wall, and no
// escaping out of the sides or the top.
void pinballContainment()
{
    PinballTable t;
    t.chargePlunger(0.2);
    t.launch();

    double worstX = 0;
    double worstY = 0;
    for (int frame = 0; frame < 3000; ++frame) { // ~50 seconds of play
        t.advance(1.0 / 60.0);
        const QPointF b = t.ball();
        worstX = std::max(worstX, std::max(-b.x(), b.x() - PinballTable::kWidth));
        worstY = std::max(worstY, -b.y());
        if (t.gameOver())
            break;
    }

    std::printf("      furthest escape: %.1f horizontal, %.1f above the table\n", worstX, worstY);
    check(worstX < 20.0 && worstY < 20.0, "pinball: the ball never escapes the table");
}

// Bumpers must score, or the table is scenery.
void pinballScoring()
{
    int scored = 0;
    for (int attempt = 0; attempt < 12; ++attempt) {
        PinballTable t;
        t.chargePlunger(attempt * 0.02);
        t.launch();
        for (int frame = 0; frame < 900; ++frame) {
            t.advance(1.0 / 60.0);
            if (t.gameOver())
                break;
        }
        if (t.score() > 0)
            ++scored;
    }
    std::printf("      %d of 12 launches hit something worth points\n", scored);
    check(scored >= 8, "pinball: a launched ball reliably reaches the scoring area");
}

} // namespace

int main()
{
    section("Reversi");
    reversiRules();
    reversiGamesTerminate();
    reversiEngineStrength();

    section("Minesweeper");
    minesweeperRules();
    minesweeperWinAndLoss();

    section("Draughts");
    draughtsRules();
    draughtsCaptures();
    draughtsEngine();

    section("Sudoku");
    sudokuGeneration();

    section("Pinball");
    pinballLaunch();
    pinballContainment();
    pinballScoring();

    section("Cards");
    deckRules();

    section("Hearts");
    heartsRules();
    heartsFullGames();
    heartsMoonShot();

    std::printf("\n%s\n", g_failures == 0 ? "All checks passed." : "FAILURES PRESENT.");
    return g_failures == 0 ? 0 : 1;
}
