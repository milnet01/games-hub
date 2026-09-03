#pragma once

#include "dealseed.h"

#include "cards/card.h"

#include <QDataStream>

#include <array>
#include <random>
#include <utility>
#include <vector>

// Hearts for four: one human (seat 0) and three computer opponents. No Qt, so
// the whole rule set is testable headlessly.
class HeartsEngine
{
public:
    static constexpr int kPlayers = 4;
    static constexpr int kTargetScore = 100;
    // Thirteen each from a single pack. save()/load() check the count back.
    static constexpr int kCardsPerHand = 13;

    enum class Phase { Passing, Playing, HandOver, GameOver };
    // Passing rotates left, right, across, then a hand with no pass at all.
    enum class PassDirection { Left, Right, Across, Hold };

    HeartsEngine();

    void newGame();

    Phase phase() const { return m_phase; }
    int handNumber() const { return m_hand; }
    PassDirection passDirection() const;
    bool heartsBroken() const { return m_heartsBroken; }
    int currentPlayer() const { return m_current; }
    int trickLeader() const { return m_leader; }
    int lastTrickWinner() const { return m_lastWinner; }
    int tricksPlayed() const { return m_tricksPlayed; }

    // Every seat accessor guards first. They are public and the seat is
    // supplied by the caller, so an out-of-range index is undefined behaviour
    // rather than a wrong answer -- and the view computes some of these seats
    // from a hit test (GHUB-0160).
    static bool seatExists(int player) { return player >= 0 && player < kPlayers; }

    const std::vector<Card>& hand(int player) const
    {
        static const std::vector<Card> none;
        return seatExists(player) ? m_hands[std::size_t(player)] : none;
    }
    const std::vector<std::pair<int, Card>>& trick() const { return m_trick; }
    int total(int player) const { return seatExists(player) ? m_totals[std::size_t(player)] : 0; }
    int handPoints(int player) const
    {
        return seatExists(player) ? m_handPoints[std::size_t(player)] : 0;
    }

    // Passing phase
    void setPass(int player, const std::vector<Card>& cards);
    bool passReady(int player) const
    {
        return seatExists(player) && m_passes[std::size_t(player)].size() == 3;
    }
    const std::vector<Card>& pass(int player) const
    {
        static const std::vector<Card> none;
        return seatExists(player) ? m_passes[std::size_t(player)] : none;
    }
    // Moves every player's three cards at once and starts the play phase.
    void executePass();

    // Play phase
    std::vector<Card> legalPlays(int player) const;
    bool isLegal(int player, const Card& c) const;
    // Plays c for player. Returns false if it is not that player's turn or the
    // card is not legal. Completing a trick leaves it on the table so the UI
    // can show it; the next play clears it.
    bool playCard(int player, const Card& c);
    bool trickComplete() const { return m_trick.size() == kPlayers; }
    // Clears a finished trick and hands the lead to its winner.
    void collectTrick();

    // Computer decisions
    std::vector<Card> chooseAiPass(int player) const;
    Card chooseAiCard(int player) const;

    // Starts the next hand after HandOver.
    void nextHand();

    // Winner once the game is over: the lowest total. On a shared low it is the
    // FIRST such seat, which is seat 0 -- the human -- so ask winnerIsShared()
    // before announcing one. Without that a tie was announced as a win and
    // recorded as a best score (GHUB-0160).
    int winner() const;
    bool winnerIsShared() const;

    // The whole position: hands, the cards chosen for the pass, the trick on
    // the table, both score columns and every flag that decides what is legal
    // next. Hearts keeps no move log, so -- like Canasta and unlike Chess --
    // its save is the position itself and the pack is what re-checks it
    // (CLAUDE.md § "A game with no move log saves the table").
    //
    // Written through QDataStream, so the format IS the member order below.
    // Adding a member means adding it at the END and bumping the view's
    // version, never reordering.
    void save(QDataStream& out) const;
    // False on anything the rules could not have produced, and then nothing is
    // written to this engine: load() builds the position separately and only
    // adopts it once every check has passed, so a corrupt blob leaves the game
    // already on screen alone.
    bool load(QDataStream& in);

private:
    void deal();
    void scoreHand();
    static int pointsFor(const Card& c);
    void removeCard(int player, const Card& c);
    bool hasSuit(int player, Suit s) const;

    std::array<std::vector<Card>, kPlayers> m_hands;
    std::array<std::vector<Card>, kPlayers> m_passes;
    std::array<int, kPlayers> m_totals {};
    std::array<int, kPlayers> m_handPoints {};

    std::vector<std::pair<int, Card>> m_trick;
    Phase m_phase = Phase::Passing;
    int m_hand = 0;
    int m_current = 0;
    int m_leader = 0;
    int m_lastWinner = -1;
    int m_tricksPlayed = 0;
    bool m_heartsBroken = false;
    std::mt19937 m_rng { dealSeed() };
};
