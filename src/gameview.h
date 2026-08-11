#pragma once

#include <QAction>
#include <QList>
#include <QString>
#include <QWidget>

// Every game in the hub is a GameView. The hub supplies the window, the
// toolbar and the status line; a game supplies its board and the handful of
// actions that belong to it (New Game, difficulty, and so on).
class GameView : public QWidget
{
    Q_OBJECT

public:
    using QWidget::QWidget;

    // Actions the hub puts on the toolbar while this game is on screen.
    virtual QList<QAction*> gameActions() { return {}; }

    // Called when the game becomes visible in the hub. Games that need a fresh
    // deal on entry override this.
    virtual void activate() { }

    // A game long enough to be worth coming back to writes its whole position
    // here and takes it back in restoreState(). The hub keeps it, so a game is
    // where you left it next time you open it — there is no save dialog and
    // nothing to remember to press. An empty QByteArray means "nothing worth
    // keeping", which is also the answer for a game that has just finished.
    virtual QByteArray saveState() const { return {}; }
    // Returns false if the state is from an older build or is corrupt, in which
    // case the game keeps the fresh one it already dealt.
    virtual bool restoreState(const QByteArray&) { return false; }

Q_SIGNALS:
    // Text for the hub's status bar.
    void statusChanged(const QString& text);
};
