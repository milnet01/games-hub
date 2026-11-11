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

    // A scoring pattern in words, as the end of a hand and the rules list it.
    static QString fanName(mahjong::Fan fan);
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
    QSize minimumSizeHint() const override { return { 900, 640 }; }

    // Protected so a test can read the table and press its buttons without a
    // copy of the geometry (the reason ReversiView gives).
    const mahjong::Game& game() const { return m_game; }
    QStringList buttonLabels() const;
    QRectF buttonRect(int index) const;
    QRectF handTileRect(int index) const;
    int pickedTile() const { return m_picked; }
    // The sentence under the centre: the last discard, or what happens next.
    QString tableSentence() const;
    // The end-of-hand panel's lines, one per row.
    QStringList endOfHandLines() const;
    mahjong::RuleSetId ruleSetInForce() const { return m_game.ruleSetId(); }

private:
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
    void apply(const mahjong::Move& move, bool loud = true);
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

    // Geometry.
    double handTileWidth() const;
    double smallTileWidth() const;
    QRectF roomRect() const;
    QRectF seatArea(int seat) const;
    QRectF centreRect() const;
    QRectF sentenceRect() const;
    QRectF turnArea(int seat) const;

    // Painting.
    void paintTile(QPainter& p, const QRectF& r, mahjong::Face face, bool raised = false) const;
    void paintBack(QPainter& p, const QRectF& r) const;
    void paintSeat(QPainter& p, int seat) const;
    void paintSets(QPainter& p, const mahjong::SeatTiles& s, QRectF row, double w) const;
    void paintPlate(QPainter& p, const QRectF& r, const QString& title, const QString& line,
                    bool lit) const;
    void paintPools(QPainter& p) const;
    void paintHand(QPainter& p) const;
    void paintButtons(QPainter& p) const;
    void paintEndOfHand(QPainter& p) const;

    QList<QAction*> m_actions;
    QActionGroup* m_ruleGroup = nullptr;
    QActionGroup* m_lengthGroup = nullptr;

    mahjong::Game m_game;
    // The settings: the next game's rule set and length. The game in progress
    // keeps the ones it began with.
    mahjong::RuleSetId m_nextRules = mahjong::RuleSetId::HongKong;
    mahjong::Length m_nextLength = mahjong::Length::FullGame;
    std::mt19937 m_rng;
    // A change to a setting, said on the table until the next game.
    QString m_note;
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
