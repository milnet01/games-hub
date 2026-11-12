#include "mahjongtableview.h"

#include "mahjongart.h"
#include "mahjongplayer.h"

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
    }
    return {};
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

QString runName(Face low)
{
    // "Three, Four and Five of Bamboo" reads long; the numbers and the suit
    // read the same on a button.
    const QString suit = mahjong::tileName(low).section(QLatin1Char(' '), 1);
    return QCoreApplication::translate("MahjongTableView", "%1-%2-%3 %4")
        .arg(mahjong::rankOf(low))
        .arg(mahjong::rankOf(low) + 1)
        .arg(mahjong::rankOf(low) + 2)
        .arg(suit);
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

    // Only the rule sets this build has: GHUB-0243 and GHUB-0244 join here.
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
    m_note = id == m_game.ruleSetId() ? QString() : tr("%1 from the next game.").arg(ruleSetLabel(id));
    refresh();
}

void MahjongTableView::setLength(mahjong::Length length)
{
    QSettings().setValue(lengthKey(), lengthWord(length));
    m_nextLength = length;
    if (length == m_game.length())
        m_note.clear();
    else
        m_note = length == mahjong::Length::FullGame ? tr("A full game from the next game.")
                                                     : tr("An East round from the next game.");
    refresh();
}

// GHUB-0016. The rules as mahjong::Hand and HongKongRules play them, and the
// controls as mousePressEvent, keyPressEvent and buildActions take them. The
// fan list comes from the rule set in force, in the words the end of a hand
// uses.
QString MahjongTableView::rulesText() const
{
    QString fans;
    const mahjong::RuleSet& rules = mahjong::ruleSet(m_game.ruleSetId());
    for (const mahjong::ScoreItem& item : rules.fanList())
        fans.append(tr("<li>%1: %n fan</li>", nullptr, item.value).arg(fanName(item.fan)));

    return tr("<h3>The aim</h3>"
              "<p>Make a winning hand before the three computer players do: four sets and a pair, "
              "from fourteen tiles. The game is played by %1.</p>"

              "<h3>The tiles</h3>"
              "<p>The tiles are the ones Mahjong Solitaire uses: circles, bamboo and diamonds "
              "numbered 1 to 9, four winds and three dragons, four of each, and one each of four "
              "flowers and four seasons.</p>"

              "<h3>How it plays</h3>"
              "<p>You sit at the bottom of the table. The player on your right plays after you, "
              "then the one across, then the one on your left.</p>"
              "<p>Each player starts with thirteen tiles. On your turn you draw a tile from the "
              "wall, then throw one away. The tile you drew stands a little apart at the right "
              "of your hand.</p>"
              "<p>A <b>flower</b> or <b>season</b> is not kept in the hand. It is laid face up "
              "in front of you and another tile is drawn in its place.</p>"
              "<p>A set is three of one tile (a <b>pung</b>), four of one tile (a <b>kong</b>), "
              "or three in a row in one suit (a <b>chow</b>).</p>"
              "<p>When someone throws a tile, any other player may <b>claim</b> it: to win, to "
              "make a pung or kong with two or three they hold, or, for the next player only, to "
              "make a chow. A claimed set is laid face up. A win beats a pung or kong, which "
              "beats a chow. Two wins go to the first player after the one who threw.</p>"
              "<p>Holding four of a tile, you may lay it down as a kong on your turn and draw "
              "another tile. You may also add the tile you just drew to a pung you laid down. Another "
              "player waiting on that tile may then win on it: that is <b>robbing a kong</b>.</p>"
              "<p>A hand needs at least %2 fan to win. If the wall runs out with nobody out, "
              "the hand is drawn and no points move.</p>"

              "<h3>Scoring</h3>"
              "<p>Every scoring feature of a win is written on the table with its value. "
              "%3 fan is the most a hand can score.</p>"
              "<ul>%4</ul>"
              "<p>A win on a thrown tile is paid by the player who threw it. A win on a tile you "
              "drew is paid by all three others. A robbed kong is paid by the player who added the tile. 3 fan is worth 8 points, 4 is 16, 5 is 24, "
              "6 is 32, 7 is 48, 8 is 64, 9 is 96 and 10 is 128.</p>"
              "<p>The dealer is East. The dealer keeps the deal after winning, or after a drawn hand; "
              "otherwise it passes to the next player. A <b>full game</b> ends when the deal has "
              "gone round the table four times, with the wind of the round moving from East to "
              "South, West and North. An <b>East round</b> ends after it has gone round once.</p>"

              "<h3>Controls</h3>"
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
              "</ul>")
        .arg(ruleSetLabel(m_game.ruleSetId()))
        .arg(rules.minimumToWin())
        .arg(mahjong::HongKongRules::kLimit)
        .arg(fans);
}

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
    }
    return {};
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
    m_note.clear();
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
        apply(mahjong::chooseMove(h, computer));
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
        if (h.toMove() == 0) {
            const int size = int(h.seat(0).concealed.size());
            if (m_picked < 0 || m_picked >= size)
                m_picked = size - 1; // the tile just drawn
        }
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
        apply(mahjong::chooseMove(h, h.toMove()));
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

