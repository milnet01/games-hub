#pragma once

// Ed25519 signature checking, and nothing else: no signing, no key making.
// Taken from TweetNaCl 20140427 (public domain; packaging/THIRD-PARTY.md), cut
// down to verification, with two changes said where they are made in
// ed25519.cpp: the message is hashed as it streams past rather than copied
// whole into memory, and a signature whose S is not below the group order is
// refused (RFC 8032 § 5.1.7), which TweetNaCl does not check.

#include <cstddef>
#include <cstdint>
#include <span>

namespace update {

// Feed the signed bytes in any number of pieces, then ask once.
class Ed25519Check
{
public:
    // key: 32 bytes. signature: 64 bytes. Any other size makes verified()
    // false whatever is fed.
    Ed25519Check(std::span<const unsigned char> key, std::span<const unsigned char> signature);

    void addData(const unsigned char* data, std::size_t length);
    // True only when the signature is the key's over exactly the bytes fed.
    // Call once.
    bool verified();

private:
    // SHA-512 of R || A || message, streamed.
    void hashBlocks(const unsigned char* data, std::size_t length);

    bool m_wellFormed = false;
    unsigned char m_key[32] {};
    unsigned char m_signature[64] {};
    unsigned char m_state[64] {};
    unsigned char m_pending[128] {};
    std::size_t m_pendingLength = 0;
    std::uint64_t m_total = 0;
};

} // namespace update
