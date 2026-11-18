#include "mahjongtableview.h"

#include "mahjongart.h"
#include "mahjongofficial.h"
#include "mahjongplayer.h"
#include "mahjongriichi.h"

#include "dealseed.h"
#include "legibility.h"
#include "sound.h"
#include "theme.h"

#include <QActionGroup>
#include <QDataStream>
#include <QDialog>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

#include <algorithm>

using mahjong::Action;
using mahjong::Face;
using mahjong::Move;
using mahjong::Phase;

namespace {

constexpr quint32 kBlobVersion = 1;
// Nearly a second, as Canasta's kAiPause: long enough to see what the
// computer did before the next one moves.
constexpr int kAiPauseMs = 950;
constexpr double kTileAspect = 1.3;
constexpr double kMargin = 10.0;
// How far the tile picked out of your hand stands up, as a share of its
// height: clear of its neighbours, short of the buttons above.
constexpr double kPickedRise = 0.1;
// Ceilings a save is read against, so a corrupt count cannot ask for
// gigabytes. A full game runs to a few dozen hands of a few hundred moves.
constexpr quint32 kMostHands = 512;
constexpr quint32 kMostMoves = 4096;

// How the dora's words are laid beside the indicators.
constexpr int kDoraFlags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap;

constexpr QColor kInk { 0xf4, 0xf0, 0xe0 };
constexpr QColor kInkDim { 0xc8, 0xd8, 0xc8 };
constexpr QColor kPlate { 0x0c, 0x2a, 0x1c, 200 };

const QString& ruleSetKey()
{
    static const QString key = QStringLiteral("mahjong4/ruleSet"); // untranslated: settings key
    return key;
}

const QString& lengthKey()
{
    static const QString key = QStringLiteral("mahjong4/length"); // untranslated: settings key
    return key;
}

// The stored words for a length, never shown.
QString lengthWord(mahjong::Length length)
{
    return length == mahjong::Length::EastRound ? QStringLiteral("east")  // untranslated: settings value
                                                : QStringLiteral("full"); // untranslated: settings value
}

QString ruleSetLabel(mahjong::RuleSetId id)
{
    switch (id) {
    case mahjong::RuleSetId::HongKong: return QCoreApplication::translate("MahjongTableView", "Hong Kong rules");
    case mahjong::RuleSetId::ChineseOfficial:
        return QCoreApplication::translate("MahjongTableView", "Chinese Official rules");
    case mahjong::RuleSetId::JapaneseRiichi:
        return QCoreApplication::translate("MahjongTableView", "Japanese Riichi rules");
    }
    return {};
}

// A Riichi pattern's han. A yakuman item carries RiichiRules::kYakuman, which
// ranks it above every han and is never a count to print.
QString hanText(int han)
{
    if (han >= mahjong::RiichiRules::kYakuman)
        return QCoreApplication::translate("MahjongTableView", "yakuman");
    return QCoreApplication::translate("MahjongTableView", "%n han", nullptr, han);
}

// A pattern's or a hand's value in the rule set's own unit: Hong Kong counts
// fan, Chinese Official points (GHUB-0243 § 4.8), Japanese Riichi han.
QString valueText(mahjong::RuleSetId id, int value)
{
    if (id == mahjong::RuleSetId::JapaneseRiichi)
        return hanText(value);
    if (id != mahjong::RuleSetId::ChineseOfficial)
        return QCoreApplication::translate("MahjongTableView", "%n fan", nullptr, value);
    return value == 1 ? QCoreApplication::translate("MahjongTableView", "1 point")
                      : QCoreApplication::translate("MahjongTableView", "%1 points").arg(value);
}

// Your hand as it is drawn: sorted, with the tile just drawn set apart at the
// end.
std::vector<Face> displayHand(const mahjong::SeatTiles& s)
{
    std::vector<Face> hand = s.concealed;
    if (s.drawn != 0) {
        const auto at = std::find(hand.begin(), hand.end(), s.drawn);
        if (at != hand.end()) {
            hand.erase(at);
            hand.push_back(s.drawn);
        }
    }
    return hand;
}

QString chowLabel(Face low)
{
    // "Three, Four and Five of Bamboo" reads long; the numbers and the suit
    // read the same on a button. One whole string per suit, so a translation
    // can put the words in its own order (GHUB-0250).
    const int r = mahjong::rankOf(low);
    QString text;
    switch (mahjong::suitOf(low)) {
    case mahjong::Suit::Circles: text = QCoreApplication::translate("MahjongTableView", "Chow %1-%2-%3 of Circles"); break;
    case mahjong::Suit::Bamboo: text = QCoreApplication::translate("MahjongTableView", "Chow %1-%2-%3 of Bamboo"); break;
    default: text = QCoreApplication::translate("MahjongTableView", "Chow %1-%2-%3 of Diamonds"); break;
    }
    return text.arg(r).arg(r + 1).arg(r + 2);
}

// A change of points with its sign: "+1500", "-1000", "0".
QString pointsText(int points)
{
    return points > 0 ? QCoreApplication::translate("MahjongTableView", "+%1").arg(points) : QString::number(points);
}

// Whether the hand's last throw came with a riichi declaration.
bool lastThrowDeclared(const mahjong::Hand& h)
{
    const std::vector<Move>& moves = h.moves();
    for (auto it = moves.rbegin(); it != moves.rend(); ++it)
        if (it->action == Action::Discard || it->action == Action::Riichi)
            return it->action == Action::Riichi;
    return false;
}

bool mayThrowFace(const std::vector<Move>& legal, Face face)
{
    return std::any_of(legal.begin(), legal.end(),
                       [face](const Move& m) { return m.action == Action::Discard && m.tile == face; });
}

// Whether a drawn hand moves points: the ready seats paid by the others.
bool drawPays(const mahjong::RuleSet& rules)
{
    const std::array<int, 4> one = rules.settleDraw({ true, false, false, false });
    return std::any_of(one.begin(), one.end(), [](int p) { return p != 0; });
}

// What `from` pays you for a win of `basic` basic points, by the rule set's
// own arithmetic, so the rules text keeps no copy of it that could drift. You
// sit south when you are not the dealer, which makes seat 3 the dealer.
int paidFor(const mahjong::RuleSet& rules, int basic, bool dealerWins, bool selfDrawn, int from)
{
    mahjong::WinContext win;
    win.winner = 0;
    win.seatWind = dealerWins ? 1 : 2;
    win.payer = selfDrawn ? -1 : 1;
    win.selfDrawn = selfDrawn;
    mahjong::HandScore score;
    score.points = basic;
    return -rules.settle(win, score)[std::size_t(from)];
}

QString tierName(mahjong::Limit tier)
{
    switch (tier) {
    case mahjong::Limit::Mangan: return QCoreApplication::translate("MahjongTableView", "Mangan");
    case mahjong::Limit::Haneman: return QCoreApplication::translate("MahjongTableView", "Haneman");
    case mahjong::Limit::Baiman: return QCoreApplication::translate("MahjongTableView", "Baiman");
    case mahjong::Limit::Sanbaiman: return QCoreApplication::translate("MahjongTableView", "Sanbaiman");
    case mahjong::Limit::Yakuman: return QCoreApplication::translate("MahjongTableView", "Yakuman");
    case mahjong::Limit::None: break;
    }
    return {};
}

} // namespace

MahjongTableView::MahjongTableView(QWidget* parent)
    : GameView(parent)
    , m_game(mahjong::RuleSetId::HongKong, mahjong::Length::FullGame)
    , m_rng(dealSeed())
{
    setMinimumSize(MahjongTableView::minimumSizeHint());
    setFocusPolicy(Qt::StrongFocus);
    m_timer = new QTimer(this);
    m_timer->setInterval(Theme::kTurnLightTickMs);
    connect(m_timer, &QTimer::timeout, this, &MahjongTableView::step);

    // Read before the toolbar is built, so the remembered choices are ticked.
    // A value this build does not know falls back to the default.
    QSettings settings;
    const int rules = settings.value(ruleSetKey(), int(mahjong::RuleSetId::HongKong)).toInt();
    m_nextRules = mahjong::knownRuleSet(rules) ? mahjong::RuleSetId(rules) : mahjong::RuleSetId::HongKong;
    m_nextLength = settings.value(lengthKey()).toString() == lengthWord(mahjong::Length::EastRound)
        ? mahjong::Length::EastRound
        : mahjong::Length::FullGame;
    buildActions();
    newGame();
}

void MahjongTableView::buildActions()
{
    auto* newAction = new QAction(tr("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MahjongTableView::newGame);
    m_actions.append(newAction);

    auto* sheet = new QAction(tr("Score sheet"), this);
    connect(sheet, &QAction::triggered, this, &MahjongTableView::showScoreSheet);
    m_actions.append(sheet);

    auto* separator = new QAction(this);
    separator->setSeparator(true);
    m_actions.append(separator);

    // Only the rule sets this build has: GHUB-0244 joins here.
    m_ruleGroup = new QActionGroup(this);
    m_ruleGroup->setExclusive(true);
    for (mahjong::RuleSetId id : mahjong::ruleSets()) {
        auto* a = new QAction(ruleSetLabel(id), this);
        a->setObjectName(QStringLiteral("mahjong4-rules-%1").arg(int(id))); // untranslated: object name
        a->setCheckable(true);
        a->setChecked(id == m_nextRules);
        m_ruleGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, id] { setRuleSet(id); });
        m_actions.append(a);
    }

    auto* separator2 = new QAction(this);
    separator2->setSeparator(true);
    m_actions.append(separator2);

    m_lengthGroup = new QActionGroup(this);
    m_lengthGroup->setExclusive(true);
    const struct {
        const char* name;
        mahjong::Length value;
    } kLengths[] = {
        { QT_TRANSLATE_NOOP("MahjongTableView", "Full game"), mahjong::Length::FullGame },
        { QT_TRANSLATE_NOOP("MahjongTableView", "East round"), mahjong::Length::EastRound },
    };
    for (const auto& entry : kLengths) {
        auto* a = new QAction(tr(entry.name), this);
        a->setObjectName(QStringLiteral("mahjong4-length-%1").arg(lengthWord(entry.value))); // untranslated: object name
        a->setCheckable(true);
        a->setChecked(entry.value == m_nextLength);
        m_lengthGroup->addAction(a);
        const mahjong::Length value = entry.value;
        connect(a, &QAction::triggered, this, [this, value] { setLength(value); });
        m_actions.append(a);
    }
}

void MahjongTableView::setRuleSet(mahjong::RuleSetId id)
{
    QSettings().setValue(ruleSetKey(), int(id));
    m_nextRules = id;
    // The game in progress keeps its rule set: its totals were scored by it.
    refresh();
}

QString MahjongTableView::pendingNote() const
{
    // Both settings, each said while it differs from the game in progress, so
    // changing one never hides the other (GHUB-0249).
    QStringList notes;
    if (m_nextRules != m_game.ruleSetId())
        notes << tr("%1 from the next game.").arg(ruleSetLabel(m_nextRules));
    if (m_nextLength != m_game.length())
        notes << (m_nextLength == mahjong::Length::FullGame ? tr("A full game from the next game.")
                                                            : tr("An East round from the next game."));
    return notes.join(QLatin1Char(' '));
}

void MahjongTableView::setLength(mahjong::Length length)
{
    QSettings().setValue(lengthKey(), lengthWord(length));
    m_nextLength = length;
    refresh();
}

