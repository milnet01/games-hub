#pragma once

#include "gameview.h"
#include "mahjongtable.h"

#include <QTimer>

#include <functional>
#include <random>
#include <vector>

class QActionGroup;

// Four-player Mahjong against three computer players (GHUB-0239): Hong Kong
// rules, every scoring feature written out on the table, and a Rule set menu
// the later rule sets join. The rules are mahjong::Game's; this is the table.
class MahjongTableView : public GameView
{
    Q_OBJECT

public:
    explicit MahjongTableView(QWidget* parent = nullptr);

    QList<QAction*> gameActions() override { return m_actions; }
    QString rulesText() const override;
    // Tiles, not cards, said rather than inherited.
    double smallestCardWidth() const override { return 0.0; }
    void activate() override;
    // The computer's pause is a single-shot; leaving the game invalidates it.
    void deactivate() override;
    TurnLight turnLight() const override { return m_turn.value(); }
    QByteArray saveState() const override;
    bool restoreState(const QByteArray& blob) override;
    // Under the legibility switch the plate carries the table's sentence.
    QString captionText() const override;
    // For --shot: every seat played by the computer through the rules core,
    // at once and silently, carrying on into the next hand when one ends.
    bool advanceForShot(int turns) override;
    // The narrowest face-up tile on the table: the discards and your sets.
    // Large play holds it to mahjong::kTileMinWidth (GHUB-0248).
    double smallestTileWidth() const;

    // A scoring pattern in words, as the end of a hand and the rules list it,
    // and one plain sentence saying what it is, for the rules list. Both live
    // in mahjongtablewords.cpp.
    static QString fanName(mahjong::Fan fan);
    static QString fanMeaning(mahjong::Fan fan);
    // The save's bytes for a game's records. Public so a test can write a
    // save holding a move that is not legal, and see it refused.
    static QByteArray encodeSave(mahjong::RuleSetId rules, mahjong::Length length, int firstDealer,
                                 const std::vector<mahjong::HandRecord>& records);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    // Left and Right move along your hand, Return or Space throws the tile
    // picked out, and the number keys press the buttons beside your hand.
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override { return { 1200, 820 }; }
    // Sized so large play's tiles clear mahjong::kTileMinWidth.
    QSize minimumSizeHint() const override;

    // Protected so a test can read the table and press its buttons without a
    // copy of the geometry (the reason ReversiView gives).
    const mahjong::Game& game() const { return m_game; }
    QStringList buttonLabels() const;
    QRectF buttonRect(int index) const;
    // Your plate: seat wind, dealer and score. No button may cover it.
    QRectF plateRect() const;
    QRectF handTileRect(int index) const;
    int pickedTile() const { return m_picked; }
    // The whole sentence: the last discard and what happens next. It is the
    // status line, and the line under the centre with the legibility switch off.
    QString tableSentence() const;
    // What is drawn under the centre. With the switch on, the caption plate
    // carries what happens next, so the centre keeps only what just happened
    // and the two never say the same thing twice (GHUB-0246).
    QString centreSentence() const;
    // The end-of-hand panel's lines, one per row.
    QStringList endOfHandLines() const;
    // The dora, the counters and the riichi sticks in words, as the top of
    // the centre says them beside the indicators. Empty where the rule set
    // has none of them.
    QString doraSentence() const;
    // Why you may not win on a thrown tile, said beside your buttons while
    // it holds; empty otherwise (GHUB-0244 § 4.5).
    QString furitenSentence() const;
    mahjong::RuleSetId ruleSetInForce() const { return m_game.ruleSetId(); }
    // A setting changed mid-game, said on the table until the next game.
    QString pendingNote() const;

private:
    // One line of the end-of-hand panel, and how it is drawn: a winner or a
    // drawn hand in gold, a total or a payment in bold.
    struct PanelLine {
        enum Look { Title, Plain, Strong };
        QString text;
        Look look = Plain;
    };
    struct Button {
        QString label;
        std::function<void()> press;
    };

    void buildActions();
    void newGame();
    void nextHand();
    void advance();
    void playComputers();
    void playHuman(const mahjong::Move& move);
    bool apply(const mahjong::Move& move, bool loud = true);
    void playComputer(int seat);
    void abandonThinking();
    void refresh();
    void step();
    std::vector<Button> buttons() const;
    void pressButton(int index);
    bool humanToAct() const;
    void showScoreSheet();
    void setRuleSet(mahjong::RuleSetId id);
    void setLength(mahjong::Length length);

