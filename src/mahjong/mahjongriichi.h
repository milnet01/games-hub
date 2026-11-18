#pragma once

#include "mahjongrules.h"

// Japanese Riichi rules (GHUB-0244): the European Mahjong Association's riichi
// rule book, 2025 edition. 136 tiles, a hand needs at least one yaku, and it
// pays from han and minipoints. The contract is
// docs/specs/GHUB-0244-japanese-riichi-mahjong.md; § 4.7 is the yaku list,
// § 4.8 how a reading is counted and § 4.6 who pays. No Qt and no words, as
// mahjongrules.h.
namespace mahjong {

class RiichiRules : public RuleSet
{
public:
    // A yakuman item's value: above any han a § 4.7 row carries.
    static constexpr int kYakuman = 100;
    static constexpr int kStartingPoints = 30000;
    static constexpr int kRiichiStake = 1000;
    // What one counter adds to a win on a discard; a third of it from each
    // payer on a self-draw.
    static constexpr int kCounterOnDiscard = 300;
    static constexpr int kCounterEach = 100;
    // What a drawn hand moves from the seats not ready to the ready ones.
    static constexpr int kNotenTotal = 3000;

    RuleSetId id() const override { return RuleSetId::JapaneseRiichi; }
    bool chowAllowed() const override { return true; }
    bool bonusTilesInPlay() const override { return false; }
    int minimumToWin() const override { return 1; }
    // Points come from han and minipoints together, so there is no fan limit
    // and no one-argument table: basicPoints() is the table.
    int limit() const override { return 0; }
    int points(int han) const override { return basicPoints(han, 30); }
    HandScore score(const WinContext& win) const override;
    std::vector<ScoreItem> fanList() const override;
    std::array<int, 4> settle(const WinContext& win, const HandScore& s) const override;
    bool shapeAllowed(Shape shape) const override
    {
        return shape == Shape::Sets || shape == Shape::SevenDifferentPairs || shape == Shape::ThirteenOrphans;
    }
    TableRules table() const override;
    bool dealerKeepsDeal(const HandEnd& end) const override
    {
        return end.dealerWon || (end.drawn && end.dealerReady);
    }
    int countersAfter(int counters, const HandEnd& end) const override
    {
        return end.dealerWon || end.drawn ? counters + 1 : 0;
    }
    std::array<int, 4> settleDraw(const std::array<bool, 4>& ready) const override;
    std::array<int, 4> finalScores(const std::array<int, 4>& totals) const override;

    // The han a § 4.7 row scores with a set claimed; 0 for a concealed-only row.
    static int openValue(Fan fan);
    // § 4.6's basic points for `han` and the rounded `fu`; 8,000 when `han` is
    // kYakuman.
    static int basicPoints(int han, int fu);
    // The named level `han` and `fu` reach, None below a mangan.
    static Limit tierFor(int han, int fu);
    // The tile an indicator points to: the next of its suit, 9 to 1; red
    // dragon to white, white to green, green to red; the winds in turn.
    static Face doraFrom(Face indicator);
};

} // namespace mahjong