// GHUB-0016. The rules as mahjong::Hand and the rule set in force play them,
// and the controls as mousePressEvent, keyPressEvent and buildActions take
// them. The pattern list comes from the rule set, grouped by value, each with
// the words the end of a hand uses and one plain sentence (GHUB-0243 § 4.8).
QString MahjongTableView::rulesText() const
{
    const mahjong::RuleSetId id = m_game.ruleSetId();
    const mahjong::RuleSet& rules = mahjong::ruleSet(id);
    const mahjong::TableRules table = rules.table();
    const bool official = id == mahjong::RuleSetId::ChineseOfficial;
    const bool riichi = id == mahjong::RuleSetId::JapaneseRiichi;

    std::vector<mahjong::ScoreItem> list = riichi ? std::vector<mahjong::ScoreItem>() : rules.fanList();
    std::stable_sort(list.begin(), list.end(),
                     [](const mahjong::ScoreItem& a, const mahjong::ScoreItem& b) { return a.value > b.value; });
    QString fans = riichi ? riichiYakuList() : QString();
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (i == 0 || list[i].value != list[i - 1].value) {
            if (i > 0)
                fans.append(QStringLiteral("</ul>")); // untranslated: markup
            fans.append(QStringLiteral("<p><b>%1</b></p><ul>") // untranslated: markup
                            .arg(valueText(id, list[i].value).toHtmlEscaped()));
        }
        fans.append(QStringLiteral("<li><b>%1.</b> %2</li>") // untranslated: markup
                        .arg(fanName(list[i].fan).toHtmlEscaped(), fanMeaning(list[i].fan).toHtmlEscaped()));
    }
    if (!list.empty())
        fans.append(QStringLiteral("</ul>")); // untranslated: markup

    QString scoring;
    if (official) {
        scoring = tr("<h3>Scoring</h3>"
                     "<p>Every scoring pattern of a win is written on the table with its points. A hand needs "
                     "at least %1 points to win, not counting flowers. There is no limit to what a hand can "
                     "score.</p>"
                     "<p>When one pattern always includes another, only the bigger one counts. A pattern made "
                     "by combining sets may join a set already used for another such pattern only once.</p>"
                     "<p>Every player who did not win pays the winner %2 points. The player who threw the "
                     "winning tile also pays the hand's points. A win on a tile you drew is paid %2 points "
                     "plus the hand's points by each of the three others. A robbed kong is paid as if the "
                     "player who added the tile had thrown it.</p>"
                     "<p>The deal passes to the next player after every hand, won or drawn. A <b>full "
                     "game</b> is sixteen hands, with the wind of the round moving from East to South, West "
                     "and North every four hands. An <b>East round</b> is four hands.</p>")
                      .arg(rules.minimumToWin())
                      .arg(mahjong::ChineseOfficialRules::kBase);
    } else if (riichi) {
        scoring = riichiScoringText();
    } else {
        // The points table from the rule set, one whole sentence a line, so the
        // text cannot keep a copy that drifts from the scoring (GHUB-0249).
        QString points;
        for (int fan = rules.minimumToWin(); fan <= rules.limit(); ++fan)
            points.append(tr("<li>%n fan is worth %1 points.</li>", nullptr, fan).arg(rules.points(fan)));
        scoring = tr("<h3>Scoring</h3>"
                     "<p>Every scoring pattern of a win is written on the table with its value. A hand needs "
                     "at least %1 fan to win, and %2 fan is the most a hand can score.</p>"
                     "<ul>%3</ul>"
                     "<p>A win on a thrown tile is paid by the player who threw it. A win on a tile you "
                     "drew is paid by all three others. A robbed kong is paid by the player who added the "
                     "tile.</p>"
                     "<p>The dealer is East. The dealer keeps the deal after winning, or after a drawn hand; "
                     "otherwise it passes to the next player. A <b>full game</b> ends when the deal has "
                     "gone round the table four times, with the wind of the round moving from East to "
                     "South, West and North. An <b>East round</b> ends after it has gone round once.</p>")
                      .arg(rules.minimumToWin())
                      .arg(rules.limit())
                      .arg(points);
    }

    QString aim;
    if (official)
        aim = tr("Make a winning hand before the three computer players do. Most winning hands are four sets "
                 "and a pair, from fourteen tiles; seven pairs and the special hands in the list below win "
                 "too. The game is played by %1.")
                  .arg(ruleSetLabel(id));
    else if (riichi)
        aim = tr("Make a winning hand before the three computer players do: four sets and a pair, from "
                 "fourteen tiles, or seven different pairs, or the thirteen orphans in the list below. A "
                 "winning hand also needs a yaku, one of the scoring patterns in that list. The game is "
                 "played by %1.")
                  .arg(ruleSetLabel(id));
    else
        aim = tr("Make a winning hand before the three computer players do: four sets and a pair, from "
                 "fourteen tiles. The game is played by %1.")
                  .arg(ruleSetLabel(id));

    // The sentences of How it plays that differ between rule sets, each read
    // from what the rule set says rather than from its name.
    const bool bonus = rules.bonusTilesInPlay();
    const QString tiles = bonus
        ? tr("<p>The tiles are the ones Mahjong Solitaire uses: circles, bamboo and diamonds "
             "numbered 1 to 9, four winds and three dragons, four of each, and one each of four "
             "flowers and four seasons.</p>")
        : tr("<p>The tiles are the ones Mahjong Solitaire uses: circles, bamboo and diamonds "
             "numbered 1 to 9, four winds and three dragons, four of each. There are no flowers or "
             "seasons.</p>");
    const QString flowers = bonus ? tr("<p>A <b>flower</b> or <b>season</b> is not kept in the hand. It is laid face up "
                                       "in front of you and another tile is drawn in its place.</p>")
                                  : QString();
    const QString twoWins = table.severalWinners
        ? tr("When two or three players win on the same tile, each of them wins, and the player who threw "
             "it pays each one.")
        : tr("Two wins go to the first player after the one who threw.");
    const QString swaps = table.noSwapCalling
        ? tr("<p>After claiming a tile you may not throw that tile again at once, nor, after a chow, the "
             "tile that would make the same run from its other end. The tiles you may not throw are "
             "dimmed.</p>")
        : QString();
    const QString addKong = table.addAHeldTile
        ? tr("You may also add a tile you hold to a pung you laid down.")
        : tr("You may also add the tile you just drew to a pung you laid down.");
    const QString drawn = drawPays(rules)
        ? tr("<p>If the wall runs out with nobody out, the hand is drawn. The players who are ready, one "
             "tile from winning, are paid by those who are not: see <b>A drawn hand</b> below.</p>")
        : tr("<p>If the wall runs out with nobody out, the hand is drawn and no points move.</p>");

    const QString play =
        tr("<h3>The aim</h3>"
           "<p>%1</p>"

           "<h3>The tiles</h3>"
           "%2"

           "<h3>How it plays</h3>"
           "<p>You sit at the bottom of the table. The player on your right plays after you, "
           "then the one across, then the one on your left.</p>"
           "<p>Each player starts with thirteen tiles. On your turn you draw a tile from the "
           "wall, then throw one away. The tile you drew stands a little apart at the right "
           "of your hand.</p>"
           "%3"
           "<p>A set is three of one tile (a <b>pung</b>), four of one tile (a <b>kong</b>), "
           "or three in a row in one suit (a <b>chow</b>).</p>"
           "<p>When someone throws a tile, any other player may <b>claim</b> it: to win, to "
           "make a pung or kong with two or three they hold, or, for the next player only, to "
           "make a chow. A claimed set is laid face up. A win beats a pung or kong, which "
           "beats a chow. %4</p>"
           "%5"
           "<p>Holding four of a tile, you may lay it down as a kong on your turn and draw "
           "another tile. %6 Another "
           "player waiting on that tile may then win on it: that is <b>robbing a kong</b>.</p>"
           "%7"
           "%8"
           "<h3>The scoring patterns</h3>"
           "%9")
            .arg(aim.toHtmlEscaped(), tiles, flowers, twoWins, swaps, addKong, drawn, scoring, fans);

    const QString controls =
        tr("<h3>Controls</h3>"
           "<p><b>With the mouse:</b></p>"
           "<ul>"
           "<li>Click a tile in your hand to pick it out, and click it again to throw it.</li>"
           "<li>Click a button beside your hand to win, claim, make a kong, pass or go on to "
           "the next hand.</li>"
           "</ul>"
           "<p><b>With the keyboard:</b></p>"
           "<ul>"
           "<li>Left and Right move along your hand. Enter or Space throws the tile picked "
           "out.</li>"
           "<li>The number keys press the buttons: 1 the first, 2 the second, and so on. Each "
           "button shows its number.</li>"
           "</ul>"
           "<p>When you can claim a tile the game waits for you, however long you take.</p>"
           "<p>With large play on, a long row of thrown tiles shows the latest ones. The "
           "number before them, such as +2, counts the earlier tiles not drawn.</p>"
           "<p><b>On the toolbar:</b></p>"
           "<ul>"
           "<li><b>New Game</b> (Ctrl+N) starts again from the first hand.</li>"
           "<li><b>Score sheet</b> lists every hand of this game and the running totals.</li>"
           "<li>The rule set and <b>Full game</b> or <b>East round</b> are remembered, and "
           "take effect from the next game.</li>"
           "</ul>");
    return QStringLiteral("%1%2").arg(play, controls);
}

// The yaku by han, yakuman first, each saying what it is worth once a set is
// claimed where that differs; then the dora, which are not yaku.
QString MahjongTableView::riichiYakuList() const
{
    const int yakuman = mahjong::RiichiRules::kYakuman;
    std::vector<mahjong::ScoreItem> list = mahjong::ruleSet(mahjong::RuleSetId::JapaneseRiichi).fanList();
    std::stable_sort(list.begin(), list.end(),
                     [](const mahjong::ScoreItem& a, const mahjong::ScoreItem& b) { return a.value > b.value; });
    QString out;
    for (std::size_t i = 0; i < list.size(); ++i) {
        const mahjong::ScoreItem& item = list[i];
        if (i == 0 || item.value != list[i - 1].value) {
            if (i > 0)
                out.append(QStringLiteral("</ul>")); // untranslated: markup
            const QString heading = item.value >= yakuman ? tr("Yakuman: the most a hand can score")
                                                          : tr("%n han", nullptr, item.value);
            out.append(QStringLiteral("<p><b>%1</b></p><ul>").arg(heading.toHtmlEscaped())); // untranslated: markup
        }
        QString open;
        if (item.value < yakuman) {
            const int value = mahjong::RiichiRules::openValue(item.fan);
            if (value == 0)
                open = tr("Only with no set claimed.");
            else if (value < item.value)
                open = tr("%n han once a set is claimed.", nullptr, value);
        }
        out.append(QStringLiteral("<li><b>%1.</b> %2 %3</li>") // untranslated: markup
                       .arg(fanName(item.fan).toHtmlEscaped(), fanMeaning(item.fan).toHtmlEscaped(),
                            open.toHtmlEscaped()));
    }
    if (!list.empty())
        out.append(QStringLiteral("</ul>")); // untranslated: markup
    out.append(QStringLiteral("<p><b>%1</b></p><ul>") // untranslated: markup
                   .arg(tr("Not yaku: counted after them, a han each").toHtmlEscaped()));
    for (mahjong::Fan fan : { mahjong::Fan::Dora, mahjong::Fan::UraDora })
        out.append(QStringLiteral("<li><b>%1.</b> %2</li>") // untranslated: markup
                       .arg(fanName(fan).toHtmlEscaped(), fanMeaning(fan).toHtmlEscaped()));
    out.append(QStringLiteral("</ul>")); // untranslated: markup
    return out;
}

