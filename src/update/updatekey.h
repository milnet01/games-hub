#pragma once

// The public half of the release-signing key (GHUB-0043 § 4.3). Empty until
// the owner makes the key: an empty key makes every install inert, so no
// request is ever made. release.yml reads the line below and fails a publish
// whose signing secret does not match it, so keep it one line in one of these
// two forms, and name the variable on no other line of this file:
//   inline constexpr std::array<unsigned char, 0> ... {};
//   inline constexpr std::array<unsigned char, 32> ... {0x1a, 0x2b, ...};

#include <array>

namespace update {

inline constexpr std::array<unsigned char, 0> kUpdatePublicKey{};

} // namespace update
