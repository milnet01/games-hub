#pragma once

#include <QString>

// Best scores that survive restarts, kept in the platform's normal settings
// location (QSettings). Two kinds of record, because "best" means the highest
// number in some games and the lowest in others.
class Scores
{
public:
    // Keys. Grouped by game so the settings file stays readable by hand.
    static QString reversiBest(int difficulty);
    static QString minesweeperBestTime(int level);
    static QString klondikeBestScore() { return QStringLiteral("klondike/best_score"); }
    static QString spiderBestMoves(int suits);
    static QString heartsBestScore() { return QStringLiteral("hearts/best_score"); }
    static QString canastaBestScore() { return QStringLiteral("canasta/best_score"); }
    static QString pinballBestScore() { return QStringLiteral("pinball/best_score"); }

    static Scores& instance();

    bool has(const QString& key) const;
    // Returns the stored best, or `fallback` when nothing is recorded yet.
    int best(const QString& key, int fallback = 0) const;

    // Records a result. Returns true when it beat the stored best (or when it
    // is the first result for that key), which is what the games use to say
    // "new best".
    bool recordHigh(const QString& key, int value);
    bool recordLow(const QString& key, int value);

    void clear();
};
