#include <chess/fen.h>
#include <chess/movegen.h>

#include <algorithm>

#include <gtest/gtest.h>

namespace chess {

namespace {

bool containsMove(const std::vector<Move>& moves, Square from, Square to, int flags = Quiet)
{
    return std::any_of(moves.begin(), moves.end(), [&](const Move& m) {
        return m.from == from && m.to == to && m.flags == flags;
    });
}

bool containsPromotion(const std::vector<Move>& moves, Square from, Square to, PieceType promo)
{
    return std::any_of(moves.begin(), moves.end(), [&](const Move& m) {
        return m.from == from && m.to == to && m.isPromotion() && m.promotion == promo;
    });
}

} // namespace

// --- Knight tests ---

TEST(MoveGen, KnightFromStartB1)
{
    auto board = Board::fromStartPos();
    auto moves = generateMoves(board);
    Square b1 = squareOf(File::B, Rank::R1);
    Square a3 = squareOf(File::A, Rank::R3);
    Square c3 = squareOf(File::C, Rank::R3);
    EXPECT_TRUE(containsMove(moves, b1, a3));
    EXPECT_TRUE(containsMove(moves, b1, c3));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == b1; }), 2);
}

TEST(MoveGen, KnightFromStartG1)
{
    auto board = Board::fromStartPos();
    auto moves = generateMoves(board);
    Square g1 = squareOf(File::G, Rank::R1);
    Square f3 = squareOf(File::F, Rank::R3);
    Square h3 = squareOf(File::H, Rank::R3);
    EXPECT_TRUE(containsMove(moves, g1, f3));
    EXPECT_TRUE(containsMove(moves, g1, h3));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == g1; }), 2);
}

TEST(MoveGen, KnightFromStartB8)
{
    auto board = fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square b8 = squareOf(File::B, Rank::R8);
    Square a6 = squareOf(File::A, Rank::R6);
    Square c6 = squareOf(File::C, Rank::R6);
    EXPECT_TRUE(containsMove(moves, b8, a6));
    EXPECT_TRUE(containsMove(moves, b8, c6));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == b8; }), 2);
}

TEST(MoveGen, KnightFromStartG8)
{
    auto board = fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square g8 = squareOf(File::G, Rank::R8);
    Square f6 = squareOf(File::F, Rank::R6);
    Square h6 = squareOf(File::H, Rank::R6);
    EXPECT_TRUE(containsMove(moves, g8, f6));
    EXPECT_TRUE(containsMove(moves, g8, h6));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == g8; }), 2);
}

TEST(MoveGen, KnightInCenterHasEightMoves)
{
    auto board = fromFen("8/8/8/4N3/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e5 = squareOf(File::E, Rank::R5);
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e5; }), 8);
}

TEST(MoveGen, KnightInCornerHasTwoMoves)
{
    auto board = fromFen("N7/8/8/8/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square a8 = squareOf(File::A, Rank::R8);
    Square b6 = squareOf(File::B, Rank::R6);
    Square c7 = squareOf(File::C, Rank::R7);
    EXPECT_TRUE(containsMove(moves, a8, b6));
    EXPECT_TRUE(containsMove(moves, a8, c7));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == a8; }), 2);
}

TEST(MoveGen, KnightCaptureAndOwnPieceBlocking)
{
    auto board = fromFen("8/3p4/8/4N3/2PP4/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e5 = squareOf(File::E, Rank::R5);
    Square d7 = squareOf(File::D, Rank::R7);
    Square f7 = squareOf(File::F, Rank::R7);
    Square g6 = squareOf(File::G, Rank::R6);
    Square c6 = squareOf(File::C, Rank::R6);
    Square g4 = squareOf(File::G, Rank::R4);
    Square f3 = squareOf(File::F, Rank::R3);
    Square d3 = squareOf(File::D, Rank::R3);
    Square c4 = squareOf(File::C, Rank::R4);
    EXPECT_TRUE(containsMove(moves, e5, d7, Capture));
    EXPECT_TRUE(containsMove(moves, e5, f7));
    EXPECT_TRUE(containsMove(moves, e5, g6));
    EXPECT_TRUE(containsMove(moves, e5, c6));
    EXPECT_TRUE(containsMove(moves, e5, g4));
    EXPECT_TRUE(containsMove(moves, e5, f3));
    EXPECT_TRUE(containsMove(moves, e5, d3));
    EXPECT_FALSE(containsMove(moves, e5, c4));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e5; }), 7);
}

