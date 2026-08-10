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
    return settings().contains(key);
}

int Scores::best(const QString& key, int fallback) const
{
    return settings().value(key, fallback).toInt();
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
    settings().clear();
    settings().sync();
}
