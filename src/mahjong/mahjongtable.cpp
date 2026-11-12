#include "mahjongtable.h"

#include <algorithm>

namespace mahjong {

namespace {

bool isBonus(Face t)
{
    const Suit s = suitOf(t);
    return s == Suit::Flower || s == Suit::Season;
}

bool numbered(Face t)
{
    const Suit s = suitOf(t);
    return s == Suit::Circles || s == Suit::Bamboo || s == Suit::Characters;
}

int countOf(const std::vector<Face>& tiles, Face t)
{
    return int(std::count(tiles.begin(), tiles.end(), t));
}

void insertSorted(std::vector<Face>& tiles, Face t)
{
    tiles.insert(std::upper_bound(tiles.begin(), tiles.end(), t), t);
}

} // namespace

// ---------------------------------------------------------------------------
// Setting up
// ---------------------------------------------------------------------------

Hand::Hand(const RuleSet& rules, std::uint32_t seed, int dealer, int prevailingWind)
    : m_rules(&rules)
    , m_seed(seed)
    , m_dealer(dealer)
    , m_prevailing(prevailingWind)
{
    std::mt19937 rng(seed);
    m_wall = fullSet();
    if (!rules.bonusTilesInPlay())
        m_wall.erase(std::remove_if(m_wall.begin(), m_wall.end(), isBonus), m_wall.end());
    shuffleInPlace(m_wall, rng);
    m_front = 0;
    m_back = int(m_wall.size()) - 1;

    for (int i = 0; i < kSeats; ++i) {
        SeatTiles& s = seatAt((dealer + i) % kSeats);
        for (int n = 0; n < kHandTiles; ++n)
            insertSorted(s.concealed, m_wall[std::size_t(m_front++)]);
    }
    // Bonus tiles in an opening hand are set aside and replaced from the back
    // of the wall, the dealer first.
    for (int i = 0; i < kSeats; ++i) {
        SeatTiles& s = seatAt((dealer + i) % kSeats);
        for (;;) {
            const auto bonus = std::find_if(s.concealed.begin(), s.concealed.end(), isBonus);
            if (bonus == s.concealed.end())
                break;
            s.bonus.push_back(*bonus);
            s.concealed.erase(bonus);
            insertSorted(s.concealed, m_wall[std::size_t(m_back--)]);
        }
    }
    drawInto(dealer, false);
    startTurn(dealer);
}

Hand Hand::arranged(const RuleSet& rules, const std::array<std::vector<Face>, kSeats>& hands,
                    std::vector<Face> wall, int toMove, int dealer, int prevailingWind)
{
    Hand h;
    h.m_rules = &rules;
    h.m_dealer = dealer;
    h.m_prevailing = prevailingWind;
    h.m_wall = std::move(wall);
    h.m_front = 0;
    h.m_back = int(h.m_wall.size()) - 1;
    for (int s = 0; s < kSeats; ++s) {
        // A bonus tile given in a hand is set aside, as the deal would.
        for (Face t : hands[std::size_t(s)])
            (isBonus(t) ? h.seatAt(s).bonus : h.seatAt(s).concealed).push_back(t);
        std::sort(h.seatAt(s).concealed.begin(), h.seatAt(s).concealed.end());
    }
    // Counted as past the first go-round, so no arranged hand is mistaken for
    // a Heavenly or Earthly one.
    h.m_discards = kSeats;
    h.startTurn(toMove);
    return h;
}

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

std::vector<int> Hand::waitingOn() const
{
    std::vector<int> seats;
    if (m_phase != Phase::Claims && m_phase != Phase::Robbing)
        return seats;
    for (int s = 0; s < kSeats; ++s)
        if (!m_options[std::size_t(s)].empty() && !m_answers[std::size_t(s)])
            seats.push_back(s);
    return seats;
}

std::vector<Face> Hand::everyTile() const
{
    std::vector<Face> all(m_wall.begin() + m_front, m_wall.begin() + std::max(m_front, m_back + 1));
    for (const SeatTiles& s : m_seats) {
        all.insert(all.end(), s.concealed.begin(), s.concealed.end());
        all.insert(all.end(), s.bonus.begin(), s.bonus.end());
        all.insert(all.end(), s.pool.begin(), s.pool.end());
        for (const Meld& m : s.melds) {
            if (m.kind == SetKind::Chow)
                all.insert(all.end(), { m.tile, Face(m.tile + 1), Face(m.tile + 2) });
            else
                all.insert(all.end(), m.kind == SetKind::Kong ? 4 : 3, m.tile);
        }
    }
    if (m_offered != 0)
        all.push_back(m_offered);
    return all;
}

WinContext Hand::contextFor(int who) const
{
    WinContext how;
    how.bonus = seat(who).bonus;
    how.seatWind = seatWind(who);
    how.prevailingWind = m_prevailing;
    how.winner = who;
    return how;
}

std::optional<std::pair<WinContext, HandScore>> Hand::winFor(int seat) const
{
    const SeatTiles& s = this->seat(seat);
    if (s.drawn == 0)
        return std::nullopt;
    WinContext how = contextFor(seat);
    how.selfDrawn = true;
    // Only a tile drawn from the wall can be its last tile; a win on a discard
    // after the wall runs out does not score it (owner's ruling, 2026-11-12).
    how.lastTile = wallLeft() == 0;
    how.winningTile = s.drawn;
    how.kongReplacement = m_afterKong;
    how.heavenly = seat == m_dealer && m_discards == 0 && m_claims == 0 && m_kongs == 0;
    return bestWin(*m_rules, s.concealed, s.melds, how);
}

std::optional<std::pair<WinContext, HandScore>> Hand::winOn(int seat, Face tile, int payer,
                                                            bool robbing) const
{
    const SeatTiles& s = this->seat(seat);
    std::vector<Face> tiles = s.concealed;
    insertSorted(tiles, tile);
    WinContext how = contextFor(seat);
    how.winningTile = tile;
    how.payer = payer;
    how.robbedKong = robbing;
    how.earthly = !robbing && m_discards == 1 && payer == m_dealer && m_claims == 0 && m_kongs == 0;
    return bestWin(*m_rules, tiles, s.melds, how);
}

std::vector<Move> Hand::turnMoves(int seat) const
{
    std::vector<Move> moves;
    const SeatTiles& s = this->seat(seat);
    const auto st = std::uint8_t(seat);
    const auto win = winFor(seat);
    if (win && win->second.total >= m_rules->minimumToWin())
        moves.push_back({ st, Action::Win, s.drawn, 0 });
    if (wallLeft() > 0) {
        for (Face t = 1; t < kFlowerFirst; ++t) {
            if (countOf(s.concealed, t) == 4)
                moves.push_back({ st, Action::ConcealedKong, t, 0 });
            const bool pung = std::any_of(s.melds.begin(), s.melds.end(), [t](const Meld& m) {
                return m.kind == SetKind::Pung && m.tile == t;
            });
            // Only the tile just drawn may be added to a pung (spec § 4.3).
            if (pung && s.drawn == t)
                moves.push_back({ st, Action::AddedKong, t, 0 });
        }
    }
    Face last = 0;
    for (Face t : s.concealed) {
        if (t != last)
            moves.push_back({ st, Action::Discard, t, 0 });
        last = t;
    }
    return moves;
}

std::vector<Move> Hand::legalMoves(int seat) const
{
    if (seat < 0 || seat >= kSeats)
        return {};
    switch (m_phase) {
    case Phase::Turn: return seat == m_turn ? turnMoves(seat) : std::vector<Move> {};
    case Phase::Claims:
    case Phase::Robbing:
        return m_answers[std::size_t(seat)] ? std::vector<Move> {} : m_options[std::size_t(seat)];
    case Phase::Over: break;
    }
    return {};
}

// ---------------------------------------------------------------------------
// Playing
// ---------------------------------------------------------------------------

bool Hand::play(const Move& move)
{
    const std::vector<Move> legal = legalMoves(move.seat);
    if (std::find(legal.begin(), legal.end(), move) == legal.end())
        return false;
    m_moves.push_back(move);
    const int who = move.seat;
    SeatTiles& s = seatAt(who);

    if (m_phase == Phase::Turn) {
        switch (move.action) {
        case Action::Discard:
            takeFrom(who, move.tile, 1);
            s.drawn = 0;
            m_afterKong = false;
            ++m_discards;
            m_offered = move.tile;
            m_offeredBy = who;
            m_lastDiscard = move.tile;
            m_lastDiscarder = who;
            m_lastClaimer = -1;
            m_lastClaim = Action::Pass;
            openClaims();
            return true;
        case Action::Win:
            win(who, 0, -1, false);
            return true;
        case Action::ConcealedKong:
            takeFrom(who, move.tile, 4);
            s.drawn = 0;
            s.melds.push_back({ SetKind::Kong, move.tile, true });
            ++m_kongs;
            finishKong(who);
            return true;
        case Action::AddedKong: {
            takeFrom(who, move.tile, 1);
            s.drawn = 0;
            // Anyone waiting on that tile may take it before the kong stands.
            m_offered = move.tile;
            m_offeredBy = who;
            for (int o = 0; o < kSeats; ++o) {
                m_options[std::size_t(o)].clear();
                m_answers[std::size_t(o)].reset();
                if (o == who)
                    continue;
                const auto w = winOn(o, move.tile, who, true);
                if (w && w->second.total >= m_rules->minimumToWin())
                    m_options[std::size_t(o)] = { { std::uint8_t(o), Action::Win, move.tile, 0 },
                                                  { std::uint8_t(o), Action::Pass, 0, 0 } };
            }
            m_phase = Phase::Robbing;
            if (waitingOn().empty())
                settleRob();
            return true;
        }
        default: break;
        }
        return false;
    }

    // Claims and Robbing: an answer, settled once every seat has given one.
    m_answers[std::size_t(who)] = move;
    if (!waitingOn().empty())
        return true;
    if (m_phase == Phase::Claims)
        settleClaims();
    else
        settleRob();
    return true;
}

void Hand::takeFrom(int seat, Face tile, int count)
{
    std::vector<Face>& c = seatAt(seat).concealed;
    for (int i = 0; i < count; ++i) {
        const auto at = std::find(c.begin(), c.end(), tile);
        if (at != c.end())
            c.erase(at);
    }
}

bool Hand::drawInto(int seat, bool fromBack)
{
    SeatTiles& s = seatAt(seat);
    for (;;) {
        if (wallLeft() == 0)
            return false;
        const Face t = fromBack ? m_wall[std::size_t(m_back--)] : m_wall[std::size_t(m_front++)];
        if (isBonus(t)) {
            // Set aside face up, and replaced from the back at once.
            s.bonus.push_back(t);
            fromBack = true;
            continue;
        }
        insertSorted(s.concealed, t);
        s.drawn = t;
        return true;
    }
}

void Hand::startTurn(int seat)
{
    m_phase = Phase::Turn;
    m_turn = seat;
    m_offered = 0;
    m_offeredBy = -1;
    for (int o = 0; o < kSeats; ++o) {
        m_options[std::size_t(o)].clear();
        m_answers[std::size_t(o)].reset();
    }
}

void Hand::openClaims()
{
    const Face t = m_offered;
    const int from = m_offeredBy;
    const int next = (from + 1) % kSeats;
    const bool more = wallLeft() > 0;
    for (int o = 0; o < kSeats; ++o) {
        std::vector<Move>& opts = m_options[std::size_t(o)];
        opts.clear();
        m_answers[std::size_t(o)].reset();
        if (o == from)
            continue;
        const auto st = std::uint8_t(o);
        const auto w = winOn(o, t, from, false);
        if (w && w->second.total >= m_rules->minimumToWin())
            opts.push_back({ st, Action::Win, t, 0 });
        // With the wall empty a set claimed now could never be followed by a
        // draw, so only a win may take the last discard.
        if (more) {
            const int held = countOf(seat(o).concealed, t);
            if (held >= 3)
                opts.push_back({ st, Action::Kong, t, 0 });
            if (held >= 2)
                opts.push_back({ st, Action::Pung, t, 0 });
            if (o == next && m_rules->chowAllowed() && numbered(t)) {
                const int rank = rankOf(t);
                for (int low = std::max(1, rank - 2); low <= std::min(7, rank); ++low) {
                    const Face first = Face(t - (rank - low));
                    bool have = true;
                    for (int k = 0; k < 3; ++k) {
                        const Face need = Face(first + k);
                        if (need != t && countOf(seat(o).concealed, need) == 0)
                            have = false;
                    }
                    if (have)
                        opts.push_back({ st, Action::Chow, t, std::uint8_t(first) });
                }
            }
        }
        if (!opts.empty())
            opts.push_back({ st, Action::Pass, 0, 0 });
    }
    m_phase = Phase::Claims;
    if (waitingOn().empty())
        passOn();
}

void Hand::passOn()
{
    seatAt(m_offeredBy).pool.push_back(m_offered);
    const int next = (m_offeredBy + 1) % kSeats;
    m_offered = 0;
    if (!drawInto(next, false)) {
        drawHand();
        return;
    }
    m_afterKong = false;
    startTurn(next);
}

void Hand::settleClaims()
{
    const Face t = m_offered;
    const int from = m_offeredBy;
    // Win first, to the first seat after the discarder; then a pung or kong;
    // then a chow. Only one seat can hold the pair a pung needs, since the
    // discard is one of the four.
    for (int i = 1; i < kSeats; ++i) {
        const int o = (from + i) % kSeats;
        const auto& a = m_answers[std::size_t(o)];
        if (a && a->action == Action::Win) {
            win(o, t, from, false);
            return;
        }
    }
    for (Action wanted : { Action::Kong, Action::Pung, Action::Chow }) {
        for (int i = 1; i < kSeats; ++i) {
            const int o = (from + i) % kSeats;
            const auto& a = m_answers[std::size_t(o)];
            if (!a || a->action != wanted)
                continue;
            SeatTiles& s = seatAt(o);
            ++m_claims;
            m_offered = 0;
            m_lastClaimer = o;
            m_lastClaim = wanted;
            if (wanted == Action::Chow) {
                for (int k = 0; k < 3; ++k)
                    if (Face(a->chowLow + k) != t)
                        takeFrom(o, Face(a->chowLow + k), 1);
                s.melds.push_back({ SetKind::Chow, Face(a->chowLow), false });
            } else {
                takeFrom(o, t, wanted == Action::Kong ? 3 : 2);
                s.melds.push_back({ wanted == Action::Kong ? SetKind::Kong : SetKind::Pung, t, false });
            }
            s.drawn = 0;
            if (wanted == Action::Kong) {
                ++m_kongs;
                finishKong(o);
            } else {
                startTurn(o);
            }
            return;
        }
    }
    passOn();
}

void Hand::settleRob()
{
    const Face t = m_offered;
    const int konger = m_offeredBy;
    for (int i = 1; i < kSeats; ++i) {
        const int o = (konger + i) % kSeats;
        const auto& a = m_answers[std::size_t(o)];
        if (a && a->action == Action::Win) {
            win(o, t, konger, true);
            return;
        }
    }
    // Nobody took it: the pung becomes a kong.
    for (Meld& m : seatAt(konger).melds) {
        if (m.kind == SetKind::Pung && m.tile == t) {
            m.kind = SetKind::Kong;
            break;
        }
    }
    m_offered = 0;
    ++m_kongs;
    finishKong(konger);
}

void Hand::finishKong(int seat)
{
    // A kong is four tiles in a set of three, so it is made up from the back.
    if (!drawInto(seat, true)) {
        drawHand();
        return;
    }
    startTurn(seat);
    m_afterKong = true;
}

void Hand::win(int seat, Face tile, int payer, bool robbing)
{
    const auto w = tile == 0 ? winFor(seat) : winOn(seat, tile, payer, robbing);
    if (tile != 0)
        insertSorted(seatAt(seat).concealed, tile);
    m_offered = 0;
    m_result = {};
    if (w) {
        m_result.win = w->first;
        m_result.score = w->second;
        const std::array<int, 4> pay = m_rules->settle(w->first, w->second);
        std::copy(pay.begin(), pay.end(), m_result.payments.begin());
    }
    m_phase = Phase::Over;
}

void Hand::drawHand()
{
    m_result = {};
    m_result.drawn = true;
    m_offered = 0;
    m_phase = Phase::Over;
}

// ---------------------------------------------------------------------------
// The game
// ---------------------------------------------------------------------------

Game::Game(RuleSetId rules, Length length, int firstDealer)
    : m_rules(rules)
    , m_length(length)
    , m_firstDealer(firstDealer)
    , m_dealer(firstDealer)
{
}

bool Game::over() const
{
    return !handInPlay() && m_dealsPassed >= (m_length == Length::EastRound ? 1 : 4) * kSeats;
}

std::vector<int> Game::leaders() const
{
    std::vector<int> seats;
    const int top = *std::max_element(m_totals.begin(), m_totals.end());
    for (int s = 0; s < kSeats; ++s)
        if (m_totals[std::size_t(s)] == top)
            seats.push_back(s);
    return seats;
}

bool Game::startHand(std::uint32_t seed)
{
    if (handInPlay() || over())
        return false;
    if (m_hand)
        m_finished.push_back({ m_hand->seed(), m_hand->moves() });
    m_hand.emplace(ruleSet(m_rules), seed, m_dealer, prevailingWind());
    return true;
}

bool Game::play(const Move& move)
{
    if (!handInPlay() || !m_hand->play(move))
        return false;
    if (m_hand->phase() == Phase::Over)
        settle();
    return true;
}

void Game::settle()
{
    const HandResult& r = m_hand->result();
    m_results.push_back(r);
    for (int s = 0; s < kSeats; ++s)
        m_totals[std::size_t(s)] += r.payments[std::size_t(s)];
    // The dealer keeps the deal on a win or a drawn hand; otherwise it passes
    // on, and every fourth pass the prevailing wind moves with it.
    if (!r.drawn && r.win.winner != m_dealer) {
        m_dealer = (m_dealer + 1) % kSeats;
        ++m_dealsPassed;
    }
}

std::vector<HandRecord> Game::records() const
{
    std::vector<HandRecord> all = m_finished;
    if (m_hand)
        all.push_back({ m_hand->seed(), m_hand->moves() });
    return all;
}

std::optional<Game> Game::replay(RuleSetId rules, Length length, int firstDealer,
                                 const std::vector<HandRecord>& records)
{
    if (!knownRuleSet(int(rules)) || firstDealer < 0 || firstDealer >= kSeats)
        return std::nullopt;
    Game g(rules, length, firstDealer);
    for (const HandRecord& r : records) {
        if (!g.startHand(r.seed))
            return std::nullopt;
        for (const Move& m : r.moves)
            if (!g.play(m))
                return std::nullopt;
    }
    return g;
}

} // namespace mahjong
