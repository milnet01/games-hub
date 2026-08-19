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

#include <chrono>
#include <cstdio>
#include <algorithm>
#include <array>
#include <map>

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
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

void canastaLevelsDiffer()
{
    // The ladder has to be a ladder. Each rung is checked against the one below
    // it, because a level that is only a label is worse than no level at all —
    // this is how Hard was found to be WEAKER than Medium, which is what a
    // player moving up to it would have run into.
    check(canastaMatch(ca::Level::Medium, ca::Level::Easy, "medium v easy", 24) * 2 > 24,
          "canasta: medium beats easy");
    check(canastaMatch(ca::Level::Hard, ca::Level::Easy, "hard v easy", 24) * 2 > 24,
          "canasta: hard beats easy");
    // Two strong sides are close together and Canasta carries a lot of luck, so
    // the top two rungs are judged over a longer run than the bottom.
    check(canastaMatch(ca::Level::Hard, ca::Level::Medium, "hard v medium", 120) * 2 > 120,
          "canasta: hard beats medium");
    check(canastaMatch(ca::Level::Expert, ca::Level::Hard, "expert v hard", 240) * 2 > 240,
          "canasta: expert beats hard");
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

    // All five house rules at once, played out by four computers, because the
    // failure that matters here is a hand that cannot legally continue.
    ca::Rules house = ca::Rules::classic();
    house.name = QStringLiteral("House");
    house.targetScore = 3000;
    house.noMeldingFirstRound = true;
    house.pileMeldCountsToOpen = false;
    house.canastaMakesRankSafe = true;
    house.wildsFewerThanNaturals = true;
    house.canastaNeededToScore = true;

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
    canastaFirstRoundSafeThrow();
    canastaLeastDamagingDiscard();
    canastaMeldOrder();
    canastaAiOpens();
    canastaTakeAndOpenTogether();
    canastaAiHoldsWhileFrozen();
    canastaUnopenedPileAndLiveRules();
    canastaWildValueGoesWhereItCounts();
    canastaCanastaNeededToScore();
    canastaWildsAcrossRanks();
    canastaWildsFewerThanNaturals();
    canastaClosedCanasta();
    canastaFirstRoundAndPileOpening();
    canastaHandSort();
    canastaSaveAndResume();

    std::printf("\n%s\n", g_failures == 0 ? "All checks passed." : "FAILURES PRESENT.");
    return g_failures == 0 ? 0 : 1;
}