// Riichi's rules between How it plays and the pattern list. Every number is
// read from RiichiRules -- its constants, its points table and its own
// payments -- so the text cannot drift from the scoring (GHUB-0244 § 4.11).
QString MahjongTableView::riichiScoringText() const
{
    using mahjong::RiichiRules;
    const mahjong::RuleSet& rules = mahjong::ruleSet(mahjong::RuleSetId::JapaneseRiichi);
    const mahjong::TableRules table = rules.table();

    const QString words =
        tr("<h3>The dead wall and the dora</h3>"
           "<p>The last %1 tiles of the wall are the <b>dead wall</b>. Nobody draws from it but a player "
           "making a kong, who takes a tile from there in its place. The hand is drawn when the rest of "
           "the wall is used up.</p>"
           "<p>One tile of the dead wall lies face up: the <b>dora indicator</b>. It points at the "
           "<b>dora</b>, a lucky tile: the next number of its suit, with a nine pointing at the one; the "
           "next dragon, red, white, green and round again; or the next wind, east, south, west, north "
           "and round again. Each kong turns one more indicator face up. A winning hand scores a han "
           "more for every dora it holds, but dora alone never make a hand a winner. The indicators lie "
           "at the top of the table, with their dora said in words.</p>"

           "<h3>Riichi</h3>"
           "<p>You are <b>ready</b> when one more tile would complete your hand. Ready, with no set "
           "claimed, you may declare <b>riichi</b> as you throw a tile, while a tile is left in the wall "
           "to draw. Declaring costs %2 points, put on the table as a <b>riichi stick</b> once nobody "
           "wins on the tile you threw. That tile lies sideways among your thrown tiles, and your plate "
           "says riichi.</p>"
           "<p>From then on you throw every tile you draw unless it wins, and claim nothing but a win; a "
           "kong is allowed only where it leaves your wait as it was. Riichi scores a han of its own, "
           "and a riichi winner also reads the <b>ura dora</b>: the tiles under the indicators, which "
           "point at more dora the same way.</p>"
           "<p>When the tile you pick out may go with a riichi, a button says so: Riichi, throwing "
           "it.</p>"

           "<h3>Furiten</h3>"
           "<p>While you are <b>furiten</b> you may not win on a tile another player throws, though you "
           "may still win on a tile you draw. You are furiten while any tile you are waiting on is one "
           "you threw yourself this hand, and after you let a winning tile go by: until your next turn, "
           "or after riichi to the end of the hand. The table says so above your buttons while it "
           "holds.</p>")
            .arg(table.deadWall)
            .arg(RiichiRules::kRiichiStake);

    const QString yaku =
        tr("<h3>Yaku and han</h3>"
           "<p>A winning hand needs at least %n <b>yaku</b>: a scoring pattern from the list below. Each "
           "is worth one or more <b>han</b>, the steps a hand's value is counted in. Some are worth less "
           "once you have claimed a set, and some need a hand with no set claimed; the list says which. "
           "The biggest hands are <b>yakuman</b>, each worth the most a hand can score.</p>"
           "<h3>Minipoints</h3>"
           "<p>A hand's <b>fu</b>, or minipoints, count how it was made. Every hand starts with some. "
           "More come for a concealed hand won on a thrown tile; for winning on a tile you drew, but not "
           "with pinfu; for each pung and kong, more for ones, nines, winds and dragons and more again "
           "for one you did not claim; for a pair of dragons, or of your own wind or the round's; and for "
           "waiting on one tile only. The total is rounded up. The end of a hand shows the fu.</p>",
           nullptr, rules.minimumToWin());

    // The basic points below a mangan, for the fu a hand usually has.
    static constexpr int kFu[] = { 20, 25, 30, 40, 50, 60, 70 };
    QString grid = QStringLiteral("<table cellpadding='3'><tr><th></th>"); // untranslated: markup
    for (int fu : kFu)
        grid += QStringLiteral("<th>%1</th>").arg(tr("%1 fu").arg(fu).toHtmlEscaped()); // untranslated: markup
    grid += QStringLiteral("</tr>"); // untranslated: markup
    const int mangan = RiichiRules::basicPoints(5, 0);
    for (int han = 1; han < 5; ++han) {
        grid += QStringLiteral("<tr><th>%1</th>").arg(hanText(han).toHtmlEscaped()); // untranslated: markup
        for (int fu : kFu) {
            const int basic = RiichiRules::basicPoints(han, fu);
            const QString cell = basic >= mangan ? tierName(mahjong::Limit::Mangan) : QString::number(basic);
            grid += QStringLiteral("<td align='right'>%1</td>").arg(cell.toHtmlEscaped()); // untranslated: markup
        }
        grid += QStringLiteral("</tr>"); // untranslated: markup
    }
    grid += QStringLiteral("</table>"); // untranslated: markup

    // The named levels, from the points table: each run of han one level
    // covers, the last open-ended.
    constexpr int kMostHan = 13;
    QString levels;
    for (int han = 5; han <= kMostHan;) {
        const mahjong::Limit tier = RiichiRules::tierFor(han, 30);
        int last = han;
        while (last < kMostHan && RiichiRules::tierFor(last + 1, 30) == tier)
            ++last;
        const QString name = tierName(tier).toHtmlEscaped();
        const int basic = RiichiRules::basicPoints(han, 30);
        if (last == kMostHan)
            levels.append(tr("<li><b>%1</b>: %2 han or more, %3 basic points.</li>").arg(name).arg(han).arg(basic));
        else if (last == han)
            levels.append(tr("<li><b>%1</b>: %2 han, %3 basic points.</li>").arg(name).arg(han).arg(basic));
        else if (last == han + 1)
            levels.append(tr("<li><b>%1</b>: %2 or %3 han, %4 basic points.</li>").arg(name).arg(han).arg(last).arg(basic));
        else
            levels.append(tr("<li><b>%1</b>: %2 to %3 han, %4 basic points.</li>").arg(name).arg(han).arg(last).arg(basic));
        han = last + 1;
    }
    levels.append(tr("<li><b>%1</b>: a yakuman hand, %2 basic points.</li>")
                  .arg(tierName(mahjong::Limit::Yakuman).toHtmlEscaped())
                  .arg(RiichiRules::basicPoints(RiichiRules::kYakuman, 0)));
    // Fewer han reach a mangan with fu enough.
    for (int han = 4; han >= 1; --han)
        for (int fu = 20; fu <= 110; fu += 10)
            if (RiichiRules::tierFor(han, fu) == mahjong::Limit::Mangan) {
                levels.append(tr("<li>%1 han with %2 fu or more is a mangan too.</li>").arg(han).arg(fu));
                break;
            }

    const QString worth =
        tr("<h3>What a hand is worth</h3>"
           "<p>Han and fu together give a hand's <b>basic points</b>. Below a mangan they are:</p>"
           "%1"
           "<p>From a <b>mangan</b>, a full hand, upward the fu no longer count, and each level has a "
           "name:</p>"
           "<ul>%2</ul>")
            .arg(grid, levels);

    // Who pays, shown on a mangan: the rule set's own sums, rounding and all.
    const int round = paidFor(rules, 1, false, false, 1);
    const QString pays =
        tr("<h3>Who pays</h3>"
           "<p>Every win is paid from its basic points, each payment rounded up to the next %1. A mangan, "
           "%2 basic points, is paid %3 by the player who threw the winning tile, or %4 when the winner is "
           "the dealer. Won on a tile drawn, it is paid %5 by the dealer and %6 by each of the other two; a "
           "dealer who draws it is paid %7 by each. A robbed kong is paid by the player who added the "
           "tile.</p>"
           "<p>A player who throws the tile that completes a third dragon set, or a fourth wind set, for "
           "someone whose others are already laid down, pays for that hand if it wins as big three "
           "dragons or big four winds: all of it when the winner draws the tile, half of it when another "
           "player throws it.</p>")
            .arg(round)
            .arg(mangan)
            .arg(paidFor(rules, mangan, false, false, 1))
            .arg(paidFor(rules, mangan, true, false, 1))
            .arg(paidFor(rules, mangan, false, true, 3))
            .arg(paidFor(rules, mangan, false, true, 1))
            .arg(paidFor(rules, mangan, true, true, 1));

    // The place bonuses, read off the final scores of four different totals.
    const int start = table.startingPoints;
    const std::array<int, 4> totals { start + 3000, start + 2000, start + 1000, start };
    const std::array<int, 4> finals = rules.finalScores(totals);
    std::array<int, 4> bonus {};
    for (std::size_t s = 0; s < 4; ++s)
        bonus[s] = finals[s] - (totals[s] - start);

    const QString game =
        tr("<h3>Counters and riichi sticks</h3>"
           "<p>After a hand the dealer wins, or a drawn hand, a <b>counter</b> is put on the table; any "
           "other win clears them. Each counter adds %1 points to a win on a thrown tile, paid by the "
           "player who threw it, or %2 from each of the others to a win on a tile drawn.</p>"
           "<p>The riichi sticks on the table go to the next player to win, and stay there through a "
           "drawn hand. Any left when the game ends go to the player with the top score.</p>"
           "<h3>A drawn hand</h3>"
           "<p>When the wall runs out with nobody out, the players who are ready share %3 points, paid "
           "evenly by those who are not. If all four are ready, or none, nothing moves. A player who "
           "declared riichi is always ready. The end of the hand shows the ready hands face up.</p>")
            .arg(RiichiRules::kCounterOnDiscard)
            .arg(RiichiRules::kCounterEach)
            .arg(RiichiRules::kNotenTotal);
    const QString end =
        tr("<h3>The dealer and the game</h3>"
           "<p>Everyone starts with %1 points. The dealer is East. The dealer keeps the deal after "
           "winning, or after a drawn hand in which the dealer is ready; otherwise it passes to the next "
           "player. A <b>full game</b> plays %2 winds of the round in turn, starting with East, each "
           "lasting until the deal has gone round the table once. An <b>East round</b> plays East alone. "
           "A score may fall below zero, and play goes on.</p>"
           "<p>At the end each player's final score is their points less the %1 they started with, plus "
           "a bonus for their place: %3 for first, %4 for second, %5 for third and %6 for last. Players "
           "sharing a place share its bonuses.</p>")
            .arg(start)
            .arg(table.fullGameRounds)
            .arg(pointsText(bonus[0]), pointsText(bonus[1]), pointsText(bonus[2]), pointsText(bonus[3]));

    return QStringLiteral("%1%2%3%4%5%6").arg(words, yaku, worth, pays, game, end); // untranslated: markup
}

// ---------------------------------------------------------------------------
// The game moving on
// ---------------------------------------------------------------------------

void MahjongTableView::activate()
{
    m_paused = false;
    m_turnFades = true;
    if (m_turn.moving())
        m_timer->start();
    advance();
}

void MahjongTableView::deactivate()
{
    m_paused = true;
    abandonThinking();
    m_turnFades = false;
    m_timer->stop();
    update();
}

void MahjongTableView::abandonThinking()
{
    ++m_generation;
    m_thinking = false;
}

void MahjongTableView::newGame()
{
    abandonThinking();
    m_game = mahjong::Game(m_nextRules, m_nextLength);
    m_resumed = false;
    m_game.startHand(std::uint32_t(m_rng()));
    m_picked = -1;
    Sound::instance().play(Sound::kShuffle);
    advance();
}

void MahjongTableView::nextHand()
{
    if (m_game.handInPlay() || m_game.over())
        return;
    abandonThinking();
    m_game.startHand(std::uint32_t(m_rng()));
    m_picked = -1;
    Sound::instance().play(Sound::kShuffle);
    advance();
}

bool MahjongTableView::humanToAct() const
{
    const mahjong::Hand* h = m_game.hand();
    return m_game.handInPlay() && !h->legalMoves(0).empty();
}

void MahjongTableView::advance()
{
    // The computers answer a claim at once -- their answers are hidden until
    // every seat has given one -- so only a turn waits out the pause.
    for (int guard = 0; guard < 16 && m_game.handInPlay(); ++guard) {
        const mahjong::Hand& h = *m_game.hand();
        if (h.phase() == Phase::Turn)
            break;
        int computer = -1;
        for (int s : h.waitingOn())
            if (s != 0)
                computer = s;
        if (computer < 0)
            break;
        playComputer(computer);
    }
    if (m_game.handInPlay()) {
        const mahjong::Hand& h = *m_game.hand();
        if (h.phase() == Phase::Turn && h.toMove() != 0 && !m_paused && !m_thinking) {
            m_thinking = true;
            const quint64 generation = m_generation;
            QTimer::singleShot(kAiPauseMs, this, [this, generation] {
                if (generation != m_generation || !m_thinking || m_paused)
                    return;
                playComputers();
            });
        }
        // The tile just drawn, or the last you may throw: after a riichi or
        // a claim some may not be.
        if (h.toMove() == 0 && !mayThrow(m_picked))
            m_picked = lastThrowable();
    }
    refresh();
}

void MahjongTableView::playComputers()
{
    m_thinking = false;
    if (!m_game.handInPlay())
        return;
    const mahjong::Hand& h = *m_game.hand();
    if (h.phase() == Phase::Turn && h.toMove() != 0)
        playComputer(h.toMove());
    advance();
}

void MahjongTableView::playHuman(const Move& move)
{
    if (!humanToAct())
        return;
    abandonThinking();
    apply(move);
    advance();
}

void MahjongTableView::playComputer(int seat)
{
    // Should the rules ever refuse what chooseMove picked, the seat makes its
    // last legal move instead -- Pass on a claim, a discard on a turn -- so a
    // mistake there costs a move rather than freezing the table (GHUB-0249).
    const mahjong::Hand& h = *m_game.hand();
    if (apply(mahjong::chooseMove(h, seat)))
        return;
    qWarning("Mahjong: the rules refused seat %d's chosen move", seat);
    const std::vector<Move> legal = h.legalMoves(seat);
    if (!legal.empty())
        apply(legal.back());
}

bool MahjongTableView::apply(const Move& move, bool loud)
{
    if (!m_game.play(move))
        return false;
    if (move.seat == 0 && (move.action == Action::Discard || move.action == Action::Riichi))
        m_picked = -1;
    if (!loud)
        return true;
    switch (move.action) {
    case Action::Discard:
    case Action::Riichi: Sound::instance().play(Sound::kDiscPlace); break;
    case Action::Pass: break;
    default: Sound::instance().play(Sound::kClick); break;
    }
    if (!m_game.handInPlay()) {
        const mahjong::HandResult& r = m_game.results().back();
        if (!r.drawn)
            Sound::instance().play(r.win.winner == 0 ? Sound::kWin : Sound::kLose);
    }
    return true;
}

bool MahjongTableView::advanceForShot(int turns)
{
    abandonThinking();
    for (int i = 0; i < turns && !m_game.over(); ++i) {
        if (!m_game.handInPlay()) {
            m_game.startHand(std::uint32_t(m_rng()));
            continue;
        }
        const mahjong::Hand& h = *m_game.hand();
        const int seat = h.phase() == Phase::Turn ? h.toMove() : h.waitingOn().front();
        apply(mahjong::chooseMove(h, seat), false);
    }
    m_picked = lastThrowable();
    refresh();
    m_turn.land();
    return true;
}

