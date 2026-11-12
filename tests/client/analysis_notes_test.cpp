#include "analysis_notes.h"

#include <gtest/gtest.h>

#include <chess/board.h>

using namespace chess;
using namespace chess::client;
using namespace chess::client::notes;

TEST(AnalysisNotes, GameHashIsStableAndContentBound)
{
    // Same chain = same identity (the eval cache's reuse property).
    const std::vector<std::string> sans = {"e4", "e5", "Nf3", "Nc6"};
    EXPECT_EQ(gameHash(Board::fromStartPos().toFen(), sans),
              gameHash(Board::fromStartPos().toFen(), sans));

    // A different chain lands elsewhere.
    EXPECT_NE(gameHash(Board::fromStartPos().toFen(), sans),
              gameHash(Board::fromStartPos().toFen(), {"e4", "e5", "Nf3"}));

    // Hex render: 16 lowercase hex digits.
    const std::string hash = gameHash(Board::fromStartPos().toFen(), sans);
    ASSERT_EQ(hash.size(), 16u);
    for (char c : hash)
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
            << "bad hex digit " << c;
}

TEST(AnalysisNotes, EncodeDecodeRoundTrip)
{
    BoardAnnotations notes;
    notes.toggleCircle(4, 3);
    notes.toggleCircle(0, 0);
    notes.addArrow(4, 1, 4, 6);
    notes.addArrow(3, 0, 5, 2);

    BoardAnnotations decoded;
    decodeAnnotations(encodeAnnotations(notes), decoded);

    EXPECT_EQ(decoded.circles(), notes.circles());
    EXPECT_EQ(decoded.arrows(), notes.arrows());
}

TEST(AnalysisNotes, MalformedTokensAreSkippedBestEffort)
{
    // Junk tokens never throw and never produce annotations; a valid token
    // in the same line still parses.
    BoardAnnotations notes;
    decodeAnnotations("zz x9 c94 c34 a8383 a0435", notes);
    ASSERT_EQ(notes.circles().size(), 1u);
    EXPECT_EQ(notes.circles()[0], std::make_pair(3, 4));
    ASSERT_EQ(notes.arrows().size(), 1u);
    EXPECT_EQ(notes.arrows()[0].from, std::make_pair(0, 4));
    EXPECT_EQ(notes.arrows()[0].to, std::make_pair(3, 5));
}
