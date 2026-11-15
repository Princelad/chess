#include "screens/ui_helpers.h"

#include <gtest/gtest.h>

namespace chess::client {
namespace {

// The audit fix: no unconditional extra trim — a clean ASCII cut keeps ALL
// budget bytes, and a multibyte cut lands on a whole-sequence boundary.
TEST(SafeTruncate, ShortStringUnchanged)
{
    EXPECT_EQ(safeTruncate("abc", 10), "abc");
    EXPECT_EQ(safeTruncate("abc", 3), "abc");
}

TEST(SafeTruncate, AsciiCutKeepsAllBudgetBytes)
{
    // 6 bytes, budget 4: the old form returned "abc..." (one byte short).
    EXPECT_EQ(safeTruncate("abcdef", 4), "abcd...");
}

TEST(SafeTruncate, MultibyteCutStaysOnSequenceBoundary)
{
    // Three "é" (2 bytes each). Budget 4 cuts before the third sequence.
    const std::string three = "\xC3\xA9\xC3\xA9\xC3\xA9";
    EXPECT_EQ(safeTruncate(three, 4), "\xC3\xA9\xC3\xA9...");

    // Budget 3 backs off to keep exactly one whole é.
    EXPECT_EQ(safeTruncate(three, 3), "\xC3\xA9...");

    // A budget landing inside a sequence excludes that sequence whole.
    const std::string aE = "a\xC3\xA9"; // 'a' + é
    EXPECT_EQ(safeTruncate(aE, 2), "a...");
    // Exactly the full byte count is returned unchanged (no ellipsis).
    EXPECT_EQ(safeTruncate(aE, 3), "a\xC3\xA9");
}

} // namespace
} // namespace chess::client
