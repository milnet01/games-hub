#include "canastaview.h"

#include "cards/cardart.h"
#include "scores.h"
#include "sound.h"
#include "theme.h"

#include <QActionGroup>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <random>

namespace ca = canasta;

namespace {

constexpr double kTick = 1.0 / 60.0;
// Long enough to follow, short enough that three computer seats do not become
// a wait. Each half-turn gets one of these.
// Long enough to follow what the other three are doing. It was 0.42, which is
// fine if you can read a card at a glance and far too fast if you cannot.
constexpr double kAiPause = 0.95;
constexpr double kFlourish = 1.3;
// Melds are drawn smaller than a hand card, but not so small that the face
// stops being drawn: below about 46 pixels wide CardArt gives up on the pips
// and leaves only the corner index, which reads as a sliver rather than a card.
// Melded cards are drawn smaller than the ones in your hand, but not so small
// that the shared card art gives up on the face — below 46 pixels wide it draws
// the corner index alone, and a column of corner indices is unreadable.
constexpr double kMeldScale = 0.74;

const QColor kInk { 0xf4, 0xea, 0xdd };
const QColor kInkDim { 0xc9, 0xb6, 0xa2 };
const QColor kPanel { 0x2a, 0x0d, 0x14, 0xe6 };

// Cards `after` holds that `before` did not. Used instead of index arithmetic
// because placing a red three removes from the middle and appends at the end.
std::vector<Card> gainedCards(std::vector<Card> before, const std::vector<Card>& after)
{
    std::vector<Card> out;
    for (const Card& c : after) {
        auto it = std::find(before.begin(), before.end(), c);
        if (it == before.end())
            out.push_back(c);
        else
            before.erase(it);
    }
    return out;
}

// Cards that appeared in a team's melds, paired with the meld they joined.
std::vector<std::pair<int, Card>> meldGains(const ca::Team& before, const ca::Team& after)
{
    std::vector<std::pair<int, Card>> out;
    for (const ca::Meld& m : after.melds) {
        const ca::Meld* old = before.meldOfRank(m.rank);
        std::vector<Card> had = old ? old->cards : std::vector<Card> {};
        for (const Card& c : m.cards) {
            auto it = std::find(had.begin(), had.end(), c);
            if (it == had.end())
                out.push_back({ m.rank, c });
            else
                had.erase(it);
        }
    }
    return out;
}

int canastaCount(const ca::Team& t, const ca::Rules& r)
{
    return int(std::count_if(t.melds.begin(), t.melds.end(),
                             [&](const ca::Meld& m) { return m.isCanasta(r); }));
}

QString seatName(int seat)
{
    switch (seat) {
    case 0: return QStringLiteral("You");
    case 1: return QStringLiteral("West");
    case 2: return QStringLiteral("North");
    default: return QStringLiteral("East");
    }
}

int loadTarget()
{
    const int t = QSettings().value(QStringLiteral("canasta/target"), 5000).toInt();
    // Anything else in the file is somebody's hand edit; fall back rather than
    // deal a game that can never end.
    for (const int allowed : { 1000, 2000, 3000, 5000 })
        if (t == allowed)
            return t;
    return 5000;
}

// --- house rules, saved between sessions -----------------------------------

const char* kHouseGroup = "canasta/house/";

void storeHouse(const ca::Rules& r)
{
    QSettings s;
    const auto put = [&](const char* key, int v) {
        s.setValue(QString::fromLatin1(kHouseGroup) + QLatin1String(key), v);
    };
    put("handSize", r.handSize);
    put("canastaSize", r.canastaSize);
    put("maxWilds", r.maxWildsPerMeld);
    put("openBelowZero", r.openMinBelowZero);
    put("openUnder1500", r.openMinUnder1500);
    put("openUnder3000", r.openMinUnder3000);
    put("openAbove3000", r.openMinAbove3000);
    put("naturalCanasta", r.naturalCanastaBonus);
    put("mixedCanasta", r.mixedCanastaBonus);
    put("redThree", r.redThreeValue);
    put("allRedThrees", r.allRedThreesValue);
    put("goingOut", r.goingOutBonus);
    put("concealed", r.concealedGoingOutBonus);
    put("requireCanasta", r.requireCanastaToGoOut ? 1 : 0);
    put("blackThreeBlocks", r.blackThreeBlocksPile ? 1 : 0);
    put("wildTake", r.unfrozenPileTakeableWithWild ? 1 : 0);
    put("wildsFewer", r.wildsFewerThanNaturals ? 1 : 0);
    put("canastaToScore", r.canastaNeededToScore ? 1 : 0);
    put("closedCanasta", r.canastaMakesRankSafe ? 1 : 0);
    put("noMeldFirstRound", r.noMeldingFirstRound ? 1 : 0);
    put("pileOpens", r.pileMeldCountsToOpen ? 1 : 0);
}

ca::Rules loadHouse()
{
    ca::Rules r = ca::Rules::classic();
    r.name = QStringLiteral("House");
    QSettings s;
    const auto get = [&](const char* key, int fallback) {
        return s.value(QString::fromLatin1(kHouseGroup) + QLatin1String(key), fallback).toInt();
    };
    r.handSize = get("handSize", r.handSize);
    r.canastaSize = get("canastaSize", r.canastaSize);
    r.maxWildsPerMeld = get("maxWilds", r.maxWildsPerMeld);
    r.openMinBelowZero = get("openBelowZero", r.openMinBelowZero);
    r.openMinUnder1500 = get("openUnder1500", r.openMinUnder1500);
    r.openMinUnder3000 = get("openUnder3000", r.openMinUnder3000);
    r.openMinAbove3000 = get("openAbove3000", r.openMinAbove3000);
    r.naturalCanastaBonus = get("naturalCanasta", r.naturalCanastaBonus);
    r.mixedCanastaBonus = get("mixedCanasta", r.mixedCanastaBonus);
    r.redThreeValue = get("redThree", r.redThreeValue);
    r.allRedThreesValue = get("allRedThrees", r.allRedThreesValue);
    r.goingOutBonus = get("goingOut", r.goingOutBonus);
    r.concealedGoingOutBonus = get("concealed", r.concealedGoingOutBonus);
    r.requireCanastaToGoOut = get("requireCanasta", 1) != 0;
    r.blackThreeBlocksPile = get("blackThreeBlocks", 1) != 0;
    r.unfrozenPileTakeableWithWild = get("wildTake", 1) != 0;
    r.wildsFewerThanNaturals = get("wildsFewer", 0) != 0;
    r.canastaNeededToScore = get("canastaToScore", 0) != 0;
    r.canastaMakesRankSafe = get("closedCanasta", 0) != 0;
    r.noMeldingFirstRound = get("noMeldFirstRound", 0) != 0;
    r.pileMeldCountsToOpen = get("pileOpens", 1) != 0;
    return r;
}

// The house-rules editor. Every field is one number from canasta::Rules, which
// is the whole reason that struct exists.
bool editHouseRules(QWidget* parent, ca::Rules& rules)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("House rules"));

    auto* form = new QFormLayout;
    const auto spin = [&](const QString& label, int value, int lo, int hi) {
        auto* box = new QSpinBox(&dlg);
        box->setRange(lo, hi);
        box->setValue(value);
        form->addRow(label, box);
        return box;
    };
    const auto tick = [&](const QString& label, bool on) {
        auto* box = new QCheckBox(&dlg);
        box->setChecked(on);
        form->addRow(label, box);
        return box;
    };

    auto* handSize = spin(QStringLiteral("Cards dealt to each player"), rules.handSize, 7, 15);
    auto* canastaSize = spin(QStringLiteral("Cards in a canasta"), rules.canastaSize, 4, 10);
    auto* maxWilds = spin(QStringLiteral("Most wild cards in one meld"), rules.maxWildsPerMeld, 0, 5);

    auto* openLow = spin(QStringLiteral("Opening minimum, score below zero"),
                         rules.openMinBelowZero, 0, 300);
    auto* openMid = spin(QStringLiteral("Opening minimum, under 1500"), rules.openMinUnder1500, 0,
                         300);
    auto* openHigh = spin(QStringLiteral("Opening minimum, 1500 to 2999"), rules.openMinUnder3000,
                          0, 300);
    auto* openTop = spin(QStringLiteral("Opening minimum, 3000 and up"), rules.openMinAbove3000, 0,
                         300);

    auto* natural = spin(QStringLiteral("Natural canasta bonus"), rules.naturalCanastaBonus, 0,
                         2000);
    auto* mixed = spin(QStringLiteral("Mixed canasta bonus"), rules.mixedCanastaBonus, 0, 2000);
    auto* redThree = spin(QStringLiteral("Each red three"), rules.redThreeValue, 0, 500);
    auto* allReds = spin(QStringLiteral("All four red threes"), rules.allRedThreesValue, 0, 2000);
    auto* goingOut = spin(QStringLiteral("Going out"), rules.goingOutBonus, 0, 1000);
    auto* concealed = spin(QStringLiteral("Going out concealed"), rules.concealedGoingOutBonus, 0,
                           1000);

    auto* needCanasta = tick(QStringLiteral("A canasta is needed to go out"),
                             rules.requireCanastaToGoOut);
    auto* blackBlocks = tick(QStringLiteral("A black three blocks the pile"),
                             rules.blackThreeBlocksPile);
    auto* wildTake = tick(QStringLiteral("An open pile can be taken with a wild card"),
                          rules.unfrozenPileTakeableWithWild);
    auto* wildsFewer = tick(QStringLiteral("A meld keeps more real cards than wild ones"),
                            rules.wildsFewerThanNaturals);
    auto* needCanastaToScore = tick(QStringLiteral("A side with no canasta counts nothing in its "
                                                   "favour"),
                                    rules.canastaNeededToScore);
    // Key still reads "closedCanasta" from when this rule was first written the
    // wrong way round: renaming it would silently untick it for anyone who has
    // already set it, and the setting itself is the same one.
    auto* closedCanasta = tick(QStringLiteral("A canasta makes its rank a safe discard"),
                               rules.canastaMakesRankSafe);
    auto* firstRound = tick(QStringLiteral("Nobody lays down in the first round"),
                            rules.noMeldingFirstRound);
    auto* pileOpens = tick(QStringLiteral("The pile can be part of your opening"),
                           rules.pileMeldCountsToOpen);

    auto* layout = new QVBoxLayout(&dlg);
    auto* blurb = new QLabel(
        QStringLiteral("These are your own rules. Classic Canasta is always still there\n"
                       "on the Rules menu, so nothing here can lose it."),
        &dlg);
    blurb->setWordWrap(true);
    layout->addWidget(blurb);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                             | QDialogButtonBox::RestoreDefaults,
                                         &dlg);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QObject::connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
                     [&] {
                         const ca::Rules c = ca::Rules::classic();
                         handSize->setValue(c.handSize);
                         canastaSize->setValue(c.canastaSize);
                         maxWilds->setValue(c.maxWildsPerMeld);
                         openLow->setValue(c.openMinBelowZero);
                         openMid->setValue(c.openMinUnder1500);
                         openHigh->setValue(c.openMinUnder3000);
                         openTop->setValue(c.openMinAbove3000);
                         natural->setValue(c.naturalCanastaBonus);
                         mixed->setValue(c.mixedCanastaBonus);
                         redThree->setValue(c.redThreeValue);
                         allReds->setValue(c.allRedThreesValue);
                         goingOut->setValue(c.goingOutBonus);
                         concealed->setValue(c.concealedGoingOutBonus);
                         needCanasta->setChecked(c.requireCanastaToGoOut);
                         blackBlocks->setChecked(c.blackThreeBlocksPile);
                         wildTake->setChecked(c.unfrozenPileTakeableWithWild);
                         wildsFewer->setChecked(c.wildsFewerThanNaturals);
                         needCanastaToScore->setChecked(c.canastaNeededToScore);
                         closedCanasta->setChecked(c.canastaMakesRankSafe);
                         firstRound->setChecked(c.noMeldingFirstRound);
                         pileOpens->setChecked(c.pileMeldCountsToOpen);
                     });

    if (dlg.exec() != QDialog::Accepted)
        return false;

    rules.handSize = handSize->value();
    rules.canastaSize = canastaSize->value();
    rules.maxWildsPerMeld = maxWilds->value();
    rules.openMinBelowZero = openLow->value();
    rules.openMinUnder1500 = openMid->value();
    rules.openMinUnder3000 = openHigh->value();
    rules.openMinAbove3000 = openTop->value();
    rules.naturalCanastaBonus = natural->value();
    rules.mixedCanastaBonus = mixed->value();
    rules.redThreeValue = redThree->value();
    rules.allRedThreesValue = allReds->value();
    rules.goingOutBonus = goingOut->value();
    rules.concealedGoingOutBonus = concealed->value();
    rules.requireCanastaToGoOut = needCanasta->isChecked();
    rules.blackThreeBlocksPile = blackBlocks->isChecked();
    rules.unfrozenPileTakeableWithWild = wildTake->isChecked();
    rules.wildsFewerThanNaturals = wildsFewer->isChecked();
    rules.canastaNeededToScore = needCanastaToScore->isChecked();
    rules.canastaMakesRankSafe = closedCanasta->isChecked();
    rules.noMeldingFirstRound = firstRound->isChecked();
    rules.pileMeldCountsToOpen = pileOpens->isChecked();
    storeHouse(rules);
    return true;
}

} // namespace

