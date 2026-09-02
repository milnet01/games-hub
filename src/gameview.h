#pragma once

#include <QAction>
#include <QFont>
#include <QList>
#include <QString>
#include <QWidget>

class QPainter;

// Every game in the hub is a GameView. The hub supplies the window, the
// toolbar and the status line; a game supplies its board and the handful of
// actions that belong to it (New Game, difficulty, and so on).
class GameView : public QWidget
{
    Q_OBJECT

public:
    // Not `using QWidget::QWidget`: the base constructor is what connects every
    // game to the hub's legibility switch, including the ones that are built
    // but not on screen.
    explicit GameView(QWidget* parent = nullptr);

    // Actions the hub puts on the toolbar while this game is on screen.
    virtual QList<QAction*> gameActions() { return {}; }

    // Called when the hub's legibility switch changes, and never at
    // construction: a game reads Legibility::instance().enabled() itself when
    // it builds. The default repaints, which is enough for a game that reads
    // the setting inside paintEvent; a game that caches geometry overrides it.
    // Play the game forward by `turns` of its OWN turns, for --shot, and
    // return false where the game has no notion of one. Anything a card game
    // only shows once a hand has been played -- a frozen pack, a finished
    // canasta and the badge that names it -- does not exist at the deal, so a
    // picture taken the moment a game opens cannot reach it (GHUB-0166).
    // Getting one used to take a throwaway harness of five source edits.
    //
    // Synchronous, unanimated and silent by contract: the shot is taken the
    // moment this returns, so a card still in the air would be photographed
    // half-way to somewhere. A game that overrides this settles itself.
    //
    // Refusing is a real answer rather than a failure. --shot refuses a turn
    // count aimed at a game that cannot play itself, for the same reason it
    // refuses an unknown game: a picture of the wrong thing still gets written
    // and still looks like an answer.
    virtual bool advanceForShot(int turns)
    {
        Q_UNUSED(turns);
        return false;
    }

    virtual void applyLegibility(bool enabled)
    {
        Q_UNUSED(enabled);
        updateGeometry();
        update();
    }

    // Called when the game becomes visible in the hub. Games that need a fresh
    // deal on entry override this.
    virtual void activate() { }
    // Called when the hub leaves this game for another page. A game that runs a
    // clock or an animation stops it here: a background game that keeps ticking
    // costs the player a Minesweeper time they never spent, and its status
    // messages land in the status bar of whatever game IS on screen.
    virtual void deactivate() { }

    // A game long enough to be worth coming back to writes its whole position
    // here and takes it back in restoreState(). The hub keeps it, so a game is
    // where you left it next time you open it — there is no save dialog and
    // nothing to remember to press. An empty QByteArray means "nothing worth
    // keeping", which is also the answer for a game that has just finished.
    virtual QByteArray saveState() const { return {}; }
    // Returns false if the state is from an older build or is corrupt, in which
    // case the game keeps the fresh one it already dealt.
    virtual bool restoreState(const QByteArray&) { return false; }

    // True while this game holds animation state that a settings change would
    // CONSUME rather than pause. Canasta's cards in flight carry a destination
    // captured when they left, so applyLegibility lands them — correctly, and
    // irreversibly. A game whose animation can simply be stopped and picked up
    // where it was answers false, which is why Pinball does not override it: a
    // frozen ball is exactly where it was.
    //
    // It exists because deactivate() FREEZES a game without SETTLING it, and
    // those are different things. A board frozen mid-deal is static — it will
    // pass any stillness probe — while still holding state the next settings
    // change will eat.
    //
    // Its consumer is the TEST HARNESS, by design, and nothing in the app
    // consults it — said here because a review has already read that absence as
    // dead machinery once (GHUB-0137). The every-game legibility block cannot
    // compare two renders of a game that is still dealing, and it cannot ask
    // the pixels either: a staggered deal has lulls where every remaining card
    // is only counting down its delay, so two matching frames mean nothing.
    // This is the one honest answer to "has it finished yet".
    //
    // The app deliberately does NOT wait on it. The one place that would
    // consult it is applyLegibility on the visible page, and Canasta already
    // clears its flights there on purpose: a card in the air carries a
    // destination captured when it left, so it would otherwise land where that
    // destination used to be. Deferring the switch until the deal settled would
    // leave it looking unresponsive for a second, which is a worse trade for
    // the reader the switch is for.
    virtual bool hasPendingAnimation() const { return false; }

    // The narrowest card this game draws at its current size, measured at the
    // SMALLEST scale it draws one at — Canasta's melds are at 0.74, so its
    // answer is 0.74 of a full card. 0 for a game that draws no cards.
    //
    // GHUB-0017 withdrew INV-3 ("no game draws a card too small to show its
    // pips") because cardWidth() is private on all six card views and no test
    // could reach it. This is that access, and it is one line per game rather
    // than six headers opened up.
    //
    // -1 is "nobody answered", and 0 is "this game draws no cards" -- said by
    // the game, not inherited. They used to be the same value, so a card game
    // that forgot to override this was skipped by cardsKeepTheirFaces in
    // silence and shipped drawing faces too small to read. Every game answers
    // it now, and the check fails on a -1 rather than passing over it.
    virtual double smallestCardWidth() const { return -1.0; }

    // What this game says ON its own play surface while the legibility switch
    // is on. The hub has a status bar and it is not read during play, so a
    // game that only speaks there says nothing to the player who most needs
    // to hear it — every game therefore repeats its status line on the board.
    //
    // The default is whatever this game last emitted through statusChanged,
    // which for most of them is already the sentence that matters. A game
    // overrides it where the surface needs something the status bar does not
    // carry, or carries too wordily: Hearts names the suit that was led.
    virtual QString captionText() const { return m_lastStatus; }

    // The raw last line, without a game's own override of captionText(). Used
    // by the tests to read what a game actually emitted.
    const QString& lastStatus() const { return m_lastStatus; }

    // The last text emitted through statusChanged, remembered by the base so
    // no game has to keep a second copy of its own status line.

    // Draws captionText() on a plate inside `area`, and does nothing at all
    // while the legibility switch is off — so a game's pass is these three
    // lines at the end of its paintEvent rather than twelve copies of the
    // same block. The font is sized from `area`, because a sentence that
    // stays put while the board grows is the thing being fixed.
    void paintStatusCaption(QPainter& p, const QRectF& area,
                            Qt::Alignment where = Qt::AlignBottom) const;
    // The font paintStatusCaption() would use. Exposed so a test can ask how
    // big the sentence is without rendering one.
    QFont captionFont(const QRectF& area) const;

    // The height to keep clear at the bottom of `area` for the caption, and 0
    // while the switch is off. A game whose board must not be covered shrinks
    // its board by exactly this and nothing is hidden.
    //
    // It is a FIXED two lines rather than the height of the current sentence:
    // a band that tracked the text would resize the board every time the text
    // changed length, and a board that jumps between moves is worse than a
    // slightly smaller one.
    double captionBand(const QRectF& area) const;

Q_SIGNALS:
    // Text for the hub's status bar.
    void statusChanged(const QString& text);

private:
    QString m_lastStatus;
};