// --- King tests ---

TEST(MoveGen, KingOpenCenterHasEightMoves)
{
    auto board = fromFen("8/8/8/4k3/8/8/8/8 b - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e5 = squareOf(File::E, Rank::R5);
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e5; }), 8);
}

TEST(MoveGen, KingInCornerHasThreeMoves)
{
    auto board = fromFen("k7/8/8/8/8/8/8/8 b - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square a8 = squareOf(File::A, Rank::R8);
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == a8; }), 3);
}

TEST(MoveGen, KingNoCastlingYet)
{
    auto board = Board::fromStartPos();
    auto moves = generateMoves(board);
    Square e1 = squareOf(File::E, Rank::R1);
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e1; }), 0);
}

TEST(MoveGen, KingCaptureAndBlock)
{
    auto board = fromFen("8/8/8/3pKp2/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e5 = squareOf(File::E, Rank::R5);
    Square d5 = squareOf(File::D, Rank::R5);
    Square f5 = squareOf(File::F, Rank::R5);
    Square d6 = squareOf(File::D, Rank::R6);
    Square e6 = squareOf(File::E, Rank::R6);
    Square f6 = squareOf(File::F, Rank::R6);
    Square d4 = squareOf(File::D, Rank::R4);
    Square e4 = squareOf(File::E, Rank::R4);
    Square f4 = squareOf(File::F, Rank::R4);
    EXPECT_TRUE(containsMove(moves, e5, d5, Capture));
    EXPECT_TRUE(containsMove(moves, e5, f5, Capture));
    EXPECT_TRUE(containsMove(moves, e5, d6));
    EXPECT_TRUE(containsMove(moves, e5, e6));
    EXPECT_TRUE(containsMove(moves, e5, f6));
    EXPECT_TRUE(containsMove(moves, e5, d4));
    EXPECT_TRUE(containsMove(moves, e5, e4));
    EXPECT_TRUE(containsMove(moves, e5, f4));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e5; }), 8);
}

// --- Pawn tests ---

TEST(MoveGen, WhitePawnStartHasSixteenMoves)
{
    auto board = Board::fromStartPos();
    auto moves = generateMoves(board);
    int pawnMoves = 0;
    for (const auto& m : moves) {
        Piece p = board.pieceAt(m.from);
        if (p.type == PieceType::Pawn) ++pawnMoves;
    }
    EXPECT_EQ(pawnMoves, 16);
}

TEST(MoveGen, BlackPawnStartHasSixteenMoves)
{
    auto board = fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    int pawnMoves = 0;
    for (const auto& m : moves) {
        Piece p = board->pieceAt(m.from);
        if (p.type == PieceType::Pawn) ++pawnMoves;
    }
    EXPECT_EQ(pawnMoves, 16);
}

TEST(MoveGen, WhitePawnSinglePushBlockedByEnemy)
{
    auto board = fromFen("8/8/8/8/4p3/4P3/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e3 = squareOf(File::E, Rank::R3);
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e3; }), 0);
}

TEST(MoveGen, WhitePawnCapture)
{
    auto board = fromFen("8/8/8/8/3p4/4P3/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e3 = squareOf(File::E, Rank::R3);
    Square e4 = squareOf(File::E, Rank::R4);
    Square d4 = squareOf(File::D, Rank::R4);
    EXPECT_TRUE(containsMove(moves, e3, e4));
    EXPECT_TRUE(containsMove(moves, e3, d4, Capture));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e3; }), 2);
}

