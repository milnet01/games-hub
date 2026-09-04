#include "dealseed.h"

#include <random>

namespace {

bool g_pinned = false;
// The default seed is never consumed: this is read only once g_pinned is set,
// and pinDealSeed() is what sets it -- after seeding. Suppressed at the site
// rather than switched off in .clang-tidy, so an accidental fixed seed
// somewhere else is still reported.
// NOLINTNEXTLINE(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed)
std::mt19937 g_sequence;

} // namespace

void pinDealSeed(unsigned seed)
{
    g_pinned = true;
    g_sequence.seed(seed);
}

unsigned dealSeed()
{
    if (g_pinned)
        return g_sequence();
    return std::random_device {}();
}
