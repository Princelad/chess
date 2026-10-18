#pragma once

#include <cstdint>
#include <string>

namespace chess::db {

// Password hashing in the self-describing format
//   pbkdf2-sha256$<iterations>$<salt-b64>$<hash-b64>
// Each call generates a fresh random 16-byte salt, so two identical
// passwords never produce the same stored string.
std::string hashPassword(const std::string& password);

// Recomputes the PBKDF2 digest with the stored parameters and compares it
// against the stored hash without leaking byte positions. Returns false for
// malformed stored values (wrong prefixes, unparsable iteration count,
// undecodable base64, digest of unexpected length).
bool verifyPassword(const std::string& password, const std::string& storedHash);

// Equality check without early exit on the first differing byte.
bool constantTimeEquals(const std::string& a, const std::string& b);

// Shared name rules for accounts and guests: non-empty, at most 32
// printable (ASCII 0x20..0x7e) characters — mirrors server.cpp JOIN checks.
bool validAccountName(const std::string& name);

// Password rules: 1..64 printable characters.
bool validPassword(const std::string& password);

} // namespace chess::db
