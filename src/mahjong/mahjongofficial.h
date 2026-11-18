#pragma once

#include "mahjongrules.h"

// Chinese Official rules (GHUB-0243): the 81 patterns of the 2006 Mahjong
// Competition Rules, 8 points to win with the flowers left out, no limit, the
// book's payment and a deal that passes every hand. The contract is
// docs/specs/GHUB-0243-chinese-official-mahjong.md; § 4.4 is the fan list and
// § 4.5 how a reading is counted. No Qt and no words, as mahjongrules.h.
namespace mahjong {

class ChineseOfficialRules : public RuleSet
{
public:
    static constexpr int kMinimum = 8;
    // What every loser pays the winner on top of the hand (book § 3.9.1).
    static constexpr int kBase = 8;

    RuleSetId id() const override { return RuleSetId::ChineseOfficial; }
    bool chowAllowed() const override { return true; }
    bool bonusTilesInPlay() const override { return true; }
    int minimumToWin() const override { return kMinimum; }
    // There is no limit; 0 says so.
    int limit() const override { return 0; }
    // A hand's points are its fan: the book has no table between them.
    int points(int fan) const override { return fan; }
    HandScore score(const WinContext& win) const override;
    std::vector<ScoreItem> fanList() const override;
    std::array<int, 4> settle(const WinContext& win, const HandScore& s) const override;
    // Seven different pairs is Riichi's; Seven pairs already holds it here.
    bool shapeAllowed(Shape shape) const override { return shape != Shape::SevenDifferentPairs; }
    bool dealerKeepsDeal(const HandEnd&) const override { return false; }

    // § 4.4's points for one pattern, 0 for a Fan this rule set never scores.
    static int fanValue(Fan fan);
    // The patterns `fan`'s row says are not scored with it.
    static const std::vector<Fan>& excludes(Fan fan);
};

} // namespace mahjong
