#pragma once

#include <QObject>

// The hub's one accessibility switch: large, high-contrast play, on or off for
// the whole collection.
//
// It is a singleton for the same reason Sound is — every game needs to reach it
// and none owns it — but unlike Sound it is persisted and it broadcasts. Games
// are built lazily and live for the session, so a game constructed before the
// switch moved would never learn about it without the signal; a game
// constructed afterwards reads enabled() itself and needs no notification.
class Legibility : public QObject
{
    Q_OBJECT

public:
    static Legibility& instance();

    // True when the player has asked for large, high-contrast play.
    bool enabled() const { return m_enabled; }
    // Stores the new value and emits changed(). A no-op if unchanged, so a
    // caller cannot count on an emission it did not actually cause.
    void setEnabled(bool on);

Q_SIGNALS:
    void changed(bool enabled);

private:
    Legibility();

    bool m_enabled = false;
};