void MahjongTableView::apply(const Move& move, bool loud)
{
    if (!m_game.play(move))
        return;
    if (move.seat == 0 && move.action == Action::Discard)
        m_picked = -1;
    if (!loud)
        return;
    switch (move.action) {
    case Action::Discard: Sound::instance().play(Sound::kDiscPlace); break;
    case Action::Pass: break;
    default: Sound::instance().play(Sound::kClick); break;
    }
    if (!m_game.handInPlay()) {
        const mahjong::HandResult& r = m_game.results().back();
        if (!r.drawn)
            Sound::instance().play(r.win.winner == 0 ? Sound::kWin : Sound::kLose);
    }
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
    m_picked = -1;
    if (m_game.handInPlay() && m_game.hand()->toMove() == 0)
        m_picked = int(m_game.hand()->seat(0).concealed.size()) - 1;
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
        if (top.size() > 1)
            return tr("Game over. %1 share the top score, %2.").arg(names.join(tr(" and "))).arg(score);
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
        said << (h->lastDiscarder() == 0
                     ? tr("You threw the %1.").arg(mahjong::tileName(h->lastDiscard()))
                     : tr("%1 threw the %2.").arg(seatName(h->lastDiscarder()), mahjong::tileName(h->lastDiscard())));
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
    if (h->toMove() == 0)
        return tr("Your turn: pick a tile and throw it.");
    return tr("%1 to play.").arg(seatName(h->toMove()));
}

QString MahjongTableView::tableSentence() const
{
    return handSentence();
}

QString MahjongTableView::centreSentence() const
{
    if (!Legibility::instance().enabled())
        return tableSentence();
    return happenedSentences().join(QLatin1Char(' '));
}

QString MahjongTableView::captionText() const
{
    const QStringList happened = happenedSentences();
    return happened.isEmpty() ? tableSentence() : nextSentence();
}

QStringList MahjongTableView::endOfHandLines() const
{
    QStringList lines;
    if (m_game.handInPlay() || m_game.results().empty())
        return lines;
    const mahjong::HandResult& r = m_game.results().back();
    if (r.drawn) {
        lines << tr("A drawn hand. No points move, and the dealer deals again.");
        return lines;
    }
    const QString who = seatName(r.win.winner);
    lines << (r.win.payer < 0 ? tr("%1 won on a tile drawn from the wall.").arg(who)
                              : tr("%1 won on the %2 from %3.")
                                    .arg(who, mahjong::tileName(r.win.winningTile), seatName(r.win.payer)));
    for (const mahjong::ScoreItem& item : r.score.items)
        lines << tr("%1 — %n fan", nullptr, item.value).arg(fanName(item.fan));
    lines << (r.score.limit ? tr("A limit hand: %1 fan, %2 points.").arg(r.score.total).arg(r.score.points)
                            : tr("Total: %1 fan, %2 points.").arg(r.score.total).arg(r.score.points));
    QStringList pays;
    for (int s = 0; s < mahjong::kSeats; ++s) {
        const int p = r.payments[std::size_t(s)];
        pays << (p > 0 ? tr("%1 +%2").arg(seatName(s)).arg(p) : tr("%1 %2").arg(seatName(s)).arg(p));
    }
    lines << pays.join(QStringLiteral(",  "));
    return lines;
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
        case Action::Chow: label = tr("Chow %1").arg(runName(Face(m.chowLow))); break;
        case Action::ConcealedKong:
        case Action::AddedKong: label = tr("Kong of %1").arg(mahjong::tileName(m.tile)); break;
        case Action::Pass: label = tr("Pass"); break;
        case Action::Discard: continue;
        }
        out.push_back({ label, [self, m] { self->playHuman(m); } });
    }
    if (h.toMove() == 0) {
        const std::vector<Face> hand = displayHand(h.seat(0));
        if (m_picked >= 0 && m_picked < int(hand.size())) {
            const Move throwIt { 0, Action::Discard, hand[std::size_t(m_picked)], 0 };
            out.push_back({ tr("Throw the %1").arg(mahjong::tileName(throwIt.tile)),
                            [self, throwIt] { self->playHuman(throwIt); } });
        }
    }
    return out;
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
                                     : tr("%1 won, %2 fan").arg(seatName(r.win.winner)).arg(r.score.total);
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
    const double h = std::clamp(c.height() * 0.16, 28.0, 90.0);
    return { c.left(), c.bottom() - h, c.width(), h };
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

