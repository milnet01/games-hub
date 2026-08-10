#include "heartsengine.h"

#include <algorithm>

namespace {

const Card kTwoOfClubs { Suit::Clubs, 2, true };
const Card kQueenOfSpades { Suit::Spades, kQueen, true };

// Aces high for trick-taking, so an Ace sorts above a King.
int trickValue(const Card& c)
{
    return c.rank == kAce ? 14 : c.rank;
}

void sortHand(std::vector<Card>& hand)
{
    std::sort(hand.begin(), hand.end(), [](const Card& a, const Card& b) {
        if (a.suit != b.suit)
            return int(a.suit) < int(b.suit);
        return trickValue(a) < trickValue(b);
    });
}

} // namespace

HeartsEngine::HeartsEngine()
{
    newGame();
}

void HeartsEngine::newGame()
{
    m_totals.fill(0);
    m_hand = 0;
    deal();
}

HeartsEngine::PassDirection HeartsEngine::passDirection() const
{
    switch (m_hand % 4) {
    case 0: return PassDirection::Left;
    case 1: return PassDirection::Right;
    case 2: return PassDirection::Across;
    default: return PassDirection::Hold;
    }
}

void HeartsEngine::deal()
{
    std::vector<Card> deck = makeDeck(1, 4);
    shuffleCards(deck, m_rng);

    for (int p = 0; p < kPlayers; ++p) {
        m_hands[std::size_t(p)].assign(deck.begin() + p * 13, deck.begin() + (p + 1) * 13);
        for (Card& c : m_hands[std::size_t(p)])
            c.faceUp = (p == 0);
        sortHand(m_hands[std::size_t(p)]);
        m_passes[std::size_t(p)].clear();
    }

    m_handPoints.fill(0);
    m_trick.clear();
    m_heartsBroken = false;
    m_tricksPlayed = 0;
    m_lastWinner = -1;

    // A hold hand skips passing entirely and goes straight to the lead.
    if (passDirection() == PassDirection::Hold) {
        m_phase = Phase::Playing;
        for (int p = 0; p < kPlayers; ++p) {
            if (std::find(m_hands[std::size_t(p)].begin(), m_hands[std::size_t(p)].end(),
                          kTwoOfClubs)
                != m_hands[std::size_t(p)].end()) {
                m_leader = p;
                m_current = p;
                break;
            }
        }
    } else {
        m_phase = Phase::Passing;
    }
}

void HeartsEngine::setPass(int player, const std::vector<Card>& cards)
{
    if (cards.size() > 3)
        return;
    m_passes[std::size_t(player)] = cards;
}

void HeartsEngine::executePass()
{
    if (m_phase != Phase::Passing)
        return;
    for (int p = 0; p < kPlayers; ++p)
        if (!passReady(p))
            return;

    const PassDirection dir = passDirection();
    std::array<std::vector<Card>, kPlayers> incoming;

    for (int p = 0; p < kPlayers; ++p) {
        int target = p;
        switch (dir) {
        case PassDirection::Left:   target = (p + 1) % kPlayers; break;
        case PassDirection::Right:  target = (p + kPlayers - 1) % kPlayers; break;
        case PassDirection::Across: target = (p + 2) % kPlayers; break;
        case PassDirection::Hold:   target = p; break;
        }
        for (const Card& c : m_passes[std::size_t(p)]) {
            removeCard(p, c);
            incoming[std::size_t(target)].push_back(c);
        }
    }

    for (int p = 0; p < kPlayers; ++p) {
        for (Card c : incoming[std::size_t(p)]) {
            c.faceUp = (p == 0);
            m_hands[std::size_t(p)].push_back(c);
        }
        sortHand(m_hands[std::size_t(p)]);
        m_passes[std::size_t(p)].clear();
    }

    // Whoever now holds the two of clubs leads.
    for (int p = 0; p < kPlayers; ++p) {
        if (std::find(m_hands[std::size_t(p)].begin(), m_hands[std::size_t(p)].end(), kTwoOfClubs)
            != m_hands[std::size_t(p)].end()) {
            m_leader = p;
            m_current = p;
            break;
        }
    }
    m_phase = Phase::Playing;
}

