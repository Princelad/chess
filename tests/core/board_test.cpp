#include <chess/board.h>

#include <gtest/gtest.h>

namespace chess {

TEST(Board, OffBoardDetectsInvalidSquares)
{
    EXPECT_FALSE(offBoard(0));
    EXPECT_FALSE(offBoard(squareOf(File::E, Rank::R4)));
    EXPECT_FALSE(offBoard(119));
    EXPECT_TRUE(offBoard(8));
    EXPECT_TRUE(offBoard(0x80));
    EXPECT_TRUE(offBoard(0x88));
    EXPECT_TRUE(offBoard(128));
    EXPECT_TRUE(offBoard(-1));
}

TEST(Board, DefaultBoardIsEmpty)
{
    Board board;
    for (int sq = 0; sq < 128; ++sq) {
        EXPECT_TRUE(board.isEmpty(sq));
        EXPECT_TRUE(board.pieceAt(sq).isNone());
    }
}

TEST(Board, SetPieceAndClear)
{
    Board board;
    const Piece rook = Piece::of(Color::Black, PieceType::Rook);
    const Square sq = squareOf(File::A, Rank::R1);
    board.setPiece(sq, rook);
    EXPECT_EQ(board.pieceAt(sq), rook);
    EXPECT_FALSE(board.isEmpty(sq));
    board.clearSquare(sq);
    EXPECT_TRUE(board.isEmpty(sq));
}

TEST(Board, AlgebraicRoundTrip)
{
    for (int file = 0; file < 8; ++file) {
        for (int rank = 0; rank < 8; ++rank) {
            const Square sq = squareOf(file, rank);
            EXPECT_EQ(stringToSquare(squareToString(sq)), sq);
        }
    }
}

TEST(Board, SquareToString)
{
    EXPECT_EQ(squareToString(squareOf(File::A, Rank::R1)), "a1");
    EXPECT_EQ(squareToString(squareOf(File::E, Rank::R4)), "e4");
    EXPECT_EQ(squareToString(squareOf(File::H, Rank::R8)), "h8");
}

TEST(Board, StringToSquare)
{
    EXPECT_EQ(stringToSquare("a1"), squareOf(File::A, Rank::R1));
    EXPECT_EQ(stringToSquare("e4"), squareOf(File::E, Rank::R4));
    EXPECT_EQ(stringToSquare("h8"), squareOf(File::H, Rank::R8));
    EXPECT_EQ(stringToSquare(""), SquareNone);
    EXPECT_EQ(stringToSquare("e"), SquareNone);
    EXPECT_EQ(stringToSquare("e42"), SquareNone);
    EXPECT_EQ(stringToSquare("i4"), SquareNone);
    EXPECT_EQ(stringToSquare("a0"), SquareNone);
    EXPECT_EQ(stringToSquare("e9"), SquareNone);
}

} // namespace chess
