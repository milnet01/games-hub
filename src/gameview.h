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

    // The narrowest card this game draws at its current size, measured at the
    // SMALLEST scale it draws one at — Canasta's melds are at 0.74, so its
    // answer is 0.74 of a full card. 0 for a game that draws no cards.
    //
    // GHUB-0017 withdrew INV-3 ("no game draws a card too small to show its
    // pips") because cardWidth() is private on all six card views and no test
    // could reach it. This is that access, and it is one line per game rather
    // than six headers opened up.
    virtual double smallestCardWidth() const { return 0.0; }

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

    // The last text emitted through statusChanged, remembered by the base so
    // no game has to keep a second copy of its own status line.
    const QString& lastStatus() const { return m_lastStatus; }

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