void HeartsEngine::removeCard(int player, const Card& c)
{
    std::vector<Card>& hand = m_hands[std::size_t(player)];
    auto it = std::find(hand.begin(), hand.end(), c);
    if (it != hand.end())
        hand.erase(it);
}

bool HeartsEngine::hasSuit(int player, Suit s) const
{
    const std::vector<Card>& hand = m_hands[std::size_t(player)];
    return std::any_of(hand.begin(), hand.end(), [s](const Card& c) { return c.suit == s; });
}

std::vector<Card> HeartsEngine::legalPlays(int player) const
{
    const std::vector<Card>& hand = m_hands[std::size_t(player)];
    if (m_phase != Phase::Playing || player != m_current)
        return {};

    const bool leading = m_trick.empty();
    const bool firstTrick = m_tricksPlayed == 0;

    if (leading) {
        // The very first lead of a hand is always the two of clubs.
        if (firstTrick) {
            if (std::find(hand.begin(), hand.end(), kTwoOfClubs) != hand.end())
                return { kTwoOfClubs };
        }

        std::vector<Card> legal;
        for (const Card& c : hand)
            if (m_heartsBroken || c.suit != Suit::Hearts)
                legal.push_back(c);
        // Only hearts left: they may be led even unbroken.
        return legal.empty() ? hand : legal;
    }

    const Suit lead = m_trick.front().second.suit;
    std::vector<Card> following;
    for (const Card& c : hand)
        if (c.suit == lead)
            following.push_back(c);
    if (!following.empty()) {
        // No blood on the first trick, even when following suit.
        if (firstTrick) {
            std::vector<Card> safe;
            for (const Card& c : following)
                if (pointsFor(c) == 0)
                    safe.push_back(c);
            if (!safe.empty())
                return safe;
        }
        return following;
    }

    // Void in the led suit: anything goes, except points on the first trick.
    if (firstTrick) {
        std::vector<Card> safe;
        for (const Card& c : hand)
            if (pointsFor(c) == 0)
                safe.push_back(c);
        if (!safe.empty())
            return safe;
    }
    return hand;
}

bool HeartsEngine::isLegal(int player, const Card& c) const
{
    const std::vector<Card> legal = legalPlays(player);
    return std::find(legal.begin(), legal.end(), c) != legal.end();
}

int HeartsEngine::pointsFor(const Card& c)
{
    if (c == kQueenOfSpades)
        return 13;
    return c.suit == Suit::Hearts ? 1 : 0;
}

bool HeartsEngine::playCard(int player, const Card& c)
{
    if (m_phase != Phase::Playing || player != m_current || trickComplete())
        return false;
    if (!isLegal(player, c))
        return false;

    removeCard(player, c);
    Card shown = c;
    shown.faceUp = true;
    m_trick.emplace_back(player, shown);

    if (c.suit == Suit::Hearts || c == kQueenOfSpades)
        m_heartsBroken = true;

    if (!trickComplete())
        m_current = (m_current + 1) % kPlayers;
    return true;
}

void HeartsEngine::collectTrick()
{
    if (!trickComplete())
        return;

    const Suit lead = m_trick.front().second.suit;
    int winner = m_trick.front().first;
    int best = trickValue(m_trick.front().second);
    int points = 0;

    for (const auto& [p, c] : m_trick) {
        points += pointsFor(c);
        if (c.suit == lead && trickValue(c) > best) {
            best = trickValue(c);
            winner = p;
        }
    }

    m_handPoints[std::size_t(winner)] += points;
    m_lastWinner = winner;
    m_leader = winner;
    m_current = winner;
    m_trick.clear();
    ++m_tricksPlayed;

    if (m_tricksPlayed == 13)
        scoreHand();
}

