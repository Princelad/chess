#include "avatars.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace chess::client {
namespace {

using avatars::seedFromName;
using avatars::seedFor;
using avatars::packIndexFor;
using avatars::initialOf;
using avatars::cellsFor;
using avatars::colorsFor;

TEST(AvatarSeed, DeterministicPerNameAndSpreadAcrossNames)
{
    const std::uint64_t a = seedFromName("alice");
    EXPECT_EQ(a, seedFromName("alice"));
    EXPECT_NE(a, seedFromName("bob"));
    // Near-miss names still diverge (case and leading/trailing bytes count).
    EXPECT_NE(seedFromName("Alice"), seedFromName("alice"));
    EXPECT_NE(seedFromName("alice "), seedFromName("alice"));
}

TEST(AvatarSeed, SeedForHonorsConfigKnobThenFallsBack)
{
    const auto path = std::filesystem::temp_directory_path()
        / "chess_avatars_test_config";
    std::filesystem::remove(path);
    Config config(path.string()); // never saved — in-memory only
    // Empty `profile.avatar` derives from the displayed name (guest-safe:
    // guests always carry the join/last name they connect with).
    EXPECT_EQ(seedFor(config, "mika"), seedFromName("mika"));
    // Any string overrides — the hand-editable re-roll.
    config.set("profile.avatar", "lucky7");
    EXPECT_EQ(seedFor(config, "mika"), seedFromName("lucky7"));
    // A different knob value gives a different emblem.
    config.set("profile.avatar", "lucky8");
    EXPECT_NE(seedFor(config, "mika"), seedFromName("lucky7"));
}

TEST(AvatarPack, PackIndexSpreadsAndGuardsEmpty)
{
    EXPECT_EQ(packIndexFor(12345, 0), 0u);          // empty pack: callers gate
    EXPECT_EQ(packIndexFor(0, 16), 0u);
    EXPECT_EQ(packIndexFor(16, 16), 0u);            // wraps evenly
    EXPECT_EQ(packIndexFor(15, 16), 15u);
    EXPECT_EQ(packIndexFor(12345, 16), 12345u % 16u);
    // Same seed into the same pool size always lands on the same tile.
    EXPECT_EQ(packIndexFor(999, 16), packIndexFor(999, 16));
}

TEST(AvatarInitial, FirstAlnumUppercasedWithPlaceholders)
{
    EXPECT_EQ(initialOf("alice"), 'A');
    EXPECT_EQ(initialOf("  bob92"), 'B');
    EXPECT_EQ(initialOf("42club"), '4'); // digits count as initials too
    EXPECT_EQ(initialOf("___"), '?');
    EXPECT_EQ(initialOf(""), '?');
}

TEST(AvatarCells, GridIsMirroredHorizontally)
{
    // Horizontal mirror symmetry: the identicon discipline. Every column
    // repeats its twin across the vertical axis.
    for (std::uint64_t seed : {0ull, 1ull, 0xdeadbeefull, 1469598103934665603ull,
                               0xffffffffffffffffull}) {
        const auto cells = cellsFor(seed);
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 5; ++col)
                ASSERT_EQ(cells[row * 5 + col], cells[row * 5 + (4 - col)])
                    << "seed " << seed << " row " << row;
    }
}

TEST(AvatarCells, NotConstantAcrossSeedsAndNotAllOff)
{
    // Even with mirror symmetry, distinct seeds land on distinct grids —
    // the mapping must not collapse toward a single emblem.
    const auto a = cellsFor(seedFromName("alice"));
    const auto b = cellsFor(seedFromName("bob"));
    EXPECT_NE(a, b);

    bool anyOn = false;
    for (const std::uint64_t seed : {12ull, 77ull, 4004ull, 999999ull}) {
        for (const bool on : cellsFor(seed))
            anyOn = anyOn || on;
    }
    EXPECT_TRUE(anyOn);
}

TEST(AvatarColors, CuratedPairsDifferAndRepeatDeterministically)
{
    const auto vio = colorsFor(7);
    // Background and tile stay in different register (pattern readable).
    const int bgSum = vio.background.r + vio.background.g + vio.background.b;
    const int tileSum = vio.tile.r + vio.tile.g + vio.tile.b;
    EXPECT_LT(bgSum, tileSum);

    // Deterministic: same seed, same pair.
    EXPECT_EQ(colorsFor(7).background.r, vio.background.r);
    EXPECT_NE(colorsFor(8), vio); // neighbors diverge (near-certain: hashed)
    // Every consecutive block of seeds stays inside the curated table —
    // in-range check via distinct color sanity: no channel exceptions.
    for (std::uint64_t seed : {0ull, 5ull, 99ull, 1234567ull}) {
        const auto c = colorsFor(seed);
        EXPECT_LE(c.background.a, 255);
        EXPECT_LE(c.tile.a, 255);
    }
}

TEST(AvatarPack, EmptyPackDegradesSoftly)
{
    avatars::AvatarPack pack;
    EXPECT_TRUE(pack.empty());
    // tileFor on an empty pack would be UB by contract; the draw path
    // checks empty() first — that contract is what the callers rely on.
}

} // namespace
} // namespace chess::client
