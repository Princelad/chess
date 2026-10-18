#include <chess/db/auth.h>

#include <cstdint>
#include <random>
#include <vector>

#include <chess/db/sha256.h>

namespace chess::db {
namespace {

// One round costs ~4 SHA-256 blocks, so a login stays comfortably under a
// tenth of a second while poisoning bulk offline guessing.
constexpr int Pbkdf2Iterations = 100000;

constexpr std::size_t SaltLength = 16;

std::string base64Encode(const std::uint8_t* data, std::size_t length)
{
    static const char* alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((length + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 3 <= length; i += 3) {
        std::uint32_t triple = (std::uint32_t(data[i]) << 16)
            | (std::uint32_t(data[i + 1]) << 8) | std::uint32_t(data[i + 2]);
        out += alphabet[(triple >> 18) & 63];
        out += alphabet[(triple >> 12) & 63];
        out += alphabet[(triple >> 6) & 63];
        out += alphabet[triple & 63];
    }
    if (i < length) {
        // One or two trailing bytes; pad with '=' to a multiple of four.
        std::uint32_t pair = std::uint32_t(data[i]) << 16;
        if (i + 1 < length)
            pair |= std::uint32_t(data[i + 1]) << 8;
        out += alphabet[(pair >> 18) & 63];
        out += alphabet[(pair >> 12) & 63];
        out += (i + 1 < length) ? alphabet[(pair >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

bool base64Decode(const std::string& text, std::vector<std::uint8_t>& out)
{
    static signed char decode[256] = {};
    static bool tableReady = false;
    if (!tableReady) {
        static const char* alphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (auto& value : decode)
            value = -1;
        for (int i = 0; alphabet[i] != '\0'; ++i)
            decode[std::uint8_t(alphabet[i])] = static_cast<signed char>(i);
        tableReady = true;
    }

    out.clear();
    std::uint32_t buffer = 0;
    int bits = 0;
    for (char c : text) {
        if (c == '=' || c == '\n' || c == '\r')
            continue;
        auto value = decode[std::uint8_t(c)];
        if (value < 0)
            return false;
        buffer = (buffer << 6) | std::uint32_t(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(std::uint8_t((buffer >> bits) & 0xff));
        }
    }
    return bits < 6 && (bits == 0 || (buffer & ((1u << bits) - 1)) == 0);
}

// "pbkdf2-sha256$<iter>$<salt-b64>$<hash-b64>" → true with out-parts filled.
bool splitStoredHash(const std::string& stored, int& iterations,
                     std::vector<std::uint8_t>& salt, std::vector<std::uint8_t>& hash)
{
    const std::string prefix = "pbkdf2-sha256$";
    if (stored.rfind(prefix, 0) != 0)
        return false;

    std::vector<std::string> parts;
    std::size_t start = prefix.size();
    while (true) {
        std::size_t next = stored.find('$', start);
        parts.push_back(stored.substr(start, (next == std::string::npos
            ? stored.size() : next) - start));
        if (next == std::string::npos)
            break;
        start = next + 1;
    }
    // Expects exactly [iterations, salt, hash].
    if (parts.size() != 3 || parts[0].empty())
        return false;

    iterations = 0;
    for (char c : parts[0]) {
        if (c < '0' || c > '9')
            return false;
        iterations = iterations * 10 + (c - '0');
        if (iterations > 10000000)
            return false;
    }
    if (iterations < 1)
        return false;
    return base64Decode(parts[1], salt) && base64Decode(parts[2], hash)
        && hash.size() == 32 && !salt.empty();
}

} // namespace

std::string hashPassword(const std::string& password)
{
    std::uint8_t salt[SaltLength];
    std::uniform_int_distribution<int> byte(0, 255);
    std::random_device random;
    for (auto& value : salt)
        value = std::uint8_t(byte(random));

    auto digest = pbkdf2Sha256(password, salt, SaltLength, Pbkdf2Iterations);
    return "pbkdf2-sha256$" + std::to_string(Pbkdf2Iterations) + "$"
        + base64Encode(salt, SaltLength) + "$"
        + base64Encode(digest.data(), digest.size());
}

bool verifyPassword(const std::string& password, const std::string& storedHash)
{
    int iterations = 0;
    std::vector<std::uint8_t> salt;
    std::vector<std::uint8_t> hash;
    if (!splitStoredHash(storedHash, iterations, salt, hash))
        return false;

    auto digest = pbkdf2Sha256(password, salt.data(), salt.size(), iterations);
    std::string computed(reinterpret_cast<const char*>(digest.data()), digest.size());
    std::string stored(reinterpret_cast<const char*>(hash.data()), hash.size());
    return constantTimeEquals(computed, stored);
}

bool constantTimeEquals(const std::string& a, const std::string& b)
{
    // Length is not a secret for fixed-size digests.
    if (a.size() != b.size())
        return false;
    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        diff |= std::uint8_t(a[i] ^ b[i]);
    return diff == 0;
}

bool validAccountName(const std::string& name)
{
    if (name.empty() || name.size() > 32)
        return false;
    for (auto c : name) {
        if (c < 0x20 || c > 0x7e)
            return false;
    }
    return true;
}

bool validPassword(const std::string& password)
{
    if (password.empty() || password.size() > 64)
        return false;
    for (auto c : password) {
        if (c < 0x20 || c > 0x7e)
            return false;
    }
    return true;
}

} // namespace chess::db
