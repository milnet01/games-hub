// Headless checks for every game's rules. Run via `ctest` or by executing
// gameshub_selftest directly; exits non-zero on the first failure.

#include "canasta/canastaai.h"
#include "canasta/canastaengine.h"
#include "cards/card.h"
#include "cards/cardcodec.h"
#include "chess/chessai.h"
#include "chess/chessboard.h"
#include "hearts/heartsengine.h"
#include "draughts/draughtsboard.h"
#include "minesweeper/minefield.h"
#include "pinball/pinballtable.h"
#include "sudoku/sudokugrid.h"
#include "reversi/ai.h"
#include "reversi/board.h"
#include "snake/snakeboard.h"
#include "freecell/freecelltable.h"
#include "pyramid/pyramidtable.h"
#include "twenty48/twenty48board.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <array>
#include <map>

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <deque>
#include <random>
#include <utility>
#include <vector>

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

    // The opening click can win the game outright, and often enough to matter:
    // 25 squares, 3 mines, and first-click safety clears a 2x2 corner, so the
    // flood fill reaches every safe square in 2.7% of deals — measured over
    // 200,000. reveal() returns early unless the state is Playing, so digging a
    // mine afterwards is a no-op and the field stays Won. That is this check
    // failing about one run in thirty-seven, and on 2026-08-19 it took down a
    // documentation-only commit's Windows leg, which is a bad way to spend a
    // morning.
    //
    // Minefield seeds itself from std::random_device and exposes no way to pin
    // it, so there is no seed to fix. Deal until the field is still playable —
    // and say so if it never is, rather than reporting a check that never ran.
    bool checked = false;
    for (int attempt = 0; attempt < 100 && !checked; ++attempt) {
        Minefield g(5, 5, 3);
        g.reveal(0, 0);
        if (g.state() != Minefield::State::Playing)
            continue;
        for (int r = 0; r < 5 && !checked; ++r)
            for (int c = 0; c < 5 && !checked; ++c)
                if (g.at(r, c).mine) {
                    g.reveal(r, c);
                    checked = true;
                }
        check(g.state() == Minefield::State::Lost, "minesweeper: digging a mine loses");
    }
    check(checked, "minesweeper: and a still-playable field was found to check it on");
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

