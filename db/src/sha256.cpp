#include <chess/db/sha256.h>

#include <cstring>
#include <vector>

namespace chess::db {
namespace {

constexpr std::uint32_t RoundConstants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

constexpr std::uint32_t rotateRight(std::uint32_t value, unsigned bits)
{
    return (value >> bits) | (value << (32 - bits));
}

std::uint32_t loadBigEndian32(const std::uint8_t* bytes)
{
    return (std::uint32_t(bytes[0]) << 24) | (std::uint32_t(bytes[1]) << 16)
        | (std::uint32_t(bytes[2]) << 8) | std::uint32_t(bytes[3]);
}

void storeBigEndian32(std::uint8_t* bytes, std::uint32_t value)
{
    bytes[0] = std::uint8_t(value >> 24);
    bytes[1] = std::uint8_t(value >> 16);
    bytes[2] = std::uint8_t(value >> 8);
    bytes[3] = std::uint8_t(value);
}

// Standard SHA-256 streaming implementation: 64-byte blocks, one transform
// per block, final padding of 0x80 / zeros / 64-bit message length.
struct Hasher {
    static constexpr std::size_t BlockSize = 64;

    std::uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };
    std::uint8_t block[BlockSize] = {};
    std::size_t buffered = 0;
    std::uint64_t totalBytes = 0;

    void transform(const std::uint8_t* chunk)
    {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = loadBigEndian32(chunk + 4 * i);
        for (int i = 16; i < 64; ++i) {
            std::uint32_t s0 = rotateRight(w[i - 15], 7) ^ rotateRight(w[i - 15], 18)
                ^ (w[i - 15] >> 3);
            std::uint32_t s1 = rotateRight(w[i - 2], 17) ^ rotateRight(w[i - 2], 19)
                ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        std::uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

        for (int i = 0; i < 64; ++i) {
            std::uint32_t s1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
            std::uint32_t ch = (e & f) ^ (~e & g);
            std::uint32_t temp1 = h + s1 + ch + RoundConstants[i] + w[i];
            std::uint32_t s0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
            std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            std::uint32_t temp2 = s0 + maj;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    void update(const std::uint8_t* data, std::size_t length)
    {
        totalBytes += length;
        while (length > 0) {
            std::size_t take = BlockSize - buffered;
            if (take > length)
                take = length;
            std::memcpy(block + buffered, data, take);
            buffered += take;
            data += take;
            length -= take;
            if (buffered == BlockSize) {
                transform(block);
                buffered = 0;
            }
        }
    }

    Sha256Digest finish()
    {
        std::uint64_t bits = totalBytes * 8;
        std::uint8_t one = 0x80;
        std::uint8_t zero = 0x00;
        update(&one, 1);
        while (buffered != 56)
            update(&zero, 1);

        std::uint8_t lengthBytes[8];
        storeBigEndian32(lengthBytes, std::uint32_t(bits >> 32));
        storeBigEndian32(lengthBytes + 4, std::uint32_t(bits));
        update(lengthBytes, 8);

        Sha256Digest digest;
        for (int i = 0; i < 8; ++i)
            storeBigEndian32(digest.data() + 4 * i, state[i]);
        return digest;
    }
};

} // namespace

Sha256Digest sha256(const std::uint8_t* data, std::size_t length)
{
    Hasher hasher;
    hasher.update(data, length);
    return hasher.finish();
}

Sha256Digest hmacSha256(const std::uint8_t* key, std::size_t keyLength,
                        const std::uint8_t* message, std::size_t messageLength)
{
    std::uint8_t keyBlock[Hasher::BlockSize] = {};
    if (keyLength > Hasher::BlockSize) {
        auto hashed = sha256(key, keyLength);
        std::memcpy(keyBlock, hashed.data(), hashed.size());
    } else {
        std::memcpy(keyBlock, key, keyLength);
    }

    std::uint8_t pad[Hasher::BlockSize];

    Hasher inner;
    for (std::size_t i = 0; i < Hasher::BlockSize; ++i)
        pad[i] = std::uint8_t(keyBlock[i] ^ 0x36);
    inner.update(pad, Hasher::BlockSize);
    inner.update(message, messageLength);
    Sha256Digest innerDigest = inner.finish();

    Hasher outer;
    for (std::size_t i = 0; i < Hasher::BlockSize; ++i)
        pad[i] = std::uint8_t(keyBlock[i] ^ 0x5c);
    outer.update(pad, Hasher::BlockSize);
    outer.update(innerDigest.data(), innerDigest.size());
    return outer.finish();
}

Sha256Digest pbkdf2Sha256(const std::string& password, const std::uint8_t* salt,
                          std::size_t saltLength, int iterations)
{
    // dkLen (32) never exceeds the HMAC output here, so PBKDF2's outer loop
    // (RFC 8018, §5.2) runs a single time. U_i chains via the previous U,
    // while F(P, S, c) accumulates the XOR over all U_1..U_c.
    std::vector<std::uint8_t> message(saltLength + 4);
    std::memcpy(message.data(), salt, saltLength);
    message[saltLength + 0] = 0;
    message[saltLength + 1] = 0;
    message[saltLength + 2] = 0;
    message[saltLength + 3] = 1; // block index INT(1), big-endian

    Sha256Digest chain = hmacSha256(
        reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
        message.data(), message.size());
    Sha256Digest accumulator = chain;

    for (int i = 1; i < iterations; ++i) {
        Sha256Digest current = hmacSha256(
            reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
            chain.data(), chain.size());
        for (std::size_t b = 0; b < chain.size(); ++b)
            chain[b] = current[b];
        for (std::size_t b = 0; b < accumulator.size(); ++b)
            accumulator[b] ^= current[b];
    }
    return accumulator;
}

} // namespace chess::db
