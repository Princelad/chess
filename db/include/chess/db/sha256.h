#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace chess::db {

using Sha256Digest = std::array<std::uint8_t, 32>;

// SHA-256 (FIPS 180-4).
Sha256Digest sha256(const std::uint8_t* data, std::size_t length);
inline Sha256Digest sha256(const std::string& text)
{
    return sha256(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}

// HMAC-SHA-256 (RFC 2104 / FIPS 198-1).
Sha256Digest hmacSha256(const std::uint8_t* key, std::size_t keyLength,
                        const std::uint8_t* message, std::size_t messageLength);

// PBKDF2-HMAC-SHA-256 (RFC 8018) producing one 32-byte key. Iterations must
// match the count used when the hash was created.
Sha256Digest pbkdf2Sha256(const std::string& password, const std::uint8_t* salt,
                          std::size_t saltLength, int iterations);

} // namespace chess::db