// The codec the four solitaires save through. A solitaire has no move log to
// replay the way Chess does, so these two things are the whole of its safety:
// a pile comes back exactly as it went in, and a pile that cannot be a real
// deal is refused rather than restored into a game nobody can win.
void cardCodecRoundTrip()
{
    std::vector<Card> deck = makeDeck(1, 4);
    std::mt19937 rng(7);
    shuffleCards(deck, rng);
    // Which way up a card lies is half of Klondike's position, so it has to
    // survive the trip as much as the card itself does.
    for (std::size_t i = 0; i < deck.size(); i += 3)
        deck[i].faceUp = true;

    QByteArray blob;
    {
        QDataStream out(&blob, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        cardcodec::writePile(out, deck);
    }

    std::vector<Card> back;
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    check(cardcodec::readPile(in, back), "codec: a pile reads back");
    check(back.size() == deck.size(), "codec: with every card still in it");

    bool identical = back.size() == deck.size();
    for (std::size_t i = 0; i < back.size() && identical; ++i) {
        identical = back[i] == deck[i] && back[i].faceUp == deck[i].faceUp
            && back[i].deck == deck[i].deck;
    }
    check(identical, "codec: in the same order, each card the same way up");
}

void cardCodecRefusals()
{
    // A length beyond any real deal must not be trusted into an allocation.
    QByteArray absurd;
    {
        QDataStream out(&absurd, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        out << qint32(cardcodec::kMaxPileSize + 1);
    }
    std::vector<Card> pile;
    QDataStream tooLong(absurd);
    tooLong.setVersion(QDataStream::Qt_6_0);
    check(!cardcodec::readPile(tooLong, pile), "codec: an impossible pile length is refused");

    // A pile that promises four cards and carries one.
    QByteArray cut;
    {
        QDataStream out(&cut, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        out << qint32(4);
        cardcodec::writeCard(out, Card { Suit::Spades, kAce, true, 0 });
    }
    std::vector<Card> partial;
    QDataStream truncated(cut);
    truncated.setVersion(QDataStream::Qt_6_0);
    check(!cardcodec::readPile(truncated, partial), "codec: a truncated pile is refused");

    // A rank no pack holds.
    QByteArray impossible;
    {
        QDataStream out(&impossible, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        out << qint32(1) << qint8(0) << qint8(kKing + 1) << qint8(1) << qint8(0);
    }
    std::vector<Card> nonsense;
    QDataStream badRank(impossible);
    badRank.setVersion(QDataStream::Qt_6_0);
    check(!cardcodec::readPile(badRank, nonsense), "codec: a card above a King is refused");
}

void cardCodecPackCheck()
{
    const std::vector<Card> deck = makeDeck(1, 4);
    check(cardcodec::matchesPack(deck, 1, 4), "codec: a whole pack is the pack it was dealt from");

    std::vector<Card> missing = deck;
    missing.pop_back();
    check(!cardcodec::matchesPack(missing, 1, 4), "codec: one card short is not");
    check(cardcodec::fitsPack(missing, 1, 4), "codec: but it does still fit inside the pack");

    // The failure a state save has to catch: a card that turns up twice.
    std::vector<Card> doubled = missing;
    doubled.push_back(doubled.front());
    check(doubled.size() == deck.size(), "codec: the doubled pack is still 52 cards");
    check(!cardcodec::matchesPack(doubled, 1, 4),
          "codec: a card appearing twice is not a one-deck pack");

    check(cardcodec::matchesPack(makeDeck(2, 4), 2, 4), "codec: two packs match two packs");
    check(!cardcodec::fitsPack(makeDeck(2, 4), 1, 4), "codec: and do not fit inside one");

    // Spider's one-suit pack holds eight of every spade and nothing else.
    const std::vector<Card> spades = makeDeck(2, 1);
    check(cardcodec::matchesPack(spades, 2, 1), "codec: Spider's one-suit pack matches itself");
    check(!cardcodec::fitsPack(spades, 2, 4), "codec: but not a four-suit one");
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
// Chess
// ---------------------------------------------------------------------------

// Counting every leaf of the move tree to a fixed depth is the standard way to
// prove a move generator: one wrong castling, en-passant or pin rule and the
// totals stop matching the published numbers for these positions.
long long perft(const chess::Board& board, int depth)
{
    if (depth == 0)
        return 1;
    long long total = 0;
    for (const chess::Move& m : board.legalMoves()) {
        chess::Board next = board;
        next.apply(m);
        total += perft(next, depth - 1);
    }
    return total;
}

chess::Board fen(const char* text)
{
    chess::Board b;
    if (!b.setFromFen(text))
        check(false, "chess: a test position failed to parse");
    return b;
}

// Finds a move by the squares it runs between, so the checks below read like
// chess rather than like array indices.
bool play(chess::ChessGame& game, const char* from, const char* to,
          chess::PieceType promotion = chess::PieceType::None)
{
    for (const chess::Move& m : game.legalMoves()) {
        if (chess::squareName(m.from) == from && chess::squareName(m.to) == to
            && m.promotion == promotion) {
            game.play(m);
            return true;
        }
    }
    return false;
}

void chessMoveGeneration()
{
    const auto start = std::chrono::steady_clock::now();

    const chess::Board opening;
    check(perft(opening, 1) == 20, "chess: twenty opening moves");
    check(perft(opening, 2) == 400, "chess: 400 positions after one move each");
    check(perft(opening, 3) == 8902, "chess: 8,902 positions at depth three");
    check(perft(opening, 4) == 197281, "chess: 197,281 positions at depth four");

    // A middlegame with both castlings, pins and a capturable en passant.
    const chess::Board tangled =
        fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    check(perft(tangled, 1) == 48, "chess: 48 moves in a tangled middlegame");
    check(perft(tangled, 2) == 2039, "chess: 2,039 at depth two there");
    check(perft(tangled, 3) == 97862, "chess: 97,862 at depth three there");

    // An endgame that turns on en passant and a promotion race.
    const chess::Board race = fen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    check(perft(race, 3) == 2812, "chess: 2,812 in the promotion race");
    check(perft(race, 4) == 43238, "chess: 43,238 at depth four there");

    // Promotions under check, with both sides able to queen.
    const chess::Board promotions =
        fen("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    check(perft(promotions, 3) == 9467, "chess: 9,467 with promotions available");

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("      move generation checked in %lld ms\n", (long long)ms);
}

void chessSpecialMoves()
{
    // Castling, both sides, with the rook landing beside the king.
    chess::Board b = fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    int castles = 0;
    for (const chess::Move& m : b.legalMoves())
        if (m.castle)
            ++castles;
    check(castles == 2, "chess: both castlings are offered when nothing is in the way");

    for (const chess::Move& m : b.legalMoves()) {
        if (!m.castle || chess::squareName(m.to) != "g1")
            continue;
        chess::Board after = b;
        after.apply(m);
        check(after.at(7, 6).type == chess::PieceType::King
                  && after.at(7, 5).type == chess::PieceType::Rook
                  && after.at(7, 7).empty(),
              "chess: castling moves the rook as well as the king");
        check(!after.canCastle(chess::Colour::White, true)
                  && !after.canCastle(chess::Colour::White, false),
              "chess: castling gives up both rights");
    }

    // Castling out of, through, or into check is all illegal.
    check(fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1").legalMoves().size()
              > fen("r3k2r/8/8/8/4r3/8/8/R3K2R w KQkq - 0 1").legalMoves().size(),
          "chess: a checked king may not castle");
    int throughCheck = 0;
    for (const chess::Move& m : fen("r3k2r/8/8/8/5r2/8/8/R3K2R w KQkq - 0 1").legalMoves())
        if (m.castle && chess::squareName(m.to) == "g1")
            ++throughCheck;
    check(throughCheck == 0, "chess: a king may not castle through an attacked square");

    // En passant, taken the move after the double step and not later.
    chess::ChessGame game;
    game.setFromFen("4k3/8/8/8/4p3/8/3P4/4K3 w - - 0 1");
    check(play(game, "d2", "d4"), "chess: a pawn may open with a double step");
    check(chess::squareName(game.board().enPassantTarget()) == "d3",
          "chess: the double step leaves an en passant square");
    check(play(game, "e4", "d3"), "chess: en passant is available the move after");
    check(game.board().at(4, 3).empty() && game.board().at(5, 3).type == chess::PieceType::Pawn,
          "chess: en passant removes the pawn that ran past");

    // Promotion offers all four pieces.
    const chess::Board promoting = fen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    int promotions = 0;
    for (const chess::Move& m : promoting.legalMoves())
        if (m.promotion != chess::PieceType::None)
            ++promotions;
    check(promotions == 4, "chess: a promoting pawn may become any of four pieces");

    chess::ChessGame promotion;
    promotion.setFromFen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    check(play(promotion, "a7", "a8", chess::PieceType::Knight),
          "chess: underpromotion is playable");
    check(promotion.board().at(0, 0).type == chess::PieceType::Knight,
          "chess: the promoted pawn becomes the piece chosen");
}

void chessEndings()
{
    check(fen("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 1").isCheckmate(),
          "chess: checkmate is recognised");
    check(fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1").isStalemate(),
          "chess: stalemate is recognised, and is not mate");
    check(!fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1").isCheckmate(),
          "chess: a stalemated king is not in check");

    check(fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1").insufficientMaterial(),
          "chess: bare kings cannot mate");
    check(fen("4k3/8/8/8/8/8/8/4KB2 w - - 0 1").insufficientMaterial(),
          "chess: king and bishop cannot mate");
    check(fen("4k3/8/8/8/8/8/8/4KN2 w - - 0 1").insufficientMaterial(),
          "chess: king and knight cannot mate");
    check(!fen("4k3/8/8/8/8/8/8/4KR2 w - - 0 1").insufficientMaterial(),
          "chess: king and rook can mate");
    check(!fen("4k1n1/8/8/8/8/8/8/4KN2 w - - 0 1").insufficientMaterial(),
          "chess: two knights are not an automatic draw");

    // Threefold repetition: shuffle the knights back to where they started.
    chess::ChessGame game;
    const char* shuffle[8][2] = { { "g1", "f3" }, { "g8", "f6" }, { "f3", "g1" }, { "f6", "g8" },
                                 { "g1", "f3" }, { "g8", "f6" }, { "f3", "g1" }, { "f6", "g8" } };
    bool played = true;
    for (const auto& step : shuffle)
        played = played && play(game, step[0], step[1]);
    check(played, "chess: the knights can shuffle back and forth");
    check(game.repetitionCount() == 3, "chess: the opening position has now occurred three times");
    check(game.result() == chess::Result::Draw
              && game.drawReason() == chess::DrawReason::Repetition,
          "chess: threefold repetition is a draw");

    chess::ChessGame fifty;
    fifty.setFromFen("4k3/8/8/8/8/8/4R3/4K3 w - - 100 60");
    check(fifty.result() == chess::Result::Draw
              && fifty.drawReason() == chess::DrawReason::FiftyMove,
          "chess: a hundred quiet plies is a draw");

    chess::ChessGame mate;
    mate.setFromFen("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 1");
    check(mate.result() == chess::Result::BlackWins, "chess: the mated side loses");

    // Undo has to put the position back exactly, rights and clocks included.
    chess::ChessGame undone;
    const std::string before = undone.board().fen();
    check(play(undone, "e2", "e4"), "chess: e4 is legal");
    check(undone.undo() && undone.board().fen() == before,
          "chess: undo restores the position exactly");
}

void chessEngine()
{
    const auto start = std::chrono::steady_clock::now();

    // A back-rank mate in one: anything but Re8 is a wasted move.
    chess::Move found;
    int score = 0;
    check(chess::searchBestMove(fen("6k1/5ppp/8/8/8/8/8/4R1K1 w - - 0 1"), 2, found, &score),
          "chess: the engine returns a move");
    check(chess::squareName(found.from) == "e1" && chess::squareName(found.to) == "e8",
          "chess: the engine finds mate in one");

    // It must also see the mate coming and stop it.
    check(chess::searchBestMove(fen("6k1/5ppp/8/8/8/8/6PP/4R1K1 b - - 0 1"), 3, found, &score),
          "chess: the engine answers as Black");
    check(chess::squareName(found.to) != "h6" || score < 0,
          "chess: the engine does not walk into mate on the back rank");

    // And it should take a free queen.
    check(chess::searchBestMove(fen("4k3/8/8/3q4/4B3/8/8/4K3 w - - 0 1"), 2, found, &score),
          "chess: the engine returns a capture");
    check(chess::squareName(found.from) == "e4" && chess::squareName(found.to) == "d5",
          "chess: the engine takes an undefended queen");

    // Against random play it should win, not merely survive.
    std::mt19937 rng { 20260810 };
    int wins = 0;
    constexpr int kGames = 4;
    for (int game = 0; game < kGames; ++game) {
        chess::ChessGame g;
        const chess::Colour engine = (game % 2 == 0) ? chess::Colour::White : chess::Colour::Black;
        while (!g.isOver() && g.history().size() < 250) {
            const std::vector<chess::Move> moves = g.legalMoves();
            if (g.toMove() == engine) {
                chess::Move m;
                chess::searchBestMove(g.board(), 3, m);
                g.play(m);
            } else {
                std::uniform_int_distribution<std::size_t> pick(0, moves.size() - 1);
                g.play(moves[pick(rng)]);
            }
        }
        const chess::Result want =
            engine == chess::Colour::White ? chess::Result::WhiteWins : chess::Result::BlackWins;
        if (g.result() == want)
            ++wins;
        else if (g.result() != chess::Result::Draw)
            check(false, "chess: the engine lost to random play");
    }

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("      engine won %d of %d against random play in %lld ms\n", wins, kGames,
                (long long)ms);
    check(wins >= kGames - 1, "chess: the engine beats random play");
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


// ---------------------------------------------------------------------------
// Snake
//
// Until GHUB-0066 these rules lived inside the widget, so nothing anywhere
// could reach them -- Snake was the one game in the collection with no
// coverage at all, in this file OR the UI test.
// ---------------------------------------------------------------------------

void snakeStartsLegal()
{
    section("Snake");

    SnakeBoard board;
    check(int(board.snake().size()) == SnakeBoard::kStartLength,
          "snake: a new game is three segments long");
    check(board.score() == 0 && !board.dead(), "snake: with nothing scored and nothing hit");

    bool allInside = true;
    for (QPoint p : board.snake())
        allInside = allInside && SnakeBoard::inBounds(p);
    check(allInside, "snake: every segment starts inside the walls");
    check(!board.occupies(board.food()), "snake: and the food is never under the snake");
}

void snakeRefusesAReversal()
{
    SnakeBoard board;   // heading right
    check(!board.turn({ -1, 0 }), "snake: turning back into your own neck is refused");
    check(board.pending() == QPoint(1, 0), "snake: and the buffered turn is left alone");
    check(board.turn({ 0, -1 }), "snake: turning across is allowed");
    check(board.pending() == QPoint(0, -1), "snake: and is what the next step takes");
    // Two squares at once would step the head straight over a body segment
    // without the collision test ever seeing it.
    check(!board.turn({ 2, 0 }) && !board.turn({ 1, 1 }) && !board.turn({ 0, 0 }),
          "snake: a direction that is not one square along an axis is refused");
}

void snakeDiesAtTheWall()
{
    // Straight at the right-hand wall from the middle. Whatever it eats on the
    // way, it dies on the step that would take the head out of the grid, and
    // not one step earlier.
    SnakeBoard board;
    const int startX = board.head().x();
    int steps = 0;
    while (!board.dead() && steps < SnakeBoard::kWidth * 2) {
        ++steps;
        board.step();
    }
    check(board.dead(), "snake: driving at the wall ends the game");
    check(steps == SnakeBoard::kWidth - startX,
          "snake: on the step that leaves the grid, and not before");
}

void snakePlaysByItsOwnRules()
{
    // The property check the rules were extracted for. Play a lot of random
    // legal games and, before every single step, work out from the CURRENT
    // position what that step must do. Then take it and hold the core to it.
    std::mt19937 rng(20260825);
    const QPoint kDirs[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

    bool deathAlwaysEarned = true;
    bool lengthAlwaysRight = true;
    bool scoreAlwaysRight = true;
    bool neverOverlaps = true;
    bool alwaysInside = true;
    bool foodAlwaysFree = true;
    int deaths = 0;
    int meals = 0;
    int selfDeaths = 0;
    int longest = 0;

    for (int game = 0; game < 60; ++game) {
        SnakeBoard board;
        for (int move = 0; move < 400 && !board.dead(); ++move) {
            // Mostly steer TOWARDS the food, sometimes at random. A pure
            // random walk almost never eats -- measured, 8 meals in 60 games --
            // so it never grows, never gets long enough to run into itself, and
            // leaves the paths this check exists for untouched. Death is still
            // never avoided: nothing here looks at where the body is.
            QPoint want = kDirs[rng() % 4];
            if (rng() % 100 < 75) {
                const QPoint gap = board.food() - board.head();
                if (gap.x() != 0 && (gap.y() == 0 || (rng() % 2) == 0))
                    want = { gap.x() > 0 ? 1 : -1, 0 };
                else if (gap.y() != 0)
                    want = { 0, gap.y() > 0 ? 1 : -1 };
            }
            board.turn(want);

            const std::deque<QPoint> before = board.snake();
            const QPoint food = board.food();
            const int scoreBefore = board.score();
            const QPoint next = before.front() + board.pending();

            const bool eating = next == food;
            const auto tailEnd = eating ? before.end() : std::prev(before.end());
            const bool shouldDie
                = !SnakeBoard::inBounds(next) || std::find(before.begin(), tailEnd, next) != tailEnd;

            const SnakeBoard::Step result = board.step();

            if ((result == SnakeBoard::Step::Died) != shouldDie)
                deathAlwaysEarned = false;
            if (result == SnakeBoard::Step::Died) {
                ++deaths;
                if (SnakeBoard::inBounds(next))
                    ++selfDeaths;
                continue;
            }
            longest = std::max(longest, int(board.snake().size()));

            if (result == SnakeBoard::Step::Ate) {
                ++meals;
                if (board.snake().size() != before.size() + 1)
                    lengthAlwaysRight = false;
                if (board.score() != scoreBefore + SnakeBoard::kFoodScore)
                    scoreAlwaysRight = false;
            } else {
                if (board.snake().size() != before.size())
                    lengthAlwaysRight = false;
                if (board.score() != scoreBefore)
                    scoreAlwaysRight = false;
            }

            std::vector<QPoint> seen(board.snake().begin(), board.snake().end());
            std::sort(seen.begin(), seen.end(),
                      [](QPoint a, QPoint b) { return std::pair(a.x(), a.y()) < std::pair(b.x(), b.y()); });
            if (std::adjacent_find(seen.begin(), seen.end()) != seen.end())
                neverOverlaps = false;
            for (QPoint p : board.snake())
                alwaysInside = alwaysInside && SnakeBoard::inBounds(p);
            if (!board.boardFull() && board.occupies(board.food()))
                foodAlwaysFree = false;
        }
    }

    std::printf("      snake: 60 games, %d deaths, %d meals, %d of them by running into "
                "itself, longest snake %d\n",
                deaths, meals, selfDeaths, longest);
    check(deaths > 0 && meals > 0, "snake: the random games actually ate and died");
    // Without this the run could be all wall deaths, and the tail-square rule
    // -- the subtlest thing in this file -- would never once be exercised.
    check(selfDeaths > 0, "snake: and some of them died by running into themselves");
    check(deathAlwaysEarned,
          "snake: it dies exactly when it leaves the grid or meets itself, never otherwise");
    check(lengthAlwaysRight, "snake: and grows by one square only when it eats");
    check(scoreAlwaysRight, "snake: scoring ten only when it eats");
    check(neverOverlaps, "snake: no square is ever occupied twice");
    check(alwaysInside, "snake: and no segment is ever outside the walls");
    check(foodAlwaysFree, "snake: food is never left under the snake");
}


// ---------------------------------------------------------------------------
// 2048
//
// Named nowhere in this file until GHUB-0066: the rules lived inside the
// widget, so the self-test could not link them.
// ---------------------------------------------------------------------------

namespace {

// A board built by hand, so a merge can be checked against a position rather
// than hunted for in a real game. Reads row by row, top to bottom.
Twenty48Board boardFrom(const std::array<int, Twenty48Board::kCells>& cells, int score = 0)
{
    Twenty48Board board;
    const bool ok = board.restore(cells, score, false);
    check(ok, "2048: the hand-built position is one the game could reach");
    return board;
}

std::array<int, Twenty48Board::kCells> cellsOf(const Twenty48Board& board)
{
    return board.cells();
}

} // namespace

void twenty48MergesOnce()
{
    section("2048");

    // Four twos in a row become two fours, never an eight. Getting this wrong
    // is the classic 2048 bug and it doubles the player's score.
    Twenty48Board board = boardFrom({ 2, 2, 2, 2,
                                      0, 0, 0, 0,
                                      0, 0, 0, 0,
                                      0, 0, 0, 0 });
    check(board.slide(Twenty48Board::Direction::Left), "2048: a row of four twos moves");
    check(board.at(0, 0) == 4 && board.at(0, 1) == 4 && board.at(0, 2) == 0
              && board.at(0, 3) == 0,
          "2048: and merges into two fours, not one eight");
    check(board.score() == 8, "2048: scoring each merge, so eight rather than sixteen");

    // The merge is front to back, so 4 2 2 makes 4 4 and not 8.
    Twenty48Board pair = boardFrom({ 4, 2, 2, 0,
                                     0, 0, 0, 0,
                                     0, 0, 0, 0,
                                     0, 0, 0, 0 });
    pair.slide(Twenty48Board::Direction::Left);
    check(pair.at(0, 0) == 4 && pair.at(0, 1) == 4,
          "2048: a merge takes the pair it meets first, leaving the four alone");

    // And the merged tile cannot merge again in the same push.
    Twenty48Board chain = boardFrom({ 2, 2, 4, 0,
                                      0, 0, 0, 0,
                                      0, 0, 0, 0,
                                      0, 0, 0, 0 });
    chain.slide(Twenty48Board::Direction::Left);
    check(chain.at(0, 0) == 4 && chain.at(0, 1) == 4,
          "2048: a tile just made by a merge does not merge again in the same push");
}

void twenty48ReachesItsTarget()
{
    // Random play tops out around 256, so the one rule that decides the game
    // has been WON is unreachable that way and needs a position.
    Twenty48Board board = boardFrom({ 1024, 1024, 0, 0,
                                      0, 0, 0, 0,
                                      0, 0, 0, 0,
                                      0, 0, 0, 0 });
    check(!board.reachedTarget(), "2048: two 1024s is not yet the target");
    board.slide(Twenty48Board::Direction::Left);
    check(board.at(0, 0) == Twenty48Board::kTarget && board.reachedTarget(),
          "2048: merging them reaches it, and the board says so");
    // And it stays reached: the game carries on afterwards rather than ending.
    board.slide(Twenty48Board::Direction::Right);
    check(board.reachedTarget(), "2048: which is not forgotten on the next push");
}

void twenty48DeadPushCostsNothing()
{
    // A push that moves nothing must not spawn, must not score and must not
    // bank an undo -- otherwise a dead key hands the player a free tile.
    Twenty48Board board = boardFrom({ 2, 4, 8, 16,
                                      4, 8, 16, 32,
                                      8, 16, 32, 64,
                                      16, 32, 64, 128 });
    const auto before = cellsOf(board);
    check(!board.slide(Twenty48Board::Direction::Left), "2048: a push that moves nothing says so");
    check(cellsOf(board) == before, "2048: and leaves the board exactly as it was");
    check(board.score() == 0, "2048: and scores nothing");
    check(!board.canUndo(), "2048: and banks no undo, so a dead key is not a free tile");
    check(!board.canMove(), "2048: a full board with no equal neighbours is the end of the game");
}

void twenty48UndoStepsBack()
{
    Twenty48Board board = boardFrom({ 2, 2, 0, 0,
                                      0, 0, 0, 0,
                                      0, 0, 0, 0,
                                      0, 0, 0, 0 });
    const auto before = cellsOf(board);
    check(!board.canUndo(), "2048: a restored board offers no undo into a game it never played");
    check(board.slide(Twenty48Board::Direction::Left), "2048: the push moves");
    check(board.canUndo() && board.score() == 4, "2048: which banks an undo and scores");
    board.undo();
    check(cellsOf(board) == before && board.score() == 0,
          "2048: and undo puts back both the board and the score");
    check(!board.canUndo(), "2048: undo does not stack");
}

void twenty48RefusesABoardItCouldNotReach()
{
    Twenty48Board board;
    std::array<int, Twenty48Board::kCells> cells {};
    cells[0] = 3;
    check(!board.restore(cells, 0, false), "2048: a tile that is not a power of two is refused");
    cells[0] = 2;
    check(!board.restore(cells, -1, false), "2048: and so is a negative score");
    std::array<int, Twenty48Board::kCells> empty {};
    check(!board.restore(empty, 0, false), "2048: an empty board is not a game in progress");
    check(board.restore(cells, 0, false), "2048: a board of powers of two is accepted");
}

void twenty48PlaysByItsOwnRules()
{
    // Push at random until the board is stuck, many times over, checking after
    // every push that nothing impossible has happened.
    std::mt19937 rng(20260825);
    const Twenty48Board::Direction kDirs[4] = {
        Twenty48Board::Direction::Left, Twenty48Board::Direction::Right,
        Twenty48Board::Direction::Up, Twenty48Board::Direction::Down
    };

    bool tilesAlwaysPowers = true;
    bool scoreNeverFalls = true;
    bool deadPushCostsNothing = true;
    bool endsOnlyWhenStuck = true;
    int games = 0;
    int stuck = 0;
    int highest = 0;

    for (int game = 0; game < 200; ++game) {
        ++games;
        Twenty48Board board;
        int pushes = 0;
        while (board.canMove() && pushes < 2000) {
            ++pushes;
            const auto before = cellsOf(board);
            const int scoreBefore = board.score();
            const Twenty48Board::Direction dir = kDirs[rng() % 4];

            if (!board.slide(dir)) {
                if (cellsOf(board) != before || board.score() != scoreBefore)
                    deadPushCostsNothing = false;
                continue;
            }
            board.spawn();

            if (board.score() < scoreBefore)
                scoreNeverFalls = false;
            for (int v : board.cells()) {
                if (!Twenty48Board::isTile(v))
                    tilesAlwaysPowers = false;
                highest = std::max(highest, v);
            }
        }
        if (!board.canMove()) {
            ++stuck;
            // Stuck means genuinely stuck: no empty square and no equal
            // neighbours anywhere.
            for (int i = 0; i < Twenty48Board::kCells; ++i) {
                if (board.cells()[std::size_t(i)] == 0)
                    endsOnlyWhenStuck = false;
            }
        }
    }

    std::printf("      2048: %d games, %d played to a stuck board, highest tile %d\n", games,
                stuck, highest);
    check(stuck > 0, "2048: the random games actually played themselves out");
    check(tilesAlwaysPowers, "2048: every tile on the board is always a power of two");
    check(scoreNeverFalls, "2048: the score never goes down");
    check(deadPushCostsNothing, "2048: a push that moves nothing changes nothing");
    check(endsOnlyWhenStuck, "2048: the game ends only on a board with no empty square left");
}


// ---------------------------------------------------------------------------
// Pyramid
//
// Named nowhere in this file until GHUB-0066: the rules lived inside the
// widget and the self-test could not link them.
// ---------------------------------------------------------------------------

namespace {

using PT = PyramidTable;

// Everything on the table right now, taken cards excluded. What the test adds
// to this is the cards it has watched leave, and the two together must be the
// pack.
std::vector<Card> pyramidOnTable(const PT& table)
{
    std::vector<Card> present;
    for (const PT::Slot& slot : table.pyramid()) {
        if (!slot.removed)
            present.push_back(slot.card);
    }
    cardcodec::gather(present, table.stock());
    cardcodec::gather(present, table.waste());
    return present;
}

} // namespace

void pyramidDealsAWholePack()
{
    section("Pyramid");

    PT table;
    check(int(table.pyramid().size()) == PT::kPyramidCards,
          "pyramid: the deal lays twenty-eight cards");
    check(int(table.stock().size()) == 52 - PT::kPyramidCards,
          "pyramid: and the rest go to the stock");
    check(table.waste().empty() && table.pairs() == 0 && table.redeals() == 0,
          "pyramid: with nothing taken and no redeal spent");
    check(cardcodec::matchesPack(pyramidOnTable(table), 1, 4),
          "pyramid: and between them they are exactly one pack");

    // Only the bottom row can be reached at the start; everything above it is
    // holding up two cards.
    int exposed = 0;
    for (int slot = 0; slot < PT::kPyramidCards; ++slot) {
        if (table.isExposed(slot))
            ++exposed;
    }
    check(exposed == PT::kRows, "pyramid: only the bottom row starts exposed");
}

void pyramidRefusesACoveredCard()
{
    PT table;
    // The apex is covered by the whole pyramid, so it can never be taken first
    // however well it would pair.
    check(!table.isExposed(0), "pyramid: the apex starts covered");
    check(!table.available(PT::Source::Pyramid, 0), "pyramid: and so cannot be picked up");
    check(!table.takeKing(PT::Source::Pyramid, 0),
          "pyramid: a covered card is refused even if it is a King");

    // Its two supports are the bottom of the second row.
    const int left = PT::slotIndex(1, 0);
    const int right = PT::slotIndex(1, 1);
    check(!table.isExposed(left) && !table.isExposed(right),
          "pyramid: the row below it is covered too");
}

void pyramidPairsMustMakeThirteen()
{
    // Built by hand rather than hunted for in a deal, so the arithmetic can be
    // checked exactly -- and built out of REAL pack cards, because restore()
    // refuses a pyramid holding the same card twice and would otherwise leave
    // the dealt table in place with every check below quietly testing that.
    std::vector<Card> deck = makeDeck(1, 4);
    auto lift = [&deck](Suit suit, int rank) {
        const auto it = std::find_if(deck.begin(), deck.end(), [&](const Card& c) {
            return c.suit == suit && c.rank == rank;
        });
        Card c = *it;
        deck.erase(it);
        c.faceUp = true;
        return c;
    };
    // The bottom row is slots 21..27.
    const Card five = lift(Suit::Clubs, 5);
    const Card eight = lift(Suit::Hearts, 8);
    const Card otherFive = lift(Suit::Spades, 5);
    const Card thirdFive = lift(Suit::Diamonds, 5);
    const Card king = lift(Suit::Clubs, kKing);

    std::vector<PT::Slot> pyramid;
    pyramid.reserve(PT::kPyramidCards);
    auto fill = [&pyramid, &deck](int from, int count) {
        for (int i = 0; i < count; ++i) {
            Card c = deck[std::size_t(from + i)];
            c.faceUp = true;
            pyramid.push_back({ c, false });
        }
    };
    fill(0, 21);
    pyramid.push_back({ five, false });        // 21
    pyramid.push_back({ eight, false });       // 22
    pyramid.push_back({ otherFive, false });   // 23
    pyramid.push_back({ thirdFive, false });   // 24
    pyramid.push_back({ king, false });        // 25
    fill(21, 2);                               // 26, 27

    PT table;
    check(table.restore(pyramid, {}, {}, 0, 0), "pyramid: the hand-built position is accepted");

    check(!table.takePair(PT::Source::Pyramid, 23, PT::Source::Pyramid, 24),
          "pyramid: five and five make ten, so they are refused");
    check(!table.takePair(PT::Source::Pyramid, 21, PT::Source::Pyramid, 21),
          "pyramid: and a card cannot be paired with itself");

    const int above = PT::slotIndex(PT::kRows - 2, 0);
    check(!table.isExposed(above), "pyramid: the card those two hold up starts covered");

    check(table.takePair(PT::Source::Pyramid, 21, PT::Source::Pyramid, 22),
          "pyramid: five and eight make thirteen, so they go");
    check(table.pyramid()[21].removed && table.pyramid()[22].removed,
          "pyramid: and both of them leave the table");
    check(table.pairs() == 1, "pyramid: counted as one pair");
    check(table.isExposed(above), "pyramid: which exposes the card they were holding up");

    check(!table.takeKing(PT::Source::Pyramid, 23), "pyramid: a five is not a King");
    check(table.takeKing(PT::Source::Pyramid, 25), "pyramid: a King is thirteen on its own");
    check(table.pyramid()[25].removed, "pyramid: and goes alone");
    check(table.pairs() == 2, "pyramid: counted like a pair, because it clears the same thirteen");
}

void pyramidExposesOnBothSupports()
{
    PT table;
    // Clear the two left-hand cards of the bottom row and the card they hold up
    // must become reachable -- and only then.
    const int leftSlot = PT::slotIndex(PT::kRows - 1, 0);
    const int rightSlot = PT::slotIndex(PT::kRows - 1, 1);
    const int above = PT::slotIndex(PT::kRows - 2, 0);

    std::vector<PT::Slot> pyramid = table.pyramid();
    pyramid[std::size_t(leftSlot)].removed = true;
    check(table.restore(pyramid, table.stock(), table.waste(), 1, 0),
          "pyramid: a position with one support gone is legal");
    check(!table.isExposed(above), "pyramid: one support gone leaves the card above covered");

    pyramid[std::size_t(rightSlot)].removed = true;
    check(table.restore(pyramid, table.stock(), table.waste(), 2, 0),
          "pyramid: and so is one with both gone");
    check(table.isExposed(above), "pyramid: both supports gone exposes it");
}

void pyramidStockAndRedeals()
{
    PT table;
    const int stockAtDeal = int(table.stock().size());

    check(table.drawFromStock(), "pyramid: a card turns from the stock");
    check(int(table.stock().size()) == stockAtDeal - 1 && table.waste().size() == 1,
          "pyramid: and lands face up on the waste");
    check(table.waste().back().faceUp, "pyramid: face up, because it is now playable");

    // Only the top of the waste is reachable.
    table.drawFromStock();
    check(table.available(PT::Source::Waste, int(table.waste().size()) - 1),
          "pyramid: the top of the waste is available");
    check(!table.available(PT::Source::Waste, 0),
          "pyramid: the card buried under it is not");

    // Empty the stock, then redeal twice, then no more.
    while (!table.stock().empty())
        table.drawFromStock();
    check(table.waste().size() == std::size_t(stockAtDeal),
          "pyramid: turning the whole stock puts every card on the waste");

    check(table.drawFromStock() && table.redeals() == 1,
          "pyramid: an empty stock turns the waste back over");
    check(table.waste().empty() && int(table.stock().size()) == stockAtDeal,
          "pyramid: and the waste is empty again");
    bool allDown = true;
    for (const Card& c : table.stock())
        allDown = allDown && !c.faceUp;
    check(allDown, "pyramid: face down, as a stock is");

    while (!table.stock().empty())
        table.drawFromStock();
    check(table.drawFromStock() && table.redeals() == PT::kMaxRedeals,
          "pyramid: a second redeal is allowed");
    while (!table.stock().empty())
        table.drawFromStock();
    check(!table.drawFromStock(), "pyramid: a third is refused");
    check(table.redeals() == PT::kMaxRedeals, "pyramid: and spends nothing");
}

void pyramidUndoStepsBack()
{
    PT table;
    check(!table.canUndo(), "pyramid: a fresh deal has nothing to undo");
    const std::vector<Card> before = pyramidOnTable(table);
    table.drawFromStock();
    check(table.canUndo(), "pyramid: turning a card banks an undo");
    table.undo();
    check(pyramidOnTable(table) == before, "pyramid: and undo puts the table back exactly");
    check(!table.canUndo(), "pyramid: with nothing left to undo");
}

void pyramidPlaysOutWithoutLosingACard()
{
    // The property the bullet asked for: play legal moves at random until the
    // game is stuck or cleared, and after EVERY move the cards still on the
    // table plus the cards taken off it must be exactly one pack.
    std::mt19937 rng(20260825);
    bool packAlwaysWhole = true;
    bool onlyExposedTaken = true;
    int games = 0;
    int cleared = 0;
    int totalTaken = 0;

    for (int game = 0; game < 120; ++game) {
        ++games;
        PT table;
        std::vector<Card> taken;

        for (int move = 0; move < 400; ++move) {
            // Every take available right now.
            struct Move { PT::Source a; int ai; PT::Source b; int bi; bool king; };
            std::vector<Move> moves;
            std::vector<std::pair<PT::Source, int>> open;
            for (int slot = 0; slot < PT::kPyramidCards; ++slot) {
                if (table.isExposed(slot))
                    open.emplace_back(PT::Source::Pyramid, slot);
            }
            if (!table.waste().empty())
                open.emplace_back(PT::Source::Waste, int(table.waste().size()) - 1);

            for (std::size_t i = 0; i < open.size(); ++i) {
                if (table.cardAt(open[i].first, open[i].second).rank == kKing) {
                    moves.push_back({ open[i].first, open[i].second, open[i].first, open[i].second,
                                      true });
                    continue;
                }
                for (std::size_t j = i + 1; j < open.size(); ++j) {
                    if (table.cardAt(open[i].first, open[i].second).rank
                            + table.cardAt(open[j].first, open[j].second).rank
                        == PT::kPairTotal) {
                        moves.push_back({ open[i].first, open[i].second, open[j].first,
                                          open[j].second, false });
                    }
                }
            }

            // Half the time turn a card instead, so the stock and the redeals
            // are exercised rather than left standing. With nothing to take,
            // turning is the only thing left; when that fails too the game is
            // stuck and this deal is over.
            if (moves.empty() || (rng() % 2) == 0) {
                if (table.drawFromStock())
                    continue;
                if (moves.empty())
                    break;
                // Nothing left to turn, but there is still a take: fall through.
            }

            const Move& m = moves[rng() % moves.size()];
            if (!table.available(m.a, m.ai) || (!m.king && !table.available(m.b, m.bi)))
                onlyExposedTaken = false;

            const Card first = table.cardAt(m.a, m.ai);
            const Card second = table.cardAt(m.b, m.bi);
            const bool ok = m.king ? table.takeKing(m.a, m.ai)
                                   : table.takePair(m.a, m.ai, m.b, m.bi);
            if (!ok) {
                onlyExposedTaken = false;
                break;
            }
            taken.push_back(first);
            if (!m.king)
                taken.push_back(second);
            ++totalTaken;

            std::vector<Card> all = pyramidOnTable(table);
            cardcodec::gather(all, taken);
            if (!cardcodec::matchesPack(all, 1, 4))
                packAlwaysWhole = false;

            if (table.cleared())
                break;
        }
        if (table.cleared())
            ++cleared;
    }

    std::printf("      pyramid: %d games, %d cleared, %d takes\n", games, cleared, totalTaken);
    check(totalTaken > 0, "pyramid: the random games actually took cards off the table");
    check(packAlwaysWhole,
          "pyramid: the cards on the table plus the cards taken are always exactly one pack");
    check(onlyExposedTaken, "pyramid: and nothing was ever taken that was not available");
}


// ---------------------------------------------------------------------------
// FreeCell
//
// Named nowhere in this file until GHUB-0066.
// ---------------------------------------------------------------------------

namespace {

using FC = FreeCellTable;

std::vector<Card> freecellOnTable(const FC& table)
{
    std::vector<Card> all;
    cardcodec::gather(all, table.columns());
    cardcodec::gather(all, table.cells());
    cardcodec::gather(all, table.foundations());
    return all;
}

} // namespace

void freecellDealsAWholePack()
{
    section("FreeCell");

    FC table;
    check(cardcodec::matchesPack(freecellOnTable(table), 1, 4),
          "freecell: the deal is exactly one pack");

    int sevens = 0;
    int sixes = 0;
    bool allFaceUp = true;
    for (const std::vector<Card>& col : table.columns()) {
        if (col.size() == 7)
            ++sevens;
        else if (col.size() == 6)
            ++sixes;
        for (const Card& c : col)
            allFaceUp = allFaceUp && c.faceUp;
    }
    check(sevens == 4 && sixes == 4, "freecell: four columns of seven and four of six");
    check(allFaceUp, "freecell: every card face up, which is the whole game");
    check(table.moves() == 0 && !table.canUndo() && !table.won(),
          "freecell: with nothing moved and nothing to undo");
}

void freecellMoveSizeIsTheRule()
{
    // One card, doubled for every empty column, times one more than the free
    // cells. Getting this wrong turns FreeCell into a different game.
    FC table;
    check(table.maxMoveSize(false) == 5,
          "freecell: four free cells and no empty column moves five cards");

    // Empty the columns and cells by hand and count again.
    std::array<std::vector<Card>, FC::kColumns> columns;
    std::array<std::vector<Card>, FC::kCells> cells;
    std::array<std::vector<Card>, FC::kFoundations> foundations;
    std::vector<Card> deck = makeDeck(1, 4);
    for (Card& c : deck)
        c.faceUp = true;
    // Everything into two columns, leaving six empty and every cell free.
    for (std::size_t i = 0; i < deck.size(); ++i)
        columns[i % 2].push_back(deck[i]);
    check(table.restore(columns, cells, foundations, 0),
          "freecell: the hand-built position is one the rules could reach");
    check(table.maxMoveSize(false) == 5 * (1 << 6),
          "freecell: six empty columns double it six times over");
    check(table.maxMoveSize(true) == 5 * (1 << 5),
          "freecell: and moving INTO an empty column cannot also stage through it");

    // Fill every cell: the multiplier collapses to the empty columns alone.
    for (int i = 0; i < FC::kCells; ++i) {
        cells[std::size_t(i)].push_back(columns[0].back());
        columns[0].pop_back();
    }
    check(table.restore(columns, cells, foundations, 0), "freecell: with the cells full");
    check(table.maxMoveSize(false) == 1 * (1 << 6),
          "freecell: no free cell leaves one card times the empty columns");
}

void freecellStacksAlternateAndDescend()
{
    std::array<std::vector<Card>, FC::kColumns> columns;
    std::array<std::vector<Card>, FC::kCells> cells;
    std::array<std::vector<Card>, FC::kFoundations> foundations;
    std::vector<Card> deck = makeDeck(1, 4);
    auto lift = [&deck](Suit suit, int rank) {
        const auto it = std::find_if(deck.begin(), deck.end(), [&](const Card& c) {
            return c.suit == suit && c.rank == rank;
        });
        Card c = *it;
        deck.erase(it);
        c.faceUp = true;
        return c;
    };

    const Card blackEight = lift(Suit::Spades, 8);
    const Card redSeven = lift(Suit::Hearts, 7);
    const Card blackSix = lift(Suit::Clubs, 6);
    const Card otherBlackSeven = lift(Suit::Clubs, 7);
    const Card redSix = lift(Suit::Diamonds, 6);

    // A card sitting ABOVE the run, so the run is shorter than the column. It
    // has to be a BLACK nine: a red one would legally continue the black eight
    // and the run would simply be four cards long.
    const Card blackNine = lift(Suit::Clubs, 9);
    columns[0] = { blackNine, blackEight, redSeven, blackSix };
    columns[1] = { otherBlackSeven };
    columns[2] = { redSix };
    for (std::size_t i = 0; i < deck.size(); ++i)
        columns[3 + (i % 5)].push_back(deck[i]);

    FC table;
    check(table.restore(columns, cells, foundations, 0),
          "freecell: the hand-built position is one the rules could reach");

    check(table.orderedRunLength(0) == 3,
          "freecell: black eight, red seven, black six is a run of three");
    check(table.orderedRunLength(1) == 1, "freecell: a lone card is a run of one");
    check(table.firstMovableIndex(0) == 1,
          "freecell: and the black nine above them is not part of it");

    check(table.canStack(redSix, 1), "freecell: a red six goes on a black seven");
    check(!table.canStack(redSeven, 1), "freecell: a red seven does not go on a black seven");
    check(!table.canStack(blackSix, 1), "freecell: nor does a black six -- same colour");

    // Taking hold above the run is refused outright. Checked on a copy,
    // because a successful lift would take the cards off the table and bank an
    // undo -- an assertion with a side effect is one that changes what the
    // checks after it are looking at.
    {
        FC probe = table;
        check(probe.lift(FC::PileKind::Column, 0, 0).empty(),
              "freecell: a card above the run cannot be picked up");
        check(!probe.canUndo(), "freecell: and a refused lift banks nothing");
        check(probe.columns()[0].size() == 4, "freecell: leaving the column as it was");

        FC ok = table;
        check(ok.lift(FC::PileKind::Column, 0, 1).size() == 3,
              "freecell: taking hold at the top of the run lifts all three");
    }
}

void freecellFoundationsGoUpInSuit()
{
    FC table;
    const Card aceOfSpades { .suit = Suit::Spades, .rank = kAce, .faceUp = true };
    const Card twoOfSpades { .suit = Suit::Spades, .rank = 2, .faceUp = true };
    const Card twoOfHearts { .suit = Suit::Hearts, .rank = 2, .faceUp = true };

    check(table.canPlaceOnFoundation(aceOfSpades, 0), "freecell: an empty foundation takes an Ace");
    check(!table.canPlaceOnFoundation(twoOfSpades, 0),
          "freecell: and nothing else, however tempting");

    std::array<std::vector<Card>, FC::kColumns> columns;
    std::array<std::vector<Card>, FC::kCells> cells;
    std::array<std::vector<Card>, FC::kFoundations> foundations;
    foundations[0] = { aceOfSpades };
    std::vector<Card> deck = makeDeck(1, 4);
    for (Card& c : deck) {
        if (!(c.suit == Suit::Spades && c.rank == kAce))
            columns[deck.size() % FC::kColumns].push_back(c);
    }
    // Spread the rest so no column is absurd.
    for (std::vector<Card>& col : columns)
        col.clear();
    int at = 0;
    for (const Card& c : deck) {
        if (c.suit == Suit::Spades && c.rank == kAce)
            continue;
        Card faceUp = c;
        faceUp.faceUp = true;
        columns[std::size_t(at++ % FC::kColumns)].push_back(faceUp);
    }
    check(table.restore(columns, cells, foundations, 1),
          "freecell: the hand-built position is one the rules could reach");
    check(table.canPlaceOnFoundation(twoOfSpades, 0), "freecell: the two of the suit follows");
    check(!table.canPlaceOnFoundation(twoOfHearts, 0), "freecell: a two of another suit does not");
}

void freecellUndoDoesNotLoseACard()
{
    // The bug this extraction found (GHUB-0126). The view lifted a run off its
    // pile when the drag began and only snapshotted at DROP time, so undoing a
    // finished move restored a table the cards had never been on -- and they
    // were gone. FreeCell's own save then refused to reload, because it
    // demands the whole pack back.
    FC table;
    const std::vector<Card> before = freecellOnTable(table);

    // Park a card in a cell, the one move legal in every deal.
    std::vector<Card> run = table.lift(FC::PileKind::Column, 0,
                                       int(table.columns()[0].size()) - 1);
    check(run.size() == 1, "freecell: the bottom card of a column lifts on its own");
    check(table.dropOnCell(run, 0), "freecell: and parks in a free cell");
    check(cardcodec::matchesPack(freecellOnTable(table), 1, 4),
          "freecell: the pack is still whole with a card in a cell");

    check(table.canUndo(), "freecell: the move banked an undo");
    table.undo();
    check(cardcodec::matchesPack(freecellOnTable(table), 1, 4),
          "freecell: and undoing it does not lose the card off the table");
    check(freecellOnTable(table) == before, "freecell: it puts every pile back exactly");

    // A drag that is put back down where it came from must leave NOTHING to
    // undo -- otherwise the undo stack fills up with moves nobody made.
    std::vector<Card> abandoned = table.lift(FC::PileKind::Column, 1,
                                             int(table.columns()[1].size()) - 1);
    check(!abandoned.empty(), "freecell: a card lifts");
    table.putBack(FC::PileKind::Column, 1, abandoned);
    check(freecellOnTable(table) == before, "freecell: putting it back changes nothing");
    check(!table.canUndo(), "freecell: and banks no undo, because nothing happened");
}

void freecellPlaysOutWithoutLosingACard()
{
    // Play legal moves at random and hold the pack to account after every one.
    std::mt19937 rng(20260825);
    bool packAlwaysWhole = true;
    bool runLimitRespected = true;
    int moves = 0;
    int foundationCards = 0;

    for (int game = 0; game < 60; ++game) {
        FC table;
        for (int move = 0; move < 300; ++move) {
            // Anything that can go home, goes home a third of the time.
            if ((rng() % 3) == 0) {
                bool sent = false;
                for (int c = 0; c < FC::kCells && !sent; ++c)
                    sent = table.sendToFoundation(FC::PileKind::Cell, c);
                for (int c = 0; c < FC::kColumns && !sent; ++c)
                    sent = table.sendToFoundation(FC::PileKind::Column, c);
                if (sent) {
                    ++moves;
                    if (!cardcodec::matchesPack(freecellOnTable(table), 1, 4))
                        packAlwaysWhole = false;
                    continue;
                }
            }

            const int from = int(rng() % FC::kColumns);
            if (table.columns()[std::size_t(from)].empty())
                continue;
            const int first = table.firstMovableIndex(from);
            const int take = first + int(rng() % std::size_t(table.columns()[std::size_t(from)].size() - std::size_t(first)));

            std::vector<Card> run = table.lift(FC::PileKind::Column, from, take);
            if (run.empty())
                continue;

            const int to = int(rng() % FC::kColumns);
            int limit = 0;
            bool placed = false;
            if (to != from)
                placed = table.dropOnColumn(run, to, &limit);
            if (!placed && limit > 0 && int(run.size()) <= limit)
                runLimitRespected = false;
            if (!placed && run.size() == 1)
                placed = table.dropOnCell(run, int(rng() % FC::kCells));
            if (!placed)
                table.putBack(FC::PileKind::Column, from, run);
            else
                ++moves;

            if (!cardcodec::matchesPack(freecellOnTable(table), 1, 4))
                packAlwaysWhole = false;
        }
        for (const std::vector<Card>& f : table.foundations())
            foundationCards += int(f.size());
    }

    std::printf("      freecell: 60 games, %d moves made, %d cards sent home\n", moves,
                foundationCards);
    check(moves > 0 && foundationCards > 0, "freecell: the random games actually played");
    check(packAlwaysWhole, "freecell: the whole pack is on the table after every single move");
    check(runLimitRespected, "freecell: a run within the limit is never refused for being too long");
}

// ---------------------------------------------------------------------------
// Canasta
// ---------------------------------------------------------------------------

namespace ca = canasta;

Card cd(Suit s, int rank) { return Card { s, rank, false, 0 }; }
Card joker(bool red) { return Card { red ? Suit::Hearts : Suit::Spades, kJoker, false, 0 }; }

// Builds a stock that deals exactly `hands` to the four seats. Cards come off
// the back, so the deal order is reversed onto the tail; `up` becomes the first
// card of the discard pile and `below` is what is left to draw.
std::vector<Card> canastaStock(const std::array<std::vector<Card>, 4>& hands, int dealer,
                              const std::vector<Card>& below, const Card& up)
{
    const int first = (dealer + 1) % ca::kSeats;
    std::vector<Card> order;
    for (std::size_t i = 0; i < hands[std::size_t(first)].size(); ++i)
        for (int s = 0; s < ca::kSeats; ++s)
            order.push_back(hands[std::size_t((first + s) % ca::kSeats)][i]);

    std::vector<Card> stock = below;
    stock.push_back(up);
    stock.insert(stock.end(), order.rbegin(), order.rend());
    return stock;
}

// Eleven cards of no interest, for the seats a check is not about.
std::vector<Card> filler(int rank)
{
    std::vector<Card> v;
    for (int i = 0; i < 11; ++i)
        v.push_back(cd(i % 2 == 0 ? Suit::Clubs : Suit::Spades, rank));
    return v;
}

void canastaDeckAndValues()
{
    const std::vector<Card> deck = makeDeck(2, 4, 4);
    check(deck.size() == 108, "canasta: the pack is 108 cards");
    check(std::count_if(deck.begin(), deck.end(), isJoker) == 4, "canasta: four jokers");

    int red = 0;
    int blue = 0;
    for (const Card& c : deck)
        (c.deck == 0 ? blue : red)++;
    check(red == 54 && blue == 54, "canasta: two packs of 54, one red-backed and one blue");
    check(std::count_if(deck.begin(), deck.end(),
                        [](const Card& c) { return isJoker(c) && c.deck == 1; })
              == 2,
          "canasta: each pack brings its own two jokers");

    // Every other game must be untouched by jokers existing at all.
    const std::vector<Card> plain = makeDeck(1, 4);
    check(plain.size() == 52 && std::none_of(plain.begin(), plain.end(), isJoker),
          "canasta: a single pack still has no jokers in it");

    const ca::Rules r;
    check(ca::cardValue(joker(true), r) == 50, "canasta: a joker is 50");
    check(ca::cardValue(cd(Suit::Spades, 2), r) == 20, "canasta: a two is 20");
    check(ca::cardValue(cd(Suit::Spades, kAce), r) == 20, "canasta: an ace is 20");
    check(ca::cardValue(cd(Suit::Spades, kKing), r) == 10, "canasta: a king is 10");
    check(ca::cardValue(cd(Suit::Spades, 8), r) == 10, "canasta: an eight is 10");
    check(ca::cardValue(cd(Suit::Spades, 7), r) == 5, "canasta: a seven is 5");
    check(ca::cardValue(cd(Suit::Spades, 4), r) == 5, "canasta: a four is 5");
    check(ca::cardValue(cd(Suit::Spades, 3), r) == 5, "canasta: a black three is 5");
    check(ca::cardValue(cd(Suit::Hearts, 3), r) == 100, "canasta: a red three is 100");

    check(ca::isWild(joker(false)) && ca::isWild(cd(Suit::Clubs, 2)),
          "canasta: jokers and twos are the wild cards");
    check(!ca::isWild(cd(Suit::Clubs, kAce)), "canasta: an ace is not wild");
    check(ca::isRedThree(cd(Suit::Diamonds, 3)) && !ca::isRedThree(cd(Suit::Clubs, 3)),
          "canasta: only the red threes are red threes");
}

void canastaOpeningBands()
{
    const ca::Rules r;
    check(ca::openRequirementFor(-200, r) == 15, "canasta: a side in the red opens on 15");
    check(ca::openRequirementFor(0, r) == 50, "canasta: from nothing you open on 50");
    check(ca::openRequirementFor(1495, r) == 50, "canasta: 1495 still opens on 50");
    check(ca::openRequirementFor(1500, r) == 90, "canasta: 1500 opens on 90");
    check(ca::openRequirementFor(2995, r) == 90, "canasta: 2995 still opens on 90");
    check(ca::openRequirementFor(3000, r) == 120, "canasta: 3000 opens on 120");
    check(ca::openRequirementFor(9000, r) == 120, "canasta: the top band does not keep rising");
}

void canastaMeldShapes()
{
    const ca::Rules r;

    ca::Meld natural { kKing, { cd(Suit::Spades, kKing), cd(Suit::Hearts, kKing),
                                cd(Suit::Clubs, kKing), cd(Suit::Diamonds, kKing),
                                cd(Suit::Spades, kKing), cd(Suit::Hearts, kKing),
                                cd(Suit::Clubs, kKing) } };
    check(natural.isCanasta(r) && natural.isNatural(r),
          "canasta: seven kings with no wilds is a natural canasta");
    check(natural.value(r) == 70, "canasta: seven kings are worth 70 in card values");

    ca::Meld mixed = natural;
    mixed.cards.back() = joker(true);
    check(mixed.isCanasta(r) && !mixed.isNatural(r),
          "canasta: a joker in the seven makes it a mixed canasta");
    check(mixed.wilds() == 1 && mixed.naturals() == 6, "canasta: wilds and naturals are counted");

    ca::Meld blacks { 3, { cd(Suit::Spades, 3), cd(Suit::Clubs, 3), cd(Suit::Spades, 3),
                           cd(Suit::Clubs, 3), cd(Suit::Spades, 3), cd(Suit::Clubs, 3),
                           cd(Suit::Spades, 3) } };
    check(!blacks.isCanasta(r), "canasta: black threes never make a canasta");

    ca::Team t;
    t.melds = { natural };
    check(t.hasCanasta(r), "canasta: a team with a canasta knows it");
    check(t.meldOfRank(kKing) != nullptr && t.meldOfRank(5) == nullptr,
          "canasta: melds are found by rank");
}

void canastaScoringTable()
{
    const ca::Rules r;

    ca::Team t;
    t.opened = true;
    // A natural canasta of aces: 7 x 20 in card values, plus the 500 bonus.
    t.melds = { ca::Meld { kAce,
                           { cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce),
                             cd(Suit::Clubs, kAce), cd(Suit::Diamonds, kAce),
                             cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce),
                             cd(Suit::Clubs, kAce) } } };
    check(ca::handScoreFor(t, {}, {}, false, false, r) == 640,
          "canasta: a natural ace canasta scores 140 plus the 500 bonus");

    // The same seven with a joker in it: 6 x 20 + 50 in cards, 300 bonus.
    ca::Team mixedTeam = t;
    mixedTeam.melds[0].cards.back() = joker(false);
    check(ca::handScoreFor(mixedTeam, {}, {}, false, false, r) == 470,
          "canasta: swapping a card for a joker gives the mixed bonus instead");

    // Red threes: a bonus to a side that opened, the same against one that did not.
    ca::Team reds;
    reds.opened = true;
    reds.redThrees = { cd(Suit::Hearts, 3), cd(Suit::Diamonds, 3) };
    check(ca::handScoreFor(reds, {}, {}, false, false, r) == 200,
          "canasta: two red threes are worth 200 to a side that melded");
    reds.opened = false;
    check(ca::handScoreFor(reds, {}, {}, false, false, r) == -200,
          "canasta: the same two count against a side that never melded");

    reds.opened = true;
    reds.redThrees.push_back(cd(Suit::Hearts, 3));
    reds.redThrees.push_back(cd(Suit::Diamonds, 3));
    check(ca::handScoreFor(reds, {}, {}, false, false, r) == 800,
          "canasta: all four red threes are 800, not 400");

    // Going out, concealed or not, and cards left in hand coming off.
    ca::Team out = t;
    check(ca::handScoreFor(out, {}, {}, true, false, r) == 740,
          "canasta: going out adds 100");
    check(ca::handScoreFor(out, {}, {}, true, true, r) == 840,
          "canasta: going out concealed adds 200 instead");
    check(ca::handScoreFor(out, { cd(Suit::Spades, kKing) }, { joker(true) }, false, false, r)
              == 640 - 10 - 50,
          "canasta: cards still in either partner's hand are deducted");
}

// Enough spare cards under the up-card that the pack still totals 108, so the
// card-conservation checks mean what they say.
const int kBelowCount = 63;

std::vector<Card> spare() { return std::vector<Card>(kBelowCount, cd(Suit::Clubs, 9)); }

// Taking the pile: what it costs, and the two things that stop you.
void canastaPileRules()
{
    // Seat 1 leads when seat 0 deals. Two sevens to match the up-card, and
    // three aces so the take clears the 50 needed to open.
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    hands[1] = { cd(Suit::Spades, 7), cd(Suit::Hearts, 7), cd(Suit::Spades, kAce),
                 cd(Suit::Hearts, kAce), cd(Suit::Clubs, kAce), cd(Suit::Clubs, 4),
                 cd(Suit::Clubs, 5), cd(Suit::Clubs, 6), cd(Suit::Clubs, 8),
                 cd(Suit::Spades, 10), cd(Suit::Spades, 2) };
    hands[2] = filler(10);
    hands[3] = filler(kJack);

    ca::Engine e;
    e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 7)), 0);
    check(e.currentSeat() == 1, "canasta: the player left of the dealer starts");
    check(e.phase() == ca::Engine::Phase::Draw, "canasta: a turn opens on the draw");
    check(e.cardsInPlay() == 108, "canasta: the deal accounts for all 108 cards");
    check(!e.pileFrozen(), "canasta: an ordinary up-card does not freeze the pile");

    check(!e.canTakePile({ cd(Suit::Spades, 7) }),
          "canasta: one matching card is not enough to take the pile");
    // Three sevens is 15, well short of the 50 this side needs to open.
    check(!e.canTakePile({ cd(Suit::Spades, 7), cd(Suit::Hearts, 7) }),
          "canasta: a take that does not reach the opening minimum is refused");
    // The same take with three aces alongside is 15 + 60.
    const std::vector<Card> opening { cd(Suit::Spades, 7), cd(Suit::Hearts, 7),
                                      cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce),
                                      cd(Suit::Clubs, kAce) };
    check(e.canTakePile(opening), "canasta: laying aces alongside makes the same take legal");

    check(e.takePile(opening), "canasta: taking the pile to open succeeds");
    check(e.cardsInPlay() == 108, "canasta: taking the pile loses no cards");
    check(e.team(1).opened, "canasta: the side is now open");
    check(e.pile().empty(), "canasta: the pile is gone once taken");
    check(e.team(1).meldOfRank(7) != nullptr && e.team(1).meldOfRank(7)->size() == 3,
          "canasta: the top card joined the meld it was taken with");
    check(e.team(1).meldOfRank(kAce) != nullptr, "canasta: the aces went down as well");

    check(e.discard(cd(Suit::Spades, 2)), "canasta: a wild card can be discarded");
    check(e.pileFrozen(), "canasta: discarding a wild freezes the pile");
    check(e.currentSeat() == 2, "canasta: the turn passes to the left");
    check(e.drawFromStock(), "canasta: the next seat draws");
    check(!e.canTakePileAtAll(),
          "canasta: a wild card on top cannot be melded, so the pile is safe");

    // A black three blocks the pile without freezing it.
    std::array<std::vector<Card>, 4> blackTop = hands;
    ca::Engine blocked;
    blocked.newGameFromStock(canastaStock(blackTop, 0, spare(), cd(Suit::Spades, 3)), 0);
    check(!blocked.pileFrozen(), "canasta: a black three does not freeze the pile");
    check(!blocked.canTakePileAtAll(), "canasta: but a black three on top blocks it");
}

// The card that froze the pile, and where it sits in it. The table draws that
// card sideways, and what it needs is the DEPTH rather than the identity:
// found by a reverse search for a wild and painted one place under the top
// card, it was drawn twice over when the freezing throw was the most recent
// one, and climbed back over every discard thrown after it (GHUB-0094).
//
// The double-draw itself is structural now -- one pass down the stack, one
// branch per card, so no card can take both -- and what is checkable here is
// the index those branches are chosen by.
void canastaFrozenPileDepth()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    hands[1] = { cd(Suit::Spades, 7), cd(Suit::Hearts, 7), cd(Suit::Spades, kAce),
                 cd(Suit::Hearts, kAce), cd(Suit::Clubs, kAce), cd(Suit::Clubs, 4),
                 cd(Suit::Clubs, 5), cd(Suit::Clubs, 6), cd(Suit::Clubs, 8),
                 cd(Suit::Spades, 10), cd(Suit::Spades, 2) };
    hands[2] = filler(10);
    hands[3] = filler(kJack);

    ca::Engine e;
    e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 7)), 0);
    check(e.freezeCardIndex() < 0, "canasta: an unfrozen pile names no freezing card");

    const std::vector<Card> opening { cd(Suit::Spades, 7), cd(Suit::Hearts, 7),
                                      cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce),
                                      cd(Suit::Clubs, kAce) };
    check(e.takePile(opening), "canasta: the pile is taken to open");
    check(e.discard(cd(Suit::Spades, 2)), "canasta: and a wild is thrown to freeze it");
    check(e.pileFrozen(), "canasta: the pile is frozen");
    check(e.freezeCardIndex() == int(e.pile().size()) - 1,
          "canasta: the freezing card is the top card while it is the last thing thrown");

    // The next seat throws something ordinary on top of it.
    check(e.drawFromStock(), "canasta: the next seat draws");
    const Card thrown = e.hand(e.currentSeat()).front();
    check(!ca::isWild(thrown), "canasta: with an ordinary card to throw");
    const int before = e.freezeCardIndex();
    check(e.discard(thrown), "canasta: which goes on the pile");
    check(e.freezeCardIndex() == before,
          "canasta: the freezing card keeps the place it was thrown at");
    check(e.freezeCardIndex() < int(e.pile().size()) - 1,
          "canasta: so a card thrown after it covers it rather than sliding underneath");
}