QRectF MahjongTableView::poolsRect() const
{
    const QRectF c = centreRect();
    const double infoH = std::clamp(c.height() * 0.08, 20.0, 34.0);
    return { c.left(), c.top() + infoH + 4.0, c.width() * 0.66, sentenceRect().top() - c.top() - infoH - 8.0 };
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
    return std::max(std::min(90.0, poolsRect().width() * 0.16), widest + 10.0);
}

double MahjongTableView::poolTileWidth() const
{
    // As large as lets the longest pool -- or a dozen tiles, so the size does
    // not jump about early in a hand -- fit its strip, every tile shown. Large
    // play never goes below the floor; a pool too long for its strip at the
    // floor shows its latest tiles and counts the rest (paintPools).
    const mahjong::Hand* h = m_game.hand();
    const QRectF pools = poolsRect();
    const double label = poolLabelWidth();
    const double stripH = pools.height() / mahjong::kSeats;
    std::size_t longest = 12;
    if (h != nullptr)
        for (int seat = 0; seat < mahjong::kSeats; ++seat)
            longest = std::max(longest, h->seat(seat).pool.size());
    const double floor = Legibility::instance().enabled() ? mahjong::kTileMinWidth : 10.0;
    double pw = handTileWidth() * 0.7;
    for (; pw > floor; pw -= 1.0) {
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
    const QString line = tr("Score %1").arg(m_game.totals()[std::size_t(seat)]);
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
    const double infoH = std::clamp(c.height() * 0.08, 20.0, 34.0);
    f.setPixelSize(std::max(12, int(infoH * (legible ? 0.72 : 0.62))));
    p.setFont(f);
    p.setPen(kInk);
    QString info = tr("%1 wind round, hand %2. %3 tiles in the wall.")
                       .arg(windName(h->prevailingWind()))
                       .arg(int(m_game.results().size()) + (m_game.handInPlay() ? 1 : 0))
                       .arg(h->wallLeft());
    if (!m_note.isEmpty())
        info += QStringLiteral("  ") + m_note;
    p.drawText(QRectF(c.left(), c.top(), c.width(), infoH), Qt::AlignLeft | Qt::AlignVCenter, info);

    // One strip per seat, in turn order from you, each labelled; the last
    // discard drawn larger to the right.
    const QRectF pools = poolsRect();
    const double label = poolLabelWidth();
    const double stripH = pools.height() / mahjong::kSeats;
    const double pw = poolTileWidth();
    const double ph = pw * kTileAspect;
    for (int i = 0; i < mahjong::kSeats; ++i) {
        const int seat = i;
        const QRectF strip(pools.left(), pools.top() + i * stripH, pools.width(), stripH);
        p.setFont(poolLabelFont());
        p.setPen(kInkDim);
        p.drawText(QRectF(strip.left(), strip.top(), label - 6.0, ph), Qt::AlignLeft | Qt::AlignVCenter,
                   seatName(seat));
        const std::vector<Face>& pool = h->seat(seat).pool;
        const int perLine = std::max(1, int((strip.width() - label) / pw));
        const int lines = std::max(1, int((stripH - 1.0) / (ph + 3.0)));
        const std::size_t room = std::size_t(perLine * lines);
        // Too many to show at this size: the first slot says how many earlier
        // tiles are not drawn, and the latest fill the rest.
        const std::size_t hidden = pool.size() > room ? pool.size() - room + 1 : 0;
        if (hidden > 0)
            p.drawText(QRectF(strip.left() + label, strip.top(), pw, ph), Qt::AlignCenter,
                       tr("+%1").arg(hidden));
        for (std::size_t k = hidden; k < pool.size(); ++k) {
            const std::size_t slot = hidden > 0 ? k - hidden + 1 : k;
            const int line = int(slot) / perLine;
            const int col = int(slot) % perLine;
            paintTile(p, QRectF(strip.left() + label + col * pw, strip.top() + line * (ph + 3.0), pw - 1.5, ph),
                      pool[k]);
        }
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
    f.setPixelSize(std::max(13, int(sentence.height() * (legible ? 0.36 : 0.3))));
    p.setFont(f);
    p.setPen(kInk);
    p.drawText(sentence, Qt::AlignCenter | Qt::TextWordWrap, centreSentence());
}

void MahjongTableView::paintHand(QPainter& p) const
{
    const mahjong::Hand* h = m_game.hand();
    if (h == nullptr)
        return;
    const std::vector<Face> hand = displayHand(h->seat(0));
    const bool yours = m_game.handInPlay() && h->toMove() == 0;
    for (int i = 0; i < int(hand.size()); ++i) {
        QRectF r = handTileRect(i);
        const bool picked = yours && i == m_picked;
        if (picked)
            r.translate(0, -r.height() * kPickedRise);
        paintTile(p, r, hand[std::size_t(i)], picked);
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

void MahjongTableView::paintEndOfHand(QPainter& p) const
{
    const QStringList lines = endOfHandLines();
    if (lines.isEmpty())
        return;
    const mahjong::HandResult& r = m_game.results().back();
    const QRectF c = centreRect().adjusted(0, 0, 0, -sentenceRect().height());
    p.save();
    p.setPen(QPen(Theme::kGold, 2.0));
    p.setBrush(QColor(0x08, 0x1c, 0x12));
    p.drawRoundedRect(c, 10, 10);
    const QRectF inner = c.adjusted(14, 10, -14, -10);
    double y = inner.top();
    if (!r.drawn) {
        // The winning hand, laid out face up.
        const mahjong::SeatTiles& s = m_game.hand()->seat(r.win.winner);
        std::vector<Face> all = s.concealed;
        const int count = int(all.size()) + int(s.melds.size()) * 4;
        const double w = std::min(smallTileWidth() * 1.1, inner.width() / std::max(14, count + 2));
        double x = inner.left();
        for (Face t : all) {
            paintTile(p, QRectF(x, y, w - 1.5, w * kTileAspect), t);
            x += w;
        }
        paintSets(p, s, QRectF(x + w * 0.5, y, inner.right() - x - w * 0.5, w * kTileAspect), w);
        y += w * kTileAspect + 10.0;
    }
    const bool legible = Legibility::instance().enabled();
    const double lineH = std::clamp((inner.bottom() - y) / std::max(4, int(lines.size())), legible ? 22.0 : 14.0, 40.0);
    QFont f = font();
    f.setPixelSize(std::max(legible ? 16 : 12, int(lineH * (legible ? 0.72 : 0.62))));
    for (int i = 0; i < int(lines.size()); ++i) {
        f.setBold(i == 0 || i >= int(lines.size()) - 2);
        p.setFont(f);
        p.setPen(i == 0 ? Theme::kGold : kInk);
        p.drawText(QRectF(inner.left(), y + i * lineH, inner.width(), lineH), Qt::AlignLeft | Qt::AlignVCenter,
                   lines[i]);
    }
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
        if (yours) {
            m_picked = std::max(0, m_picked - 1);
            refresh();
        }
        return;
    case Qt::Key_Right:
        if (yours) {
            m_picked = std::min(tiles - 1, m_picked + 1);
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
            if (action > quint8(Action::Pass))
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
    m_note.clear();
    advance();
    return true;
}
