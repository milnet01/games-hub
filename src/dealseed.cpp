#include "dealseed.h"

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

} // namespace

void pinDealSeed(unsigned seed)
{
    g_pinned = true;
    sequence().seed(seed);
}

unsigned dealSeed()
{
    if (g_pinned)
        return sequence()();
    return std::random_device {}();
}
