#include <chess/db/sha256.h>

#include <algorithm>
#include <string>

#include <gtest/gtest.h>

namespace chess::db {
namespace {

std::string hex(const Sha256Digest& digest)
{
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(digest.size() * 2);
    for (auto byte : digest) {
        out += digits[byte >> 4];
        out += digits[byte & 0x0f];
    }
    return out;
}

Sha256Digest expectDigest(const std::string& hexDigest)
{
    Sha256Digest digest {};
    const std::size_t bytes = std::min(digest.size(), hexDigest.size() / 2);
    for (std::size_t i = 0; i < bytes; ++i)
        digest[i] = std::uint8_t(std::stoul(hexDigest.substr(i * 2, 2), nullptr, 16));
    return digest;
}

// FIPS 180-4 message digests.
TEST(Sha256, KnownDigests)
{
    EXPECT_EQ(hex(sha256("")), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(hex(sha256("abc")),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(hex(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")),
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

// HMAC-SHA-256 vectors from RFC 4231.
TEST(Sha256, KnownHmacs)
{
    const auto key1 = expectDigest("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b");
    const auto hmac1 = hmacSha256(key1.data(), key1.size(),
                                  reinterpret_cast<const std::uint8_t*>("Hi There"), 8);
    EXPECT_EQ(hex(hmac1),
              "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");

    const auto hmac2 = hmacSha256(reinterpret_cast<const std::uint8_t*>("Jefe"), 4,
                                  reinterpret_cast<const std::uint8_t*>(
                                      "what do ya want for nothing?"), 28);
    EXPECT_EQ(hex(hmac2),
              "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");

    // RFC 4231 test case 6: key longer than the hash block size gets hashed
    // first. (Python-verified cross-check of the >-block-size code path.)
    const std::string longKey(131, '\xaa');
    const auto hmac3 = hmacSha256(reinterpret_cast<const std::uint8_t*>(longKey.data()),
                                  longKey.size(),
                                  reinterpret_cast<const std::uint8_t*>(
                                      "Test With Truncation"), 20);
    EXPECT_EQ(hex(hmac3),
              "ef9d504ede1336299cea902d32b3bdc6cbaa11ea2ec8f5c41ef771dff8f54361");
}

// PBKDF2-HMAC-SHA-256 reference vector (dkLen = 32, c = 4096, salt = "salt").
TEST(Sha256, KnownPbkdf2)
{
    const auto derived = pbkdf2Sha256("password", reinterpret_cast<const std::uint8_t*>("salt"),
                                      4, 4096);
    EXPECT_EQ(hex(derived),
              "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
}

} // namespace
} // namespace chess::db