TEST(MoveGen, WhitePawnPromotion)
{
    auto board = fromFen("8/4P3/8/8/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e7 = squareOf(File::E, Rank::R7);
    Square e8 = squareOf(File::E, Rank::R8);
    EXPECT_TRUE(containsPromotion(moves, e7, e8, PieceType::Knight));
    EXPECT_TRUE(containsPromotion(moves, e7, e8, PieceType::Bishop));
    EXPECT_TRUE(containsPromotion(moves, e7, e8, PieceType::Rook));
    EXPECT_TRUE(containsPromotion(moves, e7, e8, PieceType::Queen));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e7; }), 4);
}

TEST(MoveGen, WhitePawnCapturePromotion)
{
    auto board = fromFen("6n1/7P/8/8/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square h7 = squareOf(File::H, Rank::R7);
    Square g8 = squareOf(File::G, Rank::R8);
    EXPECT_TRUE(containsPromotion(moves, h7, g8, PieceType::Knight));
    EXPECT_TRUE(containsPromotion(moves, h7, g8, PieceType::Bishop));
    EXPECT_TRUE(containsPromotion(moves, h7, g8, PieceType::Rook));
    EXPECT_TRUE(containsPromotion(moves, h7, g8, PieceType::Queen));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == h7; }), 8);
}

TEST(MoveGen, WhitePawnEnPassant)
{
    auto board = fromFen("8/8/8/3pP3/8/8/8/8 w - d6 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e5 = squareOf(File::E, Rank::R5);
    Square d6 = squareOf(File::D, Rank::R6);
    EXPECT_TRUE(containsMove(moves, e5, d6, EnPassant | Capture));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e5; }), 2);
}

TEST(MoveGen, BlackPawnEnPassant)
{
    auto board = fromFen("8/8/8/8/3Pp3/8/8/8 b - f3 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e4 = squareOf(File::E, Rank::R4);
    Square f3 = squareOf(File::F, Rank::R3);
    EXPECT_TRUE(containsMove(moves, e4, f3, EnPassant | Capture));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e4; }), 2);
}

TEST(MoveGen, WhitePawnDoublePushOnlyFromStart)
{
    auto board = fromFen("8/8/8/8/8/4P3/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e3 = squareOf(File::E, Rank::R3);
    Square e4 = squareOf(File::E, Rank::R4);
    EXPECT_TRUE(containsMove(moves, e3, e4));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) {
        return m.from == e3 && m.isDoublePush();
    }), 0);
}

TEST(MoveGen, WhitePawnDoublePushFromStart)
{
    auto board = fromFen("8/8/8/8/8/8/4P3/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e2 = squareOf(File::E, Rank::R2);
    Square e3 = squareOf(File::E, Rank::R3);
    Square e4 = squareOf(File::E, Rank::R4);
    EXPECT_TRUE(containsMove(moves, e2, e3));
    EXPECT_TRUE(containsMove(moves, e2, e4, DoublePush));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e2; }), 2);
}

// --- Full board count tests ---

TEST(MoveGen, StartPositionWhiteHasTwentyMoves)
{
    auto board = Board::fromStartPos();
    auto moves = generateMoves(board);
    EXPECT_EQ(moves.size(), 20u);
}

TEST(MoveGen, StartPositionBlackHasTwentyMoves)
{
    auto board = fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    EXPECT_EQ(moves.size(), 20u);
}

// --- Filter tests ---

TEST(MoveGen, CapturesOnlyFilter)
{
    auto board = fromFen("8/8/8/3pP3/8/8/8/8 w - d6 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board, MoveFilter::CapturesOnly);
    Square e5 = squareOf(File::E, Rank::R5);
    Square d6 = squareOf(File::D, Rank::R6);
    EXPECT_EQ(moves.size(), 1u);
    EXPECT_TRUE(containsMove(moves, e5, d6, EnPassant | Capture));
}

TEST(MoveGen, CapturesOnlyOnEmptyBoard)
{
    auto board = fromFen("8/8/8/4N3/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board, MoveFilter::CapturesOnly);
    EXPECT_EQ(moves.size(), 0u);
}

} // namespace chess
