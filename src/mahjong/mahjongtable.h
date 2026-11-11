#pragma once

#include "mahjongrules.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

// Four-player Mahjong (GHUB-0239): the wall, the deal, turns, claims, kongs
// and the bonus tiles, then the game the hands make up. No Qt and no words,
// so the self-test plays whole games without a display.
//
// The save is the moves, as Chess's is (docs/design.md § Saves): a hand is
// its seed plus its move list, and a game is its hands. Draws are not moves --
// the wall decides them -- so the list holds only what a player chose, and
// replaying it through play() re-checks every one.
namespace mahjong {

constexpr int kSeats = 4;
// Each seat's opening hand, before East's first draw.
constexpr int kHandTiles = 13;

enum class Action : std::uint8_t {
    Discard,
    Chow,
    Pung,
    Kong, // claimed from a discard
    ConcealedKong,
    AddedKong,
    Win,
    Pass,
};

struct Move {
    std::uint8_t seat = 0;
    Action action = Action::Pass;
    Face tile = 0;            // the tile discarded, claimed, konged or won on
    std::uint8_t chowLow = 0; // the lowest tile of a chow, else 0

    bool operator==(const Move&) const = default;
};

enum class Phase : std::uint8_t {
    Turn,    // toMove() draws were made; it discards, wins or makes a kong
    Claims,  // a discard is on offer; waitingOn() have still to answer
    Robbing, // a tile added to a pung may be won on; waitingOn() answer
    Over,
};

// One seat's tiles.
struct SeatTiles {
    std::vector<Face> concealed; // kept sorted
    std::vector<Meld> melds;     // laid down, concealed kongs among them
    std::vector<Face> bonus;     // flowers and seasons, set aside face up
    std::vector<Face> pool;      // this seat's discards nobody claimed
    // The tile just drawn, set a little apart on screen. 0 when the turn
    // began with a claimed set rather than a draw.
    Face drawn = 0;
};

struct HandResult {
    bool drawn = false; // the wall ran out with nobody out
    WinContext win;
    HandScore score;
    std::array<int, kSeats> payments {};
};

class Hand
{
public:
    // A fresh hand: the wall built and shuffled from `seed`, thirteen tiles
    // each, bonus tiles replaced, and the dealer's first draw made.
    // `prevailingWind` is 1 for east to 4 for north.
    Hand(const RuleSet& rules, std::uint32_t seed, int dealer, int prevailingWind);

    // For a test: the four hands as given (a bonus tile among them set aside),
    // the wall to draw from in order (front first), and `toMove` to act with
    // the tiles it holds. Not a hand a game deals -- the tile count is the
    // test's business.
    static Hand arranged(const RuleSet& rules, const std::array<std::vector<Face>, kSeats>& hands,
                         std::vector<Face> wall, int toMove, int dealer, int prevailingWind);

    const RuleSet& rules() const { return *m_rules; }
    std::uint32_t seed() const { return m_seed; }
    int dealer() const { return m_dealer; }
    int prevailingWind() const { return m_prevailing; }
    // 1 for east to 4 for north: the dealer is east, and the winds run on in
    // turn order.
    int seatWind(int seat) const { return (seat - m_dealer + kSeats) % kSeats + 1; }

    Phase phase() const { return m_phase; }
    // The seat to act in a Turn, else -1.
    int toMove() const { return m_phase == Phase::Turn ? m_turn : -1; }
    // The seats still to answer a claim or a robbed kong.
    std::vector<int> waitingOn() const;
    // The tile on offer to a claim or a rob, and who let it go; 0 and -1
    // otherwise.
    Face offered() const { return m_offered; }
    int offeredBy() const { return m_offeredBy; }
    // The last tile discarded and who discarded it, kept after the claims are
    // settled so the table can say what happened.
    Face lastDiscard() const { return m_lastDiscard; }
    int lastDiscarder() const { return m_lastDiscarder; }
    // Who took the last discard for a set, and how: -1 and Pass while nobody
    // has since it was thrown.
    int lastClaimer() const { return m_lastClaimer; }
    Action lastClaim() const { return m_lastClaim; }

    const SeatTiles& seat(int s) const { return m_seats[std::size_t(s)]; }
    int wallLeft() const { return std::max(0, m_back - m_front + 1); }

    // Every move `seat` may make now; empty when nothing is theirs to make.
    // A claim's list always ends in Pass.
    std::vector<Move> legalMoves(int seat) const;
    // Plays `move` if it is one of legalMoves(move.seat), and refuses it
    // otherwise, leaving the hand as it was.
    bool play(const Move& move);