// Catching them a minus: the owner's family's name for the rule that turns a
// side's own melds against it for want of a canasta (GHUB-0098). The game says
// the phrase out loud, so the phrase and the arithmetic have to be one thing.
void canastaCaughtAMinus()
{
    ca::Team bare;
    bare.opened = true;
    // Four kings down and no canasta anywhere: 40 in card values.
    bare.melds = { ca::Meld { kKing,
                              { cd(Suit::Spades, kKing), cd(Suit::Hearts, kKing),
                                cd(Suit::Clubs, kKing), cd(Suit::Diamonds, kKing) } } };

    ca::Rules classic = ca::Rules::classic();
    check(!ca::caughtAMinus(bare, classic),
          "canasta: nobody is caught a minus while the rule is off");

    ca::Rules minus = classic;
    minus.canastaNeededToScore = true;
    check(ca::caughtAMinus(bare, minus), "canasta: a side with no canasta is caught a minus");

    // The phrase and the score are the same claim, so they are checked as one.
    const int kind = ca::handScoreFor(bare, {}, {}, false, false, classic);
    const int cruel = ca::handScoreFor(bare, {}, {}, false, false, minus);
    check(kind == 40, "canasta: those kings are worth 40 to a side that is not caught");
    check(cruel == -40, "canasta: and the same 40 against one that is");
    check(cruel < kind, "canasta: so being caught a minus always costs, never pays");

    // Seven of them is a canasta, and a canasta is the whole defence.
    ca::Team safe = bare;
    safe.melds[0].cards = { cd(Suit::Spades, kKing),   cd(Suit::Hearts, kKing),
                            cd(Suit::Clubs, kKing),    cd(Suit::Diamonds, kKing),
                            cd(Suit::Spades, kKing),   cd(Suit::Hearts, kKing),
                            cd(Suit::Clubs, kKing) };
    check(safe.melds[0].isCanasta(minus), "canasta: seven kings is a canasta");
    check(!ca::caughtAMinus(safe, minus), "canasta: which is what stops the minus");
    check(ca::handScoreFor(safe, {}, {}, false, false, minus) > 0,
          "canasta: and the side scores in its own favour again");
}

