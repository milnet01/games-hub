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

Q_SIGNALS:
    // Text for the hub's status bar.
    void statusChanged(const QString& text);
};