// ---------------------------------------------------------------------------

CanastaView::CanastaView(QWidget* parent)
    : GameView(parent)
    , m_ai { ca::Ai {}, ca::Ai {}, ca::Ai {}, ca::Ai {} }
    , m_house(loadHouse())
    , m_target(loadTarget())
{
    setMouseTracking(true);
    setMinimumSize(minimumSizeHint());

    m_timer = new QTimer(this);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, &CanastaView::tick);

    m_sortHand = QSettings().value(QStringLiteral("canasta/sortHand"), true).toBool();
    buildActions();
    newGame();
}

void CanastaView::buildActions()
{
    auto* newAction = new QAction(QStringLiteral("New Game"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &CanastaView::newGame);
    m_actions.append(newAction);

    // Deliberately NOT in m_actions, so it never reaches the toolbar: laying
    // cards down belongs on the table, where the Lay down button is. This
    // keeps the space bar working, and refresh() still uses it to decide
    // whether the move is available at all.
    m_meldAction = new QAction(QStringLiteral("Meld"), this);
    m_meldAction->setShortcut(Qt::Key_Space);
    connect(m_meldAction, &QAction::triggered, this, [this] { humanMeld(-1); });
    addAction(m_meldAction);

    m_discardAction = new QAction(QStringLiteral("Discard"), this);
    m_discardAction->setShortcut(Qt::Key_Return);
    connect(m_discardAction, &QAction::triggered, this, &CanastaView::humanDiscard);
    m_actions.append(m_discardAction);

    // Sits with the hand actions rather than with the display toggles at the
    // far end, because that end is the first thing a narrow window hides.
    auto* sort = new QAction(QStringLiteral("Sort"), this);
    sort->setCheckable(true);
    sort->setChecked(m_sortHand);
    connect(sort, &QAction::toggled, this, [this](bool on) {
        m_sortHand = on;
        QSettings().setValue(QStringLiteral("canasta/sortHand"), on);
        sortHand();
        update();
    });
    m_actions.append(sort);

    auto* sep1 = new QAction(this);
    sep1->setSeparator(true);
    m_actions.append(sep1);

    auto* levels = new QActionGroup(this);
    levels->setExclusive(true);
    const struct { const char* name; ca::Level value; } kLevels[] = {
        { "Easy", ca::Level::Easy },
        { "Medium", ca::Level::Medium },
        { "Hard", ca::Level::Hard },
        { "Expert", ca::Level::Expert },
    };
    for (const auto& entry : kLevels) {
        auto* a = new QAction(QString::fromUtf8(entry.name), this);
        a->setCheckable(true);
        a->setChecked(entry.value == m_level);
        levels->addAction(a);
        const ca::Level value = entry.value;
        connect(a, &QAction::triggered, this, [this, value] {
            m_level = value;
            for (ca::Ai& ai : m_ai)
                ai.setLevel(value);
            refresh();
        });
        m_actions.append(a);
    }

    auto* sep2 = new QAction(this);
    sep2->setSeparator(true);
    m_actions.append(sep2);

    // Which rule set is in force. Classic is never edited, so it is always here
    // to come back to.
    auto* sets = new QActionGroup(this);
    sets->setExclusive(true);
    auto* classic = new QAction(QStringLiteral("Classic"), this);
    classic->setCheckable(true);
    classic->setChecked(true);
    sets->addAction(classic);
    connect(classic, &QAction::triggered, this, [this] {
        m_useHouse = false;
        newGame();
    });
    m_actions.append(classic);

    auto* house = new QAction(QStringLiteral("House"), this);
    house->setCheckable(true);
    sets->addAction(house);
    connect(house, &QAction::triggered, this, [this] {
        m_useHouse = true;
        newGame();
    });
    m_actions.append(house);

    m_rulesAction = new QAction(QStringLiteral("House rules…"), this);
    connect(m_rulesAction, &QAction::triggered, this, [this, house] {
        if (!editHouseRules(this, m_house))
            return;
        m_useHouse = true;
        house->setChecked(true);
        newGame();
    });
    m_actions.append(m_rulesAction);

    auto* sep3 = new QAction(this);
    sep3->setSeparator(true);
    m_actions.append(sep3);

    auto* targets = new QActionGroup(this);
    targets->setExclusive(true);
    for (const int score : { 1000, 2000, 3000, 5000 }) {
        auto* a = new QAction(QStringLiteral("Play to %1").arg(score), this);
        a->setCheckable(true);
        a->setChecked(score == m_target);
        targets->addAction(a);
        connect(a, &QAction::triggered, this, [this, score] {
            m_target = score;
            QSettings().setValue(QStringLiteral("canasta/target"), score);
            newGame();
        });
        m_actions.append(a);
    }

    auto* sep4 = new QAction(this);
    sep4->setSeparator(true);
    m_actions.append(sep4);

    auto* hints = new QAction(QStringLiteral("Hints"), this);
    hints->setCheckable(true);
    hints->setChecked(m_showHints);
    connect(hints, &QAction::toggled, this, [this](bool on) {
        m_showHints = on;
        update();
    });
    m_actions.append(hints);
}

void CanastaView::newGame()
{
    ca::Rules r = m_useHouse ? m_house : ca::Rules::classic();
    r.targetScore = m_target;
    m_engine.setRules(r);
    m_engine.newGame();

    // Fresh seeds each game, so the same three opponents do not replay the same
    // decisions every time you press New Game.
    std::random_device rd;
    for (ca::Ai& ai : m_ai) {
        ai.setLevel(m_level);
        ai.seed(rd());
    }

    m_flights.clear();
    m_selected.clear();
    m_hover = -1;
    m_hoverMeld = -1;
    m_pause = 0.0;
    m_celebrate = 0.0;
    m_canastasShown = 0;
    m_awaitingContinue = false;
    m_message.clear();
    m_lastThrownBy = -1;

    // Before the deal flies, so each card is aimed at the slot it will keep.
    sortHand();
    Sound::instance().play(Sound::kShuffle);
    flyTheDeal();
    m_timer->start();
    refresh();
}

QByteArray CanastaView::saveState() const
{
    // A finished game is not worth coming back to; New Game is the answer to
    // that, and keeping it would resume onto the final scores every time.
    if (m_engine.phase() == ca::Engine::Phase::GameOver)
        return {};

    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    // 2 adds the engine's tail — rules that did not exist when 1 was written.
    out << quint32(2);
    m_engine.save(out);
    out << qint32(m_level) << m_useHouse << qint32(m_target) << m_sortHand;
    return blob;
}

bool CanastaView::restoreState(const QByteArray& blob)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    in >> version;
    // A game saved by an older build still comes back; it simply predates the
    // rules the tail carries, and their defaults stand.
    if (version < 1 || version > 2 || !m_engine.load(in, version >= 2))
        return false;

    qint32 level = 0;
    qint32 target = 0;
    in >> level >> m_useHouse >> target >> m_sortHand;
    if (in.status() != QDataStream::Ok)
        return false;

    m_level = ca::Level(std::clamp<int>(level, 0, 3));
    m_target = target;
    for (ca::Ai& ai : m_ai)
        ai.setLevel(m_level);

    // Nothing was in the air when the game was put away, and nothing is now.
    m_flights.clear();
    m_selected.clear();
    m_hover = -1;
    m_hoverMeld = -1;
    m_pressIndex = -1;
    m_dragging = false;
    m_pause = kAiPause;
    m_celebrate = 0.0;
    m_lastThrownBy = -1;
    m_message.clear();
    m_canastasShown = canastaCount(m_engine.team(0), m_engine.rules());
    // A hand that had just been scored is waiting on a click, exactly as it was.
    m_awaitingContinue = m_engine.phase() == ca::Engine::Phase::HandOver;

    // The toolbar was built before any of this was known.
    for (QAction* a : m_actions) {
        if (a->isCheckable() && a->text() == QStringLiteral("Sort"))
            a->setChecked(m_sortHand);
        if (a->isCheckable() && a->text() == (m_useHouse ? QStringLiteral("House")
                                                         : QStringLiteral("Classic")))
            a->setChecked(true);
        if (a->isCheckable() && a->text() == QStringLiteral("Play to %1").arg(m_target))
            a->setChecked(true);
    }
    const QString wanted = m_level == ca::Level::Easy ? QStringLiteral("Easy")
        : m_level == ca::Level::Medium                ? QStringLiteral("Medium")
        : m_level == ca::Level::Hard                  ? QStringLiteral("Hard")
                                                      : QStringLiteral("Expert");
    for (QAction* a : m_actions)
        if (a->isCheckable() && a->text() == wanted)
            a->setChecked(true);

    refresh();
    update();
    return true;
}

