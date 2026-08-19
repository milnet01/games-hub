#include "donate.h"

#include <QSettings>

namespace {

constexpr auto kCountKey = "donate/launches";
constexpr auto kAskKey = "donate/ask";

} // namespace

namespace donate {

int recordLaunch()
{
    // A temporary QSettings flushes in its destructor, so the new count is on
    // disk before this function returns. That is what makes the counter honest
    // for a process that is killed rather than closed.
    QSettings settings;
    const int next = settings.value(QLatin1String(kCountKey), 0).toInt() + 1;
    settings.setValue(QLatin1String(kCountKey), next);
    return next;
}

bool asksEnabled()
{
    return QSettings().value(QLatin1String(kAskKey), true).toBool();
}

void setAsksEnabled(bool on)
{
    QSettings().setValue(QLatin1String(kAskKey), on);
}

bool recordLaunchAndAsk()
{
    const int launchNumber = recordLaunch();
    return asksEnabled() && launchOwesPrompt(launchNumber);
}

} // namespace donate