    const std::vector<Move>& moves() const { return m_moves; }
    const HandResult& result() const { return m_result; }

    // Every tile in the hand, wherever it lies: the wall, the hands, the sets,
    // the bonus tiles, the pools and a tile on offer. INV-1.
    std::vector<Face> everyTile() const;

    // How the hand would score if `seat` won now: on the tile it drew, or on
    // `tile` from `payer`. Nothing when that is no winning hand. The minimum
    // is not applied; legalMoves() applies it.
    std::optional<std::pair<WinContext, HandScore>> winFor(int seat) const;
    std::optional<std::pair<WinContext, HandScore>> winOn(int seat, Face tile, int payer,
                                                          bool robbing) const;

private:
    Hand() = default;

    SeatTiles& seatAt(int s) { return m_seats[std::size_t(s)]; }
    void takeFrom(int seat, Face tile, int count);
    bool drawInto(int seat, bool fromBack);
    void startTurn(int seat);
    void openClaims();
    void passOn();
    void settleClaims();
    void settleRob();
    void finishKong(int seat);
    void win(int seat, Face tile, int payer, bool robbing);
    void drawHand();
    WinContext contextFor(int seat) const;
    std::vector<Move> turnMoves(int seat) const;

    const RuleSet* m_rules = nullptr;
    std::uint32_t m_seed = 0;
    int m_dealer = 0;
    int m_prevailing = 1;

    std::vector<Face> m_wall;
    int m_front = 0;
    int m_back = -1;
    std::array<SeatTiles, kSeats> m_seats {};

    Phase m_phase = Phase::Turn;
    int m_turn = 0;
    Face m_offered = 0;
    int m_offeredBy = -1;
    Face m_lastDiscard = 0;
    int m_lastDiscarder = -1;
    int m_lastClaimer = -1;
    Action m_lastClaim = Action::Pass;
    std::array<std::vector<Move>, kSeats> m_options {};
    std::array<std::optional<Move>, kSeats> m_answers {};

    // For the patterns that depend on how the hand went.
    int m_discards = 0;
    int m_claims = 0;
    int m_kongs = 0;
    bool m_afterKong = false;

    std::vector<Move> m_moves;
    HandResult m_result;
};

enum class Length : std::uint8_t { EastRound = 1, FullGame = 2 };

struct HandRecord {
    std::uint32_t seed = 0;
    std::vector<Move> moves;
};

// A game: hands until the deal has gone round once (an East round) or four
// times (a full game), with running totals.
class Game
{
public:
    Game(RuleSetId rules, Length length, int firstDealer = 0);

    RuleSetId ruleSetId() const { return m_rules; }
    Length length() const { return m_length; }
    int firstDealer() const { return m_firstDealer; }
    int dealer() const { return m_dealer; }
    // 1 for east to 4 for north.
    int prevailingWind() const { return std::min(kSeats, m_dealsPassed / kSeats + 1); }
    int dealsPassed() const { return m_dealsPassed; }
    const std::array<int, kSeats>& totals() const { return m_totals; }
    const std::vector<HandResult>& results() const { return m_results; }
    bool over() const;
    // The seats on the highest total once the game is over: one, or several
    // sharing it, as HeartsEngine::winnerIsShared asks.
    std::vector<int> leaders() const;

    // The hand being played, or the one just finished; null before the first.
    const Hand* hand() const { return m_hand ? &*m_hand : nullptr; }
    bool handInPlay() const { return m_hand && m_hand->phase() != Phase::Over; }
    // Deals the next hand. Refused while one is in play or the game is over.
    bool startHand(std::uint32_t seed);
    // Into the hand in play; the totals and the deal move on when it ends.
    bool play(const Move& move);

    // Every hand dealt, finished ones and the one in play, as seeds and moves.
    std::vector<HandRecord> records() const;
    // The game those records make, every move re-checked, or nothing.
    static std::optional<Game> replay(RuleSetId rules, Length length, int firstDealer,
                                      const std::vector<HandRecord>& records);

private:
    void settle();

    RuleSetId m_rules;
    Length m_length;
    int m_firstDealer = 0;
    int m_dealer = 0;
    int m_dealsPassed = 0;
    std::array<int, kSeats> m_totals {};
    std::vector<HandResult> m_results;
    std::vector<HandRecord> m_finished;
    std::optional<Hand> m_hand;
};

} // namespace mahjong