// A hand that dies with the stock gone and nobody out. Four hands sharing no
// rank with the up-card and nothing behind it to draw, so the very first turn
// finds an empty stock and a pile nobody can take. Classic scores that where it
// stands; the house rule voids it and the next hand is dealt from the same
// totals.
void canastaDeadHand()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(4);
    hands[1] = filler(5);
    hands[2] = filler(6);
    hands[3] = filler(7);
    const Card up = cd(Suit::Spades, 9);

    ca::Engine scored;
    scored.newGameFromStock(canastaStock(hands, 0, {}, up), 0);
    check(scored.phase() == ca::Engine::Phase::HandOver,
          "canasta: an empty stock nobody can take the pile against ends the hand");
    check(scored.wentOutSeat() < 0, "canasta: and it ends with nobody having gone out");
    check(scored.team(0).score == -110 && scored.team(1).score == -110,
          "canasta: classic scores that hand where it stands");

    ca::Rules dead = ca::Rules::classic();
    dead.deadHandIfNobodyGoesOut = true;
    ca::Engine e;
    e.setRules(dead);
    e.newGameFromStock(canastaStock(hands, 0, {}, up), 0);
    check(e.phase() == ca::Engine::Phase::HandOver, "canasta: the dead hand ends the same way");
    check(e.wentOutSeat() < 0, "canasta: still with nobody out");
    check(e.team(0).handScore == 0 && e.team(1).handScore == 0,
          "canasta: but the house rule scores it nothing either way");
    check(e.team(0).score == 0 && e.team(1).score == 0,
          "canasta: so neither total moves and the next hand is dealt level");
}

// Winning by REACHING the target rather than by being ahead when somebody does
// (GHUB-0123). A hand that carries both sides past it is a draw under the house
// rule, however far apart the two totals are.
//
// Built on the dead-hand position, which scores a hand instantly with nobody
// out, so the totals are exactly the cards the four seats were caught holding
// and the target can be put wherever the check needs it.
void canastaWinningIsReachingTheTarget()
{
    // Team 0 is caught with 22 low cards, team 1 with 22 kings, so both totals
    // are negative and one side is plainly ahead of the other.
    std::array<std::vector<Card>, 4> apart;
    apart[0] = filler(4);
    apart[1] = filler(kKing);
    apart[2] = filler(6);
    apart[3] = filler(kKing);
    const Card up = cd(Suit::Spades, 9);

    ca::Rules classic = ca::Rules::classic();
    classic.targetScore = -300; // both sides clear it in one hand
    ca::Engine ahead;
    ahead.setRules(classic);
    ahead.newGameFromStock(canastaStock(apart, 0, {}, up), 0);
    check(ahead.team(0).score == -110 && ahead.team(1).score == -220,
          "canasta: one hand puts both sides over the target, on different totals");
    check(ahead.phase() == ca::Engine::Phase::GameOver,
          "canasta: which ends the game");
    check(ahead.winner() == 0, "canasta: classic hands it to the higher score");

    ca::Rules house = classic;
    house.bothReachingTargetIsADraw = true;
    ca::Engine drawn;
    drawn.setRules(house);
    drawn.newGameFromStock(canastaStock(apart, 0, {}, up), 0);
    check(drawn.phase() == ca::Engine::Phase::GameOver,
          "canasta: the house rule ends the same game");
    check(drawn.winner() == ca::Engine::kDraw,
          "canasta: but calls it a draw, because both sides reached the target");
    check(ca::Engine::kDraw != 0 && ca::Engine::kDraw != 1 && ca::Engine::kDraw != -1,
          "canasta: and a draw is neither team, nor a game still running");

    // One side over it and the other not: nothing about this rule applies.
    house.targetScore = -200;
    ca::Engine won;
    won.setRules(house);
    won.newGameFromStock(canastaStock(apart, 0, {}, up), 0);
    check(won.phase() == ca::Engine::Phase::GameOver && won.winner() == 0,
          "canasta: one side over the target on its own still wins outright");

    // The exact tie, which is the same position read the same way: both sides
    // reached it. Classic plays another hand rather than declare a joint
    // winner, and that is what the house rule replaces.
    std::array<std::vector<Card>, 4> level;
    level[0] = filler(4);
    level[1] = filler(5);
    level[2] = filler(6);
    level[3] = filler(7);

    ca::Rules tieClassic = ca::Rules::classic();
    tieClassic.targetScore = -300;
    ca::Engine again;
    again.setRules(tieClassic);
    again.newGameFromStock(canastaStock(level, 0, {}, up), 0);
    check(again.team(0).score == again.team(1).score,
          "canasta: a hand that leaves the two sides level");
    check(again.phase() == ca::Engine::Phase::HandOver,
          "canasta: classic deals another rather than declare a joint winner");

    ca::Rules tieHouse = tieClassic;
    tieHouse.bothReachingTargetIsADraw = true;
    ca::Engine tied;
    tied.setRules(tieHouse);
    tied.newGameFromStock(canastaStock(level, 0, {}, up), 0);
    check(tied.phase() == ca::Engine::Phase::GameOver
              && tied.winner() == ca::Engine::kDraw,
          "canasta: the house rule calls the same tie a draw and stops there");
}

// Nothing safe left to throw: the computer has to feed one of the melds facing
// it, and the least damaging is the one with furthest to go. Checked on a
// hand-built table rather than a position played into existence.
// The house rule that bars melding for one round round bars taking the pile
// with it, so for three seats a discard cannot be punished — and the fourth
// seat is the exception, because the turn after it is the first seat playing a
// second time. The computer has to know which of those two it is sitting in.
void canastaFirstRoundSafeThrow()
{
    ca::Rules r = ca::Rules::classic();
    r.noMeldingFirstRound = true;

    // Where the window opens and shuts. Driven by playing real turns rather
    // than by poking the counter, so the arithmetic is checked against the
    // engine's own idea of whose turn it is.
    {
        std::array<std::vector<Card>, 4> hands;
        for (int s = 0; s < ca::kSeats; ++s)
            hands[std::size_t(s)] = filler(9 + s);

        ca::Engine e { r };
        e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 5)), 0);

        const bool expected[ca::kSeats] = { true, true, true, false };
        for (int turn = 0; turn < ca::kSeats; ++turn) {
            check(e.discardCannotBeTaken() == expected[turn],
                  turn < 3 ? "canasta: inside the first round a throw cannot be taken"
                           : "canasta: except on the last seat of the round, whose throw the "
                             "first seat can take on its second turn");
            check(!e.meldingAllowed(),
                  "canasta: and no seat in the first round may lay anything down");
            check(e.drawFromStock(), "canasta: the first round is draw and discard");
            check(e.discard(e.hand(e.currentSeat()).front()),
                  "canasta: and the turn ends on the throw");
        }
        check(e.meldingAllowed(),
              "canasta: the round over, the first seat plays again and may lay down");
        check(!e.discardCannotBeTaken(),
              "canasta: and from then on every throw is live");
    }

    // With the rule off there is no window at all, which is what stops the
    // block above passing on a build that ignores the rule and always says yes.
    {
        std::array<std::vector<Card>, 4> hands;
        for (int s = 0; s < ca::kSeats; ++s)
            hands[std::size_t(s)] = filler(9 + s);

        ca::Engine plain;
        plain.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 5)), 0);
        check(!plain.discardCannotBeTaken(),
              "canasta: without the house rule the very first throw is already live");
        check(plain.meldingAllowed(), "canasta: and the first seat may open straight away");
    }

    // What the computer does with the knowledge. A black three's only worth is
    // stopping the next seat taking the pile, so throwing one while nothing can
    // be taken spends the block and buys nothing. Seat 1 holds exactly one and
    // ten cards it has no use for, so there is always something else to throw.
    const auto handWithBlackThree = [] {
        std::vector<Card> v { cd(Suit::Spades, 3) };
        const int junk[] = { 4, 5, 6, 7, 8, 9, 10, kJack, kQueen, kKing };
        for (int rank : junk)
            v.push_back(cd(Suit::Diamonds, rank));
        return v;
    };

    for (ca::Level level : { ca::Level::Medium, ca::Level::Hard, ca::Level::Expert }) {
        std::array<std::vector<Card>, 4> hands;
        hands[0] = filler(9);
        hands[1] = handWithBlackThree();
        hands[2] = filler(10);
        hands[3] = filler(kJack);

        // An up-card of a rank nobody in this hand holds. With a matching rank
        // on the pile, Expert's "they have parted with it, so it is safe" bonus
        // is worth +50 and simply outbids the black three, which makes the two
        // halves below differ by more than the rule they are meant to isolate.
        ca::Engine e { r };
        e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, kAce)), 0);
        check(e.currentSeat() == 1, "canasta: seat 1 leads");
        ca::Ai ai { level };
        e.drawFromStock();
        ai.playAndDiscard(e);
        check(!ca::isBlackThree(e.pile().back()),
              "canasta: the computer keeps its black three while nobody can take the pile");
    }

    // The other half of knowing the throw is free: every judgement about
    // HANDING THE PILE OVER is measuring a risk that cannot happen, so it is
    // dropped rather than obeyed. Expert is the level that shows it — a rank
    // already sitting in the pile reads to it as one the others have parted
    // with, and therefore safe, worth +50. Nothing can be taken, so that is
    // worth nothing, and the king it would have thrown is ten points the hand
    // should keep over a four.
    //
    // This is the ONLY safety term with any force in the first round: nobody
    // has melded, because the same rule forbids it, so discardRisk is zero for
    // every rank in the hand. A test built on discardRisk would assert nothing.
    {
        const auto kingMatchingPile = [] {
            std::vector<Card> v { cd(Suit::Diamonds, kKing) };
            const int junk[] = { 4, 5, 6, 7, 8, 9, 10, kJack, kQueen, kAce };
            for (int rank : junk)
                v.push_back(cd(Suit::Clubs, rank));
            return v;
        };
        std::array<std::vector<Card>, 4> hands;
        hands[0] = filler(9);
        hands[1] = kingMatchingPile();
        hands[2] = filler(10);
        hands[3] = filler(kJack);

        ca::Engine e { r };
        e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Spades, kKing)), 0);
        ca::Ai first { ca::Level::Expert };
        e.drawFromStock();
        first.playAndDiscard(e);
        check(e.pile().back().rank != kKing,
              "canasta: inside the first round the computer stops reading the pile for "
              "safety, because there is no danger to read");

        // Same position, rule off. Now the pile can be taken, the reading means
        // something again, and the king goes — which is what stops the check
        // above passing on a computer that never throws a king anyway.
        ca::Engine plain;
        plain.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Spades, kKing)), 0);
        ca::Ai second { ca::Level::Expert };
        plain.drawFromStock();
        second.playAndDiscard(plain);
        check(plain.pile().back().rank == kKing,
              "canasta: with the pile live it throws the rank the others have already "
              "let go of");
    }

    // And the same seat, same hand, with the rule off: the block is worth
    // something again, so the black three goes. Without this half the check
    // above passes on a computer that simply never throws a black three.
    for (ca::Level level : { ca::Level::Medium, ca::Level::Hard, ca::Level::Expert }) {
        std::array<std::vector<Card>, 4> hands;
        hands[0] = filler(9);
        hands[1] = handWithBlackThree();
        hands[2] = filler(10);
        hands[3] = filler(kJack);

        ca::Engine plain;
        plain.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, kAce)), 0);
        ca::Ai ai { level };
        plain.drawFromStock();
        ai.playAndDiscard(plain);
        check(ca::isBlackThree(plain.pile().back()),
              "canasta: with the pile live again it throws the black three to block it");
    }
}

void canastaLeastDamagingDiscard()
{
    const auto meldOf = [](int rank, int n) {
        ca::Meld m;
        m.rank = rank;
        for (int i = 0; i < n; ++i)
            m.cards.push_back(cd(i % 2 == 0 ? Suit::Spades : Suit::Hearts, rank));
        return m;
    };
    const ca::Rules r = ca::Rules::classic();

    // Three melds on the other side: three fours, six fives, five sevens.
    ca::Team them;
    them.melds.push_back(meldOf(4, 3));
    them.melds.push_back(meldOf(5, 6));
    them.melds.push_back(meldOf(7, 5));

    const double four = ca::discardRisk(them, 4, 20, false, r);
    const double five = ca::discardRisk(them, 5, 20, false, r);
    const double seven = ca::discardRisk(them, 7, 20, false, r);
    check(four < seven && seven < five,
          "canasta: the least damaging throw is the meld with furthest to go");
    check(ca::discardRisk(them, 9, 20, false, r) == 0.0,
          "canasta: a rank they have not melded costs nothing to throw");
    check(ca::discardRisk(them, 5, 20, true, r) == 0.0,
          "canasta: a frozen pile needs a pair, so even their best meld is safe to feed");
    // The bigger the pile the more it costs to hand over, whatever the meld.
    check(ca::discardRisk(them, 4, 40, false, r) > four,
          "canasta: and a bigger pile costs more to hand over");

    // Once a canasta has closed the rank against them it is the safest card there is.
    ca::Rules safe = r;
    safe.canastaMakesRankSafe = true;
    ca::Team closed;
    closed.melds.push_back(meldOf(6, 7));
    check(ca::discardRisk(closed, 6, 20, false, safe) < 0.0,
          "canasta: a rank they have closed is the safest throw in the hand");
    check(ca::discardRisk(closed, 6, 20, false, r) > 0.0,
          "canasta: without that house rule it still hands them the pile");
}

// Playing for the minus, against being fed (GHUB-0099 and GHUB-0100). They are
// one judgement written as two numbers, and both come back in POINTS so that
// Ai::closingOut can weigh them against each other rather than order them: a
// minus on the table argues for ending the hand now, a pack that keeps coming
// back argues for staying in it. Checked here on hand-built positions, the way
// discardRisk and handScoreFor already are.
void canastaMinusAgainstMilking()
{
    const auto meldOf = [](int rank, int n) {
        ca::Meld m;
        m.rank = rank;
        for (int i = 0; i < n; ++i)
            m.cards.push_back(cd(i % 2 == 0 ? Suit::Spades : Suit::Hearts, rank));
        return m;
    };
    const ca::Rules classic = ca::Rules::classic();
    ca::Rules house = classic;
    house.canastaNeededToScore = true;

    // Five kings and a red three showing, and no canasta to protect them.
    ca::Team theirs;
    theirs.opened = true;
    theirs.melds.push_back(meldOf(kKing, 5));
    theirs.redThrees.push_back(cd(Suit::Hearts, 3));

    check(ca::minusOnOffer(theirs, classic) == 0,
          "canasta: with the minus rule off there is nothing on offer, however much they show");
    check(ca::minusOnOffer(theirs, house) == 2 * (50 + 100),
          "canasta: with it on, every point they show is worth two to us");
    check(ca::minusOnOffer(theirs, house) == 3 * house.goingOutBonus,
          "canasta: which here is three times the bonus going out used to be priced at");

    ca::Team safe = theirs;
    safe.melds.push_back(meldOf(7, 7));
    check(safe.hasCanasta(house), "canasta: once they have a canasta down");
    check(ca::minusOnOffer(safe, house) == 0, "canasta: the minus is off the table");

    // An unopened side is docked its red threes whatever happens, so only the
    // melds swing — there is nothing to win in the threes.
    ca::Team unopened = theirs;
    unopened.opened = false;
    check(ca::minusOnOffer(unopened, house) == 2 * 50,
          "canasta: and an unopened side only swings by its melds");

    // Being fed. Our side has nines down, so every nine thrown into the pack is
    // one we can take it on.
    ca::Team mine;
    mine.opened = true;
    mine.melds.push_back(meldOf(9, 3));

    const auto packOf = [](int nines, int queens) {
        std::vector<Card> v;
        for (int i = 0; i < nines; ++i)
            v.push_back(cd(Suit::Clubs, 9));
        for (int i = 0; i < queens; ++i)
            v.push_back(cd(Suit::Clubs, kQueen));
        return v;
    };
    const std::vector<Card> fed = packOf(4, 8);
    const std::vector<Card> starved = packOf(1, 11);
    const std::vector<Card> thin = packOf(5, 0);
    const std::vector<Card> nothing;
    const std::vector<Card> noPair { cd(Suit::Spades, 4), cd(Suit::Hearts, 5),
                                     cd(Suit::Clubs, 6) };
    const std::vector<Card> aPair { cd(Suit::Spades, kQueen), cd(Suit::Hearts, kQueen) };
    const std::vector<Card> twoWilds { cd(Suit::Spades, 2), cd(Suit::Hearts, 2) };

    check(ca::packWorthStayingFor(thin, noPair, mine, false, classic) == 0,
          "canasta: a thin pack is not worth staying in for, however takeable");
    check(ca::packWorthStayingFor(starved, noPair, mine, false, classic) == 0,
          "canasta: nor one we have almost none of down");
    check(ca::packWorthStayingFor(fed, noPair, mine, false, classic) > 0,
          "canasta: a pack we have a quarter of melded keeps feeding us");
    check(ca::packWorthStayingFor(fed, noPair, mine, false, classic)
              == 12 * classic.highCardValue,
          "canasta: and it is worth its points, so it can be weighed against a minus");

    // Frozen wants two naturals out of hand, so it is only coming back if we
    // are holding the pair that takes it.
    check(ca::packWorthStayingFor(fed, noPair, mine, true, classic) == 0,
          "canasta: frozen against us with no pair, the pack is not coming back");
    check(ca::packWorthStayingFor(fed, aPair, mine, true, classic) > 0,
          "canasta: unless we are holding a pair, which is what takes a frozen pack");
    check(ca::packWorthStayingFor(fed, twoWilds, mine, true, classic) == 0,
          "canasta: and two wild cards are not that pair, since it wants naturals");

    // An unopened side is in the frozen position whether or not anyone froze it.
    ca::Team unopenedMine = mine;
    unopenedMine.opened = false;
    check(ca::packWorthStayingFor(fed, noPair, unopenedMine, false, classic) == 0,
          "canasta: an unopened side is in the same position as a frozen pack");
    check(ca::packWorthStayingFor(nothing, aPair, mine, false, classic) == 0,
          "canasta: and an empty pack is worth nothing at all");
}

// Frozen and unfrozen, from identical positions. The only difference between
// the two runs is whether seat 1 threw a wild card or an ordinary one.
void canastaFrozenPile()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(kQueen);
    // Seat 1 opens with four aces, then throws either a two or a four.
    hands[1] = { cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce), cd(Suit::Clubs, kAce),
                 cd(Suit::Diamonds, kAce), cd(Suit::Spades, 2), cd(Suit::Clubs, 4),
                 cd(Suit::Clubs, 5), cd(Suit::Clubs, 6), cd(Suit::Clubs, 8),
                 cd(Suit::Spades, 9), cd(Suit::Spades, kJack) };
    // Seat 2 has a ten to throw and nothing else of interest.
    hands[2] = { cd(Suit::Diamonds, 10), cd(Suit::Spades, kQueen), cd(Suit::Spades, kQueen),
                 cd(Suit::Spades, kQueen), cd(Suit::Hearts, kQueen), cd(Suit::Hearts, kQueen),
                 cd(Suit::Hearts, kQueen), cd(Suit::Clubs, kQueen), cd(Suit::Clubs, kQueen),
                 cd(Suit::Clubs, kQueen), cd(Suit::Diamonds, kQueen) };
    // Seat 3 is seat 1's partner, so its side is already open. Two tens and a
    // joker: enough to take an unfrozen pile, not enough for a frozen one.
    hands[3] = { cd(Suit::Spades, 10), cd(Suit::Hearts, 10), joker(true),
                 cd(Suit::Spades, kJack), cd(Suit::Spades, kJack), cd(Suit::Spades, kJack),
                 cd(Suit::Hearts, kJack), cd(Suit::Hearts, kJack), cd(Suit::Hearts, kJack),
                 cd(Suit::Clubs, kJack), cd(Suit::Clubs, kJack) };

    for (int frozen = 0; frozen < 2; ++frozen) {
        ca::Engine e;
        e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);

        e.drawFromStock();
        const bool opened = e.meldCards({ cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce),
                                          cd(Suit::Clubs, kAce), cd(Suit::Diamonds, kAce) });
        e.discard(frozen ? cd(Suit::Spades, 2) : cd(Suit::Clubs, 4));
        e.drawFromStock();
        e.discard(cd(Suit::Diamonds, 10));

        // Seat 3 is left on its draw, because taking the pile IS the draw.
        const bool ready = opened && e.currentSeat() == 3 && e.team(1).opened
            && e.phase() == ca::Engine::Phase::Draw && !e.pile().empty()
            && e.pile().back().rank == 10 && e.pileFrozen() == (frozen != 0);
        check(ready, frozen ? "canasta: reached a frozen pile with a ten on top"
                            : "canasta: reached an open pile with a ten on top");

        const std::vector<Card> pair { cd(Suit::Spades, 10), cd(Suit::Hearts, 10) };
        const std::vector<Card> oneAndWild { cd(Suit::Spades, 10), joker(true) };

        check(!e.canTakePile({ cd(Suit::Spades, 10) }),
              frozen ? "canasta: frozen, one card cannot take the pile"
                     : "canasta: open, one card still cannot take the pile");
        check(e.canTakePile(pair) == true,
              frozen ? "canasta: frozen, two matching cards take it"
                     : "canasta: open, two matching cards take it");
        // This is the whole difference freezing makes.
        check(e.canTakePile(oneAndWild) == (frozen == 0),
              frozen ? "canasta: frozen, a card plus a wild is NOT enough"
                     : "canasta: open, a card plus a wild is enough");
    }
}

