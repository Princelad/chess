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

// Audit 5.3: the history detail panel draws its SAN record as one
// sf::Text with no scroll — a 450-ply chain is ~17,000 px on a single
// line. wrapSanChain breaks on spaces and caps the line count.
TEST(WrapSanChain, ShortTextUnchanged)
{
    EXPECT_EQ(wrapSanChain("e4 e5", 76, 12), "e4 e5");
    EXPECT_EQ(wrapSanChain("", 76, 12), "");
}

TEST(WrapSanChain, BreaksOnSpacesNeverMidToken)
{
    // Tokens of 4 bytes; width 10 fits three tokens + separators per line.
    const std::string chain = "aaaa bbbb cccc dddd";
    EXPECT_EQ(wrapSanChain(chain, 10, 12), "aaaa bbbb\ncccc dddd");
}

TEST(WrapSanChain, CapsLineCountWithTruncationNote)
{
    // 5 tokens, width fits one per line, maxLines 2: third token onward
    // is dropped for the note.
    const std::string chain = "e4 e5 Nf6 Nc6 Bc4";
    EXPECT_EQ(wrapSanChain(chain, 3, 2),
              "e4\ne5\n... long game - use Copy for the full record");
}

TEST(WrapSanChain, OversizeTokenOwnsItsLine)
{
    // A single token wider than the budget must not be split mid-token:
    // it lands on its own line, and the following token starts a fresh
    // line (the oversize token left lineLen > wrapChars behind).
    EXPECT_EQ(wrapSanChain("x eeeeeeeeeeeeeeeeeeee y", 10, 12),
              "x\neeeeeeeeeeeeeeeeeeee\ny");
}

// Audit 5.4: static_cast<unsigned short>(stoi(s)) silently wrapped
// out-of-range text ("99999" dialed 44639, "-1" dialed 65535). parsePort
// is digits-only over 1..65535.
TEST(ParsePort, AcceptsValidRange)
{
    EXPECT_EQ(parsePort("1"), std::optional<unsigned short>(1));
    EXPECT_EQ(parsePort("80"), std::optional<unsigned short>(80));
    EXPECT_EQ(parsePort("65535"), std::optional<unsigned short>(65535));
}

TEST(ParsePort, RejectsJunkAndOutOfRange)
{
    EXPECT_EQ(parsePort(""), std::nullopt);
    EXPECT_EQ(parsePort("0"), std::nullopt);
    EXPECT_EQ(parsePort("65536"), std::nullopt);
    EXPECT_EQ(parsePort("99999"), std::nullopt); // used to wrap to 44639
    EXPECT_EQ(parsePort("-1"), std::nullopt);    // used to wrap to 65535
    EXPECT_EQ(parsePort("80x"), std::nullopt);
    EXPECT_EQ(parsePort(" 80"), std::nullopt);
    EXPECT_EQ(parsePort("123456"), std::nullopt); // 6 digits: > 5 chars
}

} // namespace
} // namespace chess::client
