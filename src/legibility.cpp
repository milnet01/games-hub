#include "legibility.h"

#include <QSettings>

namespace {
// Top level, beside window/ and saved/ rather than under a game's group: this
// is the first genuinely app-wide setting in the project. Renaming it silently
// turns the switch off for a player who had turned it on, which is what the
// uitest block `legibilityPersists` is watching for.
// A function for the reason scores.h's own key helpers are: nothing is built
// at static-initialisation time, and QStringLiteral's data is static, so the
// copy this returns costs nothing.
QString settingsKey() { return QStringLiteral("display/legibility"); }
}

Legibility& Legibility::instance()
{
    static Legibility legibility;
    return legibility;
}

Legibility::Legibility()
{
    // Off by default: the app is published for strangers to download, so the
    // shipped default is the current appearance and the owner turns it on once.
    m_enabled = QSettings().value(settingsKey(), false).toBool();
}

void Legibility::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    QSettings().setValue(settingsKey(), on);
    setEnabledForSession(on);
}

void Legibility::setEnabledForSession(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    Q_EMIT changed(on);
}