void canastaWildCardRules()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    hands[1] = { cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce), cd(Suit::Clubs, kAce),
                 cd(Suit::Diamonds, kAce), cd(Suit::Spades, kQueen), cd(Suit::Hearts, kQueen),
                 cd(Suit::Spades, 2), joker(true), cd(Suit::Hearts, 2), cd(Suit::Clubs, 2),
                 cd(Suit::Spades, 3) };
    hands[2] = filler(10);
    hands[3] = filler(kJack);

    ca::Engine e;
    e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 5)), 0);
    check(e.drawFromStock(), "canasta: seat one draws");

    // A wild card has no rank of its own, so on its own it is not a meld.
    check(!e.canMeldCards({ cd(Suit::Spades, 2) }),
          "canasta: a wild card alone is not a meld");

    // Across two ranks it is placed rather than refused, and it goes where it
    // is needed: three aces are already a meld, two queens are not.
    check(e.canMeldCards({ cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce),
                           cd(Suit::Clubs, kAce), cd(Suit::Spades, kQueen),
                           cd(Suit::Hearts, kQueen), cd(Suit::Spades, 2) }),
          "canasta: a wild card across two ranks is placed, not refused");

    check(e.meldCards({ cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce), cd(Suit::Clubs, kAce),
                        cd(Suit::Diamonds, kAce) }),
          "canasta: four aces are 80, enough to open on 50");
    check(e.team(1).meldOfRank(kAce)->size() == 4, "canasta: the meld holds four");

    check(e.meldCards({ cd(Suit::Spades, 2) }, kAce),
          "canasta: a wild card joins a meld when you say which one");
    check(e.team(1).meldOfRank(kAce)->size() == 5 && e.team(1).meldOfRank(kAce)->wilds() == 1,
          "canasta: the wild card landed on the aces");

    check(e.meldCards({ joker(true), cd(Suit::Hearts, 2) }, kAce),
          "canasta: two more wilds bring it to seven");
    check(e.team(1).meldOfRank(kAce)->isCanasta(e.rules()), "canasta: seven cards is a canasta");
    check(!e.team(1).meldOfRank(kAce)->isNatural(e.rules()),
          "canasta: with three wilds in it, a mixed one");

    check(!e.canMeldCards({ cd(Suit::Clubs, 2) }, kAce),
          "canasta: a meld never holds more than three wild cards");
    check(!e.canMeldCards({ cd(Suit::Spades, kQueen), cd(Suit::Hearts, kQueen) }),
          "canasta: two of a rank is not a meld");
    check(!e.canMeldCards({ cd(Suit::Spades, 3) }),
          "canasta: a black three cannot be melded on an ordinary turn");
}

void canastaRedThrees()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    hands[1] = { cd(Suit::Hearts, 3), cd(Suit::Diamonds, 3), cd(Suit::Clubs, kKing),
                 cd(Suit::Hearts, kKing), cd(Suit::Diamonds, kKing), cd(Suit::Spades, 2),
                 cd(Suit::Clubs, 4), cd(Suit::Clubs, 5), cd(Suit::Clubs, 6), cd(Suit::Clubs, 8),
                 cd(Suit::Spades, 10) };
    hands[2] = filler(10);
    hands[3] = filler(kJack);

    ca::Engine e;
    e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);

    check(e.team(1).redThrees.size() == 2, "canasta: red threes dealt go down at once");
    check(e.hand(1).size() == 11, "canasta: each is replaced, keeping the hand at eleven");
    check(std::none_of(e.hand(1).begin(), e.hand(1).end(), ca::isRedThree),
          "canasta: no red three is left in a hand");
    check(e.cardsInPlay() == 108, "canasta: placing red threes loses no cards");

    // A red three can never be thrown away.
    check(e.drawFromStock() && !e.canDiscard(cd(Suit::Hearts, 3)),
          "canasta: a red three is never discardable");
}

void canastaGoingOut()
{
    // Seven kings is both the opening and the canasta; four fours and the drawn
    // card are what is left to clear.
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    hands[1] = { cd(Suit::Spades, kKing), cd(Suit::Hearts, kKing), cd(Suit::Clubs, kKing),
                 cd(Suit::Diamonds, kKing), cd(Suit::Spades, kKing), cd(Suit::Hearts, kKing),
                 cd(Suit::Clubs, kKing), cd(Suit::Spades, 4), cd(Suit::Hearts, 4),
                 cd(Suit::Clubs, 4), cd(Suit::Diamonds, 4) };
    hands[2] = filler(10);
    hands[3] = filler(kJack);

    ca::Engine e;
    e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);
    check(e.drawFromStock(), "canasta: seat one draws");

    check(e.meldCards({ cd(Suit::Spades, kKing), cd(Suit::Hearts, kKing), cd(Suit::Clubs, kKing),
                        cd(Suit::Diamonds, kKing), cd(Suit::Spades, kKing),
                        cd(Suit::Hearts, kKing), cd(Suit::Clubs, kKing) }),
          "canasta: seven kings go down in one opening");
    check(e.team(1).hasCanasta(e.rules()), "canasta: that is a canasta");

    check(e.meldCards({ cd(Suit::Spades, 4), cd(Suit::Hearts, 4), cd(Suit::Clubs, 4),
                        cd(Suit::Diamonds, 4) }),
          "canasta: the fours go down too");
    check(e.hand(1).size() == 1, "canasta: one card left in hand");
    check(e.discard(e.hand(1).front()), "canasta: discarding the last card goes out");
    check(e.phase() == ca::Engine::Phase::HandOver || e.phase() == ca::Engine::Phase::GameOver,
          "canasta: going out ends the hand");
    check(e.wentOutSeat() == 1, "canasta: the seat that went out is recorded");
    check(e.wasConcealed(), "canasta: laying the whole hand down in one turn is concealed");
    check(e.team(1).handScore > e.team(0).handScore,
          "canasta: the side that went out beat the side left holding cards");
    check(e.cardsInPlay() == 108, "canasta: the pack survived the whole hand");

    // Without a canasta you may not strip your hand to one card either: there
    // would be nothing legal left to do with it.
    std::array<std::vector<Card>, 4> poor = hands;
    poor[1] = { cd(Suit::Spades, kKing), cd(Suit::Hearts, kKing), cd(Suit::Clubs, kKing),
                cd(Suit::Spades, kQueen), cd(Suit::Hearts, kQueen), cd(Suit::Clubs, kQueen),
                cd(Suit::Spades, 5), cd(Suit::Hearts, 5), cd(Suit::Clubs, 5),
                cd(Suit::Diamonds, 5), cd(Suit::Spades, 5) };
    ca::Engine p;
    p.newGameFromStock(canastaStock(poor, 0, spare(), cd(Suit::Diamonds, 9)), 0);
    check(p.drawFromStock(), "canasta: seat one draws");
    check(p.meldCards({ cd(Suit::Spades, kKing), cd(Suit::Hearts, kKing), cd(Suit::Clubs, kKing),
                        cd(Suit::Spades, kQueen), cd(Suit::Hearts, kQueen),
                        cd(Suit::Clubs, kQueen) }),
          "canasta: kings and queens are 60, enough to open");
    check(!p.canMeldCards({ cd(Suit::Spades, 5), cd(Suit::Hearts, 5), cd(Suit::Clubs, 5),
                            cd(Suit::Diamonds, 5), cd(Suit::Spades, 5) }),
          "canasta: without a canasta you must keep a card to discard");
}

// Full games at every level, all four seats on autopilot. This is the check
// that matters: it proves the rules never deadlock, never lose a card, and
// always reach a winner.
void canastaFullGames()
{
    const ca::Level levels[3] = { ca::Level::Easy, ca::Level::Medium, ca::Level::Hard };
    const char* names[3] = { "easy", "medium", "hard" };

    for (int li = 0; li < 3; ++li) {
        int completed = 0;
        int hands = 0;
        int canastas = 0;
        int concealed = 0;
        long turns = 0;
        int worstHandTurns = 0;

        for (int game = 0; game < 6; ++game) {
            ca::Engine e;
            std::array<ca::Ai, ca::kSeats> ai { ca::Ai { levels[li] }, ca::Ai { levels[li] },
                                                ca::Ai { levels[li] }, ca::Ai { levels[li] } };
            for (int s = 0; s < ca::kSeats; ++s)
                ai[std::size_t(s)].seed(unsigned(game * 97 + s * 13 + li));
            e.newGame(unsigned(game * 7919 + li));

            bool sane = true;
            int handTurns = 0;
            // Generous ceiling: a real game is a few hundred turns, so this only
            // ever trips on a rule that has stopped making progress.
            for (long guard = 0; guard < 40000; ++guard) {
                if (e.phase() == ca::Engine::Phase::GameOver)
                    break;
                if (e.phase() == ca::Engine::Phase::HandOver) {
                    ++hands;
                    worstHandTurns = std::max(worstHandTurns, handTurns);
                    handTurns = 0;
                    for (int t = 0; t < ca::kTeams; ++t)
                        for (const ca::Meld& m : e.team(t).melds)
                            if (m.isCanasta(e.rules()))
                                ++canastas;
                    if (e.wasConcealed())
                        ++concealed;
                    e.nextHand();
                    continue;
                }
                if (e.cardsInPlay() != 108) {
                    sane = false;
                    break;
                }
                const int seat = e.currentSeat();
                ai[std::size_t(seat)].draw(e);
                if (e.phase() == ca::Engine::Phase::Play)
                    ai[std::size_t(seat)].playAndDiscard(e);
                // A turn must either finish or end the hand; if the seat has not
                // changed and we are still mid-turn, nothing is progressing.
                if (e.phase() == ca::Engine::Phase::Play && e.currentSeat() == seat) {
                    sane = false;
                    break;
                }
                ++turns;
                ++handTurns;
            }

            if (!sane)
                break;
            if (e.phase() == ca::Engine::Phase::GameOver) {
                ++completed;
                const int w = e.winner();
                if (w < 0 || e.team(w).score < e.rules().targetScore)
                    completed = -1000;
            }
        }

        std::printf("      %-6s %d/6 games finished, %d hands, %ld turns, %d canastas, "
                    "%d concealed, longest hand %d turns\n",
                    names[li], completed, hands, turns, canastas, concealed, worstHandTurns);
        check(completed == 6, "canasta: every game reaches a winner over the target");
        check(canastas > 0, "canasta: canastas actually get built");
    }
}

// The three levels have to be three strengths, not three labels. Hard and Easy
// play a match; Canasta carries a lot of luck, so the bar is only that Hard
// comes out ahead over a run of games.
// One level against another over a run of games. Canasta carries a lot of luck,
// so the bar is a clear majority rather than a clean sweep, and the margin is
// printed because it says more than the win count does.
int canastaMatch(ca::Level strong, ca::Level weak, const char* what, int games)
{
    int wins = 0;
    long margin = 0;

    for (int game = 0; game < games; ++game) {
        ca::Engine e;
        // Team 0 (seats 0 and 2) plays the stronger level; team 1 the weaker.
        std::array<ca::Ai, ca::kSeats> ai { ca::Ai { strong }, ca::Ai { weak },
                                            ca::Ai { strong }, ca::Ai { weak } };
        for (int s = 0; s < ca::kSeats; ++s)
            ai[std::size_t(s)].seed(unsigned(game * 31 + s));
        e.newGame(unsigned(game * 104729 + 7));

        for (long guard = 0; guard < 40000; ++guard) {
            if (e.phase() == ca::Engine::Phase::GameOver)
                break;
            if (e.phase() == ca::Engine::Phase::HandOver) {
                e.nextHand();
                continue;
            }
            const int seat = e.currentSeat();
            ai[std::size_t(seat)].draw(e);
            if (e.phase() == ca::Engine::Phase::Play)
                ai[std::size_t(seat)].playAndDiscard(e);
        }
        if (e.winner() == 0)
            ++wins;
        margin += e.team(0).score - e.team(1).score;
    }

    std::printf("      %s: won %d of %d, average margin %+ld\n", what, wins, games,
                margin / games);
    return wins;
}

// Whether the level that is supposed to be stronger has gone BACKWARDS: more
// than two standard deviations below an even split. Wins over `games` even
// matches vary by sqrt(games)/2, so two of those is sqrt(games).
//
// This is a deliberately weak claim, and it is the strongest one the top of the
// ladder can support — see canastaLevelsDiffer.
bool notTheWeakerPlayer(int wins, int games)
{
    return double(wins) >= double(games) / 2.0 - std::sqrt(double(games));
}

void canastaLevelsDiffer()
{
    // The ladder has to be a ladder. Each rung is checked against the one below
    // it, because a level that is only a label is worse than no level at all —
    // this is how Hard was found to be WEAKER than Medium, which is what a
    // player moving up to it would have run into.
    //
    // What it may CLAIM is bounded by what it can measure, and the top of the
    // ladder is close. Measured 2026-08-24 (GHUB-0110): Expert beats Hard by
    // about 1.75 points of win rate — 621 of 1200 games — which would need some
    // 3300 games to stand at two sigma and clears a bare majority at 240 as much
    // by luck as by strength. Asking for that majority made this check fail a
    // rules-CORRECT change (GHUB-0104) and a rules-WRONG one (GHUB-0101)
    // identically, and a check that cannot tell those apart is not measuring the
    // change, it is measuring the noise.
    //
    // So the top two rungs assert only that the stronger level has not fallen
    // behind. What a single new judgement actually DOES is locked by a
    // hand-built position instead — canastaAiHoldsWhileFrozen and
    // canastaFirstRoundSafeThrow are the pattern — which says why the play is
    // right rather than whether it got lucky. Owner's call, 2026-08-24.
    //
    // The two rungs against Easy keep the stricter claim. They win by miles, so
    // the sample carries it, and a level that has decayed to Easy still reddens.
    check(canastaMatch(ca::Level::Medium, ca::Level::Easy, "medium v easy", 24) * 2 > 24,
          "canasta: medium beats easy");
    check(canastaMatch(ca::Level::Hard, ca::Level::Easy, "hard v easy", 24) * 2 > 24,
          "canasta: hard beats easy");
    check(notTheWeakerPlayer(canastaMatch(ca::Level::Hard, ca::Level::Medium, "hard v medium", 120),
                             120),
          "canasta: hard has not fallen behind medium");
    check(notTheWeakerPlayer(canastaMatch(ca::Level::Expert, ca::Level::Hard, "expert v hard", 240),
                             240),
          "canasta: expert has not fallen behind hard");
}

// Two rule sets, because the owner's family plays its own. A house set has to
// drive the engine as well as the classic one does.
void canastaHouseRules()
{
    ca::Rules house;
    house.name = QStringLiteral("House");
    house.targetScore = 1500;
    house.handSize = 13;
    house.canastaSize = 6;
    house.openMinUnder1500 = 30;
    house.naturalCanastaBonus = 400;
    house.mixedCanastaBonus = 250;
    house.blackThreeBlocksPile = false;
    house.requireCanastaToGoOut = false;

    ca::Engine e { house };
    e.newGame(4242);
    check(e.rules().handSize == 13, "canasta: a house rule set is what gets dealt");
    check(e.hand(0).size() == 13, "canasta: thirteen cards each when the house says so");
    check(e.openRequirement(0) == 30, "canasta: the house opening minimum is used");
    check(e.cardsInPlay() == 108, "canasta: the pack is still whole");

    std::array<ca::Ai, ca::kSeats> ai { ca::Ai { ca::Level::Medium }, ca::Ai { ca::Level::Medium },
                                        ca::Ai { ca::Level::Medium }, ca::Ai { ca::Level::Medium } };
    bool finished = false;
    for (long guard = 0; guard < 40000 && !finished; ++guard) {
        if (e.phase() == ca::Engine::Phase::GameOver) {
            finished = true;
            break;
        }
        if (e.phase() == ca::Engine::Phase::HandOver) {
            e.nextHand();
            continue;
        }
        if (e.cardsInPlay() != 108)
            break;
        const int seat = e.currentSeat();
        ai[std::size_t(seat)].draw(e);
        if (e.phase() == ca::Engine::Phase::Play)
            ai[std::size_t(seat)].playAndDiscard(e);
    }
    check(finished, "canasta: a full game plays out under house rules too");

    // Six-card canastas, because the house said six.
    ca::Meld six { kKing, { cd(Suit::Spades, kKing), cd(Suit::Hearts, kKing),
                            cd(Suit::Clubs, kKing), cd(Suit::Diamonds, kKing),
                            cd(Suit::Spades, kKing), cd(Suit::Hearts, kKing) } };
    check(six.isCanasta(house), "canasta: a house canasta is whatever size the house says");
    check(!six.isCanasta(ca::Rules::classic()), "canasta: and the classic rules still want seven");
}

// A meld has to be readable at a glance, so wild cards go to the front of it
// wherever they arrive from. Nothing in the rules depends on the order, which
// is why this is checked on the meld rather than on the painting.
void canastaMeldOrder()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    // Seat 1 opens with four aces, then adds a joker and a two to them.
    hands[1] = { cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce), cd(Suit::Clubs, kAce),
                 cd(Suit::Diamonds, kAce), joker(true), cd(Suit::Spades, 2),
                 cd(Suit::Clubs, 5), cd(Suit::Clubs, 8), cd(Suit::Spades, kJack),
                 cd(Suit::Hearts, kJack), cd(Suit::Clubs, kQueen) };
    hands[2] = filler(10);
    hands[3] = filler(kKing);

    ca::Engine e;
    e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);
    e.drawFromStock();
    check(e.meldCards({ cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce), cd(Suit::Clubs, kAce),
                        cd(Suit::Diamonds, kAce) }),
          "canasta: four aces go down");
    check(e.meldCards({ joker(true), cd(Suit::Spades, 2) }, kAce),
          "canasta: a joker and a two join them");

    const ca::Meld* m = e.team(1).meldOfRank(kAce);
    const bool built = m != nullptr && m->size() == 6;
    check(built, "canasta: the aces meld is six cards");
    if (built) {
        check(isJoker(m->cards[0]), "canasta: the joker is at the front of the meld");
        check(m->cards[1].rank == 2, "canasta: then the two");
        check(m->cards[2].rank == kAce && m->cards[5].rank == kAce,
              "canasta: and the real cards follow");
    }
}

// The computer opening. Reported from a game where the other side needed only
// 50 and sat on it for a whole hand: the opening was built one rank at a time
// and without regard to the house rule on wild cards, so anything that had to
// be assembled from a complete meld plus a wild-boosted pair never went down —
// and what it did build was refused by the engine, silently, every turn.
void canastaAiOpens()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    // Three fours are 15 and a pair of queens with a two is 40. Neither opens
    // 50 alone; together they are 55.
    hands[1] = { cd(Suit::Spades, 4),  cd(Suit::Hearts, 4), cd(Suit::Diamonds, 4),
                 cd(Suit::Spades, kQueen), cd(Suit::Hearts, kQueen), cd(Suit::Clubs, 2),
                 cd(Suit::Spades, 6),  cd(Suit::Hearts, 7), cd(Suit::Diamonds, 8),
                 cd(Suit::Clubs, 10),  cd(Suit::Spades, kJack) };
    hands[2] = filler(5);
    hands[3] = filler(kKing);

    for (int strict = 0; strict < 2; ++strict) {
        ca::Rules r = ca::Rules::classic();
        // The rule that broke it: a pair may take one wild card, not two.
        r.wildsFewerThanNaturals = strict != 0;
        ca::Engine e { r };
        e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);

        ca::Ai ai { ca::Level::Medium };
        e.drawFromStock();
        ai.playAndDiscard(e);

        check(e.team(1).opened,
              strict ? "canasta: the computer opens by combining, under the house rule"
                     : "canasta: the computer opens by combining");
        const ca::Meld* fours = e.team(1).meldOfRank(4);
        const ca::Meld* queens = e.team(1).meldOfRank(kQueen);
        check(fours != nullptr && queens != nullptr,
              "canasta: laying the fours and the queens down together");
        if (queens != nullptr)
            check(queens->wilds() == 1, "canasta: with the wild card on the pair that needed it");
    }
}

// Opening AND taking the pile in one move, with the wild cards needed by the
// ranks going down beside the take rather than by the take itself. Reported
// from a real hand: a seven on the pile, two sevens in hand — already a meld —
// and 120 of queens, eights and wilds that the game refused, because a take
// handed every wild to the top card's rank whether it needed them or not.
void canastaTakeAndOpenTogether()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    // Seat 1 has a seven to throw.
    hands[1] = { cd(Suit::Spades, 7), cd(Suit::Clubs, kKing), cd(Suit::Hearts, kKing),
                 cd(Suit::Diamonds, kKing), cd(Suit::Spades, kKing), cd(Suit::Clubs, 4),
                 cd(Suit::Hearts, 4), cd(Suit::Diamonds, 4), cd(Suit::Spades, 4),
                 cd(Suit::Clubs, 5), cd(Suit::Hearts, 5) };
    // Seat 2 is the one with the hand: joker, a two, two queens, three eights
    // and two sevens.
    hands[2] = { joker(true), cd(Suit::Hearts, 2), cd(Suit::Hearts, kQueen),
                 cd(Suit::Spades, kQueen), cd(Suit::Clubs, 8), cd(Suit::Diamonds, 8),
                 cd(Suit::Spades, 8), cd(Suit::Clubs, 7), cd(Suit::Diamonds, 7),
                 cd(Suit::Diamonds, 6), cd(Suit::Spades, 10) };
    hands[3] = filler(kJack);

    ca::Rules r = ca::Rules::classic();
    r.wildsFewerThanNaturals = true;
    r.openMinUnder1500 = 120; // the band that hand was in, without the score
    ca::Engine e { r };
    e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);

    e.drawFromStock();
    e.discard(cd(Suit::Spades, 7));
    const bool ready = e.currentSeat() == 2 && e.pile().back().rank == 7 && !e.team(0).opened
        && e.openRequirement(0) == 120;
    check(ready, "canasta: a seven on the pile, 120 needed to open");

    // Joker 50, two 20, two queens 20, three eights 30 — 120 exactly, with the
    // two sevens taking the pile and counting nothing toward it.
    const std::vector<Card> lot { joker(true),           cd(Suit::Hearts, 2),
                                  cd(Suit::Hearts, kQueen), cd(Suit::Spades, kQueen),
                                  cd(Suit::Clubs, 8),    cd(Suit::Diamonds, 8),
                                  cd(Suit::Spades, 8),   cd(Suit::Clubs, 7),
                                  cd(Suit::Diamonds, 7) };
    check(e.canTakePile(lot), "canasta: the pile comes with the lay-down that opens you");
    check(e.takePile(lot), "canasta: and it is taken");

    const ca::Meld* sevens = e.team(0).meldOfRank(7);
    const ca::Meld* queens = e.team(0).meldOfRank(kQueen);
    const ca::Meld* eights = e.team(0).meldOfRank(8);
    const bool built = sevens != nullptr && queens != nullptr && eights != nullptr;
    check(built, "canasta: three melds go down at once");
    if (built) {
        // The sevens were already a meld, so they took no wild; both wilds went
        // where they were needed, and the pair of queens is the only rank that
        // needed one.
        check(sevens->size() == 3 && sevens->wilds() == 0,
              "canasta: the sevens take the pile on their own");
        check(queens->size() == 3 && queens->wilds() == 1,
              "canasta: a wild goes to the queens, which were two");
        check(eights->size() == 4 && eights->wilds() == 1,
              "canasta: and the other joins the eights");
    }
    check(e.team(0).opened, "canasta: which is what opens the side");
}