void CanastaView::activate()
{
    m_timer->start();
    refresh();
}

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------

bool CanastaView::animating() const
{
    return !m_flights.empty();
}

void CanastaView::tick()
{
    bool redraw = false;

    if (!m_flights.empty()) {
        for (Flight& f : m_flights) {
            if (f.delay > 0.0) {
                f.delay -= kTick;
                continue;
            }
            f.progress = std::min(1.0, f.progress + f.speed * kTick);
        }
        m_flights.erase(std::remove_if(m_flights.begin(), m_flights.end(),
                                       [](const Flight& f) {
                                           return f.delay <= 0.0 && f.progress >= 1.0;
                                       }),
                        m_flights.end());
        redraw = true;
    }

    if (m_celebrate > 0.0) {
        m_celebrate = std::max(0.0, m_celebrate - kTick);
        redraw = true;
    }

    if (!animating() && !m_awaitingContinue) {
        if (m_pause > 0.0) {
            m_pause -= kTick;
        } else {
            const ca::Engine::Phase ph = m_engine.phase();
            if (ph == ca::Engine::Phase::HandOver || ph == ca::Engine::Phase::GameOver) {
                const bool over = ph == ca::Engine::Phase::GameOver;
                if (over) {
                    const int w = m_engine.winner();
                    Sound::instance().play(w == 0 ? Sound::kWin : Sound::kLose);
                    Scores::instance().recordHigh(Scores::canastaBestScore(),
                                                  m_engine.team(0).score);
                }
                m_awaitingContinue = true;
                redraw = true;
                refresh();
            } else if (m_engine.currentSeat() != 0) {
                aiHalfTurn();
                redraw = true;
            }
        }
    }

    if (redraw)
        update();
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

QRectF CanastaView::tableRect() const
{
    const double inset = std::max(8.0, std::min(width(), height()) * 0.022);
    return QRectF(rect()).adjusted(inset, inset, -inset, -inset);
}

double CanastaView::cardWidth() const
{
    const QRectF r = tableRect();
    return std::clamp(std::min(r.width() * 0.072, r.height() * 0.10), 34.0, 88.0);
}

double CanastaView::cardHeight() const
{
    return cardWidth() * CardArt::kAspect;
}

QPointF CanastaView::stockCentre() const
{
    const QRectF r = tableRect();
    return { r.center().x() - cardWidth() * 0.80, r.top() + r.height() * 0.47 };
}

QPointF CanastaView::pileCentre() const
{
    const QRectF r = tableRect();
    return { r.center().x() + cardWidth() * 0.80, r.top() + r.height() * 0.47 };
}

// The band each side lays its melds in: yours below the middle, theirs above.
// The two bands, the centre row and the two hands have to stack without
// touching, which is what all the fractions in here are keeping apart.
QRectF CanastaView::bandFor(int team) const
{
    const QRectF r = tableRect();
    const double h = r.height() * 0.200;
    const double y = team == 0 ? r.top() + r.height() * 0.555 : r.top() + r.height() * 0.200;
    // Kept clear of the side seats' fans, which reach a card's HEIGHT in from
    // each edge — a full-width band puts the red threes underneath them.
    const double inset = std::max(r.width() * 0.05, cardHeight() * 1.15);
    return QRectF(r.left() + inset, y, r.width() - inset * 2.0, h);
}

QPointF CanastaView::handCentre(int index, int count, bool lifted) const
{
    const QRectF r = tableRect();
    const double cw = cardWidth();
    const double spread = std::min(r.width() * 0.72, count * cw * 0.66 + cw * 0.34);
    const double step = count > 1 ? (spread - cw) / (count - 1) : 0.0;
    const double x = r.center().x() - (spread - cw) * 0.5 + step * index;

    // Middle cards ride a little higher, the way a fanned hand does.
    const double t = count > 1 ? (2.0 * index / (count - 1)) - 1.0 : 0.0;
    const double rise = cardHeight() * 0.07 * (1.0 - t * t);
    const double y = r.bottom() - cardHeight() * 0.62 - rise - (lifted ? cardHeight() * 0.16 : 0.0);
    return { x, y };
}

double CanastaView::handAngle(int index, int count) const
{
    if (count <= 1)
        return 0.0;
    const double t = (2.0 * index / (count - 1)) - 1.0;
    return t * 5.0;
}

QRectF CanastaView::layDownButton() const
{
    if (m_engine.currentSeat() != 0 || m_engine.phase() != ca::Engine::Phase::Play
        || m_selected.empty() || !m_engine.meldingAllowed())
        return {};

    const QRectF r = tableRect();
    const double w = std::clamp(r.width() * 0.17, 110.0, 210.0);
    const double h = std::clamp(r.height() * 0.048, 30.0, 52.0);
    // In the gap between your melds and your hand: where you are already
    // looking once you have picked cards up.
    const double handTop = handCentre(0, 1, true).y() - cardHeight() * 0.5;
    const double y = std::min((bandFor(0).bottom() + handTop) * 0.5, handTop - h * 0.7);
    return QRectF(r.center().x() - w * 0.5, y - h * 0.5, w, h);
}

QPointF CanastaView::seatAnchor(int seat) const
{
    const QRectF r = tableRect();
    // The side seats are turned sideways, so it is the card's HEIGHT that
    // reaches towards the table edge — at less than half of it they hang off.
    const double sideInset = cardHeight() * 0.58;
    switch (seat) {
    case 0: return { r.center().x(), r.bottom() - cardHeight() * 0.5 };
    case 1: return { r.left() + sideInset, r.top() + r.height() * 0.44 };
    case 2: return { r.center().x(), r.top() + r.height() * 0.135 };
    default: return { r.right() - sideInset, r.top() + r.height() * 0.44 };
    }
}

QPointF CanastaView::opponentCentre(int seat, int index, int count) const
{
    const QRectF r = tableRect();
    const double cw = cardWidth() * 0.8;
    const QPointF anchor = seatAnchor(seat);

    if (seat == 2) {
        const double spread = std::min(r.width() * 0.42, count * cw * 0.34 + cw * 0.66);
        const double step = count > 1 ? (spread - cw) / (count - 1) : 0.0;
        return { r.center().x() - (spread - cw) * 0.5 + step * index, anchor.y() };
    }
    // Short enough that the seat's name plate still has room beside it.
    const double spread = std::min(r.height() * 0.34, count * cw * 0.34 + cw * 0.66);
    const double step = count > 1 ? (spread - cw) / (count - 1) : 0.0;
    return { anchor.x(), anchor.y() - (spread - cw) * 0.5 + step * index };
}

double CanastaView::opponentAngle(int seat, int index, int count) const
{
    const double lean = count > 1 ? ((2.0 * index / (count - 1)) - 1.0) * 4.0 : 0.0;
    if (seat == 2)
        return 180.0 + lean;
    return seat == 1 ? 90.0 + lean : 270.0 + lean;
}

std::vector<int> CanastaView::meldOrder(int team) const
{
    std::vector<int> ranks;
    for (const ca::Meld& m : m_engine.team(team).melds)
        ranks.push_back(m.rank);
    // Aces sort last so the row reads low to high, with black threes first.
    std::sort(ranks.begin(), ranks.end(), [](int a, int b) {
        const int ka = a == kAce ? 14 : a;
        const int kb = b == kAce ? 14 : b;
        return ka < kb;
    });
    return ranks;
}

QPointF CanastaView::meldCardCentre(int team, int slot, int index) const
{
    const QRectF band = bandFor(team);
    // Room is always kept for at least six melds plus the red threes, so a new
    // meld appearing never shoves the others sideways. Not named `slots`: Qt
    // defines that as a keyword macro, and it silently expands to nothing.
    const int slotCount = std::max(6, int(m_engine.team(team).melds.size()));
    const double usable = band.width() * 0.84;
    const double pitch = usable / slotCount;
    const double x = band.left() + pitch * (slot + 0.5);
    const double step = cardHeight() * kMeldScale * 0.17;
    const double y = band.top() + cardHeight() * kMeldScale * 0.5 + step * index;
    return { x, y };
}

// ---------------------------------------------------------------------------
// Hit testing
// ---------------------------------------------------------------------------

bool CanastaView::hits(const QPointF& pos, const QPointF& centre, double w, double h, double angle)
{
    QTransform t;
    t.translate(centre.x(), centre.y());
    t.rotate(angle);
    const QPointF local = t.inverted().map(pos);
    return QRectF(-w * 0.5, -h * 0.5, w, h).contains(local);
}

int CanastaView::handIndexAt(const QPointF& pos) const
{
    const int n = int(m_engine.hand(0).size());
    // Right to left, because the rightmost card is drawn on top.
    for (int i = n - 1; i >= 0; --i) {
        if (hits(pos, handCentre(i, n, isSelected(i)), cardWidth(), cardHeight(),
                 handAngle(i, n)))
            return i;
    }
    return -1;
}

int CanastaView::meldRankAt(const QPointF& pos) const
{
    const std::vector<int> ranks = meldOrder(0);
    for (std::size_t s = 0; s < ranks.size(); ++s) {
        const ca::Meld* m = m_engine.team(0).meldOfRank(ranks[s]);
        if (m == nullptr)
            continue;
        for (int i = m->size() - 1; i >= 0; --i) {
            if (hits(pos, meldCardCentre(0, int(s), i), cardWidth() * kMeldScale,
                     cardHeight() * kMeldScale, 0.0))
                return ranks[s];
        }
    }
    return -1;
}

bool CanastaView::isSelected(int index) const
{
    return std::find(m_selected.begin(), m_selected.end(), index) != m_selected.end();
}

std::vector<Card> CanastaView::selectedCards() const
{
    const std::vector<Card>& h = m_engine.hand(0);
    std::vector<Card> out;
    for (const int i : m_selected)
        if (i >= 0 && i < int(h.size()))
            out.push_back(h[std::size_t(i)]);
    return out;
}

void CanastaView::clearSelection()
{
    m_selected.clear();
}

void CanastaView::sortHand()
{
    if (!m_sortHand)
        return;

    // The selection is a list of positions, so it has to be carried across by
    // card. One position per selected card, matched once each, so a pair of
    // identical cards stays a pair rather than collapsing onto one slot.
    const std::vector<Card> picked = selectedCards();
    m_engine.sortHand(0);

    m_selected.clear();
    const std::vector<Card>& h = m_engine.hand(0);
    for (const Card& c : picked) {
        for (int i = 0; i < int(h.size()); ++i) {
            if (h[std::size_t(i)] == c && !isSelected(i)) {
                m_selected.push_back(i);
                break;
            }
        }
    }
    std::sort(m_selected.begin(), m_selected.end());
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------

void CanastaView::addFlight(Flight f)
{
    m_flights.push_back(f);
}

void CanastaView::flyHandArrivals(int seat, std::vector<Card> gained, const QPointF& from,
                                  bool faceUp, double& delay, double stagger)
{
    const std::vector<Card>& h = m_engine.hand(seat);
    const int n = int(h.size());
    for (int i = 0; i < n && !gained.empty(); ++i) {
        auto it = std::find(gained.begin(), gained.end(), h[std::size_t(i)]);
        if (it == gained.end())
            continue;
        gained.erase(it);

        Flight f;
        f.card = h[std::size_t(i)];
        f.from = from;
        f.to = seat == 0 ? handCentre(i, n, false) : opponentCentre(seat, i, n);
        f.toAngle = seat == 0 ? handAngle(i, n) : opponentAngle(seat, i, n);
        f.faceUp = faceUp;
        f.flips = faceUp;
        f.delay = delay;
        f.speed = 4.6;
        f.dest = Dest::Hand;
        f.destSeat = seat;
        addFlight(f);
        delay += stagger;
    }
}

void CanastaView::flyToPile(const Card& c, const QPointF& from)
{
    Flight f;
    f.card = c;
    f.from = from;
    f.to = pileCentre();
    f.fromAngle = 0.0;
    f.toAngle = 0.0;
    f.faceUp = true;
    f.speed = 5.0;
    f.dest = Dest::Pile;
    addFlight(f);
}

void CanastaView::flyMeldArrivals(int team, std::vector<std::pair<int, Card>> gains,
                                  const QPointF& from, double& delay)
{
    const std::vector<int> ranks = meldOrder(team);
    for (std::size_t s = 0; s < ranks.size(); ++s) {
        const ca::Meld* m = m_engine.team(team).meldOfRank(ranks[s]);
        if (m == nullptr)
            continue;
        for (int i = 0; i < m->size(); ++i) {
            const Card& c = m->cards[std::size_t(i)];
            auto it = std::find_if(gains.begin(), gains.end(), [&](const auto& g) {
                return g.first == ranks[s] && g.second == c;
            });
            if (it == gains.end())
                continue;
            gains.erase(it);

            Flight f;
            f.card = c;
            f.from = from;
            f.to = meldCardCentre(team, int(s), i);
            f.faceUp = true;
            f.speed = 4.8;
            f.delay = delay;
            f.dest = Dest::Meld;
            f.destTeam = team;
            f.destRank = ranks[s];
            addFlight(f);
            delay += 0.05;
        }
    }
}

void CanastaView::flyTheDeal()
{
    // Cards cascade out from the stock, one seat after another, which is what a
    // deal looks like. Face down for everyone but you.
    const QPointF from = stockCentre();
    double delay = 0.0;
    const int n = int(m_engine.hand(0).size());
    for (int i = 0; i < n; ++i) {
        for (int s = 0; s < ca::kSeats; ++s) {
            const std::vector<Card>& h = m_engine.hand(s);
            if (i >= int(h.size()))
                continue;
            Flight f;
            f.card = h[std::size_t(i)];
            f.from = from;
            f.to = s == 0 ? handCentre(i, int(h.size()), false)
                          : opponentCentre(s, i, int(h.size()));
            f.toAngle = s == 0 ? handAngle(i, int(h.size())) : opponentAngle(s, i, int(h.size()));
            f.faceUp = s == 0;
            f.flips = s == 0;
            f.delay = delay;
            f.speed = 7.0;
            f.dest = Dest::Hand;
            f.destSeat = s;
            addFlight(f);
            delay += 0.028;
        }
    }
    m_pause = 0.25;
}

bool CanastaView::suppressed(Dest dest, int seatOrTeam, int rank, const Card& c) const
{
    for (std::size_t i = 0; i < m_flights.size(); ++i) {
        if (i < m_consumed.size() && m_consumed[i])
            continue;
        const Flight& f = m_flights[i];
        if (f.dest != dest || !(f.card == c))
            continue;
        if (dest == Dest::Hand && f.destSeat != seatOrTeam)
            continue;
        if (dest == Dest::Meld && (f.destTeam != seatOrTeam || f.destRank != rank))
            continue;
        if (i < m_consumed.size())
            m_consumed[i] = 1;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Moves
// ---------------------------------------------------------------------------

void CanastaView::humanDraw()
{
    if (m_engine.phase() != ca::Engine::Phase::Draw || m_engine.currentSeat() != 0)
        return;

    const std::vector<Card> before = m_engine.hand(0);
    if (!m_engine.drawFromStock()) {
        announce(m_engine.lastError());
        return;
    }
    Sound::instance().play(Sound::kCardDeal);
    sortHand();
    double delay = 0.0;
    flyHandArrivals(0, gainedCards(before, m_engine.hand(0)), stockCentre(), true, delay, 0.06);
    refresh();
}

void CanastaView::humanTakePile()
{
    if (m_engine.phase() != ca::Engine::Phase::Draw || m_engine.currentSeat() != 0)
        return;

    const std::vector<Card> lay = selectedCards();
    const std::vector<Card> handBefore = m_engine.hand(0);
    const ca::Team teamBefore = m_engine.team(0);
    if (m_engine.pile().empty())
        return;

    if (!m_engine.takePile(lay)) {
        announce(m_engine.lastError());
        return;
    }
    Sound::instance().play(Sound::kCardPlace);
    clearSelection();
    sortHand();
    m_lastThrownBy = -1; // the pile it named has gone

    // Everything converges on the pile and then leaves it, which is exactly
    // what the move is.
    double delay = 0.0;
    flyMeldArrivals(0, meldGains(teamBefore, m_engine.team(0)), pileCentre(), delay);
    flyHandArrivals(0, gainedCards(handBefore, m_engine.hand(0)), pileCentre(), true, delay);
    refresh();
}

void CanastaView::humanMeld(int targetRank)
{
    if (m_engine.phase() != ca::Engine::Phase::Play || m_engine.currentSeat() != 0)
        return;
    const std::vector<Card> cards = selectedCards();
    if (cards.empty()) {
        announce(QStringLiteral("Pick the cards you want to lay down first."));
        return;
    }

    // Where the cards lift from: the middle of the fan, rather than tracking
    // each one back to its own slot, which reads as a scatter at this speed.
    const int n = int(m_engine.hand(0).size());
    const QPointF from = handCentre(n / 2, n, true);

    const ca::Team before = m_engine.team(0);
    if (!m_engine.meldCards(cards, targetRank)) {
        announce(m_engine.lastError());
        return;
    }
    Sound::instance().play(Sound::kCardPlace);
    clearSelection();

    double delay = 0.0;
    flyMeldArrivals(0, meldGains(before, m_engine.team(0)), from, delay);
    refresh();
}

void CanastaView::humanDiscard()
{
    if (m_engine.phase() != ca::Engine::Phase::Play || m_engine.currentSeat() != 0)
        return;
    if (m_selected.size() != 1) {
        announce(QStringLiteral("Pick exactly one card to throw away."));
        return;
    }

    const int index = m_selected.front();
    const std::vector<Card>& h = m_engine.hand(0);
    if (index < 0 || index >= int(h.size()))
        return;
    const Card c = h[std::size_t(index)];
    const QPointF from = handCentre(index, int(h.size()), true);

    if (!m_engine.discard(c)) {
        announce(m_engine.lastError());
        return;
    }
    Sound::instance().play(Sound::kCardPlace);
    clearSelection();
    flyToPile(c, from);
    m_lastThrown = c;
    m_lastThrownBy = 0;
    m_pause = kAiPause;
    refresh();
}

void CanastaView::aiHalfTurn()
{
    const int seat = m_engine.currentSeat();
    const ca::Engine::Phase phase = m_engine.phase();
    const int team = ca::teamOf(seat);
    const ca::Team teamBefore = m_engine.team(team);
    const std::vector<Card> handBefore = m_engine.hand(seat);
    const std::size_t pileBefore = m_engine.pile().size();

    if (phase == ca::Engine::Phase::Draw) {
        const bool took = m_ai[std::size_t(seat)].draw(m_engine);
        const QPointF source = took ? pileCentre() : stockCentre();
        if (took)
            m_lastThrownBy = -1; // the pile it named has gone
        Sound::instance().play(took ? Sound::kCardPlace : Sound::kCardDeal);

        double delay = 0.0;
        flyMeldArrivals(team, meldGains(teamBefore, m_engine.team(team)), source, delay);
        flyHandArrivals(seat, gainedCards(handBefore, m_engine.hand(seat)), source, seat == 0,
                        delay, 0.03);
        m_pause = took ? kAiPause * 1.4 : kAiPause * 0.5;
    } else {
        m_ai[std::size_t(seat)].playAndDiscard(m_engine);

        double delay = 0.0;
        flyMeldArrivals(team, meldGains(teamBefore, m_engine.team(team)), seatAnchor(seat), delay);
        if (m_engine.pile().size() > pileBefore) {
            Sound::instance().play(Sound::kCardPlace);
            flyToPile(m_engine.pile().back(), seatAnchor(seat));
            m_lastThrown = m_engine.pile().back();
            m_lastThrownBy = seat;
        }
        m_pause = kAiPause;
    }

    // A canasta on your side is worth marking.
    const int now = canastaCount(m_engine.team(0), m_engine.rules());
    if (now > m_canastasShown) {
        m_canastasShown = now;
        m_celebrate = kFlourish;
        Sound::instance().play(Sound::kBumper);
    }
    refresh();
}

void CanastaView::announce(const QString& text)
{
    m_message = text;
    Sound::instance().play(Sound::kBack);
    refresh();
    update();
}

void CanastaView::refresh()
{
    const ca::Team& us = m_engine.team(0);
    const ca::Team& them = m_engine.team(1);

    // Keep the flourish honest even when the human's own meld made the canasta.
    const int now = canastaCount(us, m_engine.rules());
    if (now > m_canastasShown) {
        m_canastasShown = now;
        m_celebrate = kFlourish;
        Sound::instance().play(Sound::kBumper);
    }

    m_meldAction->setEnabled(m_engine.phase() == ca::Engine::Phase::Play
                             && m_engine.currentSeat() == 0 && !m_selected.empty()
                             && m_engine.meldingAllowed());
    m_discardAction->setEnabled(m_engine.phase() == ca::Engine::Phase::Play
                                && m_engine.currentSeat() == 0 && m_selected.size() == 1);

    QString what;
    switch (m_engine.phase()) {
    case ca::Engine::Phase::GameOver:
        what = m_engine.winner() == 0 ? QStringLiteral("You and North win the game.")
                                      : QStringLiteral("West and East win the game.");
        break;
    case ca::Engine::Phase::HandOver:
        what = QStringLiteral("Hand over — click to deal the next one.");
        break;
    case ca::Engine::Phase::Draw:
        if (m_engine.currentSeat() != 0)
            what = QStringLiteral("%1 is drawing.").arg(seatName(m_engine.currentSeat()));
        else if (m_selected.empty())
            what = QStringLiteral("Your turn: take from the stock, or take the pile with the cards you pick.");
        else
            // Cards picked up but Meld greyed out is the one place the board
            // looks broken rather than sequenced, so say why.
            what = QStringLiteral("Your turn: click the pile to take it with those cards, or draw "
                                  "first — melding comes after the draw.");
        break;
    case ca::Engine::Phase::Play:
        if (m_engine.currentSeat() != 0)
            what = QStringLiteral("%1 is playing.").arg(seatName(m_engine.currentSeat()));
        else if (!m_engine.meldingAllowed())
            what = QStringLiteral("First round: nobody lays anything down yet — just throw a "
                                  "card away.");
        else
            what = QStringLiteral("Lay down what you want, then throw one card away.");
        break;
    }
    if (!m_message.isEmpty())
        what = m_message;

    const int need = m_engine.openRequirement(0);
    const QString opening = us.opened ? QStringLiteral("open")
                                      : QStringLiteral("need %1 to open").arg(need);

    Q_EMIT statusChanged(QStringLiteral("%1  ·  You %2  Them %3  (to %4)  ·  %5, %6  ·  stock %7%8")
                             .arg(what)
                             .arg(us.score)
                             .arg(them.score)
                             .arg(m_engine.rules().targetScore)
                             .arg(m_engine.rules().name)
                             .arg(opening)
                             .arg(m_engine.stockCount())
                             .arg(m_engine.pileFrozen() ? QStringLiteral("  ·  pile FROZEN")
                                                        : QString()));
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void CanastaView::mousePressEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();
    m_message.clear();

    if (m_awaitingContinue) {
        if (m_engine.phase() == ca::Engine::Phase::GameOver) {
            newGame();
        } else {
            m_awaitingContinue = false;
            m_engine.nextHand();
            m_canastasShown = 0;
            m_lastThrownBy = -1;
            sortHand();
            Sound::instance().play(Sound::kShuffle);
            flyTheDeal();
            refresh();
        }
        update();
        return;
    }

    if (animating() || m_engine.currentSeat() != 0) {
        update();
        return;
    }

    if (layDownButton().contains(pos)) {
        humanMeld(-1);
        update();
        return;
    }

    // A card in your hand. Whether this picks it up or starts a drag is not
    // known yet — that is settled when the button comes back up.
    const int index = handIndexAt(pos);
    if (index >= 0) {
        m_pressIndex = index;
        m_pressPos = pos;
        m_dragPos = pos;
        m_dragging = false;
        update();
        return;
    }

    // One of your melds: add the picked cards to it. This is how a wild card
    // gets onto a meld, since a wild has no rank of its own.
    const int rank = meldRankAt(pos);
    if (rank >= 0 && !m_selected.empty()) {
        humanMeld(rank);
        update();
        return;
    }

    if (hits(pos, stockCentre(), cardWidth(), cardHeight(), 0.0)) {
        humanDraw();
        update();
        return;
    }

    if (hits(pos, pileCentre(), cardWidth(), cardHeight(), 0.0)) {
        // The pile is where cards come from before you have drawn, and where
        // they go afterwards.
        if (m_engine.phase() == ca::Engine::Phase::Draw)
            humanTakePile();
        else
            humanDiscard();
        update();
        return;
    }

    update();
}

void CanastaView::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();

    // Past a few pixels a press stops being a click and becomes a drag. A card
    // that was not already picked up is dragged on its own, which is what
    // grabbing one card off a fan means.
    if (m_pressIndex >= 0 && !m_dragging
        && (pos - m_pressPos).manhattanLength() > cardWidth() * 0.14) {
        if (!isSelected(m_pressIndex)) {
            m_selected.clear();
            m_selected.push_back(m_pressIndex);
        }
        m_dragging = true;
        Sound::instance().play(Sound::kClick);
        refresh();
    }
    if (m_dragging) {
        m_dragPos = pos;
        m_hoverMeld = meldRankAt(pos);
        update();
        return;
    }

    const int wasCard = m_hover;
    const int wasMeld = m_hoverMeld;
    const bool wasButton = m_overButton;
    m_hover = m_engine.currentSeat() == 0 ? handIndexAt(pos) : -1;
    m_hoverMeld = m_selected.empty() ? -1 : meldRankAt(pos);
    m_overButton = layDownButton().contains(pos);
    if (m_hover != wasCard || m_hoverMeld != wasMeld || m_overButton != wasButton)
        update();
}

void CanastaView::mouseReleaseEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();
    const int pressed = m_pressIndex;
    const bool dragged = m_dragging;
    m_pressIndex = -1;
    m_dragging = false;

    if (pressed < 0) {
        update();
        return;
    }

    // A press that never moved is the old click: pick the card up, or put it
    // back down.
    if (!dragged) {
        const auto it = std::find(m_selected.begin(), m_selected.end(), pressed);
        if (it == m_selected.end())
            m_selected.push_back(pressed);
        else
            m_selected.erase(it);
        Sound::instance().play(Sound::kClick);
        refresh();
        update();
        return;
    }

    // Dropped on one of your melds: the cards join that meld, which is how a
    // wild card is told where it belongs.
    const int rank = meldRankAt(pos);
    if (rank >= 0) {
        humanMeld(rank);
        update();
        return;
    }
    // Dropped on the discard pile: taking it is the draw, throwing onto it
    // ends the turn.
    if (hits(pos, pileCentre(), cardWidth(), cardHeight(), 0.0)) {
        if (m_engine.phase() == ca::Engine::Phase::Draw)
            humanTakePile();
        else
            humanDiscard();
        update();
        return;
    }
    // Anywhere else on your own half of the table: lay them down.
    if (pos.y() < handCentre(0, 1, true).y() - cardHeight() * 0.5) {
        humanMeld(-1);
        update();
        return;
    }

    // Dropped back on the fan: nothing happens, and the cards stay picked up.
    update();
}

void CanastaView::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    if (m_hover != -1 || m_hoverMeld != -1) {
        m_hover = -1;
        m_hoverMeld = -1;
        update();
    }
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void CanastaView::paintCard(QPainter& p, const Card& c, const QPointF& centre, double angle,
                            bool faceUp, double scale) const
{
    const double w = cardWidth() * scale;
    const double h = cardHeight() * scale;
    p.save();
    p.translate(centre);
    if (angle != 0.0)
        p.rotate(angle);
    const QRectF r(-w * 0.5, -h * 0.5, w, h);
    if (faceUp)
        CardArt::paintFace(p, r, c);
    else
        CardArt::paintBack(p, r, c.deck);
    p.restore();
}

void CanastaView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    m_consumed.assign(m_flights.size(), 0);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    paintTable(p);
    paintMelds(p);
    paintOpponents(p);
    paintCentre(p);
    paintCentreStrip(p);
    paintHand(p);
    paintLayDown(p);
    paintFlights(p);
    paintDrag(p);
    paintScores(p);
    paintSummary(p);
}

void CanastaView::paintTable(QPainter& p)
{
    const QRectF table = tableRect();
    Theme::paintWoodFrame(p, table, std::max(6.0, table.width() * 0.014), 12.0);
    Theme::paintFelt(p, table, Theme::kFeltClaretTop, Theme::kFeltClaretBottom, 8.0);

    // A gold hairline round each side's meld band, so the two halves of the
    // table read as two partnerships rather than one crowd of cards.
    for (int team = 0; team < ca::kTeams; ++team) {
        QColor edge = Theme::kGold;
        edge.setAlpha(team == 0 ? 70 : 40);
        Theme::paintInlay(p, bandFor(team).adjusted(-4, -4, 4, 4), 8.0, edge);
    }

    // Whose turn it is.
    if (m_engine.phase() == ca::Engine::Phase::Draw
        || m_engine.phase() == ca::Engine::Phase::Play) {
        const QPointF a = seatAnchor(m_engine.currentSeat());
        QRadialGradient glow(a, cardHeight() * 1.5);
        QColor c = Theme::kGold;
        c.setAlpha(60);
        glow.setColorAt(0.0, c);
        c.setAlpha(0);
        glow.setColorAt(1.0, c);
        p.setBrush(glow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(a, cardHeight() * 1.5, cardHeight() * 1.5);
    }
}

void CanastaView::paintMelds(QPainter& p)
{
    const double scale = kMeldScale;

    for (int team = 0; team < ca::kTeams; ++team) {
        const ca::Team& t = m_engine.team(team);
        const std::vector<int> ranks = meldOrder(team);

        for (std::size_t s = 0; s < ranks.size(); ++s) {
            const ca::Meld* m = t.meldOfRank(ranks[s]);
            if (m == nullptr)
                continue;

            const bool canasta = m->isCanasta(m_engine.rules());
            // A canasta gets a ring: gold for a natural one, silver for mixed —
            // the traditional red/black pile marker, in the table's own metals.
            if (canasta) {
                const QPointF top = meldCardCentre(team, int(s), 0);
                const QPointF bottom = meldCardCentre(team, int(s), m->size() - 1);
                QRectF ring(top.x() - cardWidth() * scale * 0.62, top.y() - cardHeight() * scale * 0.62,
                            cardWidth() * scale * 1.24,
                            (bottom.y() - top.y()) + cardHeight() * scale * 1.24);
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(m->isNatural(m_engine.rules()) ? Theme::kGold
                                                             : QColor(0xd0, 0xd6, 0xdd),
                              2.2));
                p.drawRoundedRect(ring, 6, 6);
            }

            const bool lit = team == 0 && m_hoverMeld == ranks[s];
            for (int i = 0; i < m->size(); ++i) {
                const Card& c = m->cards[std::size_t(i)];
                if (suppressed(Dest::Meld, team, ranks[s], c))
                    continue;
                paintCard(p, c, meldCardCentre(team, int(s), i), 0.0, true, scale);
            }

            // At this size the shared card art draws the corner index only, so
            // a stack of sevens and a stack of eights look alike from across
            // the table. The badge is what actually names the meld.
            const QPointF last = meldCardCentre(team, int(s), m->size() - 1);
            const double bw = cardWidth() * scale * 1.20;
            const double bh = cardWidth() * scale * 0.52;
            const QRectF badge(last.x() - bw * 0.5, last.y() + cardHeight() * scale * 0.5 + 3.0,
                               bw, bh);
            QPainterPath plate;
            plate.addRoundedRect(badge, bh * 0.4, bh * 0.4);
            p.fillPath(plate, kPanel);

            QFont bf = p.font();
            bf.setPixelSize(std::max(9, int(bh * 0.72)));
            bf.setBold(true);
            p.setFont(bf);
            p.setPen(canasta ? Theme::kGold : kInkDim);
            const QString name = ranks[s] == 3 ? QStringLiteral("3♠") : rankLabel(ranks[s]);
            p.drawText(badge, Qt::AlignCenter,
                       QStringLiteral("%1 ×%2").arg(name).arg(m->size()));
            if (lit) {
                const QPointF centre = meldCardCentre(team, int(s), 0);
                const double w = cardWidth() * scale;
                const double h = cardHeight() * scale;
                CardArt::paintHighlight(p, QRectF(centre.x() - w * 0.5, centre.y() - h * 0.5, w, h),
                                        Theme::kGold);
            }
        }

        // Red threes, off to the right where they cannot be mistaken for melds.
        const QRectF band = bandFor(team);
        for (std::size_t i = 0; i < t.redThrees.size(); ++i) {
            const QPointF centre(band.right() - cardWidth() * scale * 0.7,
                                 band.top() + cardHeight() * scale * 0.5
                                     + cardHeight() * scale * 0.26 * double(i));
            paintCard(p, t.redThrees[i], centre, 0.0, true, scale);
        }
    }
}

void CanastaView::paintOpponents(QPainter& p)
{
    QFont f = p.font();
    f.setPixelSize(std::max(10, int(cardWidth() * 0.24)));
    f.setBold(true);

    for (int seat = 1; seat < ca::kSeats; ++seat) {
        const std::vector<Card>& h = m_engine.hand(seat);
        const int n = int(h.size());
        for (int i = 0; i < n; ++i) {
            if (suppressed(Dest::Hand, seat, 0, h[std::size_t(i)]))
                continue;
            paintCard(p, h[std::size_t(i)], opponentCentre(seat, i, n),
                      opponentAngle(seat, i, n), false, 0.8);
        }

        // A small plate, placed clear of the cards: below the top seat, and
        // inboard of the two side seats, where the fan cannot reach it.
        const QPointF a = seatAnchor(seat);
        const double bw = cardWidth() * 1.7;
        const double bh = cardWidth() * 0.46;
        QPointF at;
        if (seat == 2)
            // Beside the fan, not under it: under it is the opponents' meld
            // band, and the plate landed on top of their melds.
            at = { a.x() + tableRect().width() * 0.23 + bw * 0.5, a.y() };
        else if (seat == 1)
            at = { a.x() + cardHeight() * 0.60 + bw * 0.5, a.y() };
        else
            at = { a.x() - cardHeight() * 0.60 - bw * 0.5, a.y() };

        const QRectF box(at.x() - bw * 0.5, at.y() - bh * 0.5, bw, bh);
        QPainterPath plate;
        plate.addRoundedRect(box, bh * 0.35, bh * 0.35);
        p.fillPath(plate, kPanel);
        const bool active = m_engine.currentSeat() == seat;
        p.setBrush(Qt::NoBrush);
        QColor edge = Theme::kGold;
        edge.setAlpha(active ? 170 : 60);
        p.setPen(QPen(edge, 1.1));
        p.drawPath(plate);

        p.setFont(f);
        p.setPen(active ? Theme::kGold : kInkDim);
        p.drawText(box, Qt::AlignCenter, QStringLiteral("%1  %2").arg(seatName(seat)).arg(n));
    }
}

void CanastaView::paintCentre(QPainter& p)
{
    const QPointF stock = stockCentre();
    const QPointF pile = pileCentre();
    const double cw = cardWidth();
    const double ch = cardHeight();

    // Stock: a squared-up block with a few edges showing, and the two packs
    // mixed the way they are on a real table.
    const int depth = std::min(6, std::max(0, m_engine.stockCount()));
    if (depth == 0) {
        CardArt::paintSlot(p, QRectF(stock.x() - cw * 0.5, stock.y() - ch * 0.5, cw, ch));
    } else {
        for (int i = depth - 1; i >= 0; --i) {
            Card back;
            // The stock's own order is hidden from the view on purpose, so the
            // backs alternate rather than claiming to know which pack is where.
            back.deck = i % 2;
            paintCard(p, back, QPointF(stock.x() - i * 0.9, stock.y() - i * 0.9), 0.0, false);
        }
    }
    // The stock count, whether the pile is frozen and what was just thrown are
    // all one strip below the row — see paintCentreStrip.

    // Discard pile: squared up, so only the top card shows. A frozen pile keeps
    // the wild card that froze it turned sideways underneath, as at a table.
    const std::vector<Card>& cards = m_engine.pile();
    if (cards.empty()) {
        CardArt::paintSlot(p, QRectF(pile.x() - cw * 0.5, pile.y() - ch * 0.5, cw, ch));
    } else {
        const int edges = std::min(5, int(cards.size()) - 1);
        for (int i = edges; i >= 1; --i) {
            const QRectF r(pile.x() - cw * 0.5 + i * 1.1, pile.y() - ch * 0.5 + i * 1.1, cw, ch);
            QPainterPath path;
            path.addRoundedRect(r, cw * 0.075, cw * 0.075);
            p.fillPath(path, QColor(0xe8, 0xe3, 0xd8));
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0, 0, 0, 50), 1));
            p.drawPath(path);
        }
        if (m_engine.pileFrozen()) {
            const auto wild = std::find_if(cards.rbegin(), cards.rend(), ca::isWild);
            if (wild != cards.rend())
                paintCard(p, *wild, QPointF(pile.x(), pile.y() + ch * 0.06), 90.0, true);
        }
        const Card& top = cards.back();
        if (!suppressed(Dest::Pile, 0, 0, top))
            paintCard(p, top, pile, 0.0, true);
    }

    // Gold ring when the pile is yours for the taking.
    if (m_showHints && m_engine.currentSeat() == 0
        && m_engine.phase() == ca::Engine::Phase::Draw && m_engine.canTakePileAtAll()) {
        CardArt::paintHighlight(p, QRectF(pile.x() - cw * 0.54, pile.y() - ch * 0.54, cw * 1.08,
                                          ch * 1.08),
                                Theme::kGold);
    }

}