// ---------------------------------------------------------------------------
// Words
// ---------------------------------------------------------------------------

QString MahjongTableView::seatName(int seat) const
{
    switch (seat) {
    case 0: return tr("You");
    case 1: return tr("Right");
    case 2: return tr("Across");
    default: return tr("Left");
    }
}

QString MahjongTableView::windName(int wind) const
{
    switch (wind) {
    case 1: return tr("East");
    case 2: return tr("South");
    case 3: return tr("West");
    default: return tr("North");
    }
}

QString MahjongTableView::handSentence() const
{
    const mahjong::Hand* h = m_game.hand();
    if (h == nullptr)
        return {};
    if (m_game.over()) {
        const std::vector<int> top = m_game.leaders();
        QStringList names;
        for (int s : top)
            names << seatName(s);
        const int score = m_game.totals()[std::size_t(top.front())];
        // A whole sentence for each size of tie, so a translation never has
        // to join names with a word of its own (GHUB-0250).
        if (top.size() == 2)
            return tr("Game over. %1 and %2 share the top score, %3.").arg(names[0], names[1]).arg(score);
        if (top.size() == 3)
            return tr("Game over. %1, %2 and %3 share the top score, %4.")
                .arg(names[0], names[1], names[2])
                .arg(score);
        if (top.size() > 3)
            return tr("Game over. All four share the top score, %1.").arg(score);
        return top.front() == 0 ? tr("Game over. You win, with %1.").arg(score)
                                : tr("Game over. %1 wins, with %2.").arg(names.front()).arg(score);
    }
    if (!m_game.handInPlay()) {
        const mahjong::HandResult& r = m_game.results().back();
        if (r.drawn)
            return tr("The wall is empty: a drawn hand. Press Next hand.");
        return r.win.winner == 0 ? tr("You win the hand! Press Next hand.")
                                 : tr("%1 wins the hand. Press Next hand.").arg(seatName(r.win.winner));
    }
    if (h->phase() == Phase::Robbing) {
        const QString who = seatName(h->offeredBy());
        const QString tile = mahjong::tileName(h->offered());
        return h->legalMoves(0).empty() ? tr("%1 adds the %2 to a pung.").arg(who, tile)
                                        : tr("%1 adds the %2 to a pung. You may win on it.").arg(who, tile);
    }
    QStringList said = happenedSentences();
    said << nextSentence();
    return said.join(QLatin1Char(' '));
}

// What just happened: the last discard and who took it, in whole sentences,
// each translated alone. Empty between hands and while a kong is robbed,
// whose sentences say it all.
QStringList MahjongTableView::happenedSentences() const
{
    QStringList said;
    const mahjong::Hand* h = m_game.hand();
    if (h == nullptr || m_game.over() || !m_game.handInPlay() || h->phase() == Phase::Robbing)
        return said;
    if (h->lastDiscard() != 0 && h->lastDiscarder() >= 0) {
        const QString tile = mahjong::tileName(h->lastDiscard());
        const QString who = seatName(h->lastDiscarder());
        if (lastThrowDeclared(*h))
            said << (h->lastDiscarder() == 0 ? tr("You declared riichi and threw the %1.").arg(tile)
                                             : tr("%1 declared riichi and threw the %2.").arg(who, tile));
        else
            said << (h->lastDiscarder() == 0 ? tr("You threw the %1.").arg(tile)
                                             : tr("%1 threw the %2.").arg(who, tile));
        if (h->lastClaimer() >= 0) {
            const QString set = h->lastClaim() == Action::Chow ? tr("a chow")
                : h->lastClaim() == Action::Kong              ? tr("a kong")
                                                               : tr("a pung");
            said << (h->lastClaimer() == 0 ? tr("You took it for %1.").arg(set)
                                           : tr("%1 took it for %2.").arg(seatName(h->lastClaimer()), set));
        }
    }
    return said;
}

// What happens next, during play.
QString MahjongTableView::nextSentence() const
{
    const mahjong::Hand* h = m_game.hand();
    if (h->phase() == Phase::Claims)
        return h->legalMoves(0).empty() ? tr("Waiting for the others.") : tr("You may claim it.");
    if (h->toMove() != 0)
        return tr("%1 to play.").arg(seatName(h->toMove()));
    if (h->riichi(0))
        return tr("You declared riichi: throw the tile you drew, or win.");
    // After a claim that bars a face or two, the sentence names them, as the
    // dimmed tiles show them.
    const std::vector<Move> legal = h->legalMoves(0);
    std::vector<Face> barred;
    for (Face f : h->seat(0).concealed)
        if (!mayThrowFace(legal, f) && std::find(barred.begin(), barred.end(), f) == barred.end())
            barred.push_back(f);
    if (barred.size() == 1)
        return tr("Your turn: pick a tile and throw it. After that claim you may not throw the %1.")
            .arg(mahjong::tileName(barred[0]));
    if (barred.size() > 1)
        return tr("Your turn: pick a tile and throw it. After that claim you may not throw the %1 or the %2.")
            .arg(mahjong::tileName(barred[0]), mahjong::tileName(barred[1]));
    return tr("Your turn: pick a tile and throw it.");
}

QString MahjongTableView::furitenSentence() const
{
    if (!m_game.handInPlay())
        return {};
    switch (m_game.hand()->furiten(0)) {
    case mahjong::Furiten::OwnDiscard: return tr("You cannot win on a thrown tile: you threw one you are waiting on.");
    case mahjong::Furiten::Passed: return tr("You cannot win on a thrown tile: you passed a winning tile.");
    case mahjong::Furiten::PassedAfterRiichi:
        return tr("You cannot win on a thrown tile this hand: you passed a winning tile after riichi.");
    case mahjong::Furiten::None: break;
    }
    return {};
}

QString MahjongTableView::doraSentence() const
{
    const mahjong::Hand* h = m_game.hand();
    if (h == nullptr)
        return {};
    QStringList said;
    for (Face indicator : h->doraIndicators())
        said << tr("Dora: the %1 (shown by the %2).")
                    .arg(mahjong::tileName(mahjong::RiichiRules::doraFrom(indicator)), mahjong::tileName(indicator));
    // Counters and sticks are said only while there are some, so a rule set
    // with neither says nothing here.
    const int counters = h->start().counters;
    if (counters == 1)
        said << tr("1 counter on the table.");
    else if (counters > 1)
        said << tr("%n counters on the table.", nullptr, counters);
    const int sticks = h->sticks() / std::max(1, h->rules().table().riichiStake);
    if (sticks == 1)
        said << tr("1 riichi stick on the table.");
    else if (sticks > 1)
        said << tr("%n riichi sticks on the table.", nullptr, sticks);
    return said.join(QLatin1Char(' '));
}

// The status line: the whole sentence, and furiten after it.
QString MahjongTableView::tableSentence() const
{
    const QString furiten = furitenSentence();
    return furiten.isEmpty() ? handSentence() : QStringLiteral("%1 %2").arg(handSentence(), furiten);
}

// Furiten is not in it: paintPools draws it on a line of its own.
QString MahjongTableView::centreSentence() const
{
    if (!Legibility::instance().enabled())
        return handSentence();
    return happenedSentences().join(QLatin1Char(' '));
}

QString MahjongTableView::captionText() const
{
    const QStringList happened = happenedSentences();
    return happened.isEmpty() ? handSentence() : nextSentence();
}

QStringList MahjongTableView::endOfHandLines() const
{
    QStringList lines;
    for (const PanelLine& line : panelLines())
        lines << line.text;
    return lines;
}

std::vector<MahjongTableView::PanelLine> MahjongTableView::panelLines() const
{
    if (m_game.handInPlay() || m_game.results().empty())
        return {};
    const mahjong::HandResult& r = m_game.results().back();
    std::vector<PanelLine> lines = m_game.ruleSetId() == mahjong::RuleSetId::JapaneseRiichi ? riichiEndLines(r)
                                                                                           : classicEndLines(r);
    // Once the game is over, each seat's total and the final score the rule
    // set makes of it, where that differs from the total (GHUB-0244 § 4.9).
    if (m_game.over() && m_game.finalScores() != m_game.totals()) {
        const std::array<int, mahjong::kSeats> finals = m_game.finalScores();
        for (int s = 0; s < mahjong::kSeats; ++s)
            lines.push_back({ tr("%1: %2 points, final %3")
                                  .arg(seatName(s))
                                  .arg(m_game.totals()[std::size_t(s)])
                                  .arg(pointsText(finals[std::size_t(s)])),
                              PanelLine::Strong });
    }
    return lines;
}

QString MahjongTableView::paymentsLine(const std::array<int, mahjong::kSeats>& payments) const
{
    QStringList pays;
    for (int s = 0; s < mahjong::kSeats; ++s) {
        const int p = payments[std::size_t(s)];
        pays << (p > 0 ? tr("%1 +%2").arg(seatName(s)).arg(p) : tr("%1 %2").arg(seatName(s)).arg(p));
    }
    return pays.join(QStringLiteral(",  "));
}

QString MahjongTableView::winnerLine(const mahjong::WinContext& win) const
{
    const QString who = seatName(win.winner);
    return win.payer < 0 ? tr("%1 won on a tile drawn from the wall.").arg(who)
                         : tr("%1 won on the %2 from %3.")
                               .arg(who, mahjong::tileName(win.winningTile), seatName(win.payer));
}

// Hong Kong and Chinese Official: one winner, its patterns and the total.
std::vector<MahjongTableView::PanelLine> MahjongTableView::classicEndLines(const mahjong::HandResult& r) const
{
    std::vector<PanelLine> lines;
    if (r.drawn) {
        mahjong::HandEnd end;
        end.drawn = true;
        end.dealerReady = r.ready[std::size_t(m_game.hand()->dealer())];
        lines.push_back({ mahjong::ruleSet(m_game.ruleSetId()).dealerKeepsDeal(end)
                              ? tr("A drawn hand. No points move, and the dealer deals again.")
                              : tr("A drawn hand. No points move, and the deal passes on."),
                          PanelLine::Title });
        return lines;
    }
    lines.push_back({ winnerLine(r.win), PanelLine::Title });
    const mahjong::RuleSetId id = m_game.ruleSetId();
    for (const mahjong::ScoreItem& item : r.score.items)
        lines.push_back({ tr("%1 — %2").arg(fanName(item.fan), valueText(id, item.value)), PanelLine::Plain });
    if (id == mahjong::RuleSetId::ChineseOfficial)
        lines.push_back({ tr("Total: %1.").arg(valueText(id, r.score.total)), PanelLine::Strong });
    else
        lines.push_back({ r.score.limit ? tr("A limit hand: %1 fan, %2 points.").arg(r.score.total).arg(r.score.points)
                                        : tr("Total: %1 fan, %2 points.").arg(r.score.total).arg(r.score.points),
                          PanelLine::Strong });
    lines.push_back({ paymentsLine(r.payments), PanelLine::Strong });
    return lines;
}

// Japanese Riichi: every winner in turn, the riichi sticks, the payments; or
// a drawn hand's ready seats (GHUB-0244 § 4.11).
std::vector<MahjongTableView::PanelLine> MahjongTableView::riichiEndLines(const mahjong::HandResult& r) const
{
    std::vector<PanelLine> lines;
    const mahjong::RuleSet& rules = mahjong::ruleSet(m_game.ruleSetId());
    if (r.drawn) {
        const int dealer = m_game.hand()->dealer();
        mahjong::HandEnd end;
        end.drawn = true;
        end.dealerReady = r.ready[std::size_t(dealer)];
        lines.push_back({ rules.dealerKeepsDeal(end) ? tr("A drawn hand, and the dealer deals again.")
                                                     : tr("A drawn hand, and the deal passes on."),
                          PanelLine::Title });
        for (int s = 0; s < mahjong::kSeats; ++s) {
            const QString paid = pointsText(r.payments[std::size_t(s)]);
            const bool ready = r.ready[std::size_t(s)];
            lines.push_back({ ready ? tr("%1 — ready, %2").arg(seatName(s), paid)
                                    : tr("%1 — not ready, %2").arg(seatName(s), paid),
                              ready ? PanelLine::Strong : PanelLine::Plain });
        }
        const int sticks = r.sticksAfter / mahjong::RiichiRules::kRiichiStake;
        if (sticks == 1)
            lines.push_back({ tr("1 riichi stick stays on the table."), PanelLine::Plain });
        else if (sticks > 1)
            lines.push_back({ tr("%n riichi sticks stay on the table.", nullptr, sticks), PanelLine::Plain });
        return lines;
    }

    // The sticks the first winner takes are what its net holds beyond the
    // wins' own payments: the other seats' stakes and the sticks it found.
    int won = rules.settle(r.win, r.score)[std::size_t(r.win.winner)];
    riichiWinLines(lines, r.win, r.score);
    for (const auto& [win, score] : r.alsoWon) {
        won += rules.settle(win, score)[std::size_t(r.win.winner)];
        riichiWinLines(lines, win, score);
    }
    const int sticks = r.payments[std::size_t(r.win.winner)] - won;
    if (sticks > 0)
        lines.push_back({ r.win.winner == 0 ? tr("You take the riichi sticks on the table: %1 points.").arg(sticks)
                                            : tr("%1 takes the riichi sticks on the table: %2 points.")
                                                  .arg(seatName(r.win.winner))
                                                  .arg(sticks),
                          PanelLine::Plain });
    lines.push_back({ paymentsLine(r.payments), PanelLine::Strong });
    return lines;
}