// Feeding the table while the pile is frozen. Reported as a partner's mistake,
// and it was one: every card laid down is a rank the opposition will then never
// throw, so while the pile is frozen and out of reach a side keeps its hand to
// itself. The exception is a canasta, which is worth more than any pile.
void canastaAiHoldsWhileFrozen()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    // Seat 1 opens with four aces, then holds two sevens and a spare ace.
    hands[1] = { cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce), cd(Suit::Clubs, kAce),
                 cd(Suit::Diamonds, kAce), cd(Suit::Spades, kAce), cd(Suit::Spades, 7),
                 cd(Suit::Hearts, 7), cd(Suit::Clubs, 7), cd(Suit::Spades, 2),
                 cd(Suit::Clubs, 5), cd(Suit::Hearts, 8) };
    hands[2] = filler(10);
    hands[3] = filler(kJack);

    for (int frozen = 0; frozen < 2; ++frozen) {
        ca::Engine e;
        // A wild card as the up-card freezes the pile from the start.
        e.newGameFromStock(canastaStock(hands, 0, spare(),
                                        frozen ? cd(Suit::Hearts, 2) : cd(Suit::Diamonds, 9)),
                           0);
        check(e.pileFrozen() == (frozen != 0),
              frozen ? "canasta: the pile starts frozen" : "canasta: the pile starts open");

        ca::Ai ai { ca::Level::Medium };
        e.drawFromStock();
        ai.playAndDiscard(e);

        const ca::Meld* aces = e.team(1).meldOfRank(kAce);
        check(aces != nullptr && aces->size() >= 4, "canasta: it opens either way");
        // The sevens are the tell: three of them stand as a meld on their own,
        // and the question is only whether it lays them down now.
        const ca::Meld* sevens = e.team(1).meldOfRank(7);
        check((sevens == nullptr) == (frozen != 0),
              frozen ? "canasta: and holds its sevens back while the pile is frozen"
                     : "canasta: and lays its sevens down while the pile is open");
    }
}

// The other half of holding while frozen (GHUB-0104): the release is keyed on
// HAND SIZE. A big hand has draws still to come, so a rank held back is a pair
// waiting to happen; a small one does not, and the points are better on the
// table than caught in it. One position proves both halves — the same sevens
// stay back on a hand of twelve and go down on a hand of seven.
void canastaAiPlaysOutWhenTheHandGetsSmall()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    // Four aces to open on, three sevens that stand as a meld on their own,
    // and four singles to throw.
    hands[1] = { cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce), cd(Suit::Clubs, kAce),
                 cd(Suit::Diamonds, kAce), cd(Suit::Spades, 7),  cd(Suit::Hearts, 7),
                 cd(Suit::Clubs, 7),      cd(Suit::Clubs, 4),    cd(Suit::Clubs, 5),
                 cd(Suit::Clubs, 6),      cd(Suit::Clubs, 8) };
    hands[2] = filler(10);
    hands[3] = filler(kJack);

    ca::Engine e;
    // Every draw is an ace, so the second turn opens on a hand of seven that
    // still holds the sevens. A wild up-card freezes the pile from the start.
    const std::vector<Card> aces(kBelowCount, cd(Suit::Diamonds, kAce));
    e.newGameFromStock(canastaStock(hands, 0, aces, cd(Suit::Hearts, 2)), 0);
    check(e.pileFrozen(), "canasta: the pile starts frozen");

    ca::Ai ai { ca::Level::Medium };
    check(e.drawFromStock(), "canasta: seat 1 draws to a hand of twelve");
    ai.playAndDiscard(e);
    const ca::Meld* opened = e.team(1).meldOfRank(kAce);
    check(opened != nullptr && opened->size() == 5, "canasta: and opens on its aces");
    check(e.team(1).meldOfRank(7) == nullptr,
          "canasta: holding the sevens back, because the hand is still big");

    check(e.drawFromStock() && e.discard(cd(Suit::Clubs, 10)), "canasta: seat 2 throws a ten");
    check(e.drawFromStock() && e.discard(cd(Suit::Clubs, kJack)), "canasta: seat 3 throws a jack");
    check(e.drawFromStock() && e.discard(cd(Suit::Clubs, 9)), "canasta: seat 0 throws a nine");

    check(e.currentSeat() == 1, "canasta: and the turn comes back round");
    check(e.drawFromStock(), "canasta: seat 1 draws again");
    check(e.hand(1).size() == 7, "canasta: onto a hand of seven, two-thirds of a deal gone");
    const std::vector<Card>& h = e.hand(1);
    check(std::count_if(h.begin(), h.end(), [](const Card& c) { return c.rank == 7; }) == 3,
          "canasta: still holding its three sevens");
    check(e.pileFrozen(), "canasta: with the pile still frozen");

    ai.playAndDiscard(e);
    check(e.team(1).meldOfRank(7) != nullptr,
          "canasta: which now go down, because a small hand will not build the pair");
}

// The owner's opening play, whole (GHUB-0122). Holding four eights and a joker
// against a bar of 50, you open on the JOKER and two eights — 70, over the bar
// — rather than on the four eights, which is 40 and does not even clear it. The
// other two eights stay in hand, and then you freeze the pack that only your
// side now holds the key to.
//
// One position, because the play is one move: opening on the minimum, choosing
// what the minimum is made of, and freezing. Any of the three alone is a
// different and worse move.
void canastaAiOpensOnAJokerToKeepThePair()
{
    std::array<std::vector<Card>, 4> hands;
    // Seat 0, 2 and 3 are driven by hand, so their cards only have to be
    // something to throw.
    hands[0] = filler(kKing);
    hands[1] = { cd(Suit::Spades, 8), cd(Suit::Hearts, 8), cd(Suit::Clubs, 8),
                 joker(true),         cd(Suit::Spades, 2), cd(Suit::Clubs, 2),
                 cd(Suit::Clubs, 4),  cd(Suit::Clubs, 5),  cd(Suit::Clubs, 6),
                 cd(Suit::Clubs, 9),  cd(Suit::Clubs, 10) };
    hands[2] = filler(10);
    hands[3] = filler(kJack);

    // Draws go seat 1, 2, 3, 0, then seat 1 again — and the stock is drawn from
    // the back, so the fifth from the end is seat 1's second draw. It is the
    // fourth eight; everything else is a nine nobody can use.
    // Queens, deliberately: seat 1 holds none, so no draw of its own hands it
    // a second pair the wild-card branch could open on instead.
    std::vector<Card> below(kBelowCount, cd(Suit::Clubs, kQueen));
    below[kBelowCount - 5] = cd(Suit::Diamonds, 8);

    ca::Engine e;
    e.newGameFromStock(canastaStock(hands, 0, below, cd(Suit::Diamonds, 7)), 0);
    check(!e.pileFrozen(), "canasta: the pack starts open");

    ca::Ai ai { ca::Level::Hard };
    check(e.drawFromStock(), "canasta: seat 1 draws");
    ai.playAndDiscard(e);
    check(!e.team(1).opened,
          "canasta: three eights and a joker cannot open on 50, so it does not");

    check(e.drawFromStock() && e.discard(cd(Suit::Clubs, 10)), "canasta: seat 2 throws a ten");
    check(e.drawFromStock() && e.discard(cd(Suit::Clubs, kJack)), "canasta: seat 3 throws a jack");
    check(e.drawFromStock() && e.discard(cd(Suit::Clubs, kKing)), "canasta: seat 0 throws a king");

    check(e.currentSeat() == 1 && e.pile().size() == 5,
          "canasta: the turn comes back round to a pack of five");
    check(e.drawFromStock(), "canasta: seat 1 draws its fourth eight");
    const std::vector<Card>& h = e.hand(1);
    check(std::count_if(h.begin(), h.end(), [](const Card& c) { return c.rank == 8; }) == 4,
          "canasta: so it holds four of them");

    ai.playAndDiscard(e);

    const ca::Meld* eights = e.team(1).meldOfRank(8);
    check(eights != nullptr && eights->size() == 3 && eights->wilds() == 1,
          "canasta: which opens on the joker and TWO eights, not on all four");
    const std::vector<Card>& after = e.hand(1);
    check(std::count_if(after.begin(), after.end(), [](const Card& c) { return c.rank == 8; }) == 2,
          "canasta: leaving the pair that takes a frozen pack in hand");
    check(e.pileFrozen(),
          "canasta: and freezing the pack, which only that pair now opens");
}

// The two limits on freezing, checked on figures rather than on a hand played
// into existence (GHUB-0113). Reaching a third freeze needs the pack taken
// twice in between, and the position where a freeze would cost us our own
// access needs a whole hand built to arrive at it.
void canastaFreezeLimits()
{
    check(ca::freezeBudgetLeft(0) && ca::freezeBudgetLeft(1),
          "canasta: two freezes a hand are allowed");
    check(!ca::freezeBudgetLeft(2) && !ca::freezeBudgetLeft(3),
          "canasta: and a third is not, however many wild cards are spare");

    const ca::Rules r = ca::Rules::classic();
    // A pack of ten, four of them in a rank this side has melded — over the
    // quarter that makes the next throw into it likely ours.
    std::vector<Card> pack;
    for (int i = 0; i < 4; ++i)
        pack.push_back(cd(Suit::Spades, 6));
    for (int i = 0; i < 6; ++i)
        pack.push_back(cd(Suit::Hearts, kKing));
    const std::vector<Card> hand { cd(Suit::Spades, 9), cd(Suit::Hearts, 9) };

    ca::Team mine;
    mine.opened = true;
    ca::Meld sixes;
    sixes.rank = 6;
    for (int i = 0; i < 3; ++i)
        sixes.cards.push_back(cd(Suit::Clubs, 6));
    mine.melds.push_back(sixes);

    check(ca::freezeCostsUsThePack(pack, hand, mine, r),
          "canasta: freezing a pack already coming back to us costs us the pack");

    // The same pack with only one card in a rank we hold is not coming back,
    // so the freeze costs us nothing.
    std::vector<Card> theirs(10, cd(Suit::Hearts, kKing));
    theirs[0] = cd(Suit::Spades, 6);
    check(!ca::freezeCostsUsThePack(theirs, hand, mine, r),
          "canasta: a pack that is not coming back is free to freeze");

    // And a side that has not opened is already held to two naturals out of
    // hand, so a freeze takes nothing from it whatever the pack looks like.
    ca::Team shut = mine;
    shut.opened = false;
    check(!ca::freezeCostsUsThePack(pack, hand, shut, r),
          "canasta: a side that has not opened is shut out of the pack already");
}

// The three reasons to spend a wild card freezing the pack (GHUB-0101), on
// figures. A price on a wild card is a tuned threshold and GHUB-0110 settled
// that the ladder cannot measure one; a reason can be stated and checked.
void canastaFreezeReasons()
{
    const ca::Rules r = ca::Rules::classic();
    const auto meldOf = [](int rank, int n) {
        ca::Meld m;
        m.rank = rank;
        for (int i = 0; i < n; ++i)
            m.cards.push_back(cd(i % 2 == 0 ? Suit::Spades : Suit::Hearts, rank));
        return m;
    };

    // A hand of six that touches nothing either side has down.
    const std::vector<Card> quiet { cd(Suit::Clubs, 4), cd(Suit::Clubs, 5),
                                    cd(Suit::Clubs, 6), cd(Suit::Clubs, 9),
                                    cd(Suit::Spades, 9), cd(Suit::Clubs, 10) };

    ca::Team bare;
    ca::Team open;
    open.opened = true;
    open.melds.push_back(meldOf(kKing, 4));

    check(ca::freezeIsWorthTheWild(quiet, bare, open, r),
          "canasta: freeze when their side is in and ours is not");
    check(!ca::freezeIsWorthTheWild(quiet, open, bare, r),
          "canasta: but not merely because we can, with nothing to gain by it");

    // Fishing: two nines in hand against nines already on our own table.
    ca::Team fishing;
    fishing.opened = true;
    fishing.melds.push_back(meldOf(9, 3));
    check(ca::freezeIsWorthTheWild(quiet, fishing, bare, r),
          "canasta: freeze when holding back a pair of a rank we have melded");

    // Feeding: a hand where a third or more of the cards throw into their melds.
    const std::vector<Card> feeders { cd(Suit::Clubs, kKing), cd(Suit::Spades, kKing),
                                      cd(Suit::Clubs, 4),     cd(Suit::Clubs, 5),
                                      cd(Suit::Clubs, 6),     cd(Suit::Clubs, 10) };
    check(ca::feedPressure(feeders, open, r) >= 1.0 / 3.0,
          "canasta: two kings in six against their king meld is a third of the hand");
    check(ca::feedPressure(quiet, open, r) == 0.0,
          "canasta: and a hand touching nothing of theirs feeds them nothing");
    check(ca::freezeIsWorthTheWild(feeders, open, open, r),
          "canasta: freeze when this hand will keep feeding them whatever it throws");

    // A rank they have closed is no longer a feeder under that house rule, so
    // the same hand stops arguing for a freeze.
    ca::Rules safe = r;
    safe.canastaMakesRankSafe = true;
    ca::Team closed;
    closed.opened = true;
    closed.melds.push_back(meldOf(kKing, 7));
    check(ca::feedPressure(feeders, closed, safe) == 0.0,
          "canasta: a rank they have closed cannot be fed, so it is no reason to freeze");
}

// Counting the pack (GHUB-0106). Two packs means eight of every rank, and the
// ones this seat can see are the ones nobody else can be holding. Nothing
// checked this before, though both halves of it were already in use.
void canastaCountsThePack()
{
    const auto meldOf = [](int rank, int n) {
        ca::Meld m;
        m.rank = rank;
        for (int i = 0; i < n; ++i)
            m.cards.push_back(cd(i % 2 == 0 ? Suit::Spades : Suit::Hearts, rank));
        return m;
    };

    const std::vector<Card> hand { cd(Suit::Clubs, kKing), cd(Suit::Spades, kKing),
                                   cd(Suit::Clubs, 9) };
    const std::vector<Card> pack { cd(Suit::Hearts, kKing), cd(Suit::Diamonds, 4),
                                   cd(Suit::Clubs, 4) };
    ca::Team mine;
    mine.melds.push_back(meldOf(kKing, 3));
    ca::Team theirs;
    theirs.melds.push_back(meldOf(kKing, 2));

    check(ca::seenSoFar(hand, pack, mine, theirs, kKing) == 8,
          "canasta: two in hand, one in the pack, three down and two more theirs is all eight");
    check(ca::seenSoFar(hand, pack, mine, theirs, 4) == 2,
          "canasta: a rank counts wherever it shows, melded or not");
    check(ca::seenSoFar(hand, pack, mine, theirs, 7) == 0,
          "canasta: and a rank nobody has shown is not accounted for at all");

    // A rank with all eight visible cannot take the pack off anybody, so it is
    // the safest throw there is — and nothing safer exists, which is what makes
    // the top of this slope the cliff rather than needing one of its own.
    const int pileSize = 20;
    const double dead = ca::packCountSafety(0, pileSize);
    check(dead > ca::packCountSafety(1, pileSize),
          "canasta: a rank fully accounted for is safer than one with a card out");
    check(ca::packCountSafety(1, pileSize) > ca::packCountSafety(4, pileSize),
          "canasta: and the more are unaccounted for, the more the throw risks");
    check(ca::packCountSafety(4, pileSize) > ca::packCountSafety(8, pileSize),
          "canasta: past the cap it still worsens, because the penalty keeps running");
    check(ca::packCountSafety(4, 40) < ca::packCountSafety(4, 4),
          "canasta: and the bigger the pack, the more an unaccounted rank costs");
}

// What a black three is worth as a block (GHUB-0109). It stops the pack being
// taken for exactly one turn, so it is worth what would be taken in that turn:
// a fat pack, and a seat to the left that could actually take it.
void canastaBlackThreeTiming()
{
    check(ca::blackThreeWorth(9, 1.0) > ca::blackThreeWorth(2, 1.0),
          "canasta: a black three is worth more thrown at a fat pack than a thin one");
    check(ca::blackThreeWorth(9, 0.3) < ca::blackThreeWorth(9, 1.0),
          "canasta: and worth less against a side that could not take it anyway");

    // The seat to the left is always an opponent — partners sit opposite — so
    // how live that side is grades the block exactly as it grades a dangerous
    // throw. A side already in takes the pack on ordinary terms; one still out
    // has to open off it, and on the top band that is a long way off.
    ca::Team in;
    in.opened = true;
    const ca::Team out;
    check(ca::blackThreeWorth(9, ca::throwCaution(in, 50))
              > ca::blackThreeWorth(9, ca::throwCaution(out, 120)),
          "canasta: so the block is spent on a live seat, not on one still shut out");
}

// The first canasta as insurance rather than a bonus (GHUB-0107). Under
// canastaNeededToScore a side that ends the hand without one has its melds and
// its red threes taken OFF its score rather than added, so the first canasta is
// worth far more than the 300 it pays — and getting one down beats holding
// cards back for the pack.
//
// One position, played twice: the same frozen table under Classic and under the
// minus rule. The hand is nine cards, deliberately clear of the hand-size
// release GHUB-0104 ships, so the only thing that can move the lone ace is this.
void canastaFirstCanastaIsInsurance()
{
    check(ca::closeFirstUnderAMinus(ca::Meld { 5, std::vector<Card>(6, cd(Suit::Spades, 5)) },
                                    ca::Meld { 6, std::vector<Card>(4, cd(Suit::Spades, 6)) }),
          "canasta: caught a minus, the meld nearest a canasta takes the wild card first");
    check(!ca::closeFirstUnderAMinus(ca::Meld { 6, std::vector<Card>(4, cd(Suit::Spades, 6)) },
                                     ca::Meld { 5, std::vector<Card>(6, cd(Suit::Spades, 5)) }),
          "canasta: and the one furthest away waits");

    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(kKing);
    // Three aces to open on — 60 clears the 50 bar — three sevens the trim
    // holds back, and five singles.
    hands[1] = { cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce), cd(Suit::Clubs, kAce),
                 cd(Suit::Spades, 7),    cd(Suit::Hearts, 7),    cd(Suit::Clubs, 7),
                 cd(Suit::Clubs, 4),     cd(Suit::Clubs, 5),     cd(Suit::Clubs, 6),
                 cd(Suit::Clubs, 9),     cd(Suit::Clubs, 10) };
    hands[2] = filler(10);
    hands[3] = filler(kJack);

    // A wild up-card is covered by another card off the stock, so the deal eats
    // one more than usual and every draw shifts down by one: seat 1's second
    // draw is the SIXTH from the end, not the fifth.
    std::vector<Card> below(kBelowCount, cd(Suit::Clubs, kQueen));
    below[kBelowCount - 6] = cd(Suit::Diamonds, kAce);

    for (int minus = 0; minus < 2; ++minus) {
        ca::Rules rules = ca::Rules::classic();
        rules.canastaNeededToScore = minus != 0;
        ca::Engine e;
        e.setRules(rules);
        // A wild up-card freezes the pack from the start.
        e.newGameFromStock(canastaStock(hands, 0, below, cd(Suit::Hearts, 2)), 0);
        check(e.pileFrozen(), "canasta: the pack starts frozen");
        check(e.rules().canastaNeededToScore == (minus != 0),
              "canasta: with the minus rule set as the arm asks");

        ca::Ai ai { ca::Level::Medium };
        check(e.drawFromStock(), "canasta: seat 1 draws");
        ai.playAndDiscard(e);
        const ca::Meld* opened = e.team(1).meldOfRank(kAce);
        check(opened != nullptr && opened->size() == 3,
              "canasta: and opens on three aces, the least the bar asks for");

        check(e.drawFromStock() && e.discard(cd(Suit::Clubs, 10)), "canasta: seat 2 throws");
        check(e.drawFromStock() && e.discard(cd(Suit::Clubs, kJack)), "canasta: seat 3 throws");
        check(e.drawFromStock() && e.discard(cd(Suit::Clubs, kKing)), "canasta: seat 0 throws");

        check(e.currentSeat() == 1, "canasta: the turn comes back round");
        check(e.drawFromStock(), "canasta: seat 1 draws its fourth ace");
        check(e.hand(1).size() == 9,
              "canasta: onto a hand of nine, clear of the hand-size release");
        const std::vector<Card>& h = e.hand(1);
        check(std::count_if(h.begin(), h.end(), [](const Card& c) { return c.rank == kAce; }) == 1,
              "canasta: holding one ace against the three already down");
        check(e.pileFrozen(), "canasta: with the pack still frozen");

        ai.playAndDiscard(e);
        const ca::Meld* now = e.team(1).meldOfRank(kAce);
        check(now != nullptr && now->size() == (minus ? 4 : 3),
              minus ? "canasta: caught a minus, the lone ace goes down towards the canasta"
                    : "canasta: under Classic it stays in hand, because the pack is the prize");
        check(e.team(1).meldOfRank(7) == nullptr,
              "canasta: while the sevens stay back either way, having nothing down yet");
    }
}

// Running the pack dead rather than letting the hand score (GHUB-0114). The
// owner's tactic, and it is worth points under House and nothing at all under
// Classic — so the check asks the rule both ways round.
void canastaRunsThePackDead()
{
    ca::Rules house = ca::Rules::classic();
    house.deadHandIfNobodyGoesOut = true;

    check(ca::runTheHandDead(-110, 0, 6, house),
          "canasta: behind with the stock nearly gone, kill the hand rather than score it");
    check(!ca::runTheHandDead(-110, 0, 6, ca::Rules::classic()),
          "canasta: under Classic the hand is scored where it stands, so there is nothing to gain");
    check(!ca::runTheHandDead(-110, 0, 40, house),
          "canasta: and not with a stock nobody can empty in the turns left");
    check(!ca::runTheHandDead(400, 0, 6, house),
          "canasta: nor when the hand as it stands is ours");
    check(!ca::runTheHandDead(0, house.goingOutBonus, 6, house),
          "canasta: nor when it is theirs by less than the going-out bonus is worth");
    check(ca::runTheHandDead(0, house.goingOutBonus + 1, 6, house),
          "canasta: but a point past that is enough");

    // And the seat acts on it: a pack it could take, and it draws instead,
    // because taking the pack does not empty the stock and drawing does.
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(4);
    hands[1] = filler(kKing); // 110 points in hand, and a king on the pack
    hands[2] = filler(5);
    hands[3] = filler(6);
    const std::vector<Card> below(6, cd(Suit::Clubs, 9));

    for (int dead = 0; dead < 2; ++dead) {
        ca::Rules rules = ca::Rules::classic();
        rules.deadHandIfNobodyGoesOut = dead != 0;
        ca::Engine e;
        e.setRules(rules);
        e.newGameFromStock(canastaStock(hands, 0, below, cd(Suit::Diamonds, kKing)), 0);

        check(e.currentSeat() == 1 && e.stockCount() == 6,
              "canasta: seat 1 to play with six cards left in the stock");
        std::vector<Card> take;
        check(e.findPileTake(take),
              "canasta: and a pack it could take, opening on kings off the top card");

        ca::Ai ai { ca::Level::Hard };
        const bool took = ai.draw(e);
        check(took == (dead == 0),
              dead ? "canasta: running it dead, it leaves the pack and draws instead"
                   : "canasta: with the hand scored where it stands, it takes the pack");
    }
}

