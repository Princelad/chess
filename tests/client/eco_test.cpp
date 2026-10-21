// Unit tests for the pure ECO opening-name lookup (11.5): the longest
// table prefix whose tokens begin the SAN history wins; check/mate
// suffixes normalize away; unknown positions return no label.
#include <gtest/gtest.h>

#include "eco.h"

#include <string>
#include <vector>

namespace chess {
namespace client {
namespace {

std::optional<Opening> lookup(const std::vector<std::string>& sans)
{
    return ecoLookup(sans);
}

std::vector<std::string> san(const std::string& line)
{
    std::vector<std::string> tokens;
    std::size_t pos = 0;
    while (pos < line.size()) {
        const std::size_t sp = line.find(' ', pos);
        if (sp == std::string::npos) {
            tokens.push_back(line.substr(pos));
            break;
        }
        tokens.push_back(line.substr(pos, sp - pos));
        pos = sp + 1;
    }
    return tokens;
}

TEST(EcoTest, FirstPlyUmbrellasResolve)
{
    const auto e4 = lookup({"e4"});
    ASSERT_TRUE(e4.has_value());
    EXPECT_EQ(e4->code, "B00");
    EXPECT_EQ(e4->name, "King's Pawn Opening");

    const auto c4 = lookup({"c4"});
    ASSERT_TRUE(c4.has_value());
    EXPECT_EQ(c4->code, "A10");

    const auto d4 = lookup({"d4"});
    ASSERT_TRUE(d4.has_value());
    EXPECT_EQ(d4->code, "A40");
}

TEST(EcoTest, LongestPrefixWins)
{
    // e4 c6 is the plain Caro-Kann umbrella; the Advance line deepens it.
    const auto plain = lookup({"e4", "c6"});
    ASSERT_TRUE(plain.has_value());
    EXPECT_EQ(plain->code, "B10");
    EXPECT_EQ(plain->name, "Caro-Kann Defence");

    const auto advance = lookup({"e4", "c6", "d4", "d5", "e5"});
    ASSERT_TRUE(advance.has_value());
    EXPECT_EQ(advance->code, "B12");
    EXPECT_EQ(advance->name, "Caro-Kann: Advance Variation");

    // Deep Najdorf refines over the "...d6" Open Sicilian umbrella.
    const auto najdorf = lookup(san(
        "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 a6"));
    ASSERT_TRUE(najdorf.has_value());
    EXPECT_EQ(najdorf->code, "B90");
    EXPECT_EQ(najdorf->name, "Najdorf Defence");
}

TEST(EcoTest, CastlingTokensMatch)
{
    const auto open = lookup(san(
        "e4 e5 Nf3 Nc6 Bb5 Nf6 O-O Nxe4"));
    ASSERT_TRUE(open.has_value());
    EXPECT_EQ(open->code, "C80");
    EXPECT_EQ(open->name, "Ruy Lopez: Open Variation");
}

TEST(EcoTest, CheckSuffixesNormalize)
{
    // The Bogo-Indian entry is spelled "Nf3 Bb4+" — games may emit the
    // check in the SAN or not; both must name the same line.
    const auto checked = lookup({"d4", "Nf6", "c4", "e6", "Nf3", "Bb4+"});
    ASSERT_TRUE(checked.has_value());
    EXPECT_EQ(checked->code, "E11");

    const auto plain = lookup({"d4", "Nf6", "c4", "e6", "Nf3", "Bb4"});
    ASSERT_TRUE(plain.has_value());
    EXPECT_EQ(plain->code, "E11");
}

TEST(EcoTest, EntryLongerThanHistoryNeedsMoreMoves)
{
    // Sans the last token, the deep Spanish-Four-Knights entry cannot
    // match; the next-shallowest prefix carries the label.
    const auto four = lookup({"e4", "e5", "Nf3", "Nc6", "Nc3", "Nf6"});
    ASSERT_TRUE(four.has_value());
    EXPECT_EQ(four->code, "C47");

    const auto spanish = lookup(
        {"e4", "e5", "Nf3", "Nc6", "Nc3", "Nf6", "Bb5"});
    ASSERT_TRUE(spanish.has_value());
    EXPECT_EQ(spanish->code, "C48");
}

TEST(EcoTest, UnknownRepliesHideTheLabel)
{
    // Unlisted first moves (junk or exotic side-pawn pushes) name nothing;
    // anything covered by a listed prefix labels the game regardless of
    // where it later wanders, per longest-prefix-wins.
    EXPECT_FALSE(lookup({}).has_value());
    EXPECT_FALSE(lookup({"h3"}).has_value());
    EXPECT_FALSE(lookup({"Na3", "Nf6"}).has_value());
    EXPECT_FALSE(lookup({"a4", "e5", "Nf3"}).has_value());
}

TEST(EcoTest, GambitBranchesRefine)
{
    const auto danish = lookup({"e4", "e5", "d4", "exd4", "c3"});
    ASSERT_TRUE(danish.has_value());
    EXPECT_EQ(danish->code, "C21");
    EXPECT_EQ(danish->name, "Danish Gambit");

    const auto bishop = lookup({"e4", "e5", "f4", "exf4", "Bc4"});
    ASSERT_TRUE(bishop.has_value());
    EXPECT_EQ(bishop->code, "C33");
    EXPECT_EQ(bishop->name, "King's Gambit: Bishop's Gambit");
}

} // namespace
} // namespace client
} // namespace chess
