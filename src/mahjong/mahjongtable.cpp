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

std::vector<Face> without(std::vector<Face> tiles, Face t, int count)
{
    for (int i = 0; i < count; ++i) {
        const auto at = std::find(tiles.begin(), tiles.end(), t);
        if (at != tiles.end())
            tiles.erase(at);
    }
    return tiles;
}

bool nothingClaimed(const std::vector<Meld>& melds)
{
    return std::all_of(melds.begin(), melds.end(), [](const Meld& m) { return m.concealed; });
}

// After a chow, the face that would make the same run from the other end:
// having claimed the 3 for 4-5, the 6; the 6 for 4-5, the 3 (GHUB-0244 § 4.4).
Face otherEnd(Face claimed, Face low)
{
    if (claimed == low && rankOf(low) <= 6)
        return Face(low + 3);
    if (claimed == low + 2 && rankOf(low) >= 2)
        return Face(low - 1);
    return 0;
}

} // namespace

// ---------------------------------------------------------------------------
// Setting up
// ---------------------------------------------------------------------------

Hand::Hand(const RuleSet& rules, std::uint32_t seed, int dealer, int prevailingWind, const HandStart& start)
    : m_rules(&rules)
    , m_table(rules.table())
    , m_start(start)
    , m_seed(seed)
    , m_dealer(dealer)
    , m_prevailing(prevailingWind)
    , m_revealed(m_table.doraIndicators ? 1 : 0)
    , m_sticks(start.sticks)
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
                    std::vector<Face> wall, int toMove, int dealer, int prevailingWind, const HandStart& start)
{
    Hand h;
    h.m_rules = &rules;
    h.m_table = rules.table();
    h.m_start = start;
    h.m_sticks = start.sticks;
    h.m_revealed = h.m_table.doraIndicators ? 1 : 0;
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
    h.m_pastFirstRound = true;
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

std::vector<Face> Hand::doraIndicators() const
{
    std::vector<Face> out;
    const int size = int(m_wall.size());
    for (int k = 0; k < m_revealed; ++k) {
        const int at = size - 5 - 2 * k;
        if (at >= 0)
            out.push_back(m_wall[std::size_t(at)]);
    }
    return out;
}

std::vector<Face> Hand::uraIndicators() const
{
    std::vector<Face> out;
    const int size = int(m_wall.size());
    for (int k = 0; k < m_revealed; ++k) {
        const int at = size - 6 - 2 * k;
        if (at >= 0)
            out.push_back(m_wall[std::size_t(at)]);
    }
    return out;
}

Furiten Hand::furiten(int who) const
{
    if (!m_table.furiten)
        return Furiten::None;
    const SeatTiles& s = seat(who);
    // In its own turn the seat holds the tile it drew too; its waits are read
    // without it.
    const std::vector<Face> hand =
        int(s.concealed.size()) % 3 == 2 && s.drawn != 0 ? without(s.concealed, s.drawn, 1) : s.concealed;
    const std::vector<Face>& thrown = m_thrown[std::size_t(who)];
    if (!thrown.empty())
        for (Face w : waitingTiles(*m_rules, hand, s.melds))
            if (std::find(thrown.begin(), thrown.end(), w) != thrown.end())
                return Furiten::OwnDiscard;
    if (m_passedAfterRiichi[std::size_t(who)])
        return Furiten::PassedAfterRiichi;
    if (m_passed[std::size_t(who)])
        return Furiten::Passed;
    return Furiten::None;
}

WinContext Hand::contextFor(int who) const
{
    const auto at = std::size_t(who);
    WinContext how;
    how.bonus = seat(who).bonus;
    how.seatWind = seatWind(who);
    how.prevailingWind = m_prevailing;
    how.winner = who;
    how.riichi = m_riichi[at] && !m_doubleRiichi[at];
    how.doubleRiichi = m_doubleRiichi[at];
    how.ippatsu = m_ippatsu[at];
    how.firstGoRound = !m_pastFirstRound && m_claims == 0 && m_kongs == 0 && m_thrown[at].empty();
    how.doraIndicators = doraIndicators();
    if (m_riichi[at])
        how.uraIndicators = uraIndicators();
    how.counters = m_start.counters;
    how.liable = m_liable[at];
    return how;
}

int Hand::onView(Face tile) const
{
    int n = 0;
    for (const SeatTiles& s : m_seats) {
        n += int(std::count(s.pool.begin(), s.pool.end(), tile));
        for (const Meld& m : s.melds) {
            if (m.concealed)
                continue;
            if (m.kind == SetKind::Chow)
                n += tile >= m.tile && tile <= m.tile + 2 ? 1 : 0;
            else if (m.tile == tile)
                n += m.kind == SetKind::Kong ? 4 : 3;
        }
    }
    return n;
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
    how.lastOfItsKind = onView(s.drawn) == 3;
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
    how.lastTileClaim = !robbing && wallLeft() == 0;
    how.lastOfItsKind = onView(tile) == 3;
    how.earthly = !robbing && m_discards == 1 && payer == m_dealer && m_claims == 0 && m_kongs == 0;
    return bestWin(*m_rules, tiles, s.melds, how);
}

bool Hand::mayWinOn(int seat) const
{
    return furiten(seat) == Furiten::None;
}

bool Hand::kongRoom() const
{
    return m_table.mostKongs == 0 || m_kongs < m_table.mostKongs;
}

// After riichi a concealed kong may not change the hand: the drawn tile is
// the fourth of a concealed pung, the waits stay the same, and in every
// winning hand those three tiles are a pung (GHUB-0244 § 4.4).
bool Hand::riichiKong(int who, Face tile) const
{
    const SeatTiles& s = seat(who);
    if (s.drawn != tile)
        return false;
    const std::vector<Face> before = without(s.concealed, tile, 1);
    const std::vector<Face> waits = waitingTiles(*m_rules, before, s.melds);
    std::vector<Meld> melds = s.melds;
    melds.push_back({ SetKind::Kong, tile, true });
    if (waits.empty() || waitingTiles(*m_rules, without(before, tile, 3), melds) != waits)
        return false;
    const int needed = 4 - int(s.melds.size());
    for (Face w : waits) {
        std::vector<Face> whole = before;
        insertSorted(whole, w);
        if (!pungInEveryReading(whole, needed, tile))
            return false;
    }
    return true;
}

bool Hand::canThrowAfter(const std::vector<Face>& rest, const std::vector<Face>& barred) const
{
    if (!m_table.noSwapCalling)
        return true;
    return std::any_of(rest.begin(), rest.end(),
                       [&barred](Face t) { return std::find(barred.begin(), barred.end(), t) == barred.end(); });
}

std::vector<Move> Hand::turnMoves(int seat) const
{
    std::vector<Move> moves;
    const SeatTiles& s = this->seat(seat);
    const auto st = std::uint8_t(seat);
    const bool declared = m_riichi[std::size_t(seat)];
    const auto win = winFor(seat);
    if (win && win->second.qualifying >= m_rules->minimumToWin())
        moves.push_back({ st, Action::Win, s.drawn, 0 });
    const bool afterADraw = !m_table.kongsAfterADraw || s.drawn != 0;
    if (wallLeft() > 0 && kongRoom() && afterADraw) {
        for (Face t = 1; t < kFlowerFirst; ++t) {
            if (countOf(s.concealed, t) == 4 && (!declared || riichiKong(seat, t)))
                moves.push_back({ st, Action::ConcealedKong, t, 0 });
            const bool pung = std::any_of(s.melds.begin(), s.melds.end(), [t](const Meld& m) {
                return m.kind == SetKind::Pung && m.tile == t;
            });
            // Only the tile just drawn may be added to a pung, unless the rule
            // set lets any tile held be (GHUB-0244 § 4.4).
            const bool tileToAdd = m_table.addAHeldTile ? countOf(s.concealed, t) > 0 : s.drawn == t;
            if (pung && tileToAdd && !declared)
                moves.push_back({ st, Action::AddedKong, t, 0 });
        }
    }
    if (declared) {
        // After riichi the tile drawn is thrown, unless it wins or makes a kong.
        if (s.drawn != 0)
            moves.push_back({ st, Action::Discard, s.drawn, 0 });
        return moves;
    }
    Face last = 0;
    for (Face t : s.concealed) {
        if (t != last && std::find(m_barred.begin(), m_barred.end(), t) == m_barred.end())
            moves.push_back({ st, Action::Discard, t, 0 });
        last = t;
    }
    if (m_table.riichiStake > 0 && nothingClaimed(s.melds) && wallLeft() >= 1) {
        last = 0;
        for (Face t : s.concealed) {
            if (t != last && !waitingTiles(*m_rules, without(s.concealed, t, 1), s.melds).empty())
                moves.push_back({ st, Action::Riichi, t, 0 });
            last = t;
        }
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
    const auto at = std::size_t(who);
    SeatTiles& s = seatAt(who);

    if (m_phase == Phase::Turn) {
        switch (move.action) {
        case Action::Riichi:
        case Action::Discard: {
            const bool declaring = move.action == Action::Riichi;
            // Double riichi: declared on the seat's first throw, with nothing
            // claimed or konged before it.
            const bool first = !m_pastFirstRound && m_thrown[at].empty() && m_claims == 0 && m_kongs == 0;
            takeFrom(who, move.tile, 1);
            s.drawn = 0;
            m_afterKong = false;
            ++m_discards;
            m_thrown[at].push_back(move.tile);
            m_ippatsu[at] = false; // ippatsu lasts to the seat's next throw
            m_barred.clear();
            if (declaring) {
                m_riichi[at] = true;
                m_doubleRiichi[at] = first;
                m_ippatsu[at] = true;
                m_riichiAt[at] = s.pool.size();
                m_pendingStake = who;
            }
            m_offered = move.tile;
            m_offeredBy = who;
            m_lastDiscard = move.tile;
            m_lastDiscarder = who;
            m_lastClaimer = -1;
            m_lastClaim = Action::Pass;
            openClaims();
            return true;
        }
        case Action::Win:
            finish({ who }, 0, -1, false);
            return true;
        case Action::ConcealedKong:
            s.drawn = 0;
            if (m_table.robConcealedKong) {
                openRob(who, move.tile, true);
                return true;
            }
            takeFrom(who, move.tile, 4);
            s.melds.push_back({ SetKind::Kong, move.tile, true });
            ++m_kongs;
            finishKong(who);
            return true;
        case Action::AddedKong:
            s.drawn = 0;
            openRob(who, move.tile, false);
            return true;
        default: break;
        }
        return false;
    }

    // Claims and Robbing: an answer, settled once every seat has given one.
    m_answers[at] = move;
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
    m_drewBonus = false;
    for (;;) {
        if (wallLeft() == 0)
            return false;
        const Face t = fromBack ? m_wall[std::size_t(m_back--)] : m_wall[std::size_t(m_front++)];
        if (isBonus(t)) {
            // Set aside face up, and replaced from the back at once.
            s.bonus.push_back(t);
            m_drewBonus = true;
            fromBack = true;
            continue;
        }
        insertSorted(s.concealed, t);
        s.drawn = t;
        m_passed[std::size_t(seat)] = false; // a draw ends furiten for a pass
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
        m_completes[std::size_t(o)] = false;
        if (o == from)
            continue;
        const auto st = std::uint8_t(o);
        const auto w = winOn(o, t, from, false);
        m_completes[std::size_t(o)] = w.has_value();
        if (w && w->second.qualifying >= m_rules->minimumToWin() && mayWinOn(o))
            opts.push_back({ st, Action::Win, t, 0 });
        // With the wall empty a set claimed now could never be followed by a
        // draw, so only a win may take the last discard. After riichi a seat
        // claims nothing but a win.
        if (more && !m_riichi[std::size_t(o)]) {
            const std::vector<Face>& held = seat(o).concealed;
            const int count = countOf(held, t);
            if (count >= 3 && kongRoom())
                opts.push_back({ st, Action::Kong, t, 0 });
            if (count >= 2 && canThrowAfter(without(held, t, 2), { t }))
                opts.push_back({ st, Action::Pung, t, 0 });
            if (o == next && m_rules->chowAllowed() && numbered(t)) {
                const int rank = rankOf(t);
                for (int low = std::max(1, rank - 2); low <= std::min(7, rank); ++low) {
                    const Face first = Face(t - (rank - low));
                    std::vector<Face> rest = held;
                    bool have = true;
                    for (int k = 0; k < 3; ++k) {
                        const Face need = Face(first + k);
                        if (need == t)
                            continue;
                        if (countOf(rest, need) == 0)
                            have = false;
                        rest = without(rest, need, 1);
                    }
                    if (have && canThrowAfter(rest, { t, otherEnd(t, first) }))
                        opts.push_back({ st, Action::Chow, t, std::uint8_t(first) });
                }
            }
        }
        if (!opts.empty())
            opts.push_back({ st, Action::Pass, 0, 0 });
    }
    m_phase = Phase::Claims;
    if (waitingOn().empty()) {
        notePassed();
        stake();
        passOn();
    }
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

std::vector<int> Hand::winners(int from) const
{
    std::vector<int> seats;
    for (int i = 1; i < kSeats; ++i) {
        const int o = (from + i) % kSeats;
        const auto& a = m_answers[std::size_t(o)];
        if (a && a->action == Action::Win) {
            seats.push_back(o);
            if (!m_table.severalWinners)
                break;
        }
    }
    return seats;
}

void Hand::notePassed()
{
    if (!m_table.furiten)
        return;
    for (std::size_t o = 0; o < kSeats; ++o) {
        if (!m_completes[o])
            continue;
        (m_riichi[o] ? m_passedAfterRiichi[o] : m_passed[o]) = true;
        m_completes[o] = false;
    }
}

void Hand::stake()
{
    if (m_pendingStake < 0)
        return;
    m_stakes[std::size_t(m_pendingStake)] += m_table.riichiStake;
    m_sticks += m_table.riichiStake;
    m_pendingStake = -1;
}

void Hand::noteLiability(int who, int from)
{
    if (!m_table.liability)
        return;
    int dragons = 0;
    int winds = 0;
    for (const Meld& m : seat(who).melds) {
        if (m.concealed || m.kind == SetKind::Chow)
            continue;
        dragons += suitOf(m.tile) == Suit::Dragon ? 1 : 0;
        winds += suitOf(m.tile) == Suit::Wind ? 1 : 0;
    }
    const Suit claimed = suitOf(seat(who).melds.back().tile);
    if ((claimed == Suit::Dragon && dragons == 3) || (claimed == Suit::Wind && winds == 4))
        m_liable[std::size_t(who)] = from;
}

void Hand::settleClaims()
{
    const Face t = m_offered;
    const int from = m_offeredBy;
    // Win first, to the first seat after the discarder -- or to every seat
    // that claimed it, where the rule set says so; then a pung or kong; then a
    // chow. Only one seat can hold the pair a pung needs, since the discard
    // is one of the four.
    const std::vector<int> won = winners(from);
    if (!won.empty()) {
        finish(won, t, from, false);
        return;
    }
    notePassed();
    stake();
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
            m_passed[std::size_t(o)] = false; // a claim ends furiten for a pass
            m_ippatsu.fill(false);            // any claim ends every ippatsu
            if (wanted == Action::Chow) {
                for (int k = 0; k < 3; ++k)
                    if (Face(a->chowLow + k) != t)
                        takeFrom(o, Face(a->chowLow + k), 1);
                s.melds.push_back({ SetKind::Chow, Face(a->chowLow), false });
            } else {
                takeFrom(o, t, wanted == Action::Kong ? 3 : 2);
                s.melds.push_back({ wanted == Action::Kong ? SetKind::Kong : SetKind::Pung, t, false });
                noteLiability(o, from);
            }
            s.drawn = 0;
            if (wanted == Action::Kong) {
                ++m_kongs;
                finishKong(o);
            } else {
                if (m_table.noSwapCalling)
                    m_barred = { t, wanted == Action::Chow ? otherEnd(t, Face(a->chowLow)) : Face(0) };
                startTurn(o);
            }
            return;
        }
    }
    passOn();
}

// Anyone waiting on a kong's tile may take it before the kong stands: on an
// added kong, any winning hand; on a concealed one, only Thirteen orphans.
// While it may be robbed, a concealed kong lies as a concealed pung beside
// the tile on offer, as an added kong's pung does.
void Hand::openRob(int who, Face tile, bool concealedKong)
{
    if (concealedKong) {
        takeFrom(who, tile, 4);
        seatAt(who).melds.push_back({ SetKind::Pung, tile, true });
    } else {
        takeFrom(who, tile, 1);
    }
    m_offered = tile;
    m_offeredBy = who;
    for (int o = 0; o < kSeats; ++o) {
        const auto at = std::size_t(o);
        m_options[at].clear();
        m_answers[at].reset();
        m_completes[at] = false;
        if (o == who)
            continue;
        const auto w = winOn(o, tile, who, true);
        if (!w || (concealedKong && w->first.shape != Shape::ThirteenOrphans))
            continue;
        m_completes[at] = true;
        if (w->second.qualifying >= m_rules->minimumToWin() && mayWinOn(o))
            m_options[at] = { { std::uint8_t(o), Action::Win, tile, 0 }, { std::uint8_t(o), Action::Pass, 0, 0 } };
    }
    m_phase = Phase::Robbing;
    if (waitingOn().empty())
        settleRob();
}

void Hand::settleRob()
{
    const Face t = m_offered;
    const int konger = m_offeredBy;
    const std::vector<int> won = winners(konger);
    if (!won.empty()) {
        finish(won, t, konger, true);
        return;
    }
    notePassed();
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
    // Each kong that stands turns the next kan dora indicator face up before
    // its replacement is drawn, and ends every ippatsu.
    if (m_table.doraIndicators)
        m_revealed = std::min(m_revealed + 1, 5);
    m_ippatsu.fill(false);
    // A kong is four tiles in a set of three, so it is made up from the back.
    if (!drawInto(seat, true)) {
        drawHand();
        return;
    }
    startTurn(seat);
    // A tile that replaced a flower is the flower's replacement, not the
    // kong's (GHUB-0243 § 4.3).
    m_afterKong = !m_drewBonus;
}

void Hand::finish(const std::vector<int>& seats, Face tile, int payer, bool robbing)
{
    m_result = {};
    // A win on the declaring throw voids the riichi: nothing is staked.
    m_pendingStake = -1;
    for (std::size_t i = 0; i < seats.size(); ++i) {
        const int who = seats[i];
        const auto w = tile == 0 ? winFor(who) : winOn(who, tile, payer, robbing);
        if (!w)
            continue;
        const std::array<int, 4> pay = m_rules->settle(w->first, w->second);
        for (std::size_t p = 0; p < kSeats; ++p)
            m_result.payments[p] += pay[p];
        if (i == 0) {
            m_result.win = w->first;
            m_result.score = w->second;
        } else {
            m_result.alsoWon.push_back(*w);
        }
    }
    // The tile joins the first winner's hand; any other winner's is in its
    // WinContext::tiles.
    if (tile != 0)
        insertSorted(seatAt(seats.front()).concealed, tile);
    for (std::size_t p = 0; p < kSeats; ++p)
        m_result.payments[p] -= m_stakes[p];
    // Each winner's own stake from this hand returns to it; the rest of the
    // sticks go to the first winner.
    for (int who : seats) {
        m_result.payments[std::size_t(who)] += m_stakes[std::size_t(who)];
        m_sticks -= m_stakes[std::size_t(who)];
    }
    m_result.payments[std::size_t(seats.front())] += m_sticks;
    m_sticks = 0;
    m_result.sticksAfter = m_sticks;
    m_offered = 0;
    m_phase = Phase::Over;
}

void Hand::drawHand()
{
    m_result = {};
    m_result.drawn = true;
    for (int s = 0; s < kSeats; ++s)
        m_result.ready[std::size_t(s)] = !waitingTiles(*m_rules, seat(s).concealed, seat(s).melds).empty();
    m_result.payments = m_rules->settleDraw(m_result.ready);
    for (std::size_t p = 0; p < kSeats; ++p)
        m_result.payments[p] -= m_stakes[p];
    m_result.sticksAfter = m_sticks;
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
    m_totals.fill(ruleSet(rules).table().startingPoints);
}

bool Game::over() const
{
    const int rounds = m_length == Length::EastRound ? 1 : ruleSet(m_rules).table().fullGameRounds;
    return !handInPlay() && m_dealsPassed >= rounds * kSeats;
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

std::array<int, kSeats> Game::finalScores() const
{
    return ruleSet(m_rules).finalScores(m_totals);
}

bool Game::startHand(std::uint32_t seed)
{
    if (handInPlay() || over())
        return false;
    if (m_hand)
        m_finished.push_back({ m_hand->seed(), m_hand->moves() });
    m_hand.emplace(ruleSet(m_rules), seed, m_dealer, prevailingWind(), HandStart { m_totals, m_counters, m_sticks });
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
    const RuleSet& rules = ruleSet(m_rules);
    const HandResult& r = m_hand->result();
    m_results.push_back(r);
    for (int s = 0; s < kSeats; ++s)
        m_totals[std::size_t(s)] += r.payments[std::size_t(s)];
    m_sticks = r.sticksAfter;
    HandEnd end;
    end.drawn = r.drawn;
    end.dealerWon = !r.drawn
        && (r.win.winner == m_dealer
            || std::any_of(r.alsoWon.begin(), r.alsoWon.end(),
                           [this](const auto& w) { return w.first.winner == m_dealer; }));
    end.dealerReady = r.drawn && r.ready[std::size_t(m_dealer)];
    m_counters = rules.countersAfter(m_counters, end);
    // The rule set says whether the dealer keeps the deal; every fourth pass
    // the prevailing wind moves with it.
    if (!rules.dealerKeepsDeal(end)) {
        m_dealer = (m_dealer + 1) % kSeats;
        ++m_dealsPassed;
    }
    // The sticks left at the end go to the top total, split evenly and
    // rounded down on a tie.
    if (over() && m_sticks > 0) {
        const std::vector<int> top = leaders();
        for (int s : top)
            m_totals[std::size_t(s)] += m_sticks / int(top.size());
        m_sticks = 0;
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
