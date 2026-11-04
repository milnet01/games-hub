#include "dealseed.h"

#include <mutex>
#include <random>

namespace {

bool g_pinned = false;

// Function-local rather than file-scope, so nothing is constructed at
// static-initialisation time where a throw could not be caught.
//
// The default seed is never consumed: this is read only once g_pinned is set,
// and pinDealSeed() is what sets it -- after seeding. Suppressed at the site
// rather than switched off in .clang-tidy, so an accidental fixed seed
// somewhere else is still reported.
std::mt19937& sequence()
{
    // NOLINTNEXTLINE(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed)
    static std::mt19937 rng;
    return rng;
}

// Pinned, the sequence is shared state, and the board games' computer players
// draw from it on their worker threads (GHUB-0227) -- so every touch of it
// takes this lock.
std::mutex& sequenceLock()
{
    static std::mutex lock;
    return lock;
}

} // namespace

void pinDealSeed(unsigned seed)
{
    const std::lock_guard<std::mutex> hold(sequenceLock());
    g_pinned = true;
    sequence().seed(seed);
}

void unpinDealSeed()
{
    const std::lock_guard<std::mutex> hold(sequenceLock());
    g_pinned = false;
}

unsigned dealSeed()
{
    const std::lock_guard<std::mutex> hold(sequenceLock());
    if (g_pinned)
        return sequence()();
    return std::random_device {}();
}