// Fishing (GHUB-0103): throwing one of three or more of a rank, one at a time,
// so the seat that discards to you reads it as safe, follows with it, and hands
// you the pack. Checked on figures — what the bait is worth is the judgement,
// and the position it pays off in is four turns away by construction.
void canastaFishing()
{
    check(ca::fishingWorth(2, 12, 4) == 0.0,
          "canasta: two of a rank is the key itself, and breaking it is not fishing");
    check(ca::fishingWorth(3, 12, 4) > 0.0,
          "canasta: three is bait, because one can go and a pair still stands");
    check(ca::fishingWorth(4, 12, 4) > 0.0, "canasta: and so is four");

    check(ca::fishingWorth(3, 3, 4) == 0.0,
          "canasta: but not at a pack too thin to be worth advertising for");
    check(ca::fishingWorth(3, 20, 4) > ca::fishingWorth(3, 6, 4),
          "canasta: and the fatter the pack, the more the bait is worth");

    check(ca::fishingWorth(3, 12, 0) == 0.0,
          "canasta: nobody holds one to follow with once all eight are accounted for");
    check(ca::fishingWorth(3, 12, 1) < ca::fishingWorth(3, 12, 3),
          "canasta: and the fewer left out there, the less likely the bait is taken");
    check(ca::fishingWorth(3, 12, 3) == ca::fishingWorth(3, 12, 6),
          "canasta: past three unaccounted for, one more changes nothing");
}

// What a discard's safety judgement is worth against this opponent
// (GHUB-0121). Checked on figures rather than on a position played into
// existence, the way discardRisk and openRequirementFor already are — the claim
// is about the shape of a curve, and a hand of cards is a poor way to state one.
void canastaThrowCaution()
{
    ca::Team opened;
    opened.opened = true;
    ca::Team shut; // has not opened

    // An opened side has to come back exactly 1.0, or every existing judgement
    // in chooseDiscard is silently rescaled by this change.
    check(std::abs(ca::throwCaution(opened, 50) - 1.0) < 1e-9,
          "canasta: an opened side weighs the throw exactly as it always did");
    check(std::abs(ca::throwCaution(opened, 120) - 1.0) < 1e-9,
          "canasta: whatever band they opened on");

    // The owner's reading: while they cannot open they can barely take the pack,
    // and how badly they are stuck is what grades it.
    check(ca::throwCaution(shut, 50) < ca::throwCaution(opened, 50),
          "canasta: an unopened side is less dangerous to throw at");
    check(ca::throwCaution(shut, 120) < ca::throwCaution(shut, 90)
              && ca::throwCaution(shut, 90) < ca::throwCaution(shut, 50)
              && ca::throwCaution(shut, 50) < ca::throwCaution(shut, 15),
          "canasta: 15 and 50 barely matter, 90 does, 120 most of all");

    // Never to nothing. Opening off the pack is hard rather than impossible,
    // and it is exactly how a side stuck all hand comes back in one move.
    check(ca::throwCaution(shut, 120) >= 0.30 * 0.999,
          "canasta: but caution never falls away altogether");
    check(ca::throwCaution(shut, 15) > 0.85,
          "canasta: and at the easiest band it barely moves at all");
}

// Going out the house way (GHUB-0120): the last action is a thrown card, so a
// lay-down that empties the hand is refused however legal its melds are. The
// one exception is the hand that finishes on all four black threes — everything
// else onto melds, the threes down together, and nothing thrown.
//
// Both halves are proved from the same position under both rule sets, because
// "refused" says nothing on its own: the identical lay-down has to be ALLOWED
// with the rule off, or the check is only observing a malformed meld. The
// refusal message is asserted for the same reason.
void canastaHouseGoesOutOnAThrownCard()
{
    // Seven aces open and make a canasta in one move, so going out is legal
    // from here and this check is about the way out rather than about the
    // canasta requirement standing in front of it.
    const std::vector<Card> aces { cd(Suit::Spades, kAce),  cd(Suit::Hearts, kAce),
                                   cd(Suit::Clubs, kAce),   cd(Suit::Diamonds, kAce),
                                   cd(Suit::Spades, kAce),  cd(Suit::Hearts, kAce),
                                   cd(Suit::Clubs, kAce) };
    const std::vector<Card> threes { cd(Suit::Spades, 3), cd(Suit::Clubs, 3),
                                     cd(Suit::Spades, 3), cd(Suit::Clubs, 3) };
    const std::vector<Card> sevens { cd(Suit::Spades, 7), cd(Suit::Hearts, 7),
                                     cd(Suit::Clubs, 7), cd(Suit::Diamonds, 7) };

    // Cards come off the back, so the last card here is the one drawn first: an
    // ace, which extends the canasta and makes the remaining hand a lay-down
    // that empties itself in both arms.
    std::vector<Card> below(kBelowCount - 1, cd(Suit::Clubs, 9));
    below.push_back(cd(Suit::Diamonds, kAce));

    for (int house = 0; house < 2; ++house) {
        for (int blackThrees = 0; blackThrees < 2; ++blackThrees) {
            const std::vector<Card>& rest = blackThrees != 0 ? threes : sevens;

            std::array<std::vector<Card>, 4> hands;
            hands[0] = filler(10);
            hands[1] = aces;
            hands[1].insert(hands[1].end(), rest.begin(), rest.end());
            hands[2] = filler(kJack);
            hands[3] = filler(kKing);

            ca::Rules r = ca::Rules::classic();
            r.goingOutNeedsADiscard = house != 0;
            ca::Engine e(r);
            e.newGameFromStock(canastaStock(hands, 0, below, cd(Suit::Diamonds, 6)), 0);
            check(e.currentSeat() == 1, "canasta: seat 1 leads on the going-out check");
            e.drawFromStock();
            check(e.meldCards(aces), "canasta: seven aces open and make a canasta");

            // Everything still in hand, laid down at once. Under the classic
            // rule this is a legal way out; under the house rule only the black
            // threes earn it.
            std::vector<Card> out { cd(Suit::Diamonds, kAce) };
            out.insert(out.end(), rest.begin(), rest.end());
            check(e.hand(1).size() == out.size(),
                  "canasta: the lay-down being tested is the whole hand");

            // meldCards rather than canMeldCards: the const form validates into
            // a LOCAL error string and never touches m_error, so lastError()
            // after it is whatever some earlier move left behind. Playing the
            // move for real is also the stronger claim — the allowed arms end
            // the hand rather than merely being judged legal.
            const bool allowed = e.meldCards(out);
            const bool expected = house == 0 || blackThrees != 0;
            check(allowed == expected,
                  house == 0 ? "canasta: the classic game lets you meld out with anything"
                             : (blackThrees != 0
                                    ? "canasta: the house rule lets you finish on four black threes"
                                    : "canasta: but otherwise refuses a lay-down that empties "
                                      "the hand"));
            if (allowed)
                check(e.phase() != ca::Engine::Phase::Play && e.wentOutSeat() == 1,
                      "canasta: and the hand ends on it, with seat 1 out");
            else
                check(e.lastError().contains(QStringLiteral("throwing your last card")),
                      "canasta: and refuses it for that reason, not a malformed meld");
        }
    }
}

// What a side that has not opened may do with the pile, and what changing the
// rules mid-game does to the game.
//
// Classic Canasta lets an unopened side take the pile AS its opening — that is
// the strongest move in the game — but freezes it against them, so they need
// two natural cards matching the top rather than one and a wild. Both halves
// are checked here, because the first was reported as missing on the strength
// of having seen the second.
void canastaUnopenedPileAndLiveRules()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    hands[1] = { cd(Suit::Hearts, kKing), cd(Suit::Clubs, 4), cd(Suit::Hearts, 4),
                 cd(Suit::Diamonds, 4), cd(Suit::Spades, 5), cd(Suit::Hearts, 5),
                 cd(Suit::Clubs, 5), cd(Suit::Spades, 6), cd(Suit::Hearts, 6),
                 cd(Suit::Clubs, 6), cd(Suit::Spades, 8) };
    // Two kings AND one king with a wild, so both routes can be tried from the
    // same hand.
    hands[2] = { joker(true), cd(Suit::Clubs, 2), cd(Suit::Diamonds, kAce),
                 cd(Suit::Spades, kAce), cd(Suit::Spades, kKing), cd(Suit::Clubs, kKing),
                 cd(Suit::Diamonds, 10), cd(Suit::Clubs, 9), cd(Suit::Diamonds, 9),
                 cd(Suit::Clubs, 7), cd(Suit::Clubs, 3) };
    hands[3] = filler(kJack);

    ca::Engine e; // classic
    e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);
    e.drawFromStock();
    e.discard(cd(Suit::Hearts, kKing));
    check(e.currentSeat() == 2 && !e.team(0).opened && e.pile().back().rank == kKing,
          "canasta: an unopened side is on lead with a king on the pile");

    // Two natural kings: legal in the classic game, and it opens the side off
    // the pile. This is the move that was seen being played.
    const std::vector<Card> pair { cd(Suit::Spades, kKing), cd(Suit::Clubs, kKing),
                                   cd(Suit::Diamonds, kAce), cd(Suit::Spades, kAce),
                                   joker(true) };
    check(e.canTakePile(pair), "canasta: classic lets an unopened side take the pile to open");

    // One king and a wild: refused in the classic game, because the pile is
    // frozen against a side that has not opened.
    check(!e.canTakePile({ cd(Suit::Spades, kKing), cd(Suit::Clubs, 2), cd(Suit::Diamonds, kAce),
                           cd(Suit::Spades, kAce), joker(true) }),
          "canasta: but not with one king and a wild card");

    // The rule change lands on the game in front of you rather than throwing it
    // away — same seat, same hands, same scores, and now the move is legal.
    ca::Rules live = ca::Rules::classic();
    live.pileFrozenUntilOpened = false;
    const int seatBefore = e.currentSeat();
    const std::size_t handBefore = e.hand(2).size();
    e.applyRules(live);
    check(e.currentSeat() == seatBefore && e.hand(2).size() == handBefore
              && e.cardsInPlay() == 108,
          "canasta: changing the rules keeps the hand exactly where it was");
    check(e.canTakePile({ cd(Suit::Spades, kKing), cd(Suit::Clubs, 2), cd(Suit::Diamonds, kAce),
                          cd(Suit::Spades, kAce), joker(true) }),
          "canasta: and the house rule takes effect on this hand, not the next one");

    // The three numbers that shaped the deal do not move under it.
    ca::Rules bigger = live;
    bigger.handSize = 15;
    bigger.decks = 3;
    e.applyRules(bigger);
    check(e.rules().handSize == 11 && e.rules().decks == 2 && e.cardsInPlay() == 108,
          "canasta: the deal's own numbers wait for the next deal");
    check(e.pendingRules().handSize == 15, "canasta: and are what the next game is dealt from");
}

// Which wild card lands where, when it decides whether a move is legal at all.
// Reported: 90 needed, a king on the pile, and a hand holding a joker, a two,
// two aces and a king. The kings take the pile and count nothing toward the
// opening, so the joker has to go on the ACES — 20 + 20 + 50 is the 90 — and
// the two goes on the kings. Placed the other way round it is 60 and refused.
void canastaWildValueGoesWhereItCounts()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    hands[1] = { cd(Suit::Hearts, kKing), cd(Suit::Clubs, 4), cd(Suit::Hearts, 4),
                 cd(Suit::Diamonds, 4), cd(Suit::Spades, 5), cd(Suit::Hearts, 5),
                 cd(Suit::Clubs, 5), cd(Suit::Spades, 6), cd(Suit::Hearts, 6),
                 cd(Suit::Clubs, 6), cd(Suit::Spades, 8) };
    hands[2] = { joker(true), cd(Suit::Clubs, 2), cd(Suit::Diamonds, kAce),
                 cd(Suit::Spades, kAce), cd(Suit::Spades, kKing), cd(Suit::Clubs, 10),
                 cd(Suit::Diamonds, 10), cd(Suit::Clubs, 9), cd(Suit::Diamonds, 9),
                 cd(Suit::Clubs, 7), cd(Suit::Clubs, 3) };
    hands[3] = filler(kJack);

    ca::Rules r = ca::Rules::classic();
    r.pileMeldCountsToOpen = false;  // the house rule that makes placement matter
    r.pileFrozenUntilOpened = false; // and the one that lets an unopened side take it
    r.openMinUnder1500 = 90;
    ca::Engine e { r };
    e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);

    e.drawFromStock();
    e.discard(cd(Suit::Hearts, kKing));
    check(e.currentSeat() == 2 && e.pile().back().rank == kKing && e.openRequirement(0) == 90,
          "canasta: a king on the pile, 90 needed to open");

    const std::vector<Card> lot { joker(true), cd(Suit::Clubs, 2), cd(Suit::Diamonds, kAce),
                                  cd(Suit::Spades, kAce), cd(Suit::Spades, kKing) };
    check(e.canTakePile(lot), "canasta: the joker opens on the aces while the two takes the pile");
    check(e.takePile(lot), "canasta: and the move goes through");

    const ca::Meld* aces = e.team(0).meldOfRank(kAce);
    const ca::Meld* kings = e.team(0).meldOfRank(kKing);
    const bool built = aces != nullptr && kings != nullptr;
    check(built, "canasta: both melds go down");
    if (built) {
        check(aces->value(r) == 90, "canasta: the aces are the 90 that opens the side");
        check(kings->size() == 3, "canasta: and the kings are a meld with the pile's king in it");
    }
    check(e.team(0).opened, "canasta: the side is open");

    // And the classic rule the house one lifts: the pile is frozen against a
    // side that has not opened, so one king and a wild is not enough there.
    ca::Rules strict = r;
    strict.pileFrozenUntilOpened = true;
    ca::Engine frozen { strict };
    frozen.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);
    frozen.drawFromStock();
    frozen.discard(cd(Suit::Hearts, kKing));
    check(!frozen.canTakePile(lot),
          "canasta: classic keeps the pile frozen until a side has opened");
    check(frozen.canTakePile({ joker(true), cd(Suit::Clubs, 2), cd(Suit::Diamonds, kAce),
                               cd(Suit::Spades, kAce) })
              == false,
          "canasta: and no lay-down without two kings takes it");
}

// A side that never made a canasta counts nothing in its favour. Reported from
// a hand where the other side was caught with two melds, a red three and both
// hands full, and still came out +45 — which is right in the classic game and
// wrong at the owner's table.
void canastaCanastaNeededToScore()
{
    // Their table: five sixes and five aces down, a red three, and sixteen
    // cards still in the two hands. No canasta anywhere.
    ca::Team them;
    them.opened = true;
    them.melds.push_back(ca::Meld { 6,
                                    { cd(Suit::Spades, 6), cd(Suit::Hearts, 6),
                                      cd(Suit::Clubs, 6), cd(Suit::Diamonds, 6),
                                      cd(Suit::Spades, 6) } });
    them.melds.push_back(ca::Meld { kAce,
                                    { cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce),
                                      cd(Suit::Clubs, kAce), cd(Suit::Diamonds, kAce),
                                      cd(Suit::Spades, kAce) } });
    them.redThrees.push_back(cd(Suit::Hearts, 3));
    const std::vector<Card> west { cd(Suit::Spades, kKing), cd(Suit::Hearts, kQueen),
                                   cd(Suit::Clubs, 9) };
    const std::vector<Card> east { cd(Suit::Diamonds, 4) };

    const ca::Rules classic = ca::Rules::classic();
    ca::Rules house = classic;
    house.canastaNeededToScore = true;

    // Melded: five sixes at 5 and five aces at 20 is 125. In hand: 10 + 10 + 10
    // + 5 is 35. A red three is 100 either way; only its sign moves.
    check(ca::handScoreFor(them, west, east, false, false, classic) == 125 + 100 - 35,
          "canasta: classic pays a side that opened but never made a canasta");
    check(ca::handScoreFor(them, west, east, false, false, house) == -125 - 100 - 35,
          "canasta: the house rule takes their melds and their red three off them");

    // A canasta anywhere on the side and everything counts again, house rule or
    // not — the rule is about having one, not about how much is down.
    them.melds[0].cards.push_back(cd(Suit::Hearts, 6));
    them.melds[0].cards.push_back(cd(Suit::Clubs, 6));
    check(them.melds[0].isCanasta(house), "canasta: seven sixes are a canasta");
    const int melded = 7 * 5 + 5 * 20;
    check(ca::handScoreFor(them, west, east, false, false, house)
              == melded + house.naturalCanastaBonus + 100 - 35,
          "canasta: and with one, the same side scores as it always did");

    // The going-out side is unaffected by the rule, since it needs a canasta to
    // go out at all.
    check(ca::handScoreFor(them, west, east, true, false, house)
              == melded + house.naturalCanastaBonus + 100 - 35 + house.goingOutBonus,
          "canasta: going out still pays on top");
}

// Opening with wild cards spread across two ranks. Reported from a real hand:
// 90 needed, two jacks, two tens, a joker and a two — 110 in total, and no way
// to lay it down, because the engine refused a multi-rank lay-down containing a
// wild rather than working out where the wilds had to go.
void canastaWildsAcrossRanks()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    hands[1] = { joker(true), cd(Suit::Clubs, 2), cd(Suit::Clubs, kJack),
                 cd(Suit::Hearts, kJack), cd(Suit::Diamonds, 10), cd(Suit::Clubs, 10),
                 cd(Suit::Spades, kQueen), cd(Suit::Hearts, 8), cd(Suit::Clubs, 7),
                 cd(Suit::Diamonds, 6), cd(Suit::Spades, 4) };
    hands[2] = filler(5);
    hands[3] = filler(kKing);

    const std::vector<Card> lot { joker(true), cd(Suit::Clubs, 2), cd(Suit::Clubs, kJack),
                                  cd(Suit::Hearts, kJack), cd(Suit::Diamonds, 10),
                                  cd(Suit::Clubs, 10) };

    // The opening band that hand was in: 1500 to 2999 wants 90.
    ca::Rules r = ca::Rules::classic();
    r.wildsFewerThanNaturals = true; // the house set it was played under
    ca::Engine e { r };
    e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);
    check(e.openRequirement(1) == 50, "canasta: a side on nothing needs 50");

    e.drawFromStock();
    check(e.canMeldCards(lot), "canasta: two ranks and two wilds can go down together");
    check(e.meldCards(lot), "canasta: and they do");

    const ca::Meld* jacks = e.team(1).meldOfRank(kJack);
    const ca::Meld* tens = e.team(1).meldOfRank(10);
    const bool split = jacks != nullptr && tens != nullptr && jacks->size() == 3
        && tens->size() == 3 && jacks->wilds() == 1 && tens->wilds() == 1;
    check(split, "canasta: one wild each, rather than both on one meld");
    check(e.team(1).opened, "canasta: which is what opens the side");
    check(jacks->value(r) + tens->value(r) == 110, "canasta: worth the 110 it looks worth");

    // The rule that a meld keeps more real cards than wild ones still binds:
    // two naturals cannot take two wilds however they are spread.
    ca::Engine tight { r };
    tight.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);
    tight.drawFromStock();
    check(!tight.canMeldCards({ joker(true), cd(Suit::Clubs, 2), cd(Suit::Clubs, kJack),
                                cd(Suit::Hearts, kJack) }),
          "canasta: two wilds on one pair of jacks is still refused");
}

// A meld holds more real cards than wild ones: three sixes carry two wilds, and
// the third wild waits for a fourth six. Checked in exactly that shape.
void canastaWildsFewerThanNaturals()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    // Four aces to open with, then sixes and wilds to build on.
    hands[1] = { cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce), cd(Suit::Clubs, kAce),
                 cd(Suit::Diamonds, kAce), cd(Suit::Spades, 6), cd(Suit::Hearts, 6),
                 cd(Suit::Clubs, 6), cd(Suit::Diamonds, 6), joker(true), joker(false),
                 cd(Suit::Spades, 2) };
    hands[2] = filler(10);
    hands[3] = filler(kKing);

    const std::vector<Card> threeSixes { cd(Suit::Spades, 6), cd(Suit::Hearts, 6),
                                         cd(Suit::Clubs, 6) };

    for (int strict = 0; strict < 2; ++strict) {
        ca::Rules r = ca::Rules::classic();
        r.wildsFewerThanNaturals = strict != 0;
        ca::Engine e { r };
        e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);

        e.drawFromStock();
        check(e.meldCards({ cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce),
                            cd(Suit::Clubs, kAce), cd(Suit::Diamonds, kAce) }),
              "canasta: four aces open the side");
        check(e.meldCards(threeSixes), "canasta: and three sixes go down beside them");

        // Two wilds onto three sixes is fine either way: three beats two.
        check(e.meldCards({ joker(true), joker(false) }, 6),
              "canasta: two wilds join three sixes");
        // The third would make it three against three.
        check(e.canMeldCards({ cd(Suit::Spades, 2) }, 6) == (strict == 0),
              strict ? "canasta: a third wild is refused while the sixes are only three"
                     : "canasta: the classic game allows the third wild");

        // A fourth six changes the count, and then it is allowed.
        check(e.meldCards({ cd(Suit::Diamonds, 6) }, 6), "canasta: a fourth six goes down");
        check(e.canMeldCards({ cd(Suit::Spades, 2) }, 6),
              "canasta: and now the third wild is welcome");
    }
}