// One strip under the centre row carrying everything about it in words: how
// much stock is left, whether the pile is frozen, and what was just thrown and
// by whom. The last of those is the point — three computer seats play faster
// than a card can be picked out, so the name of it stays on screen until the
// next card replaces it.
void CanastaView::paintCentreStrip(QPainter& p)
{
    const double cw = cardWidth();
    const bool haveThrow = m_lastThrownBy >= 0 && !m_engine.pile().empty();

    struct Part {
        QString text;
        QColor colour;
    };
    std::vector<Part> parts;
    parts.push_back({ QStringLiteral("stock %1").arg(m_engine.stockCount()), kInkDim });
    if (m_engine.pileFrozen())
        parts.push_back({ QStringLiteral("FROZEN"), QColor(0x9a, 0xd8, 0xf0) });
    if (haveThrow) {
        parts.push_back({ QStringLiteral("%1 threw").arg(seatName(m_lastThrownBy)), kInkDim });
        // Suit colours as they are on the card, but lifted off the dark plate:
        // black ink on claret cannot be read at all.
        parts.push_back({ isJoker(m_lastThrown)
                              ? QStringLiteral("Joker")
                              : QStringLiteral("%1 %2").arg(rankLabel(m_lastThrown.rank),
                                                            suitSymbol(m_lastThrown.suit)),
                          isRed(m_lastThrown) ? QColor(0xff, 0x92, 0x92)
                                              : QColor(0xf4, 0xea, 0xdd) });
    }

    QFont f = p.font();
    f.setPixelSize(std::max(12, int(cw * 0.28)));
    f.setBold(true);
    const QFontMetricsF fm(f);
    const double gap = fm.horizontalAdvance(QLatin1Char('0')) * 1.6;
    const double pad = gap;

    double text = 0.0;
    for (const Part& part : parts)
        text += fm.horizontalAdvance(part.text);
    text += gap * double(parts.size() - 1);

    const double h = fm.height() * 1.45;
    const QRectF plate(tableRect().center().x() - (text + pad * 2.0) * 0.5,
                       pileCentre().y() + cardHeight() * 0.55, text + pad * 2.0, h);

    QPainterPath path;
    path.addRoundedRect(plate, h * 0.32, h * 0.32);
    p.fillPath(path, kPanel);
    p.setBrush(Qt::NoBrush);
    QColor edge = Theme::kGold;
    edge.setAlpha(haveThrow ? 130 : 60);
    p.setPen(QPen(edge, 1.2));
    p.drawPath(path);

    p.setFont(f);
    double x = plate.left() + pad;
    for (const Part& part : parts) {
        const double w = fm.horizontalAdvance(part.text);
        p.setPen(part.colour);
        p.drawText(QRectF(x, plate.top(), w, h), Qt::AlignCenter, part.text);
        x += w + gap;
    }
}

