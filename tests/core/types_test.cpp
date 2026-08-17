#include <chess/types.h>

#include <gtest/gtest.h>

namespace chess {

TEST(Types, ColorOpposite)
{
    EXPECT_EQ(opposite(Color::White), Color::Black);
    EXPECT_EQ(opposite(Color::Black), Color::White);
    EXPECT_EQ(opposite(Color::None), Color::None);
    EXPECT_EQ(opposite(opposite(Color::White)), Color::White);
}

TEST(Types, PieceCombinesColorAndType)
{
    Piece wp = Piece::of(Color::White, PieceType::Pawn);
    EXPECT_EQ(wp.color, Color::White);
    EXPECT_EQ(wp.type, PieceType::Pawn);
    EXPECT_TRUE(Piece::None().isNone());
    EXPECT_FALSE(wp.isNone());
}

TEST(Types, SquareLayout)
{
    EXPECT_EQ(squareOf(File::A, Rank::R1), 0);
    EXPECT_EQ(squareOf(File::B, Rank::R1), 1);
    EXPECT_EQ(squareOf(File::H, Rank::R1), 7);
    EXPECT_EQ(squareOf(File::A, Rank::R2), 16);
    EXPECT_EQ(squareOf(File::E, Rank::R4), 0x34);
    EXPECT_EQ(squareOf(File::H, Rank::R8), 119);
}

TEST(Types, SquareFileRankRoundTrip)
{
    for (int file = 0; file < 8; ++file) {
        for (int rank = 0; rank < 8; ++rank) {
            Square sq = squareOf(file, rank);
            EXPECT_EQ(static_cast<int>(fileOf(sq)), file);
            EXPECT_EQ(static_cast<int>(rankOf(sq)), rank);
            EXPECT_EQ(squareOf(fileOf(sq), rankOf(sq)), sq);
        }
    }
}
} // namespace chess