    QString seatName(int seat) const;
    QString windName(int wind) const;
    QString handSentence() const;
    QStringList happenedSentences() const;
    QString nextSentence() const;
    // The tiles of your hand you may throw now: all of them, but for a
    // riichi or a claim that bars some (GHUB-0244 § 4.4).
    bool mayThrow(int index) const;
    int lastThrowable() const;
    // A win, as the end of a Riichi hand lists it: who and on what, the ura
    // indicators, each yaku and dora with its han, then the total.
    void riichiWinLines(std::vector<PanelLine>& lines, const mahjong::WinContext& win,
                        const mahjong::HandScore& score) const;
    std::vector<PanelLine> riichiEndLines(const mahjong::HandResult& r) const;
    // Hong Kong and Chinese Official: the one winner, its patterns, the total.
    std::vector<PanelLine> classicEndLines(const mahjong::HandResult& r) const;
    // The end-of-hand panel's lines with their looks; endOfHandLines() is the
    // words alone.
    std::vector<PanelLine> panelLines() const;
    // "Right won on the Five of Bamboo from You."
    QString winnerLine(const mahjong::WinContext& win) const;
    // Each seat's net for the hand, on one line.
    QString paymentsLine(const std::array<int, mahjong::kSeats>& payments) const;
    // Under Riichi the rules text's sections between How it plays and the
    // pattern list, and the pattern list itself, by han.
    QString riichiScoringText() const;
    QString riichiYakuList() const;
    // The seats whose tiles the end-of-hand panel lays out face up: each
    // winner, or the seats ready at a drawn hand where that moves points.
    std::vector<int> shownSeats() const;

    // Geometry.
    double handTileWidth() const;
    double smallTileWidth() const;
    QRectF roomRect() const;
    QRectF seatArea(int seat) const;
    QRectF centreRect() const;
    QRectF sentenceRect() const;
    // The furiten line under the sentence while it holds; 0 otherwise.
    double furitenLineHeight() const;
    QRectF turnArea(int seat) const;
    // Your row above the hand, where the plate, your sets and the buttons sit.
    double baseRowTop() const;
    // The buttons, laid out right to left and wrapping upward a row at a time,
    // never over your plate or your sets. Positions are relative to the row's
    // top when `rows` is asked for alone.
    std::vector<QRectF> layOutButtons(int* rows) const;
    double buttonHeight() const;
    // Your sets and bonus tiles, measured in tile widths.
    double setsSpan(const mahjong::SeatTiles& s) const;
    // How narrow a set or discard may be drawn: the floor under large play.
    double minimumFaceWidth() const;
    // Whether the rule set turns dora indicators up, so the centre has a row
    // for them between the round's line and the pools.
    bool showsDora() const;
    double infoHeight() const;
    // The centre between the round's line and the sentence, before the dora
    // row is taken from its top.
    QRectF centreBody() const;
    // The row's height: the indicators at `tileWidth`, or the words at the
    // floor where they need more (`words`, from doraWordsHeight()).
    double doraRowHeight(double tileWidth, double words) const;
    double doraWordsHeight() const;
    QFont doraFont(int pixels) const;
    int doraFloorPixels() const;
    QRectF poolsRect() const;
    QFont poolLabelFont() const;
    double poolLabelWidth() const;
    double poolTileWidth() const;
    double yourSetWidth() const;

    // Painting.
    void paintTile(QPainter& p, const QRectF& r, mahjong::Face face, bool raised = false) const;
    void paintBack(QPainter& p, const QRectF& r) const;
    void paintSeat(QPainter& p, int seat) const;
    void paintSets(QPainter& p, const mahjong::SeatTiles& s, QRectF row, double w) const;
    void paintPlate(QPainter& p, const QRectF& r, const QString& title, const QString& line,
                    bool lit) const;
    void paintDora(QPainter& p, const QRectF& row, double w) const;
    void paintPools(QPainter& p) const;
    // One seat's thrown tiles in its strip; a riichi's tile lies sideways.
    void paintPool(QPainter& p, int seat, const QRectF& strip, double label) const;
    void paintHand(QPainter& p) const;
    void paintButtons(QPainter& p) const;
    void paintEndOfHand(QPainter& p) const;
    // A seat's tiles face up from `at`: the concealed ones, then its sets.
    // `extra` is a winning tile the seat's hand does not hold: a second
    // winner's, since the tile joins the first winner's hand.
    void paintShownHand(QPainter& p, int seat, mahjong::Face extra, QPointF at, double right, double w) const;
    // The panel's lines from `top`, in columns where one would not hold them.
    void paintPanelLines(QPainter& p, const std::vector<PanelLine>& lines, const QRectF& area) const;

    QList<QAction*> m_actions;
    QActionGroup* m_ruleGroup = nullptr;
    QActionGroup* m_lengthGroup = nullptr;

    mahjong::Game m_game;
    // The settings: the next game's rule set and length. The game in progress
    // keeps the ones it began with.
    mahjong::RuleSetId m_nextRules = mahjong::RuleSetId::HongKong;
    mahjong::Length m_nextLength = mahjong::Length::FullGame;
    std::mt19937 m_rng;
    bool m_resumed = false;

    // Your hand: the tile picked out to throw, as an index into it.
    int m_picked = -1;

    // The computer's pause, guarded as Noughts and Crosses' is.
    bool m_thinking = false;
    bool m_paused = false;
    quint64 m_generation = 0;

    QTimer* m_timer = nullptr;
    TurnLightState m_turn;
    bool m_turnFades = false;
};