void MahjongTableView::riichiWinLines(std::vector<PanelLine>& lines, const mahjong::WinContext& win,
                                      const mahjong::HandScore& score) const
{
    lines.push_back({ winnerLine(win), PanelLine::Title });
    // The ura indicators are turned up for a riichi winner alone, and named
    // here in words, on one line where there are few: the dora row at the
    // top of the table shows only the indicators face up during play.
    const std::vector<Face>& ura = win.uraIndicators;
    if (ura.size() == 1)
        lines.push_back({ tr("Ura dora indicator: the %1.").arg(mahjong::tileName(ura[0])), PanelLine::Plain });
    else if (ura.size() == 2)
        lines.push_back({ tr("Ura dora indicators: the %1 and the %2.")
                              .arg(mahjong::tileName(ura[0]), mahjong::tileName(ura[1])),
                          PanelLine::Plain });
    else
        for (Face indicator : ura)
            lines.push_back({ tr("Ura dora indicator: the %1.").arg(mahjong::tileName(indicator)), PanelLine::Plain });
    for (const mahjong::ScoreItem& item : score.items)
        lines.push_back({ tr("%1 — %2").arg(fanName(item.fan), hanText(item.value)), PanelLine::Plain });
    QString total;
    switch (score.tier) {
    case mahjong::Limit::None: total = tr("Total: %1 han, %2 fu.").arg(score.total).arg(score.fu); break;
    case mahjong::Limit::Mangan:
        // Below five han only the fu make it a mangan, so they are said.
        total = score.total < 5 ? tr("Mangan: %1 han, %2 fu.").arg(score.total).arg(score.fu)
                                : tr("Mangan: %1 han.").arg(score.total);
        break;
    case mahjong::Limit::Haneman: total = tr("Haneman: %1 han.").arg(score.total); break;
    case mahjong::Limit::Baiman: total = tr("Baiman: %1 han.").arg(score.total); break;
    case mahjong::Limit::Sanbaiman: total = tr("Sanbaiman: %1 han.").arg(score.total); break;
    case mahjong::Limit::Yakuman: total = tr("Yakuman."); break;
    }
    lines.push_back({ total, PanelLine::Strong });
    // On a self-draw each of the others pays a share of the counters.
    const int perCounter = win.payer < 0 ? mahjong::RiichiRules::kCounterEach * (mahjong::kSeats - 1)
                                         : mahjong::RiichiRules::kCounterOnDiscard;
    if (win.counters == 1)
        lines.push_back({ tr("1 counter adds %1 points.").arg(perCounter), PanelLine::Plain });
    else if (win.counters > 1)
        lines.push_back({ tr("%n counters add %1 points.", nullptr, win.counters).arg(win.counters * perCounter),
                          PanelLine::Plain });
}

// ---------------------------------------------------------------------------
// Buttons
// ---------------------------------------------------------------------------

std::vector<MahjongTableView::Button> MahjongTableView::buttons() const
{
    std::vector<Button> out;
    auto* self = const_cast<MahjongTableView*>(this);
    if (m_game.over()) {
        out.push_back({ tr("New game"), [self] { self->newGame(); } });
        return out;
    }
    if (!m_game.handInPlay()) {
        out.push_back({ tr("Next hand"), [self] { self->nextHand(); } });
        return out;
    }
    const mahjong::Hand& h = *m_game.hand();
    const std::vector<Move> legal = h.legalMoves(0);
    for (const Move& m : legal) {
        QString label;
        switch (m.action) {
        case Action::Win: label = tr("Win"); break;
        case Action::Kong: label = tr("Kong"); break;
        case Action::Pung: label = tr("Pung"); break;
        case Action::Chow: label = chowLabel(Face(m.chowLow)); break;
        case Action::ConcealedKong:
        case Action::AddedKong: label = tr("Kong of %1").arg(mahjong::tileName(m.tile)); break;
        case Action::Pass: label = tr("Pass"); break;
        case Action::Discard:
        case Action::Riichi: continue;
        }
        out.push_back({ label, [self, m] { self->playHuman(m); } });
    }
    if (h.toMove() == 0) {
        const std::vector<Face> hand = displayHand(h.seat(0));
        if (m_picked >= 0 && m_picked < int(hand.size()) && mayThrowFace(legal, hand[std::size_t(m_picked)])) {
            const Face tile = hand[std::size_t(m_picked)];
            // A riichi goes with the throw, so its button stands beside it.
            const Move declare { 0, Action::Riichi, tile, 0 };
            if (std::find(legal.begin(), legal.end(), declare) != legal.end())
                out.push_back({ tr("Riichi, throwing the %1").arg(mahjong::tileName(tile)),
                                [self, declare] { self->playHuman(declare); } });
            const Move throwIt { 0, Action::Discard, tile, 0 };
            out.push_back({ tr("Throw the %1").arg(mahjong::tileName(tile)),
                            [self, throwIt] { self->playHuman(throwIt); } });
        }
    }
    return out;
}

bool MahjongTableView::mayThrow(int index) const
{
    const mahjong::Hand* h = m_game.hand();
    if (!m_game.handInPlay() || h->toMove() != 0)
        return false;
    const std::vector<Face> hand = displayHand(h->seat(0));
    return index >= 0 && index < int(hand.size()) && mayThrowFace(h->legalMoves(0), hand[std::size_t(index)]);
}

int MahjongTableView::lastThrowable() const
{
    const mahjong::Hand* h = m_game.hand();
    if (!m_game.handInPlay() || h->toMove() != 0)
        return -1;
    const std::vector<Face> hand = displayHand(h->seat(0));
    const std::vector<Move> legal = h->legalMoves(0);
    for (int i = int(hand.size()) - 1; i >= 0; --i)
        if (mayThrowFace(legal, hand[std::size_t(i)]))
            return i;
    return -1;
}

QStringList MahjongTableView::buttonLabels() const
{
    QStringList labels;
    for (const Button& b : buttons())
        labels << b.label;
    return labels;
}

void MahjongTableView::pressButton(int index)
{
    const std::vector<Button> all = buttons();
    if (index >= 0 && index < int(all.size()))
        all[std::size_t(index)].press();
}

void MahjongTableView::refresh()
{
    const mahjong::Hand* h = m_game.hand();
    // § 4.7: the seat to move, and nobody while a claim is waited on, between
    // hands, or once the game is over.
    const int lit = m_game.handInPlay() && h->phase() == Phase::Turn ? h->toMove() : -1;
    m_turn.setSeat(lit, m_turnFades);
    if (m_turnFades && m_turn.moving())
        m_timer->start();
    update();
    Q_EMIT statusChanged(tableSentence());
}

void MahjongTableView::step()
{
    if (!(m_turnFades && m_turn.step(Theme::kTurnLightTickMs)))
        m_timer->stop();
    update();
}

