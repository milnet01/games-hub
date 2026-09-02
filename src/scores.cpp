#include "scores.h"

#include <QSettings>

namespace {
// One settings object for the process. QSettings resolves its file from the
// application and organisation names set in main(), so the test binary — which
// uses a different application name — never touches real scores.
QSettings& settings()
{
    static QSettings store;
    return store;
}
}

Scores& Scores::instance()
{
    static Scores scores;
    return scores;
}

QString Scores::reversiBest(int difficulty)
{
    static const char* names[] = { "easy", "medium", "hard" };
    return QStringLiteral("reversi/best_discs_%1")
        .arg(QString::fromUtf8(names[qBound(0, difficulty, 2)]));
}

QString Scores::minesweeperBestTime(int level)
{
    static const char* names[] = { "beginner", "intermediate", "expert" };
    return QStringLiteral("minesweeper/best_time_%1")
        .arg(QString::fromUtf8(names[qBound(0, level, 2)]));
}

QString Scores::spiderBestMoves(int suits)
{
    return QStringLiteral("spider/best_moves_%1_suit").arg(suits);
}

bool Scores::has(const QString& key) const
{
    // A key holding something that is not a number counts as no record at all.
    // QVariant::toInt() answers 0 for one, and for the recordLow games — times
    // and move counts, where smaller is better — a best of 0 is a score nobody
    // can beat, so every future result would be refused. Reading it as absent
    // means the next result simply replaces it.
    if (!settings().contains(key))
        return false;
    bool ok = false;
    settings().value(key).toInt(&ok);
    return ok;
}

int Scores::best(const QString& key, int fallback) const
{
    bool ok = false;
    const int value = settings().value(key, fallback).toInt(&ok);
    return ok ? value : fallback;
}

bool Scores::recordHigh(const QString& key, int value)
{
    if (has(key) && value <= best(key))
        return false;
    settings().setValue(key, value);
    settings().sync();
    return true;
}

bool Scores::recordLow(const QString& key, int value)
{
    if (has(key) && value >= best(key))
        return false;
    settings().setValue(key, value);
    settings().sync();
    return true;
}

void Scores::clear()
{
    // Scores only. QSettings::clear() on this scope takes the whole store with
    // it — the legibility switch, the mute, the donate counter, every saved
    // game and every remembered window size — none of which this class owns.
    // Nothing calls it in the app today, but GHUB-0068 anticipates a
    // reset-everything button, and this is the name someone reaches for.
    //
    // Matched by shape rather than by a list, because the keys are declared
    // partly here and partly as constants inside the views: a list here would
    // be a second copy, and the game whose key it missed would keep its record
    // through a reset with nothing to say so.
    QSettings& s = settings();
    const QStringList keys = s.allKeys();
    for (const QString& key : keys) {
        const QString leaf = key.section(QLatin1Char('/'), -1);
        if (leaf.startsWith(QLatin1String("best_")) || leaf == QLatin1String("wins"))
            s.remove(key);
    }
    s.sync();
}