void CanastaView::paintHand(QPainter& p)
{
    const std::vector<Card>& h = m_engine.hand(0);
    const int n = int(h.size());
    std::vector<int> meldable;
    if (m_showHints)
        meldable = m_engine.meldableRanks(0);

    for (int i = 0; i < n; ++i) {
        const Card& c = h[std::size_t(i)];
        if (suppressed(Dest::Hand, 0, 0, c))
            continue;

        const bool picked = isSelected(i);
        // A card being dragged is drawn under the cursor instead, never in
        // both places at once.
        if (m_dragging && picked)
            continue;
        const QPointF centre = handCentre(i, n, picked || i == m_hover);
        const double angle = handAngle(i, n);
        paintCard(p, c, centre, angle, true);

        const bool useful = std::find(meldable.begin(), meldable.end(), c.rank) != meldable.end();
        if (!picked && !useful)
            continue;

        p.save();
        p.translate(centre);
        p.rotate(angle);
        const QRectF r(-cardWidth() * 0.5, -cardHeight() * 0.5, cardWidth(), cardHeight());
        CardArt::paintHighlight(p, r, picked ? Theme::kGold : QColor(0x8f, 0xd0, 0xa8));
        p.restore();
    }
}

// The button that lays your picked cards down. It lives on the felt rather
// than in the toolbar because that is where your hand and your eyes are.
void CanastaView::paintLayDown(QPainter& p)
{
    const QRectF r = layDownButton();
    if (r.isEmpty() || m_dragging)
        return;

    // Built from the table's own materials — the dark plate and gold edge the
    // score panels and seat labels use — rather than from a window widget.
    const bool legal = m_engine.canMeldCards(selectedCards(), -1);

    QPainterPath path;
    path.addRoundedRect(r, r.height() * 0.30, r.height() * 0.30);
    Theme::paintDropShadow(p, r, r.height() * 0.30, 5);
    p.fillPath(path, kPanel);
    p.setBrush(Qt::NoBrush);
    QColor edge = legal ? Theme::kGold : QColor(0x8a, 0x6e, 0x48);
    if (m_overButton)
        edge = edge.lighter(125);
    p.setPen(QPen(edge, m_overButton ? 3.0 : 2.0));
    p.drawPath(path);

    QFont f = p.font();
    f.setPixelSize(std::max(15, int(r.height() * 0.46)));
    f.setBold(true);
    p.setFont(f);
    p.setPen(legal ? Theme::kGold : kInkDim);
    p.drawText(r, Qt::AlignCenter,
               m_selected.size() == 1 ? QStringLiteral("Lay it down")
                                      : QStringLiteral("Lay down %1").arg(m_selected.size()));
}