void MahjongTableView::showScoreSheet()
{
    QString html = QStringLiteral("<table cellpadding='4'><tr><th></th>"); // untranslated: markup
    for (int s = 0; s < mahjong::kSeats; ++s)
        html += QStringLiteral("<th>%1</th>").arg(seatName(s).toHtmlEscaped()); // untranslated: markup
    html += QStringLiteral("<th></th></tr>"); // untranslated: markup
    std::array<int, mahjong::kSeats> running {};
    int number = 0;
    for (const mahjong::HandResult& r : m_game.results()) {
        ++number;
        html += QStringLiteral("<tr><td>%1</td>") // untranslated: markup
                    .arg(tr("Hand %1").arg(number).toHtmlEscaped());
        for (int s = 0; s < mahjong::kSeats; ++s) {
            running[std::size_t(s)] += r.payments[std::size_t(s)];
            html += QStringLiteral("<td align='right'>%1</td>").arg(running[std::size_t(s)]); // untranslated: markup
        }
        const QString what = r.drawn ? tr("Drawn")
                                     : tr("%1 won, %2")
                                           .arg(seatName(r.win.winner), valueText(m_game.ruleSetId(), r.score.total));
        html += QStringLiteral("<td>%1</td></tr>").arg(what.toHtmlEscaped()); // untranslated: markup
    }
    html += QStringLiteral("</table>"); // untranslated: markup
    if (number == 0)
        html = tr("No hand has finished yet.").toHtmlEscaped();

    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Score sheet"));
    auto* layout = new QVBoxLayout(dialog);
    auto* label = new QLabel(html);
    label->setTextFormat(Qt::RichText);
    QFont f = label->font();
    f.setPointSizeF(f.pointSizeF() * (Legibility::instance().enabled() ? 1.6 : 1.25));
    label->setFont(f);
    auto* scroll = new QScrollArea;
    scroll->setWidget(label);
    scroll->setWidgetResizable(true);
    layout->addWidget(scroll);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(box);
    dialog->resize(640, 480);
    dialog->open();
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

QRectF MahjongTableView::roomRect() const
{
    return { 0.0, 0.0, double(width()), double(height()) - captionBand(QRectF(rect())) };
}

double MahjongTableView::handTileWidth() const
{
    // Fourteen tiles and the gap before the drawn one across the bottom, at
    // no more than a seventh of the room's height.
    const QRectF room = roomRect();
    return std::max(16.0, std::min((room.width() - 2 * kMargin) / 15.5, room.height() * 0.14 / kTileAspect));
}

double MahjongTableView::smallTileWidth() const
{
    // Your sets, the others' sets and the discards' starting size. Large play
    // keeps them at the floor where the window allows (GHUB-0248).
    const double tw = handTileWidth();
    return Legibility::instance().enabled() ? std::max(tw * 0.6, std::min(mahjong::kTileMinWidth, tw)) : tw * 0.6;
}

QRectF MahjongTableView::seatArea(int seat) const
{
    const QRectF room = roomRect();
    const double sw = smallTileWidth();
    int rows = 1;
    layOutButtons(&rows);
    // A second row of buttons takes its height from the centre, not from you.
    const double bottomTop = baseRowTop() - std::max(0, rows - 1) * (buttonHeight() + 6.0);
    const double side = std::max(150.0, room.width() * 0.17);
    const double plate = std::max(34.0, room.height() * 0.055);
    const double topHeight = std::max(plate, sw * kTileAspect) + sw * kTileAspect + 14.0;
    switch (seat) {
    case 0: return { kMargin, bottomTop, room.width() - 2 * kMargin, room.bottom() - kMargin - bottomTop };
    case 1: return { room.width() - kMargin - side, kMargin, side, bottomTop - 2 * kMargin };
    case 2: return { kMargin + side + kMargin, kMargin, room.width() - 2 * (side + 2 * kMargin), topHeight };
    default: return { kMargin, kMargin, side, bottomTop - 2 * kMargin };
    }
}

QRectF MahjongTableView::centreRect() const
{
    const QRectF top = seatArea(2);
    const QRectF bottom = seatArea(0);
    return { top.left(), top.bottom() + kMargin, top.width(), bottom.top() - kMargin - (top.bottom() + kMargin) };
}

QRectF MahjongTableView::sentenceRect() const
{
    const QRectF c = centreRect();
    const double h = std::clamp(c.height() * 0.16, 28.0, 90.0) + furitenLineHeight();
    return { c.left(), c.bottom() - h, c.width(), h };
}

double MahjongTableView::furitenLineHeight() const
{
    if (furitenSentence().isEmpty())
        return 0.0;
    return std::clamp(centreRect().height() * 0.07, Legibility::instance().enabled() ? 28.0 : 22.0, 40.0);
}

QRectF MahjongTableView::turnArea(int seat) const
{
    return seatArea(seat).adjusted(-kMargin * 0.6, -kMargin * 0.6, kMargin * 0.6, kMargin * 0.6);
}

QRectF MahjongTableView::handTileRect(int index) const
{
    const mahjong::Hand* h = m_game.hand();
    if (h == nullptr)
        return {};
    const mahjong::SeatTiles& s = h->seat(0);
    const int count = int(s.concealed.size());
    if (index < 0 || index >= count)
        return {};
    const QRectF room = roomRect();
    const double tw = handTileWidth();
    const double gap = s.drawn != 0 ? tw * 0.5 : 0.0;
    const double span = count * tw + gap;
    const double left = (room.width() - span) / 2.0;
    const double top = room.bottom() - kMargin - tw * kTileAspect;
    const bool apart = s.drawn != 0 && index == count - 1;
    return { left + index * tw + (apart ? gap : 0.0), top, tw - 2.0, tw * kTileAspect };
}

double MahjongTableView::baseRowTop() const
{
    const QRectF room = roomRect();
    const double handTop = room.bottom() - kMargin - handTileWidth() * kTileAspect;
    return handTop - (smallTileWidth() * kTileAspect + 8.0) - 6.0;
}

QRectF MahjongTableView::plateRect() const
{
    const QRectF room = roomRect();
    return { kMargin, baseRowTop(), std::min(230.0, (room.width() - 2 * kMargin) * 0.22),
             std::max(34.0, room.height() * 0.055) };
}

double MahjongTableView::buttonHeight() const
{
    return std::max(24.0, smallTileWidth() * kTileAspect - 2.0);
}

double MahjongTableView::setsSpan(const mahjong::SeatTiles& s) const
{
    double span = 0.0;
    for (const mahjong::Meld& m : s.melds)
        span += (m.kind == mahjong::SetKind::Kong ? 4 : 3) + 0.3;
    return span + double(s.bonus.size());
}

double MahjongTableView::minimumFaceWidth() const
{
    const double sw = smallTileWidth();
    return Legibility::instance().enabled() ? std::min(mahjong::kTileMinWidth, sw) : sw * 0.6;
}

std::vector<QRectF> MahjongTableView::layOutButtons(int* rows) const
{
    // Right to left along the row above your hand, the first button leftmost.
    // Your plate and your sets keep their room: a button that would reach them
    // starts a new row above instead (GHUB-0248).
    const std::vector<Button> all = buttons();
    std::vector<QRectF> out(all.size());
    const QRectF room = roomRect();
    const double h = buttonHeight();
    QFont f = font();
    f.setBold(true);
    f.setPixelSize(std::max(13, int((h + 6.0) * (Legibility::instance().enabled() ? 0.46 : 0.4))));
    const QFontMetricsF fm(f);
    const double right = room.width() - kMargin;
    const double plateRight = plateRect().right() + 10.0;
    double left = plateRight;
    if (const mahjong::Hand* hand = m_game.hand()) {
        const double span = setsSpan(hand->seat(0));
        if (span > 0.0)
            left += span * minimumFaceWidth() + 10.0;
    }
    // Whatever your sets need, the buttons keep half the row.
    left = std::min(left, plateRight + (right - plateRight) * 0.5);
    double x = right;
    double top = baseRowTop() + 2.0;
    int count = all.empty() ? 0 : 1;
    for (int i = int(all.size()) - 1; i >= 0; --i) {
        const QString text = QStringLiteral("%1  %2").arg(i + 1).arg(all[std::size_t(i)].label);
        const double w = std::min(fm.horizontalAdvance(text) + (h + 6.0) * 0.8, right - left);
        if (x - w < left - 0.5 && x < right) {
            x = right;
            top -= h + 6.0;
            ++count;
        }
        out[std::size_t(i)] = QRectF(x - w, top, w, h);
        x -= w + 8.0;
    }
    if (rows != nullptr)
        *rows = count;
    return out;
}

QRectF MahjongTableView::buttonRect(int index) const
{
    const std::vector<QRectF> all = layOutButtons(nullptr);
    return index >= 0 && index < int(all.size()) ? all[std::size_t(index)] : QRectF();
}

double MahjongTableView::yourSetWidth() const
{
    const double sw = smallTileWidth();
    const mahjong::Hand* h = m_game.hand();
    const double span = h != nullptr ? setsSpan(h->seat(0)) : 0.0;
    if (span <= 0.0)
        return sw;
    // From your plate to the first button on your row, on one line.
    double right = roomRect().width() - kMargin;
    for (const QRectF& r : layOutButtons(nullptr))
        if (r.top() > baseRowTop() - 1.0)
            right = std::min(right, r.left());
    const double room = right - 10.0 - (plateRect().right() + 10.0);
    return std::clamp(room / span, std::min(minimumFaceWidth(), sw), sw);
}

bool MahjongTableView::showsDora() const
{
    return m_game.hand() != nullptr && mahjong::ruleSet(m_game.ruleSetId()).table().doraIndicators;
}

double MahjongTableView::infoHeight() const
{
    return std::clamp(centreRect().height() * 0.08, 20.0, 34.0);
}

QRectF MahjongTableView::centreBody() const
{
    const QRectF c = centreRect();
    const double infoH = infoHeight();
    return { c.left(), c.top() + infoH + 4.0, c.width() * 0.66, sentenceRect().top() - c.top() - infoH - 8.0 };
}

QFont MahjongTableView::doraFont(int pixels) const
{
    QFont f = font();
    f.setBold(true);
    f.setPixelSize(pixels);
    return f;
}

int MahjongTableView::doraFloorPixels() const
{
    return Legibility::instance().enabled() ? 16 : 12;
}

double MahjongTableView::doraWordsHeight() const
{
    // Measured beside the indicators at their largest, so the room it asks
    // for holds the words at the floor whatever size the tiles come out.
    const mahjong::Hand* h = m_game.hand();
    if (h == nullptr)
        return 0.0;
    const double width = centreRect().width() - double(h->doraIndicators().size()) * handTileWidth() * 0.7 - 10.0;
    const QFontMetricsF fm(doraFont(doraFloorPixels()));
    return fm.boundingRect(QRectF(0.0, 0.0, std::max(40.0, width), 1.0e4), kDoraFlags, doraSentence()).height() + 6.0;
}

double MahjongTableView::doraRowHeight(double tileWidth, double words) const
{
    return showsDora() ? std::max(tileWidth * kTileAspect + 8.0, words) : 0.0;
}

QRectF MahjongTableView::poolsRect() const
{
    return centreBody().adjusted(0.0, doraRowHeight(poolTileWidth(), doraWordsHeight()), 0.0, 0.0);
}

QFont MahjongTableView::poolLabelFont() const
{
    // Sized from the hand's tiles, not the pool's, so the label and the pool
    // width do not depend on each other.
    QFont f = font();
    f.setBold(true);
    f.setPixelSize(std::max(11, int(std::min(handTileWidth() * 0.7 * kTileAspect * 0.45, 20.0))));
    return f;
}

double MahjongTableView::poolLabelWidth() const
{
    const QFontMetricsF fm(poolLabelFont());
    double widest = 0.0;
    for (int seat = 0; seat < mahjong::kSeats; ++seat)
        widest = std::max(widest, fm.horizontalAdvance(seatName(seat)));
    return std::max(std::min(90.0, centreBody().width() * 0.16), widest + 10.0);
}

double MahjongTableView::poolTileWidth() const
{
    // As large as lets the longest pool -- or a dozen tiles, so the size does
    // not jump about early in a hand -- fit its strip, every tile shown. Large
    // play never goes below the floor; a pool too long for its strip at the
    // floor shows its latest tiles and counts the rest (paintPools). The dora
    // row above the pools is drawn at the same size, so it shrinks with them.
    const mahjong::Hand* h = m_game.hand();
    const QRectF pools = centreBody();
    const double label = poolLabelWidth();
    const double words = showsDora() ? doraWordsHeight() : 0.0;
    std::size_t longest = 12;
    if (h != nullptr)
        for (int seat = 0; seat < mahjong::kSeats; ++seat)
            // A riichi's sideways tile takes about a slot more.
            longest = std::max(longest, h->seat(seat).pool.size() + (h->riichi(seat) ? 1 : 0));
    const double floor = Legibility::instance().enabled() ? mahjong::kTileMinWidth : 10.0;
    double pw = handTileWidth() * 0.7;
    for (; pw > floor; pw -= 1.0) {
        const double stripH = (pools.height() - doraRowHeight(pw, words)) / mahjong::kSeats;
        const int perLine = std::max(1, int((pools.width() - label) / pw));
        const int lines = int((longest + std::size_t(perLine) - 1) / std::size_t(perLine));
        if (lines * (pw * kTileAspect + 3.0) <= stripH - 4.0)
            break;
    }
    return std::max(pw, floor);
}

double MahjongTableView::smallestTileWidth() const
{
    return std::min({ poolTileWidth(), yourSetWidth(), smallTileWidth() });
}

QSize MahjongTableView::minimumSizeHint() const
{
    // Tall enough that large play's tiles clear mahjong::kTileMinWidth, and the
    // same with the switch off, so turning it on never grows the window.
    return { 900, 760 };
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void MahjongTableView::paintTile(QPainter& p, const QRectF& r, Face face, bool raised) const
{
    const double w = r.width();
    const double radius = w * 0.10;
    const double d = w * 0.08;
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(mahjong::kTileBackDark);
    p.drawRoundedRect(r.translated(d, d), radius, radius);
    p.setBrush(mahjong::kTileEdge);
    p.drawRoundedRect(r.translated(d * 0.45, d * 0.45), radius, radius);
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, raised ? QColor(0xff, 0xec, 0xa8) : mahjong::kFaceTop);
    g.setColorAt(1.0, raised ? QColor(0xf2, 0xc8, 0x5a) : mahjong::kFaceBottom);
    p.setBrush(g);
    p.setPen(QPen(QColor(0x8a, 0x7a, 0x58), 1.0));
    p.drawRoundedRect(r, radius, radius);
    mahjong::paintTileFace(p, r, face, Legibility::instance().enabled(), font());
    p.restore();
}

void MahjongTableView::paintBack(QPainter& p, const QRectF& r) const
{
    const double radius = r.width() * 0.12;
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(mahjong::kTileBackDark);
    p.drawRoundedRect(r.translated(r.width() * 0.08, r.width() * 0.08), radius, radius);
    p.setBrush(mahjong::kTileBack);
    p.drawRoundedRect(r, radius, radius);
    p.restore();
}

void MahjongTableView::paintPlate(QPainter& p, const QRectF& r, const QString& title, const QString& line,
                                  bool lit) const
{
    p.save();
    p.setPen(lit ? QPen(Theme::kGold, 2.0) : Qt::NoPen);
    p.setBrush(kPlate);
    p.drawRoundedRect(r, 6, 6);
    const bool legible = Legibility::instance().enabled();
    QFont f = font();
    f.setBold(true);
    f.setPixelSize(std::max(legible ? 16 : 12, int(r.height() * (legible ? 0.36 : 0.32))));
    const QRectF inner = r.adjusted(8, 2, -8, -2);
    // Smaller rather than cut short: "Right — East, dealer" must read whole.
    while (f.pixelSize() > 11 && QFontMetricsF(f).horizontalAdvance(title) > inner.width())
        f.setPixelSize(f.pixelSize() - 1);
    p.setFont(f);
    p.setPen(kInk);
    p.drawText(QRectF(inner.left(), inner.top(), inner.width(), inner.height() / 2), Qt::AlignLeft | Qt::AlignVCenter,
               title);
    f.setBold(false);
    p.setFont(f);
    p.setPen(kInkDim);
    p.drawText(QRectF(inner.left(), inner.center().y(), inner.width(), inner.height() / 2),
               Qt::AlignLeft | Qt::AlignVCenter, line);
    p.restore();
}

void MahjongTableView::paintSets(QPainter& p, const mahjong::SeatTiles& s, QRectF row, double w) const
{
    // The sets laid down, then the bonus tiles, flowing left to right and on
    // to the next line. A concealed kong shows its two ends face down.
    double x = row.left();
    double y = row.top();
    const double h = w * kTileAspect;
    const auto place = [&](int count) {
        if (x + count * w > row.right() + 0.5 && x > row.left()) {
            x = row.left();
            y += h + 4.0;
        }
    };
    for (const mahjong::Meld& m : s.melds) {
        const int count = m.kind == mahjong::SetKind::Kong ? 4 : 3;
        place(count);
        for (int k = 0; k < count; ++k) {
            const QRectF r(x + k * w, y, w - 1.5, h);
            const Face f = m.kind == mahjong::SetKind::Chow ? Face(m.tile + k) : m.tile;
            if (m.concealed && (k == 0 || k == 3))
                paintBack(p, r);
            else
                paintTile(p, r, f);
        }
        x += count * w + w * 0.3;
    }
    for (Face b : s.bonus) {
        place(1);
        paintTile(p, QRectF(x, y, w - 1.5, h), b);
        x += w;
    }
}

void MahjongTableView::paintSeat(QPainter& p, int seat) const
{
    const mahjong::Hand* h = m_game.hand();
    // The range test is for GCC as much as for safety: without it -Warray-bounds
    // cannot see that `seat` stays inside the four.
    if (h == nullptr || seat < 0 || seat >= mahjong::kSeats)
        return;
    const QRectF area = seatArea(seat);
    const QRectF room = roomRect();
    const double sw = smallTileWidth();
    const double plateH = std::max(34.0, room.height() * 0.055);
    const int wind = h->seatWind(seat);
    const QString title = seat == h->dealer() ? tr("%1 — %2, dealer").arg(seatName(seat), windName(wind))
                                              : tr("%1 — %2").arg(seatName(seat), windName(wind));
    // A riichi seat's stake is gone from its score once nobody has won on
    // its declaring throw; the total itself moves when the hand is settled.
    int score = m_game.totals()[std::size_t(seat)];
    const bool staking = h->phase() == Phase::Claims && h->offeredBy() == seat && lastThrowDeclared(*h);
    if (m_game.handInPlay() && h->riichi(seat) && !staking)
        score -= h->rules().table().riichiStake;
    const QString line = h->riichi(seat) ? tr("Score %1, riichi").arg(score) : tr("Score %1").arg(score);
    const bool toMove = m_game.handInPlay() && h->toMove() == seat;
    const mahjong::SeatTiles& s = h->seat(seat);

    if (seat == 0) {
        // Your sets on one line between the plate and the buttons, never
        // wrapping down into your hand (GHUB-0248).
        const QRectF plate = plateRect();
        paintPlate(p, plate, title, line, toMove);
        const double w = yourSetWidth();
        const double span = setsSpan(s);
        paintSets(p, s, QRectF(plate.right() + 10.0, plate.top(), span * w + 1.0, w * kTileAspect), w);
        return;
    }
    QRectF plate;
    QRectF rest;
    if (seat == 2) {
        plate = QRectF(area.left(), area.top(), std::min(240.0, area.width() * 0.3), plateH);
        rest = QRectF(plate.right() + 10.0, area.top(), area.right() - plate.right() - 10.0, area.height());
    } else {
        plate = QRectF(area.left(), area.top(), area.width(), plateH);
        rest = QRectF(area.left(), plate.bottom() + 8.0, area.width(), area.bottom() - plate.bottom() - 8.0);
    }
    paintPlate(p, plate, title, line, toMove);

    // The concealed tiles as backs, then the sets and bonus tiles face up.
    const double bw = sw * 0.62;
    const double bh = bw * kTileAspect;
    double x = rest.left();
    double y = rest.top();
    for (std::size_t i = 0; i < s.concealed.size(); ++i) {
        if (x + bw > rest.right() + 0.5) {
            x = rest.left();
            y += bh + 3.0;
        }
        paintBack(p, QRectF(x, y, bw - 1.5, bh));
        x += bw;
    }
    if (seat == 2) {
        // Across has one row for its sets: the whole width under the plate,
        // narrower tiles before a second line that would reach the centre.
        const double span = setsSpan(s);
        const double w = span > 0.0 ? std::clamp(area.width() / span, std::min(minimumFaceWidth(), sw), sw) : sw;
        const double top = std::max(plate.bottom(), y + bh) + 8.0;
        paintSets(p, s, QRectF(area.left(), top, area.width(), area.bottom() - top), w);
        return;
    }
    paintSets(p, s, QRectF(rest.left(), y + bh + 8.0, rest.width(), rest.bottom() - y - bh - 8.0), sw);
}

void MahjongTableView::paintDora(QPainter& p, const QRectF& row, double w) const
{
    // The indicators face up, then what they point to in words, with the
    // counters and the sticks: the tiles alone would leave the dora to be
    // worked out.
    double x = row.left();
    for (Face indicator : m_game.hand()->doraIndicators()) {
        paintTile(p, QRectF(x, row.top() + 2.0, w - 1.5, w * kTileAspect), indicator);
        x += w;
    }
    const QRectF words(x + 10.0, row.top(), row.right() - x - 10.0, row.height() - 4.0);
    const QString text = doraSentence();
    // As large as the round's line where it fits, smaller rather than cut
    // short, and never below the floor the row was given room for.
    QFont f = doraFont(std::max(doraFloorPixels(),
                                int(infoHeight() * (Legibility::instance().enabled() ? 0.72 : 0.62))));
    while (f.pixelSize() > doraFloorPixels()
           && QFontMetricsF(f).boundingRect(words, kDoraFlags, text).height() > words.height())
        f.setPixelSize(f.pixelSize() - 1);
    p.setFont(f);
    p.setPen(kInk);
    p.drawText(words, kDoraFlags, text);
}

void MahjongTableView::paintPool(QPainter& p, int seat, const QRectF& strip, double label) const
{
    const mahjong::Hand* h = m_game.hand();
    const std::vector<Face>& pool = h->seat(seat).pool;
    const double pw = poolTileWidth();
    const double ph = pw * kTileAspect;
    const double across = strip.width() - label;
    // The tile thrown with a riichi lies sideways, as the book lays it, so
    // its row holds one tile fewer; a slot or two is kept back for it.
    const std::size_t sideways = h->riichi(seat) ? h->riichiAt(seat) : pool.size();
    const int perLine = std::max(1, int(across / pw));
    const int lines = std::max(1, int((strip.height() - 1.0) / (ph + 3.0)));
    const std::size_t room = std::size_t(std::max(1, perLine * lines - (sideways < pool.size() ? 2 : 0)));
    // Too many to show at this size: the first slot says how many earlier
    // tiles are not drawn, and the latest fill the rest.
    const std::size_t hidden = pool.size() > room ? pool.size() - room + 1 : 0;
    double x = 0.0;
    int line = 0;
    const auto place = [&](double width) {
        if (x + width > across + 0.5 && x > 0.0) {
            x = 0.0;
            ++line;
        }
        const QPointF at(strip.left() + label + x, strip.top() + line * (ph + 3.0));
        x += width;
        return at;
    };
    if (hidden > 0)
        p.drawText(QRectF(place(pw), QSizeF(pw, ph)), Qt::AlignCenter, tr("+%1").arg(hidden));
    for (std::size_t k = hidden; k < pool.size(); ++k) {
        if (k != sideways) {
            paintTile(p, QRectF(place(pw), QSizeF(pw - 1.5, ph)), pool[k]);
            continue;
        }
        // Turned a quarter, its long side along the row and its foot on the
        // row's line.
        const QPointF at = place(ph);
        const QRectF slot(at.x(), at.y() + ph - (pw - 1.5), ph, pw - 1.5);
        p.save();
        p.translate(slot.center());
        p.rotate(90.0);
        paintTile(p, QRectF(-(pw - 1.5) / 2.0, -ph / 2.0, pw - 1.5, ph), pool[k]);
        p.restore();
    }
}

void MahjongTableView::paintPools(QPainter& p) const
{
    const mahjong::Hand* h = m_game.hand();
    if (h == nullptr)
        return;
    const QRectF c = centreRect();
    const QRectF sentence = sentenceRect();
    const bool legible = Legibility::instance().enabled();

    // The round, the wall and any note, along the top of the centre.
    QFont f = font();
    f.setBold(true);
    const double infoH = infoHeight();
    f.setPixelSize(std::max(12, int(infoH * (legible ? 0.72 : 0.62))));
    p.setFont(f);
    p.setPen(kInk);
    QString info = tr("%1 wind round, hand %2. %3 tiles in the wall.")
                       .arg(windName(h->prevailingWind()))
                       .arg(int(m_game.results().size()) + (m_game.handInPlay() ? 1 : 0))
                       .arg(h->wallLeft());
    const QString note = pendingNote();
    if (!note.isEmpty())
        info = tr("%1  %2", "the round and the wall, then a setting waiting for the next game").arg(info, note);
    p.drawText(QRectF(c.left(), c.top(), c.width(), infoH), Qt::AlignLeft | Qt::AlignVCenter, info);

    // One strip per seat, in turn order from you, each labelled; the last
    // discard drawn larger to the right.
    const QRectF pools = poolsRect();
    const double label = poolLabelWidth();
    const double stripH = pools.height() / mahjong::kSeats;
    const double pw = poolTileWidth();
    const double ph = pw * kTileAspect;
    if (showsDora())
        paintDora(p, QRectF(c.left(), centreBody().top(), c.width(), doraRowHeight(pw, doraWordsHeight())), pw);
    for (int seat = 0; seat < mahjong::kSeats; ++seat) {
        const QRectF strip(pools.left(), pools.top() + seat * stripH, pools.width(), stripH);
        p.setFont(poolLabelFont());
        p.setPen(kInkDim);
        p.drawText(QRectF(strip.left(), strip.top(), label - 6.0, ph), Qt::AlignLeft | Qt::AlignVCenter,
                   seatName(seat));
        paintPool(p, seat, strip, label);
    }

    // The tile on offer, or the last one thrown, large.
    const Face shown = h->offered() != 0 ? h->offered() : h->lastDiscard();
    if (shown != 0 && m_game.handInPlay()) {
        const QRectF side(pools.right() + 10.0, pools.top(), c.right() - pools.right() - 10.0, pools.height());
        const double w = std::min({ handTileWidth() * 1.25, side.width() * 0.5, side.height() / kTileAspect * 0.9 });
        const QRectF r(side.center().x() - w / 2.0, side.center().y() - w * kTileAspect / 2.0, w, w * kTileAspect);
        paintTile(p, r, shown, h->offered() != 0);
    }

    // The sentence under the centre: the status bar is not read during play.
    // Furiten, while it holds, takes a line of its own beneath, just above
    // your buttons and in gold, so it is not lost in what just happened.
    const double furitenH = furitenLineHeight();
    const QRectF said(sentence.left(), sentence.top(), sentence.width(), sentence.height() - furitenH);
    if (furitenH > 0.0) {
        const QRectF below(sentence.left(), said.bottom(), sentence.width(), furitenH);
        f.setPixelSize(std::max(legible ? 18 : 13, int(furitenH * 0.62)));
        p.setFont(f);
        p.setPen(Theme::kGold);
        p.drawText(below, Qt::AlignCenter | Qt::TextWordWrap, furitenSentence());
    }
    f.setPixelSize(std::max(13, int(said.height() * (legible ? 0.36 : 0.3))));
    p.setFont(f);
    p.setPen(kInk);
    p.drawText(said, Qt::AlignCenter | Qt::TextWordWrap, centreSentence());
}

void MahjongTableView::paintHand(QPainter& p) const
{
    const mahjong::Hand* h = m_game.hand();
    if (h == nullptr)
        return;
    const std::vector<Face> hand = displayHand(h->seat(0));
    const bool yours = m_game.handInPlay() && h->toMove() == 0;
    const std::vector<Move> legal = yours ? h->legalMoves(0) : std::vector<Move>();
    for (int i = 0; i < int(hand.size()); ++i) {
        QRectF r = handTileRect(i);
        const bool picked = yours && i == m_picked;
        if (picked)
            r.translate(0, -r.height() * kPickedRise);
        paintTile(p, r, hand[std::size_t(i)], picked);
        // A tile you may not throw now, after a riichi or a claim, is dimmed.
        if (yours && !mayThrowFace(legal, hand[std::size_t(i)])) {
            p.save();
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0x00, 0x00, 0x00, 120));
            p.drawRoundedRect(r, r.width() * 0.10, r.width() * 0.10);
            p.restore();
        }
        if (picked && keyboardCursorShown())
            Theme::paintCardCursor(p, r, Legibility::instance().enabled());
    }
}

