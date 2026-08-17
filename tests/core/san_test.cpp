#include <chess/board.h>
#include <chess/fen.h>
#include <chess/move.h>
#include <chess/movegen.h>
#include <chess/san.h>

#include <gtest/gtest.h>

namespace chess {
namespace {

TEST(SAN, SimplePawnPush)
{
    auto board = Board::fromStartPos();
    Move m = doublePushMove(squareOf(File::E, Rank::R2), squareOf(File::E, Rank::R4));
    EXPECT_EQ(san::toSan(board, m), "e4");
}

TEST(SAN, SinglePawnPush)
{
    auto board = Board::fromStartPos();
    Move m = move(squareOf(File::D, Rank::R2), squareOf(File::D, Rank::R3));
    EXPECT_EQ(san::toSan(board, m), "d3");
}

TEST(SAN, PawnCapture)
{
    auto board = *Board::fromFen("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");
    Move m = captureMove(squareOf(File::E, Rank::R4), squareOf(File::D, Rank::R5));
    EXPECT_EQ(san::toSan(board, m), "exd5");
}

TEST(SAN, KnightMove)
{
    auto board = Board::fromStartPos();
    Move m = move(squareOf(File::G, Rank::R1), squareOf(File::F, Rank::R3));
    EXPECT_EQ(san::toSan(board, m), "Nf3");
}

TEST(SAN, KnightCapture)
{
    auto board = *Board::fromFen("rnbqkbnr/pppp1ppp/8/4p3/4N3/8/PPPPPPPP/RNBQKB1R w KQkq - 0 2");
    Move m = captureMove(squareOf(File::E, Rank::R4), squareOf(File::D, Rank::R6));
    EXPECT_EQ(san::toSan(board, m), "Nxd6+");
}

TEST(SAN, DisambiguationByFile)
{
    auto board = *Board::fromFen("rnbqkbnr/pppppppp/8/8/8/2N1N3/PPPPPPPP/R1BQKB1R w KQkq - 0 1");
    Move m = move(squareOf(File::C, Rank::R3), squareOf(File::D, Rank::R5));
    EXPECT_EQ(san::toSan(board, m), "Ncd5");
}

TEST(SAN, DisambiguationByRank)
{
    auto board = *Board::fromFen("4k3/8/8/8/8/8/R7/R3K3 w - - 0 1");
    Move m = move(squareOf(File::A, Rank::R1), squareOf(File::A, Rank::R3));
    EXPECT_EQ(san::toSan(board, m), "R1a3");
}

TEST(SAN, CastlingKingSide)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    Move m = castleMove(squareOf(File::E, Rank::R1), squareOf(File::G, Rank::R1));
    EXPECT_EQ(san::toSan(board, m), "O-O");
}

TEST(SAN, CastlingQueenSide)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    Move m = castleMove(squareOf(File::E, Rank::R1), squareOf(File::C, Rank::R1));
    EXPECT_EQ(san::toSan(board, m), "O-O-O");
}

TEST(SAN, Promotion)
{
    auto board = *Board::fromFen("8/4P3/8/8/8/8/8/4K2k w - - 0 1");
    Move m = promotionMove(squareOf(File::E, Rank::R7), squareOf(File::E, Rank::R8), PieceType::Queen);
    EXPECT_EQ(san::toSan(board, m), "e8=Q");
}

TEST(SAN, PromotionCapture)
{
    auto board = *Board::fromFen("3r3k/4P3/8/8/8/8/8/4K3 w - - 0 1");
    Move m{squareOf(File::E, Rank::R7), squareOf(File::D, Rank::R8), Promotion | Capture};
    m.promotion = PieceType::Knight;
    EXPECT_EQ(san::toSan(board, m), "exd8=N");
}

TEST(SAN, Check)
{
    // White bishop on c4, black king on f8. Bc4 attacks f7 — move Bf7+ is check
    auto board = *Board::fromFen("rnbqk2r/pppp1ppp/5n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4");
    Move m = captureMove(squareOf(File::C, Rank::R4), squareOf(File::F, Rank::R7));
    EXPECT_EQ(san::toSan(board, m), "Bxf7+");
}

TEST(SAN, Checkmate)
{
    // Ra8# — back-rank mate
    auto board = *Board::fromFen("6k1/5ppp/8/8/8/8/8/R3K2R w KQ - 0 1");
    Move m = move(squareOf(File::A, Rank::R1), squareOf(File::A, Rank::R8));
    EXPECT_EQ(san::toSan(board, m), "Ra8#");
}

} // namespace
} // namespace chess