// The cards under the cursor while they are being dragged, fanned the way they
// sit in your hand.
void CanastaView::paintDrag(QPainter& p)
{
    if (!m_dragging)
        return;

    const std::vector<Card> cards = selectedCards();
    const int n = int(cards.size());
    const double step = cardWidth() * 0.42;
    const double left = m_dragPos.x() - step * (n - 1) * 0.5;
    for (int i = 0; i < n; ++i)
        paintCard(p, cards[std::size_t(i)], QPointF(left + step * i, m_dragPos.y()), 0.0, true);
}

void CanastaView::paintFlights(QPainter& p)
{
    for (const Flight& f : m_flights) {
        if (f.delay > 0.0)
            continue;
        // Ease in and out, so a card leaves and lands softly.
        const double t = f.progress * f.progress * (3.0 - 2.0 * f.progress);
        const QPointF at = f.from + (f.to - f.from) * t;
        const double angle = f.fromAngle + (f.toAngle - f.fromAngle) * t;
        // A lift off the table on the way, which reads as depth.
        const double hop = std::sin(t * M_PI) * cardHeight() * 0.10;
        const bool faceUp = f.flips ? t > 0.45 : f.faceUp;
        paintCard(p, f.card, QPointF(at.x(), at.y() - hop), angle, faceUp);
    }
}