void HeartsEngine::scoreHand()
{
    // Shooting the moon: one player takes all 26, so everyone else takes 26
    // instead. Far more fun than the alternative of subtracting.
    int shooter = -1;
    for (int p = 0; p < kPlayers; ++p)
        if (m_handPoints[std::size_t(p)] == 26)
            shooter = p;

    if (shooter >= 0) {
        for (int p = 0; p < kPlayers; ++p)
            m_totals[std::size_t(p)] += (p == shooter) ? 0 : 26;
    } else {
        for (int p = 0; p < kPlayers; ++p)
            m_totals[std::size_t(p)] += m_handPoints[std::size_t(p)];
    }

    const bool over = std::any_of(m_totals.begin(), m_totals.end(),
                                  [](int t) { return t >= kTargetScore; });
    m_phase = over ? Phase::GameOver : Phase::HandOver;
}

void HeartsEngine::nextHand()
{
    if (m_phase != Phase::HandOver)
        return;
    ++m_hand;
    deal();
}

int HeartsEngine::winner() const
{
    return int(std::min_element(m_totals.begin(), m_totals.end()) - m_totals.begin());
}

// ---------------------------------------------------------------------------
// Computer play
// ---------------------------------------------------------------------------

std::vector<Card> HeartsEngine::chooseAiPass(int player) const
{
    std::vector<Card> hand = m_hands[std::size_t(player)];

    // Rank each card by how much trouble it is worth shedding. High spades are
    // the priority: the Queen alone is half the points in the hand.
    std::sort(hand.begin(), hand.end(), [](const Card& a, const Card& b) {
        auto danger = [](const Card& c) {
            if (c == kQueenOfSpades)
                return 1000;
            if (c.suit == Suit::Spades && trickValue(c) > trickValue(kQueenOfSpades))
                return 900 + trickValue(c);
            if (c.suit == Suit::Hearts)
                return 400 + trickValue(c);
            return trickValue(c) * 10;
        };
        return danger(a) > danger(b);
    });

    hand.resize(std::min<std::size_t>(3, hand.size()));
    return hand;
}

Card HeartsEngine::chooseAiCard(int player) const
{
    const std::vector<Card> legal = legalPlays(player);
    if (legal.empty())
        return {};

    const bool leading = m_trick.empty();

    if (leading) {
        // Lead the lowest card available; low leads rarely win tricks, which
        // is the whole aim.
        return *std::min_element(legal.begin(), legal.end(), [](const Card& a, const Card& b) {
            const int pa = pointsFor(a) * 20 + trickValue(a);
            const int pb = pointsFor(b) * 20 + trickValue(b);
            return pa < pb;
        });
    }

    const Suit lead = m_trick.front().second.suit;
    int highest = 0;
    for (const auto& [p, c] : m_trick)
        if (c.suit == lead)
            highest = std::max(highest, trickValue(c));

    const bool mustFollow = legal.front().suit == lead && legal.back().suit == lead;
    const bool lastToPlay = m_trick.size() == kPlayers - 1;
    int trickPoints = 0;
    for (const auto& [p, c] : m_trick)
        trickPoints += pointsFor(c);

    if (mustFollow) {
        // Duck under the current winner when we can.
        std::vector<Card> under;
        for (const Card& c : legal)
            if (trickValue(c) < highest)
                under.push_back(c);

        if (!under.empty()) {
            // Playing last with a clean trick, take it as cheaply as possible
            // rather than wasting a high card.
            return *std::max_element(under.begin(), under.end(), [](const Card& a, const Card& b) {
                return trickValue(a) < trickValue(b);
            });
        }
        if (lastToPlay && trickPoints == 0) {
            // Nothing to lose: dump the highest card and keep the low ones.
            return *std::max_element(legal.begin(), legal.end(), [](const Card& a, const Card& b) {
                return trickValue(a) < trickValue(b);
            });
        }
        return *std::min_element(legal.begin(), legal.end(), [](const Card& a, const Card& b) {
            return trickValue(a) < trickValue(b);
        });
    }

    // Void in the led suit — the moment to unload the dangerous cards.
    return *std::max_element(legal.begin(), legal.end(), [](const Card& a, const Card& b) {
        auto value = [](const Card& c) {
            if (c == kQueenOfSpades)
                return 1000;
            if (c.suit == Suit::Spades && trickValue(c) > trickValue(kQueenOfSpades))
                return 800 + trickValue(c);
            if (c.suit == Suit::Hearts)
                return 300 + trickValue(c);
            return trickValue(c);
        };
        return value(a) < value(b);
    });
}