void MahjongTableView::paintButtons(QPainter& p) const
{
    const std::vector<Button> all = buttons();
    const bool legible = Legibility::instance().enabled();
    for (int i = 0; i < int(all.size()); ++i) {
        const QRectF r = buttonRect(i);
        p.save();
        p.setPen(QPen(Theme::kGold, legible ? 3.0 : 2.0));
        p.setBrush(QColor(0x2a, 0x1e, 0x08));
        p.drawRoundedRect(r, 8, 8);
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(std::max(13, int(r.height() * (legible ? 0.5 : 0.44))));
        p.setFont(f);
        p.setPen(kInk);
        p.drawText(r, Qt::AlignCenter, QStringLiteral("%1  %2").arg(i + 1).arg(all[std::size_t(i)].label));
        p.restore();
    }
}

std::vector<int> MahjongTableView::shownSeats() const
{
    std::vector<int> seats;
    if (m_game.handInPlay() || m_game.results().empty())
        return seats;
    const mahjong::HandResult& r = m_game.results().back();
    if (!r.drawn) {
        seats.push_back(r.win.winner);
        for (const auto& also : r.alsoWon)
            seats.push_back(also.first.winner);
    } else if (drawPays(mahjong::ruleSet(m_game.ruleSetId()))) {
        // The ready hands are shown, as the players lay them down.
        for (int s = 0; s < mahjong::kSeats; ++s)
            if (r.ready[std::size_t(s)])
                seats.push_back(s);
    }
    return seats;
}