void CanastaView::paintScores(QPainter& p)
{
    const QRectF table = tableRect();
    const double w = std::max(150.0, table.width() * 0.21);
    const double h = std::max(52.0, table.height() * 0.095);

    struct Plate {
        QString title;
        int score;
        int hand;
        bool ours;
    };
    const Plate plates[2] = {
        { QStringLiteral("You & North"), m_engine.team(0).score, m_engine.team(0).handScore, true },
        { QStringLiteral("West & East"), m_engine.team(1).score, m_engine.team(1).handScore, false },
    };

    // Sized off the plate's HEIGHT in pixels, not its width in points: point
    // sizes scaled from the width overflowed the plate and clipped the title.
    QFont title = p.font();
    title.setPixelSize(std::max(10, int(h * 0.26)));
    QFont big = p.font();
    big.setPixelSize(std::max(16, int(h * 0.44)));
    big.setBold(true);

    for (int i = 0; i < 2; ++i) {
        const QRectF r(i == 0 ? table.left() + 10 : table.right() - w - 10, table.top() + 8, w, h);
        QPainterPath path;
        path.addRoundedRect(r, 8, 8);
        p.fillPath(path, kPanel);
        QColor edge = Theme::kGold;
        edge.setAlpha(plates[i].ours ? 150 : 80);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(edge, 1.2));
        p.drawPath(path);

        p.setFont(title);
        p.setPen(kInkDim);
        p.drawText(r.adjusted(10, 5, -10, 0), Qt::AlignLeft | Qt::AlignTop, plates[i].title);
        p.setFont(big);
        p.setPen(kInk);
        p.drawText(r.adjusted(10, 0, -10, -4), Qt::AlignLeft | Qt::AlignBottom,
                   QString::number(plates[i].score));

        if (m_engine.handNumber() > 1 || m_engine.phase() == ca::Engine::Phase::HandOver) {
            p.setFont(title);
            p.setPen(plates[i].hand >= 0 ? QColor(0x9f, 0xd8, 0xa8) : QColor(0xe2, 0x9b, 0x9b));
            p.drawText(r.adjusted(0, 0, -10, -4), Qt::AlignRight | Qt::AlignBottom,
                       QStringLiteral("%1%2 last hand")
                           .arg(plates[i].hand >= 0 ? QStringLiteral("+") : QString())
                           .arg(plates[i].hand));
        }
    }

    // Canasta flourish.
    if (m_celebrate > 0.0) {
        const double k = m_celebrate / kFlourish;
        QFont f = p.font();
        f.setPointSizeF(std::max(20.0, table.width() * 0.058));
        f.setBold(true);
        p.setFont(f);
        QColor c = Theme::kGold;
        c.setAlpha(int(235 * std::min(1.0, k * 1.6)));
        p.setPen(c);
        p.drawText(QRectF(table.left(), table.center().y() - table.height() * 0.10, table.width(),
                          table.height() * 0.12),
                   Qt::AlignCenter, QStringLiteral("CANASTA!"));
    }
}