// A house rule with teeth: once a canasta is made it is finished, so it takes
// no more cards and the other side can throw that rank away safely.
void canastaClosedCanasta()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    // Seven aces: enough to open and make the canasta in one lay-down.
    hands[1] = { cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce), cd(Suit::Clubs, kAce),
                 cd(Suit::Diamonds, kAce), cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce),
                 cd(Suit::Clubs, kAce), cd(Suit::Diamonds, kAce), cd(Suit::Clubs, 5),
                 cd(Suit::Clubs, 8), cd(Suit::Spades, kQueen) };
    // Seat 2 throws an ace, which seat 3 — seat 1's partner — would love.
    hands[2] = { cd(Suit::Hearts, kAce), cd(Suit::Spades, 10), cd(Suit::Hearts, 10),
                 cd(Suit::Clubs, 10), cd(Suit::Diamonds, 10), cd(Suit::Spades, kJack),
                 cd(Suit::Hearts, kJack), cd(Suit::Clubs, kJack), cd(Suit::Diamonds, kJack),
                 cd(Suit::Spades, 9), cd(Suit::Hearts, 9) };
    hands[3] = { cd(Suit::Spades, kAce), cd(Suit::Clubs, kAce), cd(Suit::Spades, kKing),
                 cd(Suit::Hearts, kKing), cd(Suit::Clubs, kKing), cd(Suit::Diamonds, kKing),
                 cd(Suit::Spades, 7), cd(Suit::Hearts, 7), cd(Suit::Clubs, 7),
                 cd(Suit::Diamonds, 7), cd(Suit::Spades, 8) };

    const std::vector<Card> seven { cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce),
                                    cd(Suit::Clubs, kAce), cd(Suit::Diamonds, kAce),
                                    cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce),
                                    cd(Suit::Clubs, kAce) };

    for (int safe = 0; safe < 2; ++safe) {
        ca::Rules r = ca::Rules::classic();
        r.canastaMakesRankSafe = safe != 0;
        ca::Engine e { r };
        e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);

        e.drawFromStock();
        check(e.meldCards(seven), "canasta: seven aces make a canasta in one go");
        check(e.team(1).meldOfRank(kAce)->isCanasta(r), "canasta: and it is a canasta");
        e.discard(cd(Suit::Spades, kQueen));

        // Seat 2 throws the ace onto the pile.
        e.drawFromStock();
        e.discard(cd(Suit::Hearts, kAce));
        const bool ready = e.currentSeat() == 3 && e.pile().back().rank == kAce;
        check(ready, "canasta: an ace is thrown to the side holding the ace canasta");

        // Taking the pile is the draw, so that comes first. This is the whole
        // of the rule: the ace is a safe discard against a side holding an ace
        // canasta.
        check(e.canTakePile({ cd(Suit::Spades, kAce), cd(Suit::Clubs, kAce) }) == (safe == 0),
              safe ? "canasta: a rank your side has a canasta in cannot take the pile"
                   : "canasta: and without the rule it takes the pile as usual");

        // The canasta itself stays open either way — your own side goes on
        // adding to it, which is what separates this from closing the meld.
        e.drawFromStock();
        check(e.canMeldCards({ cd(Suit::Spades, kAce) }, kAce),
              safe ? "canasta: your own side still adds to that canasta"
                   : "canasta: a canasta takes another card in the classic game");
        check(e.meldCards({ cd(Suit::Spades, kAce) }, kAce)
                  && e.team(1).meldOfRank(kAce)->size() == 8,
              "canasta: and the canasta grows to eight");
    }
}

// Two of the owner's family's rules, both off in the classic set.
//
// The first holds the whole opening round open: nobody lays anything down until
// every seat has played once, so the pile has something in it before anyone can
// take it. The second stops the pile being what opens you — the meld that
// captures the top card counts nothing toward the minimum, so the minimum has
// to be made up from the other melds going down beside it.
void canastaFirstRoundAndPileOpening()
{
    std::array<std::vector<Card>, 4> hands;
    hands[0] = filler(9);
    // Seat 1 leads, and can open on its very first turn with four aces — worth
    // 80 against a minimum of 50 — unless the first-round rule stops it.
    hands[1] = { cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce), cd(Suit::Clubs, kAce),
                 cd(Suit::Diamonds, kAce), cd(Suit::Diamonds, 6), cd(Suit::Clubs, 5),
                 cd(Suit::Clubs, 8), cd(Suit::Spades, kJack), cd(Suit::Hearts, kJack),
                 cd(Suit::Clubs, kJack), cd(Suit::Spades, kQueen) };
    hands[2] = filler(10);
    hands[3] = filler(kKing);

    const std::vector<Card> aces { cd(Suit::Spades, kAce), cd(Suit::Hearts, kAce),
                                   cd(Suit::Clubs, kAce), cd(Suit::Diamonds, kAce) };

    for (int held = 0; held < 2; ++held) {
        ca::Rules r = ca::Rules::classic();
        r.noMeldingFirstRound = held != 0;
        ca::Engine e { r };
        e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);

        check(e.meldingAllowed() == (held == 0),
              held ? "canasta: the first round starts closed to melding"
                   : "canasta: classic lets you open on the first turn");
        e.drawFromStock();
        check(e.canMeldCards(aces) == (held == 0),
              held ? "canasta: four aces cannot open in the first round"
                   : "canasta: four aces open straight away in the classic game");

        // Round the table once. Everyone draws and throws; nobody lays down.
        e.discard(cd(Suit::Spades, kQueen));
        for (int i = 0; i < 3; ++i) {
            const int seat = e.currentSeat();
            check(e.meldingAllowed() == (held == 0),
                  held ? "canasta: still closed part way round the first round"
                       : "canasta: still open part way round the first round");
            e.drawFromStock();
            e.discard(e.hand(seat).front());
        }

        check(e.currentSeat() == 1, "canasta: the first round is back round to the leader");
        check(e.meldingAllowed(), "canasta: melding is open once every seat has played");
        e.drawFromStock();
        check(e.canMeldCards(aces), "canasta: and the aces go down on the second round");
    }

    // The pile rule, in the shape it was reported: a six is thrown, and the
    // next seat holds a joker and two sixes against a minimum of 50.
    hands[2] = { joker(true), cd(Suit::Spades, 6), cd(Suit::Hearts, 6), cd(Suit::Spades, kAce),
                 cd(Suit::Hearts, kAce), cd(Suit::Clubs, kAce), cd(Suit::Diamonds, kAce),
                 cd(Suit::Clubs, kJack), cd(Suit::Spades, kJack), cd(Suit::Hearts, kJack),
                 cd(Suit::Clubs, 9) };
    const std::vector<Card> sixes { joker(true), cd(Suit::Spades, 6), cd(Suit::Hearts, 6) };
    std::vector<Card> sixesAndAces = sixes;
    sixesAndAces.insert(sixesAndAces.end(), aces.begin(), aces.end());

    for (int strict = 0; strict < 2; ++strict) {
        ca::Rules r = ca::Rules::classic();
        r.pileMeldCountsToOpen = strict == 0;
        ca::Engine e { r };
        e.newGameFromStock(canastaStock(hands, 0, spare(), cd(Suit::Diamonds, 9)), 0);

        e.drawFromStock();
        e.discard(cd(Suit::Diamonds, 6));
        const bool ready = e.currentSeat() == 2 && e.phase() == ca::Engine::Phase::Draw
            && !e.pile().empty() && e.pile().back().rank == 6 && !e.team(0).opened;
        check(ready, "canasta: reached a six on the pile with an unopened side to play");

        // The sixes come to 65 with the top card, which opens you in the classic
        // game and counts for nothing under the house rule.
        check(e.canTakePile(sixes) == (strict == 0),
              strict ? "canasta: the pile meld alone cannot open you"
                     : "canasta: classic lets the pile meld open you");
        check(e.canTakePile(sixesAndAces),
              strict ? "canasta: but four aces beside it open you and the pile comes"
                     : "canasta: aces alongside take the pile too");
    }

    // Every house rule at once, played out by four computers, because the
    // failure that matters here is a hand that cannot legally continue. Said
    // without a count: the list grows, and a stale number reads as a claim that
    // the set is complete when it is not.
    ca::Rules house = ca::Rules::classic();
    house.name = QStringLiteral("House");
    house.targetScore = 3000;
    house.noMeldingFirstRound = true;
    house.pileMeldCountsToOpen = false;
    house.canastaMakesRankSafe = true;
    house.wildsFewerThanNaturals = true;
    house.canastaNeededToScore = true;
    // The house way out (GHUB-0120). It belongs here more than any of the
    // others: it REMOVES a legal move, and removing one is how a seat ends up
    // with nothing it may do — which this loop's guard is what catches.
    house.goingOutNeedsADiscard = true;

    std::array<ca::Ai, ca::kSeats> ai { ca::Ai { ca::Level::Hard }, ca::Ai { ca::Level::Hard },
                                        ca::Ai { ca::Level::Hard }, ca::Ai { ca::Level::Hard } };
    bool finished = false;
    bool whole = true;
    bool laidTooEarly = false;
    bool tookASafeRank = false;
    int handsPlayed = 0;
    std::array<int, ca::kTeams> handsOpened { 0, 0 };
    // Several games rather than one, because "a side never opened" is a claim
    // about a run of hands and one hand proves nothing either way.
    for (const unsigned seed : { 2718u, 31415u, 1618u, 1414u }) {
    ca::Engine e { house };
    e.newGame(seed);
    finished = false;
    for (long guard = 0; guard < 40000 && !finished; ++guard) {
        if (e.phase() == ca::Engine::Phase::GameOver) {
            // The last hand is scored straight into GameOver, so it is counted
            // here rather than below.
            ++handsPlayed;
            for (int t = 0; t < ca::kTeams; ++t)
                if (e.team(t).opened)
                    ++handsOpened[std::size_t(t)];
            finished = true;
            break;
        }
        if (e.phase() == ca::Engine::Phase::HandOver) {
            // A side that never opens all hand is the shape of failure that
            // does not announce itself: the engine refuses, the computer keeps
            // playing, and nothing on screen says why.
            ++handsPlayed;
            for (int t = 0; t < ca::kTeams; ++t)
                if (e.team(t).opened)
                    ++handsOpened[std::size_t(t)];
            e.nextHand();
            continue;
        }
        if (e.cardsInPlay() != 108) {
            whole = false;
            break;
        }
        if (!e.meldingAllowed() && (!e.team(0).melds.empty() || !e.team(1).melds.empty()))
            laidTooEarly = true;
        // A pile whose top card matches one of your own canastas must never be
        // takeable — that is the safe-discard rule, watched all game long.
        const int me = ca::teamOf(e.currentSeat());
        if (!e.pile().empty() && e.phase() == ca::Engine::Phase::Draw) {
            const ca::Meld* mine = e.team(me).meldOfRank(e.pile().back().rank);
            if (mine != nullptr && mine->isCanasta(house) && e.canTakePileAtAll())
                tookASafeRank = true;
        }
        const int seat = e.currentSeat();
        ai[std::size_t(seat)].draw(e);
        if (e.phase() == ca::Engine::Phase::Play)
            ai[std::size_t(seat)].playAndDiscard(e);
    }
    if (!finished)
        break;
    }
    check(finished, "canasta: a full game plays out under all five house rules");
    check(whole, "canasta: and the pack stays whole all the way through");
    check(!laidTooEarly, "canasta: nothing reached the table during a first round");
    check(!tookASafeRank, "canasta: and no side could ever take a pile topped by its own canasta");

    std::printf("      canasta: %d hands, opened by us %d, by them %d\n", handsPlayed,
                handsOpened[0], handsOpened[1]);
    check(handsPlayed > 0 && handsOpened[0] * 2 >= handsPlayed * 1
              && handsOpened[1] * 2 >= handsPlayed * 1,
          "canasta: both sides open in most hands rather than sitting on their cards");
}

// A game to 5000 is several sittings, so it has to survive being put away. The
// test is that a position written out and read back is the same position.
void canastaSaveAndResume()
{
    ca::Rules house = ca::Rules::classic();
    house.name = QStringLiteral("House");
    house.targetScore = 2000;
    house.canastaMakesRankSafe = true;
    house.wildsFewerThanNaturals = true;
    house.canastaNeededToScore = true;

    ca::Engine e { house };
    e.newGame(1234);
    std::array<ca::Ai, ca::kSeats> ai { ca::Ai { ca::Level::Hard }, ca::Ai { ca::Level::Hard },
                                        ca::Ai { ca::Level::Hard }, ca::Ai { ca::Level::Hard } };
    // Far enough in that there are melds on the table and a pile to take.
    for (int turn = 0; turn < 40; ++turn) {
        if (e.phase() == ca::Engine::Phase::HandOver || e.phase() == ca::Engine::Phase::GameOver)
            break;
        const int seat = e.currentSeat();
        ai[std::size_t(seat)].draw(e);
        if (e.phase() == ca::Engine::Phase::Play)
            ai[std::size_t(seat)].playAndDiscard(e);
    }

    QByteArray blob;
    {
        QDataStream out(&blob, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        e.save(out);
    }
    check(!blob.isEmpty(), "canasta: a game in progress writes out");

    ca::Engine back;
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    check(back.load(in), "canasta: and reads back");

    bool same = back.currentSeat() == e.currentSeat() && back.phase() == e.phase()
        && back.stockCount() == e.stockCount() && back.pile().size() == e.pile().size()
        && back.dealer() == e.dealer() && back.pileFrozen() == e.pileFrozen()
        && back.rules().targetScore == house.targetScore && back.rules().canastaMakesRankSafe
        && back.rules().canastaNeededToScore && back.openRequirement(0) == e.openRequirement(0);
    for (int s = 0; s < ca::kSeats; ++s)
        same = same && back.hand(s) == e.hand(s);
    for (int t = 0; t < ca::kTeams; ++t) {
        same = same && back.team(t).score == e.team(t).score
            && back.team(t).opened == e.team(t).opened
            && back.team(t).melds.size() == e.team(t).melds.size()
            && back.team(t).redThrees.size() == e.team(t).redThrees.size();
        for (std::size_t i = 0; i < e.team(t).melds.size() && same; ++i)
            same = same && back.team(t).melds[i].cards == e.team(t).melds[i].cards;
    }
    check(same, "canasta: the resumed table is the same table");
    check(back.cardsInPlay() == 108, "canasta: with the whole pack still in it");

    // And it carries on rather than merely looking right. A hand that had
    // already finished is dealt on, which is exactly what resuming into one
    // should do.
    bool played = false;
    for (int turn = 0; turn < 20 && !played; ++turn) {
        if (back.phase() == ca::Engine::Phase::GameOver)
            break;
        if (back.phase() == ca::Engine::Phase::HandOver) {
            back.nextHand();
            continue;
        }
        const int seat = back.currentSeat();
        ai[std::size_t(seat)].draw(back);
        if (back.phase() == ca::Engine::Phase::Play)
            ai[std::size_t(seat)].playAndDiscard(back);
        played = back.cardsInPlay() == 108;
    }
    check(played || back.phase() == ca::Engine::Phase::GameOver,
          "canasta: and the resumed game plays on");

    // Junk must be refused rather than half-read.
    ca::Engine fresh;
    fresh.newGame(99);
    const std::vector<Card> before = fresh.hand(0);
    QByteArray rubbish = blob.left(blob.size() / 3);
    QDataStream bad(rubbish);
    bad.setVersion(QDataStream::Qt_6_0);
    check(!fresh.load(bad), "canasta: a truncated save is refused");
    check(fresh.hand(0) == before, "canasta: and refusing one changes nothing");
}

// Sorting a hand is cosmetic, so the two things worth proving are that the
// order is the one a player expects and that nothing is lost on the way.
void canastaHandSort()
{
    check(ca::sortsBefore(joker(true), cd(Suit::Spades, 2)), "canasta: jokers lead the fan");
    check(ca::sortsBefore(cd(Suit::Spades, 2), cd(Suit::Hearts, kAce)),
          "canasta: then the twos, the other wild card");
    check(ca::sortsBefore(cd(Suit::Hearts, kAce), cd(Suit::Clubs, kKing)),
          "canasta: aces above kings");
    check(ca::sortsBefore(cd(Suit::Clubs, kKing), cd(Suit::Clubs, 4)),
          "canasta: and downward from there");
    check(ca::sortsBefore(cd(Suit::Clubs, 4), cd(Suit::Spades, 3)),
          "canasta: black threes at the far end");
    check(!ca::sortsBefore(cd(Suit::Clubs, kKing), cd(Suit::Clubs, kKing)),
          "canasta: a card does not sort before itself");

    ca::Engine e;
    e.newGame(9001);
    const std::vector<Card> before = e.hand(0);
    e.sortHand(0);
    const std::vector<Card>& after = e.hand(0);

    check(after.size() == before.size(), "canasta: sorting keeps the hand the same size");
    std::vector<Card> pool = before;
    bool same = true;
    for (const Card& c : after) {
        auto it = std::find(pool.begin(), pool.end(), c);
        if (it == pool.end()) {
            same = false;
            break;
        }
        pool.erase(it);
    }
    check(same && pool.empty(), "canasta: sorting deals no new card and drops none");

    bool ordered = true;
    for (std::size_t i = 1; i < after.size(); ++i)
        if (ca::sortsBefore(after[i], after[i - 1]))
            ordered = false;
    check(ordered, "canasta: a sorted hand is in order");
    check(e.cardsInPlay() == 108, "canasta: the pack is still whole after a sort");
}

// ---------------------------------------------------------------------------
// Saved boards
// ---------------------------------------------------------------------------

// Minesweeper, Reversi and Draughts keep no move log, so there is nothing to
// replay a save against. What stands in for that is each core's restore(),
// which is the same job cardcodec's pack check does for the solitaires: it has
// to refuse a board this game could never have reached. A save round-trip is
// checked through the widgets in the UI test; what is checked here is the
// refusing, because those paths need a board built deliberately wrong and no
// amount of playing produces one.
void savedBoardsAreRechecked()
{
    Minefield field(9, 9, 10);
    field.reveal(4, 4);
    std::vector<Minefield::Square> squares = field.squares();
    check(Minefield(9, 9, 10).restore(squares), "saves: a real minefield is taken back");

    // The numbers are recomputed rather than read, so moving a mine has to move
    // the count next to it as well.
    std::vector<Minefield::Square> renumbered = squares;
    for (Minefield::Square& s : renumbered)
        s.neighbours = 7;
    Minefield relaid(9, 9, 10);
    check(relaid.restore(renumbered) && relaid.at(4, 4).neighbours != 7,
          "saves: a minefield's numbers are worked out again, not believed");

    std::vector<Minefield::Square> extra = squares;
    for (Minefield::Square& s : extra)
        if (!s.mine && !s.revealed) {
            s.mine = true;
            break;
        }
    check(!Minefield(9, 9, 10).restore(extra),
          "saves: a minefield with the wrong number of mines is refused");
    check(!Minefield(9, 9, 10).restore(std::vector<Minefield::Square>(80)),
          "saves: a minefield of the wrong size is refused");

    std::vector<Minefield::Square> contradictory = squares;
    contradictory[0].revealed = true;
    contradictory[0].flagged = true;
    contradictory[0].mine = false;
    check(!Minefield(9, 9, 10).restore(contradictory),
          "saves: a square both dug and flagged is refused");

    Board reversi;
    check(Board().restore(reversi.cells()), "saves: the opening Reversi board is taken back");

    std::array<Cell, kCells> stripped {};
    check(!Board().restore(stripped),
          "saves: a Reversi board with fewer than the opening four discs is refused");

    std::array<Cell, kCells> impossible = reversi.cells();
    impossible[0] = Cell(9);
    check(!Board().restore(impossible),
          "saves: a Reversi square holding something that is not a disc is refused");

    const DraughtsBoard opening;
    check(DraughtsBoard().restore(opening.cells()),
          "saves: the opening draughts board is taken back");

    std::vector<Piece> onLight = opening.cells();
    onLight[0] = Piece::RedMan; // (0,0) is a light square, where nothing ever stands
    check(!DraughtsBoard().restore(onLight),
          "saves: a draughts piece on a light square is refused");

    std::vector<Piece> wipedOut = opening.cells();
    for (Piece& p : wipedOut)
        if (belongsTo(p, Side::White))
            p = Piece::Empty;
    check(!DraughtsBoard().restore(wipedOut),
          "saves: a draughts board with a side already wiped out is refused");

    check(!DraughtsBoard().restore(std::vector<Piece>(32, Piece::Empty)),
          "saves: a draughts board of the wrong size is refused");
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

    section("Saved boards");
    savedBoardsAreRechecked();

    section("Chess");
    chessMoveGeneration();
    chessSpecialMoves();
    chessEndings();
    chessEngine();

    section("Sudoku");
    sudokuGeneration();

    section("Pinball");
    pinballLaunch();
    pinballContainment();
    pinballScoring();

    section("Cards");
    deckRules();
    cardCodecRoundTrip();
    cardCodecRefusals();
    cardCodecPackCheck();

    section("Hearts");
    heartsRules();
    heartsFullGames();
    heartsMoonShot();

    section("Canasta");
    canastaDeckAndValues();
    canastaOpeningBands();
    canastaMeldShapes();
    canastaScoringTable();
    canastaPileRules();
    canastaFrozenPile();
    canastaWildCardRules();
    canastaRedThrees();
    canastaGoingOut();
    canastaFullGames();
    canastaLevelsDiffer();
    canastaHouseRules();
    canastaDeadHand();
    canastaWinningIsReachingTheTarget();
    canastaFrozenPileDepth();
    canastaCaughtAMinus();
    canastaFirstRoundSafeThrow();
    canastaLeastDamagingDiscard();
    canastaMinusAgainstMilking();
    canastaMeldOrder();
    canastaAiOpens();
    canastaTakeAndOpenTogether();
    canastaAiHoldsWhileFrozen();
    canastaAiPlaysOutWhenTheHandGetsSmall();
    canastaAiOpensOnAJokerToKeepThePair();
    canastaFreezeLimits();
    canastaFreezeReasons();
    canastaCountsThePack();
    canastaBlackThreeTiming();
    canastaFirstCanastaIsInsurance();
    canastaRunsThePackDead();
    canastaFishing();
    canastaThrowCaution();
    canastaHouseGoesOutOnAThrownCard();
    canastaUnopenedPileAndLiveRules();
    canastaWildValueGoesWhereItCounts();
    canastaCanastaNeededToScore();
    canastaWildsAcrossRanks();
    canastaWildsFewerThanNaturals();
    canastaClosedCanasta();
    canastaFirstRoundAndPileOpening();
    canastaHandSort();
    canastaSaveAndResume();

    snakeStartsLegal();
    snakeRefusesAReversal();
    snakeDiesAtTheWall();
    snakePlaysByItsOwnRules();

    twenty48MergesOnce();
    twenty48ReachesItsTarget();
    twenty48DeadPushCostsNothing();
    twenty48UndoStepsBack();
    twenty48RefusesABoardItCouldNotReach();
    twenty48PlaysByItsOwnRules();

    pyramidDealsAWholePack();
    pyramidRefusesACoveredCard();
    pyramidPairsMustMakeThirteen();
    pyramidExposesOnBothSupports();
    pyramidStockAndRedeals();
    pyramidUndoStepsBack();
    pyramidPlaysOutWithoutLosingACard();

    freecellDealsAWholePack();
    freecellMoveSizeIsTheRule();
    freecellStacksAlternateAndDescend();
    freecellFoundationsGoUpInSuit();
    freecellUndoDoesNotLoseACard();
    freecellPlaysOutWithoutLosingACard();

    std::printf("\n%s\n", g_failures == 0 ? "All checks passed." : "FAILURES PRESENT.");
    return g_failures == 0 ? 0 : 1;
}