void MahjongTableView::paintShownHand(QPainter& p, int seat, Face extra, QPointF at, double right, double w) const
{
    const mahjong::SeatTiles& s = m_game.hand()->seat(seat);
    std::vector<Face> all = s.concealed;
    if (extra != 0)
        all.insert(std::upper_bound(all.begin(), all.end(), extra), extra);
    double x = at.x();
    for (Face t : all) {
        paintTile(p, QRectF(x, at.y(), w - 1.5, w * kTileAspect), t);
        x += w;
    }
    paintSets(p, s, QRectF(x + w * 0.5, at.y(), right - x - w * 0.5, w * kTileAspect), w);
}

void MahjongTableView::paintPanelLines(QPainter& p, const std::vector<PanelLine>& lines, const QRectF& area) const
{
    // One column, or two side by side where that reads larger or one would
    // run past the panel's foot; either way every line is sized to fit its
    // column's width rather than run into the next.
    const bool legible = Legibility::instance().enabled();
    const double least = legible ? 22.0 : 14.0;
    const int count = int(lines.size());
    const auto layout = [&](int columns, double* lineH) {
        *lineH = std::clamp(area.height() / std::max(4, (count + columns - 1) / columns), least, 40.0);
        QFont f = font();
        f.setBold(true); // measured at the widest look
        f.setPixelSize(std::max(legible ? 16 : 12, int(*lineH * (legible ? 0.72 : 0.62))));
        const double room = area.width() / columns - (columns > 1 ? 16.0 : 0.0);
        const auto widest = [&lines](const QFont& font) {
            const QFontMetricsF fm(font);
            double w = 0.0;
            for (const PanelLine& line : lines)
                w = std::max(w, fm.horizontalAdvance(line.text));
            return w;
        };
        while (f.pixelSize() > 10 && widest(f) > room)
            f.setPixelSize(f.pixelSize() - 1);
        return f;
    };
    double oneH = 0.0;
    double twoH = 0.0;
    const QFont one = layout(1, &oneH);
    const QFont two = layout(2, &twoH);
    // Two columns where they read larger, or where one would run past the
    // panel's foot; never below the floor the one column keeps.
    const bool split = two.pixelSize() >= one.pixelSize()
        && (count * least > area.height() || two.pixelSize() > one.pixelSize() + 2);
    const int columns = split ? 2 : 1;
    const int perColumn = (count + columns - 1) / columns;
    const double columnW = area.width() / columns;
    const double lineH = split ? twoH : oneH;
    QFont f = split ? two : one;
    for (int i = 0; i < count; ++i) {
        const PanelLine& line = lines[std::size_t(i)];
        f.setBold(line.look != PanelLine::Plain);
        p.setFont(f);
        p.setPen(line.look == PanelLine::Title ? Theme::kGold : kInk);
        const int column = i / perColumn;
        const int row = i % perColumn;
        p.drawText(QRectF(area.left() + column * columnW, area.top() + row * lineH, columnW, lineH),
                   Qt::AlignLeft | Qt::AlignVCenter, line.text);
    }
}

void MahjongTableView::paintEndOfHand(QPainter& p) const
{
    const std::vector<PanelLine> lines = panelLines();
    if (lines.empty())
        return;
    const mahjong::HandResult& r = m_game.results().back();
    // Between hands in large play the sentence under the centre is empty --
    // the caption plate says what is next -- so the panel takes its room.
    const QRectF c = centreSentence().isEmpty() ? centreRect()
                                                : centreRect().adjusted(0, 0, 0, -sentenceRect().height());
    p.save();
    p.setPen(QPen(Theme::kGold, 2.0));
    p.setBrush(QColor(0x08, 0x1c, 0x12));
    p.drawRoundedRect(c, 10, 10);
    const QRectF inner = c.adjusted(14, 10, -14, -10);
    double y = inner.top();
    // The winning hands, or the ready ones, laid out face up. One winner's
    // hand needs no name; several each have theirs at the left. Where
    // several hands and their many lines cannot both fit, the lines win: they
    // are the result, and hands that small could not be read anyway.
    std::vector<int> seats = shownSeats();
    const double least = Legibility::instance().enabled() ? 22.0 : 14.0;
    const double linesNeed = double(lines.size()) * least;
    if (seats.size() > 1
        && inner.height() - linesNeed < double(seats.size()) * (minimumFaceWidth() * kTileAspect + 6.0))
        seats.clear();
    if (!seats.empty()) {
        const bool named = seats.size() > 1 || r.drawn;
        const double label = named ? poolLabelWidth() : 0.0;
        // A second winner's hand lacks the tile, which joined the first's.
        std::vector<Face> extra(seats.size(), 0);
        for (std::size_t i = 1; i < seats.size() && !r.drawn; ++i)
            extra[i] = r.alsoWon[i - 1].first.winningTile;
        int count = 0;
        for (std::size_t i = 0; i < seats.size(); ++i) {
            const mahjong::SeatTiles& s = m_game.hand()->seat(seats[i]);
            count = std::max(count, int(s.concealed.size()) + (extra[i] != 0 ? 1 : 0) + int(s.melds.size()) * 4);
        }
        double w = std::min(smallTileWidth() * 1.1, (inner.width() - label) / std::max(14, count + 2));
        if (seats.size() > 1) {
            // Several hands share the panel with many lines: the lines keep
            // their floor, and the hands take what height is left.
            const double rows = double(seats.size());
            w = std::min({ w, (inner.height() * 0.45 / rows - 6.0) / kTileAspect,
                           ((inner.height() - linesNeed) / rows - 6.0) / kTileAspect });
        }
        for (std::size_t i = 0; i < seats.size(); ++i) {
            if (named) {
                p.setFont(poolLabelFont());
                p.setPen(kInkDim);
                p.drawText(QRectF(inner.left(), y, label - 6.0, w * kTileAspect), Qt::AlignLeft | Qt::AlignVCenter,
                           seatName(seats[i]));
            }
            paintShownHand(p, seats[i], extra[i], QPointF(inner.left() + label, y), inner.right(), w);
            y += w * kTileAspect + (seats.size() > 1 ? 6.0 : 10.0);
        }
        if (seats.size() > 1)
            y += 4.0;
    }
    paintPanelLines(p, lines, QRectF(inner.left(), y, inner.width(), inner.bottom() - y));
    p.restore();
}

void MahjongTableView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF all(rect());
    Theme::paintFelt(p, all, Theme::kFeltGreenTop, Theme::kFeltGreenBottom);

    const bool legible = Legibility::instance().enabled();
    const TurnLight lit = m_turn.value();
    if (lit.leavingSeat >= 0)
        Theme::paintTurnLight(p, turnArea(lit.leavingSeat), lit.leavingLevel, legible);
    if (lit.seat >= 0)
        Theme::paintTurnLight(p, turnArea(lit.seat), lit.level, legible);

    for (int seat = 0; seat < mahjong::kSeats; ++seat)
        paintSeat(p, seat);
    paintPools(p);
    paintEndOfHand(p);
    paintHand(p);
    paintButtons(p);
    paintStatusCaption(p, all);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void MahjongTableView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const QPointF at = event->position();
    const int count = int(buttons().size());
    for (int i = 0; i < count; ++i) {
        if (buttonRect(i).contains(at)) {
            pressButton(i);
            return;
        }
    }
    if (!m_game.handInPlay() || m_game.hand()->toMove() != 0)
        return;
    const int tiles = int(m_game.hand()->seat(0).concealed.size());
    for (int i = 0; i < tiles; ++i) {
        QRectF r = handTileRect(i);
        if (i == m_picked)
            r.translate(0, -r.height() * kPickedRise);
        if (!r.contains(at))
            continue;
        if (!mayThrow(i))
            return; // dimmed: the sentence says why
        if (i == m_picked) {
            // A second click on the tile picked out throws it.
            const std::vector<Face> hand = displayHand(m_game.hand()->seat(0));
            playHuman({ 0, Action::Discard, hand[std::size_t(i)], 0 });
        } else {
            m_picked = i;
            Sound::instance().play(Sound::kClick);
            refresh();
        }
        return;
    }
}

void MahjongTableView::keyPressEvent(QKeyEvent* event)
{
    const int key = event->key();
    if (key >= Qt::Key_1 && key <= Qt::Key_9) {
        pressButton(key - Qt::Key_1);
        return;
    }
    const bool yours = m_game.handInPlay() && m_game.hand()->toMove() == 0;
    const int tiles = yours ? int(m_game.hand()->seat(0).concealed.size()) : 0;
    switch (key) {
    case Qt::Key_Left:
    case Qt::Key_Right:
        // To the next tile you may throw that way, past any dimmed ones.
        if (yours) {
            const int step = key == Qt::Key_Left ? -1 : 1;
            for (int i = m_picked + step; i >= 0 && i < tiles; i += step) {
                if (mayThrow(i)) {
                    m_picked = i;
                    break;
                }
            }
            refresh();
        }
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (yours && m_picked >= 0 && m_picked < tiles) {
            const std::vector<Face> hand = displayHand(m_game.hand()->seat(0));
            playHuman({ 0, Action::Discard, hand[std::size_t(m_picked)], 0 });
        } else if (!m_game.handInPlay()) {
            pressButton(0); // Next hand, or New game
        }
        return;
    default: break;
    }
    GameView::keyPressEvent(event);
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

QByteArray MahjongTableView::encodeSave(mahjong::RuleSetId rules, mahjong::Length length, int firstDealer,
                                        const std::vector<mahjong::HandRecord>& records)
{
    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << kBlobVersion << quint8(rules) << quint8(length) << quint8(firstDealer) << quint32(records.size());
    for (const mahjong::HandRecord& r : records) {
        out << quint32(r.seed) << quint32(r.moves.size());
        for (const Move& m : r.moves)
            out << quint8(m.seat) << quint8(m.action) << quint8(m.tile) << quint8(m.chowLow);
    }
    return blob;
}

// The game's seeds and moves. The totals, the dealer and every table are what
// replaying them gives, so a save cannot disagree with itself.
QByteArray MahjongTableView::saveState() const
{
    if (m_game.over())
        return {};
    const std::vector<mahjong::HandRecord> records = m_game.records();
    const bool moved = std::any_of(records.begin(), records.end(),
                                   [](const mahjong::HandRecord& r) { return !r.moves.empty(); });
    if (!moved && !m_resumed)
        return {};
    return encodeSave(m_game.ruleSetId(), m_game.length(), m_game.firstDealer(), records);
}

bool MahjongTableView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    quint8 rules = 0;
    quint8 length = 0;
    quint8 dealer = 0;
    quint32 hands = 0;
    in >> version >> rules >> length >> dealer >> hands;
    if (in.status() != QDataStream::Ok || version != kBlobVersion || !mahjong::knownRuleSet(rules)
        || (length != quint8(mahjong::Length::EastRound) && length != quint8(mahjong::Length::FullGame))
        || dealer >= mahjong::kSeats || hands == 0 || hands > kMostHands)
        return false;
    std::vector<mahjong::HandRecord> records(hands);
    for (mahjong::HandRecord& r : records) {
        quint32 seed = 0;
        quint32 count = 0;
        in >> seed >> count;
        if (in.status() != QDataStream::Ok || count > kMostMoves)
            return false;
        r.seed = seed;
        r.moves.resize(count);
        for (Move& m : r.moves) {
            quint8 seat = 0;
            quint8 action = 0;
            quint8 tile = 0;
            quint8 low = 0;
            in >> seat >> action >> tile >> low;
            if (action > quint8(mahjong::kLastAction))
                return false;
            m = { seat, Action(action), Face(tile), low };
        }
        if (in.status() != QDataStream::Ok)
            return false;
    }
    if (!in.atEnd())
        return false;
    // Every move replayed and re-checked; one that is not legal refuses the
    // whole save, and the game on screen is left alone.
    std::optional<mahjong::Game> game =
        mahjong::Game::replay(mahjong::RuleSetId(rules), mahjong::Length(length), dealer, records);
    if (!game || game->over())
        return false;

    abandonThinking();
    m_game = std::move(*game);
    m_resumed = true;
    m_picked = -1;
    advance();
    return true;
}