void CanastaView::paintSummary(QPainter& p)
{
    if (!m_awaitingContinue)
        return;

    const QRectF table = tableRect();
    const bool over = m_engine.phase() == ca::Engine::Phase::GameOver;
    const ca::Team& us = m_engine.team(0);
    const ca::Team& them = m_engine.team(1);

    // Sized off the table, not off the panel, because the panel is then sized
    // off the text. A fixed panel clipped the second team's line.
    QFont heading = p.font();
    heading.setPixelSize(std::max(20, int(table.height() * 0.046)));
    heading.setBold(true);
    QFont body = p.font();
    body.setPixelSize(std::max(14, int(table.height() * 0.029)));

    QString title;
    if (over)
        title = m_engine.winner() == 0 ? QStringLiteral("You win!") : QStringLiteral("They win");
    else
        title = QStringLiteral("Hand %1").arg(m_engine.handNumber());

    const QString out = m_engine.wentOutSeat() >= 0
        ? QStringLiteral("%1 went out%2.")
              .arg(seatName(m_engine.wentOutSeat()))
              .arg(m_engine.wasConcealed() ? QStringLiteral(", concealed") : QString())
        : QStringLiteral("The stock ran out.");
    const QString lines = QStringLiteral("%1\n\nYou & North   %2%3     →  %4\n"
                                         "West & East   %5%6     →  %7\n\n%8")
                              .arg(out)
                              .arg(us.handScore >= 0 ? QStringLiteral("+") : QString())
                              .arg(us.handScore)
                              .arg(us.score)
                              .arg(them.handScore >= 0 ? QStringLiteral("+") : QString())
                              .arg(them.handScore)
                              .arg(them.score)
                              .arg(over ? QStringLiteral("Click for a new game.")
                                        : QStringLiteral("Click to deal the next hand."));

    // Measure first, then draw a panel that fits. The text is what decides the
    // size; the table only caps it.
    const QFontMetricsF hm(heading);
    const QFontMetricsF bm(body);
    const double pad = std::max(16.0, table.width() * 0.025);
    const QRectF measured = bm.boundingRect(QRectF(0, 0, table.width() * 0.86, table.height()),
                                            Qt::AlignHCenter | Qt::AlignTop, lines);
    const double w = std::clamp(std::max(measured.width(), hm.horizontalAdvance(title)) + pad * 2.0,
                                table.width() * 0.42, table.width() * 0.92);
    const double h = hm.height() + measured.height() + pad * 2.4;
    const QRectF panel(table.center().x() - w * 0.5, table.center().y() - h * 0.5, w, h);

    QPainterPath path;
    path.addRoundedRect(panel, 14, 14);
    Theme::paintDropShadow(p, panel, 14, 6);
    p.fillPath(path, kPanel);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Theme::kGold, 1.6));
    p.drawPath(path);

    p.setFont(heading);
    p.setPen(kInk);
    p.drawText(QRectF(panel.left(), panel.top() + pad * 0.7, panel.width(), hm.height()),
               Qt::AlignHCenter | Qt::AlignTop, title);

    p.setFont(body);
    p.setPen(kInkDim);
    p.drawText(QRectF(panel.left() + pad, panel.top() + pad * 0.7 + hm.height() + pad * 0.5,
                      panel.width() - pad * 2.0, measured.height() + pad),
               Qt::AlignHCenter | Qt::AlignTop, lines);
}
