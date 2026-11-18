#include "mahjongtableview.h"

// The words for every scoring pattern of four-player Mahjong: a name for the
// end of a hand, and one plain sentence for the rules text (GHUB-0243 § 4.8).
// A Fan means one thing under every rule set that scores it, so each has one
// name and one sentence here. The third suit is drawn as diamonds, so the
// sentences say diamonds where the rule book says characters.

QString MahjongTableView::fanName(mahjong::Fan fan)
{
    using mahjong::Fan;
    switch (fan) {
    case Fan::SelfDrawn: return tr("Self-drawn");
    case Fan::ConcealedHand: return tr("Concealed hand");
    case Fan::NoBonusTiles: return tr("No flowers or seasons");
    case Fan::OwnFlower: return tr("Own flower");
    case Fan::OwnSeason: return tr("Own season");
    case Fan::AllFourFlowers: return tr("All four flowers");
    case Fan::AllFourSeasons: return tr("All four seasons");
    case Fan::AllChows: return tr("All chows");
    case Fan::DragonPung: return tr("Pung of dragons");
    case Fan::SeatWindPung: return tr("Pung of your own wind");
    case Fan::PrevailingWindPung: return tr("Pung of the round's wind");
    case Fan::LastTile: return tr("Drew the last tile of the wall to win");
    case Fan::KongReplacement: return tr("Won on a kong's replacement tile");
    case Fan::RobbingAKong: return tr("Robbing a kong");
    case Fan::AllPungs: return tr("All pungs");
    case Fan::MixedOneSuit: return tr("Mixed one suit");
    case Fan::SmallThreeDragons: return tr("Small three dragons");
    case Fan::PureOneSuit: return tr("Pure one suit");
    case Fan::BigThreeDragons: return tr("Big three dragons");
    case Fan::SmallFourWinds: return tr("Small four winds");
    case Fan::BigFourWinds: return tr("Big four winds");
    case Fan::AllHonours: return tr("All honours");
    case Fan::AllTerminals: return tr("All ones and nines");
    case Fan::ThirteenOrphans: return tr("Thirteen orphans");
    case Fan::NineGates: return tr("Nine gates");
    case Fan::FourConcealedPungs: return tr("Four concealed pungs");
    case Fan::FourKongs: return tr("Four kongs");
    case Fan::HeavenlyHand: return tr("Heavenly hand");
    case Fan::EarthlyHand: return tr("Earthly hand");
    case Fan::AllGreen: return tr("All green");
    case Fan::SevenShiftedPairs: return tr("Seven shifted pairs");
    case Fan::PureTerminalChows: return tr("Pure terminal chows");
    case Fan::QuadrupleChow: return tr("Quadruple chow");
    case Fan::FourPureShiftedPungs: return tr("Four shifted pungs");
    case Fan::FourPureShiftedChows: return tr("Four shifted chows");
    case Fan::ThreeKongs: return tr("Three kongs");
    case Fan::AllTerminalsAndHonours: return tr("All ones, nines and honours");
    case Fan::SevenPairs: return tr("Seven pairs");
    case Fan::GreaterHonoursKnitted: return tr("Greater honours and knitted tiles");
    case Fan::AllEvenPungs: return tr("All even pungs");
    case Fan::PureTripleChow: return tr("Pure triple chow");
    case Fan::PureShiftedPungs: return tr("Three shifted pungs");
    case Fan::UpperTiles: return tr("Upper tiles");
    case Fan::MiddleTiles: return tr("Middle tiles");
    case Fan::LowerTiles: return tr("Lower tiles");
    case Fan::PureStraight: return tr("Pure straight");
    case Fan::ThreeSuitedTerminalChows: return tr("Three-suited terminal chows");
    case Fan::PureShiftedChows: return tr("Three shifted chows");
    case Fan::AllFives: return tr("All fives");
    case Fan::TriplePung: return tr("Triple pung");
    case Fan::ThreeConcealedPungs: return tr("Three concealed pungs");
    case Fan::LesserHonoursKnitted: return tr("Lesser honours and knitted tiles");
    case Fan::KnittedStraight: return tr("Knitted straight");
    case Fan::UpperFour: return tr("Upper four");
    case Fan::LowerFour: return tr("Lower four");
    case Fan::BigThreeWinds: return tr("Big three winds");
    case Fan::MixedStraight: return tr("Mixed straight");
    case Fan::ReversibleTiles: return tr("Reversible tiles");
    case Fan::MixedTripleChow: return tr("Mixed triple chow");
    case Fan::MixedShiftedPungs: return tr("Mixed shifted pungs");
    case Fan::ChickenHand: return tr("Chicken hand");
    case Fan::LastTileClaim: return tr("Won on the last discard");
    case Fan::TwoConcealedKongs: return tr("Two concealed kongs");
    case Fan::MixedShiftedChows: return tr("Mixed shifted chows");
    case Fan::AllTypes: return tr("All types");
    case Fan::MeldedHand: return tr("Melded hand");
    case Fan::TwoDragonPungs: return tr("Two dragon pungs");
    case Fan::OutsideHand: return tr("Outside hand");
    case Fan::FullyConcealed: return tr("Fully concealed hand");
    case Fan::TwoMeldedKongs: return tr("Two melded kongs");
    case Fan::LastOfItsKind: return tr("Last of its kind");
    case Fan::AllChowsNoHonours: return tr("All chows, no honours");
    case Fan::TileHog: return tr("Tile hog");
    case Fan::DoublePung: return tr("Double pung");
    case Fan::TwoConcealedPungs: return tr("Two concealed pungs");
    case Fan::ConcealedKong: return tr("Concealed kong");
    case Fan::AllSimples: return tr("All simples");
    case Fan::PureDoubleChow: return tr("Pure double chow");
    case Fan::MixedDoubleChow: return tr("Mixed double chow");
    case Fan::ShortStraight: return tr("Short straight");
    case Fan::TwoTerminalChows: return tr("Two terminal chows");
    case Fan::TerminalOrHonourPung: return tr("Pung of ones, nines or a wind");
    case Fan::MeldedKong: return tr("Melded kong");
    case Fan::OneVoidedSuit: return tr("One suit missing");
    case Fan::NoHonours: return tr("No honours");
    case Fan::EdgeWait: return tr("Edge wait");
    case Fan::ClosedWait: return tr("Closed wait");
    case Fan::SingleWait: return tr("Single wait");
    case Fan::Flower: return tr("Flower or season");
    }
    return {};
}

