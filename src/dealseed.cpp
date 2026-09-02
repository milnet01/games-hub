#include "dealseed.h"

#include <random>

namespace {

bool g_pinned = false;
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