QString MahjongTableView::fanMeaning(mahjong::Fan fan)
{
    using mahjong::Fan;
    switch (fan) {
    case Fan::SelfDrawn: return tr("Won on a tile you drew from the wall.");
    case Fan::ConcealedHand: return tr("No set laid down, and won on a tile another player threw.");
    case Fan::NoBonusTiles: return tr("No flowers or seasons set aside.");
    case Fan::OwnFlower: return tr("The flower numbered for your seat: East 1, South 2, West 3, North 4.");
    case Fan::OwnSeason: return tr("The season numbered for your seat: East 1, South 2, West 3, North 4.");
    case Fan::AllFourFlowers: return tr("All four flowers set aside.");
    case Fan::AllFourSeasons: return tr("All four seasons set aside.");
    case Fan::AllChows: return tr("Four chows, and a pair that scores nothing.");
    case Fan::DragonPung: return tr("A pung or kong of one dragon. Each one counts.");
    case Fan::SeatWindPung: return tr("A pung or kong of your own seat's wind.");
    case Fan::PrevailingWindPung: return tr("A pung or kong of the round's wind.");
    case Fan::LastTile: return tr("Won on the last tile of the wall, drawn yourself.");
    case Fan::KongReplacement: return tr("Won on the tile drawn to replace a kong.");
    case Fan::RobbingAKong: return tr("Won on a tile another player added to a pung to make a kong.");
    case Fan::AllPungs: return tr("Four pungs or kongs, and a pair.");
    case Fan::MixedOneSuit: return tr("Tiles of one suit, with winds or dragons.");
    case Fan::SmallThreeDragons: return tr("Pungs of two dragons, and a pair of the third.");
    case Fan::PureOneSuit: return tr("Every tile of one suit, with no winds or dragons.");
    case Fan::BigThreeDragons: return tr("Pungs of all three dragons.");
    case Fan::SmallFourWinds: return tr("Pungs of three winds, and a pair of the fourth.");
    case Fan::BigFourWinds: return tr("Pungs of all four winds.");
    case Fan::AllHonours: return tr("Only winds and dragons.");
    case Fan::AllTerminals: return tr("Only ones and nines.");
    case Fan::ThirteenOrphans:
        return tr("One each of every one, nine, wind and dragon, and a second of any of them.");
    case Fan::NineGates:
        return tr("One, one, one, two to eight, nine, nine, nine of one suit, held concealed, and one more "
                  "tile of that suit.");
    case Fan::FourConcealedPungs: return tr("Four pungs or kongs, none of them claimed.");
    case Fan::FourKongs: return tr("Four kongs.");
    case Fan::HeavenlyHand: return tr("The dealer wins on the tiles dealt.");
    case Fan::EarthlyHand: return tr("A player wins on the dealer's first discard.");
    case Fan::AllGreen: return tr("Only the two, three, four, six and eight of bamboo, and the green dragon.");
    case Fan::SevenShiftedPairs: return tr("Seven pairs of one suit, each one higher than the last.");
    case Fan::PureTerminalChows:
        return tr("Two chows of one-two-three and two of seven-eight-nine in one suit, and a pair of fives of it.");
    case Fan::QuadrupleChow: return tr("Four identical chows.");
    case Fan::FourPureShiftedPungs: return tr("Four pungs of one suit, each one higher than the last.");
    case Fan::FourPureShiftedChows:
        return tr("Four chows of one suit, each one higher than the last, or each two higher.");
    case Fan::ThreeKongs: return tr("Three kongs.");
    case Fan::AllTerminalsAndHonours: return tr("Only ones, nines, winds and dragons.");
    case Fan::SevenPairs: return tr("Seven pairs. Four alike count as two pairs.");
    case Fan::GreaterHonoursKnitted:
        return tr("All seven winds and dragons, and seven different tiles of a knitted straight.");
    case Fan::AllEvenPungs: return tr("Pungs and a pair, all of twos, fours, sixes or eights.");
    case Fan::PureTripleChow: return tr("Three identical chows.");
    case Fan::PureShiftedPungs: return tr("Three pungs of one suit, each one higher than the last.");
    case Fan::UpperTiles: return tr("Only sevens, eights and nines.");
    case Fan::MiddleTiles: return tr("Only fours, fives and sixes.");
    case Fan::LowerTiles: return tr("Only ones, twos and threes.");
    case Fan::PureStraight: return tr("One-two-three, four-five-six and seven-eight-nine of one suit.");
    case Fan::ThreeSuitedTerminalChows:
        return tr("One-two-three and seven-eight-nine in two suits, and a pair of fives of the third.");
    case Fan::PureShiftedChows:
        return tr("Three chows of one suit, each one higher than the last, or each two higher.");
    case Fan::AllFives: return tr("A five in every set and in the pair.");
    case Fan::TriplePung: return tr("Pungs of one number in all three suits.");
    case Fan::ThreeConcealedPungs: return tr("Three pungs or kongs, none of them claimed.");
    case Fan::LesserHonoursKnitted:
        return tr("Fourteen different tiles: winds, dragons and tiles of a knitted straight.");
    case Fan::KnittedStraight:
        return tr("One, four and seven of one suit, two, five and eight of another, and three, six and nine of "
                  "the third.");
    case Fan::UpperFour: return tr("Only sixes, sevens, eights and nines.");
    case Fan::LowerFour: return tr("Only ones, twos, threes and fours.");
    case Fan::BigThreeWinds: return tr("Pungs of three winds.");
    case Fan::MixedStraight:
        return tr("One-two-three, four-five-six and seven-eight-nine, each in a different suit.");
    case Fan::ReversibleTiles:
        return tr("Only the one to five, eight and nine of circles, the two, four, five, six, eight and nine "
                  "of bamboo, and the white dragon.");
    case Fan::MixedTripleChow: return tr("The same chow in all three suits.");
    case Fan::MixedShiftedPungs: return tr("A pung in each suit, each one higher than the last.");
    case Fan::ChickenHand: return tr("A winning hand that scores nothing else, flowers aside.");
    case Fan::LastTileClaim: return tr("Won on the tile thrown after the wall's last tile was drawn.");
    case Fan::TwoConcealedKongs: return tr("Two kongs, neither of them claimed.");
    case Fan::MixedShiftedChows: return tr("A chow in each suit, each one higher than the last.");
    case Fan::AllTypes: return tr("Circles, bamboo, diamonds, winds and dragons, all in the hand.");
    case Fan::MeldedHand: return tr("Four sets laid down, and won on a thrown tile that pairs your last tile.");
    case Fan::TwoDragonPungs: return tr("Pungs of two dragons.");
    case Fan::OutsideHand: return tr("A one, a nine, a wind or a dragon in every set and in the pair.");
    case Fan::FullyConcealed: return tr("No set laid down, and won on a tile you drew.");
    case Fan::TwoMeldedKongs: return tr("Two kongs, at least one of them claimed.");
    case Fan::LastOfItsKind: return tr("Won on a tile whose other three are already in view.");
    case Fan::AllChowsNoHonours:
        return tr("Four chows and a pair, with no winds or dragons. A knitted straight counts as three chows.");
    case Fan::TileHog: return tr("All four of one numbered tile in the hand, but not as a kong. Each one counts.");
    case Fan::DoublePung: return tr("Pungs of one number in two suits.");
    case Fan::TwoConcealedPungs: return tr("Two pungs or kongs, neither of them claimed.");
    case Fan::ConcealedKong: return tr("A kong of four tiles you drew yourself.");
    case Fan::AllSimples: return tr("No ones, nines, winds or dragons.");
    case Fan::PureDoubleChow: return tr("Two identical chows.");
    case Fan::MixedDoubleChow: return tr("The same chow in two suits.");
    case Fan::ShortStraight: return tr("Two chows of one suit making six in a row.");
    case Fan::TwoTerminalChows: return tr("One-two-three and seven-eight-nine of one suit.");
    case Fan::TerminalOrHonourPung:
        return tr("A pung or kong of ones, of nines, or of a wind that scores nothing else. Each one counts.");
    case Fan::MeldedKong: return tr("A kong claimed from a thrown tile, or made by adding to a pung.");
    case Fan::OneVoidedSuit: return tr("Tiles of exactly two of the three suits.");
    case Fan::NoHonours: return tr("No winds or dragons.");
    case Fan::EdgeWait: return tr("Waited only on the three of one-two, or the seven of eight-nine.");
    case Fan::ClosedWait: return tr("Waited only on the middle tile of a chow.");
    case Fan::SingleWait: return tr("Waited only on the tile that made the pair.");
    case Fan::Flower:
        return tr("A flower or season set aside. Each one counts, but not toward the 8 points needed to win.");
    }
    return {};
}
