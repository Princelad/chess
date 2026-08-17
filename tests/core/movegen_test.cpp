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

TEST(MoveGen, KingCastlingBothSides)
{
    auto board = fromFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e1 = squareOf(File::E, Rank::R1);
    EXPECT_TRUE(containsMove(moves, e1, squareOf(File::G, Rank::R1), Castle));
    EXPECT_TRUE(containsMove(moves, e1, squareOf(File::C, Rank::R1), Castle));
}

TEST(MoveGen, BlackCastlingBothSides)
{
    auto board = fromFen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e8 = squareOf(File::E, Rank::R8);
    EXPECT_TRUE(containsMove(moves, e8, squareOf(File::G, Rank::R8), Castle));
    EXPECT_TRUE(containsMove(moves, e8, squareOf(File::C, Rank::R8), Castle));
}

TEST(MoveGen, CastlingKingSideOnly)
{
    auto board = fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w Kk - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e1 = squareOf(File::E, Rank::R1);
    EXPECT_TRUE(containsMove(moves, e1, squareOf(File::G, Rank::R1), Castle));
    EXPECT_FALSE(containsMove(moves, e1, squareOf(File::C, Rank::R1), Castle));
}

TEST(MoveGen, CastlingQueenSideOnly)
{
    auto board = fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w Qq - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e1 = squareOf(File::E, Rank::R1);
    EXPECT_FALSE(containsMove(moves, e1, squareOf(File::G, Rank::R1), Castle));
    EXPECT_TRUE(containsMove(moves, e1, squareOf(File::C, Rank::R1), Castle));
}

TEST(MoveGen, NoCastlingRights)
{
    auto board = fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e1 = squareOf(File::E, Rank::R1);
    EXPECT_FALSE(containsMove(moves, e1, squareOf(File::G, Rank::R1), Castle));
    EXPECT_FALSE(containsMove(moves, e1, squareOf(File::C, Rank::R1), Castle));
}

TEST(MoveGen, CastlingBlockedByPiece)
{
    auto board = fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/RN2K2R w KQkq - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e1 = squareOf(File::E, Rank::R1);
    EXPECT_TRUE(containsMove(moves, e1, squareOf(File::G, Rank::R1), Castle));
    EXPECT_FALSE(containsMove(moves, e1, squareOf(File::C, Rank::R1), Castle));
}

TEST(MoveGen, CastlingCapturesOnlyEmpty)
{
    auto board = Board::fromStartPos();
    auto moves = generateMoves(board, MoveFilter::CapturesOnly);
    for (const auto& m : moves) {
        EXPECT_FALSE(m.isCastle());
    }
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

// --- Rook tests ---

TEST(MoveGen, RookOpenFileAndRank)
{
    auto board = fromFen("8/8/8/4R3/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e5 = squareOf(File::E, Rank::R5);
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e5; }), 14);
}

TEST(MoveGen, RookBlockedByOwnPiece)
{
    auto board = fromFen("8/8/8/2P1R3/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e5 = squareOf(File::E, Rank::R5);
    Square d5 = squareOf(File::D, Rank::R5);
    Square c5 = squareOf(File::C, Rank::R5);
    EXPECT_TRUE(containsMove(moves, e5, d5));
    EXPECT_FALSE(containsMove(moves, e5, c5));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e5; }), 11);
}

TEST(MoveGen, RookCapturesEnemy)
{
    auto board = fromFen("8/8/8/2p1R3/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e5 = squareOf(File::E, Rank::R5);
    Square c5 = squareOf(File::C, Rank::R5);
    EXPECT_TRUE(containsMove(moves, e5, c5, Capture));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e5; }), 12);
}

// --- Bishop tests ---

TEST(MoveGen, BishopOpenDiagonals)
{
    auto board = fromFen("8/8/8/4B3/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e5 = squareOf(File::E, Rank::R5);
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e5; }), 13);
}

TEST(MoveGen, BishopCorner)
{
    auto board = fromFen("B7/8/8/8/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square a8 = squareOf(File::A, Rank::R8);
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == a8; }), 7);
}

TEST(MoveGen, BishopBlockedMidDiag)
{
    auto board = fromFen("8/8/1B6/8/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square b6 = squareOf(File::B, Rank::R6);
    EXPECT_TRUE(containsMove(moves, b6, squareOf(File::A, Rank::R7)));
    EXPECT_TRUE(containsMove(moves, b6, squareOf(File::C, Rank::R7)));
    EXPECT_TRUE(containsMove(moves, b6, squareOf(File::D, Rank::R8)));
    EXPECT_TRUE(containsMove(moves, b6, squareOf(File::C, Rank::R5)));
    EXPECT_TRUE(containsMove(moves, b6, squareOf(File::D, Rank::R4)));
    EXPECT_TRUE(containsMove(moves, b6, squareOf(File::E, Rank::R3)));
    EXPECT_TRUE(containsMove(moves, b6, squareOf(File::F, Rank::R2)));
    EXPECT_TRUE(containsMove(moves, b6, squareOf(File::G, Rank::R1)));
    EXPECT_TRUE(containsMove(moves, b6, squareOf(File::A, Rank::R5)));
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == b6; }), 9);
}

// --- Queen tests ---

TEST(MoveGen, QueenOpenBoard)
{
    auto board = fromFen("8/8/8/4Q3/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square e5 = squareOf(File::E, Rank::R5);
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == e5; }), 27);
}

TEST(MoveGen, QueenCorner)
{
    auto board = fromFen("Q7/8/8/8/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board);
    Square a8 = squareOf(File::A, Rank::R8);
    EXPECT_EQ(std::count_if(moves.begin(), moves.end(), [&](const Move& m) { return m.from == a8; }), 21);
}

// --- CapturesOnly filter ---

TEST(MoveGen, CapturesOnlyRook)
{
    auto board = fromFen("8/8/8/2p1R3/8/8/8/8 w - - 0 1");
    ASSERT_TRUE(board.has_value());
    auto moves = generateMoves(*board, MoveFilter::CapturesOnly);
    Square e5 = squareOf(File::E, Rank::R5);
    Square c5 = squareOf(File::C, Rank::R5);
    EXPECT_EQ(moves.size(), 1u);
    EXPECT_TRUE(containsMove(moves, e5, c5, Capture));
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

// --- isAttacked tests ---

TEST(MoveGen, KnightAttacksSquare)
{
    // White knight on f3 (37) attacks d4 (51) via offset +14
    auto board = *Board::fromFen("8/8/8/8/8/5N2/8/8 w - - 0 1");
    Square d4 = squareOf(File::D, Rank::R4);
    EXPECT_TRUE(isAttacked(board, d4, Color::White));
}

TEST(MoveGen, BlackKnightAttacksSquare)
{
    // Black knight on c6 (82) attacks d4 (51) via offset -31
    auto board = *Board::fromFen("8/8/2n5/8/8/8/8/8 w - - 0 1");
    Square d4 = squareOf(File::D, Rank::R4);
    EXPECT_TRUE(isAttacked(board, d4, Color::Black));
}

TEST(MoveGen, KnightDoesNotAttackFar)
{
    auto board = Board::fromStartPos();
    Square a1 = squareOf(File::A, Rank::R1);
    EXPECT_FALSE(isAttacked(board, a1, Color::White));
}

TEST(MoveGen, KingAdjacentAttack)
{
    auto board = *Board::fromFen("8/8/8/4k3/4K3/8/8/8 w - - 0 1");
    Square d4 = squareOf(File::D, Rank::R4);
    EXPECT_TRUE(isAttacked(board, d4, Color::Black));
    EXPECT_TRUE(isAttacked(board, squareOf(File::F, Rank::R5), Color::White));
}

TEST(MoveGen, PawnAttacksForward)
{
    // White pawn on d5 (67) attacks c6 (82) and e6 (84) via +15/+17
    auto board = *Board::fromFen("8/8/8/3P4/8/8/8/8 w - - 0 1");
    Square c6 = squareOf(File::C, Rank::R6);
    Square e6 = squareOf(File::E, Rank::R6);
    EXPECT_TRUE(isAttacked(board, c6, Color::White));
    EXPECT_TRUE(isAttacked(board, e6, Color::White));
}

TEST(MoveGen, BlackPawnAttacksForward)
{
    // Black pawn on e5 (68) attacks d4 (51) and f4 (53) via -15/-17
    auto board = *Board::fromFen("8/8/8/4p3/8/8/8/8 w - - 0 1");
    Square d4 = squareOf(File::D, Rank::R4);
    Square f4 = squareOf(File::F, Rank::R4);
    EXPECT_TRUE(isAttacked(board, d4, Color::Black));
    EXPECT_TRUE(isAttacked(board, f4, Color::Black));
}

TEST(MoveGen, PawnDoesNotAttackBackward)
{
    // White pawn on d4 (51) does NOT attack c3 (34) or e3 (36)
    auto board = *Board::fromFen("8/8/8/8/3P4/8/8/8 w - - 0 1");
    Square c3 = squareOf(File::C, Rank::R3);
    Square e3 = squareOf(File::E, Rank::R3);
    EXPECT_FALSE(isAttacked(board, c3, Color::White));
    EXPECT_FALSE(isAttacked(board, e3, Color::White));
}

TEST(MoveGen, RookOnOpenFile)
{
    auto board = *Board::fromFen("8/8/8/4R3/8/8/8/8 w - - 0 1");
    Square e1 = squareOf(File::E, Rank::R1);
    Square e8 = squareOf(File::E, Rank::R8);
    Square a5 = squareOf(File::A, Rank::R5);
    EXPECT_TRUE(isAttacked(board, e1, Color::White));
    EXPECT_TRUE(isAttacked(board, e8, Color::White));
    EXPECT_TRUE(isAttacked(board, a5, Color::White));
}

TEST(MoveGen, RookBlocked)
{
    // Own bishop on e4 blocks rook on e5 from reaching e1
    auto board = *Board::fromFen("8/8/8/4R3/4B3/8/8/8 w - - 0 1");
    Square e1 = squareOf(File::E, Rank::R1);
    EXPECT_FALSE(isAttacked(board, e1, Color::White));
}

TEST(MoveGen, BishopOnOpenDiagonal)
{
    auto board = *Board::fromFen("8/8/8/4B3/8/8/8/8 w - - 0 1");
    Square h8 = squareOf(File::H, Rank::R8);
    Square a1 = squareOf(File::A, Rank::R1);
    EXPECT_TRUE(isAttacked(board, h8, Color::White));
    EXPECT_TRUE(isAttacked(board, a1, Color::White));
}

TEST(MoveGen, BishopBlocked)
{
    // Bishop d4, pawn c3 blocks diagonal to b2
    auto board = *Board::fromFen("8/8/8/8/3B4/2P5/8/8 w - - 0 1");
    Square b2 = squareOf(File::B, Rank::R2);
    EXPECT_FALSE(isAttacked(board, b2, Color::White));
}

TEST(MoveGen, QueenCombinesRookAndBishop)
{
    auto board = *Board::fromFen("8/8/8/4Q3/8/8/8/8 w - - 0 1");
    Square e1 = squareOf(File::E, Rank::R1);
    Square a5 = squareOf(File::A, Rank::R5);
    Square h8 = squareOf(File::H, Rank::R8);
    EXPECT_TRUE(isAttacked(board, e1, Color::White));
    EXPECT_TRUE(isAttacked(board, a5, Color::White));
    EXPECT_TRUE(isAttacked(board, h8, Color::White));
}

TEST(MoveGen, SlidingBlockedByFriendly)
{
    // White rook on e5, white bishop on g5 blocks rook from reaching h5
    auto board = *Board::fromFen("8/8/8/4R1B1/8/8/8/8 w - - 0 1");
    Square h5 = squareOf(File::H, Rank::R5);
    EXPECT_FALSE(isAttacked(board, h5, Color::White));
}

// --- inCheck tests ---

TEST(MoveGen, InCheckAtStart)
{
    auto board = Board::fromStartPos();
    EXPECT_FALSE(inCheck(board, Color::White));
    EXPECT_FALSE(inCheck(board, Color::Black));
}

TEST(MoveGen, WhiteKingInCheck)
{
    // Black rook on a1 checks white king on e1 via open first rank
    auto board = *Board::fromFen("8/8/8/8/8/8/8/r3K2R w - - 0 1");
    EXPECT_TRUE(inCheck(board, Color::White));
}

TEST(MoveGen, BlackKingInCheck)
{
    // White rook on a8 checks black king on e8 via open eighth rank
    auto board = *Board::fromFen("R3k2r/8/8/8/8/8/8/4K3 b - - 0 1");
    EXPECT_TRUE(inCheck(board, Color::Black));
}

TEST(MoveGen, NotInCheckNearbyThreat)
{
    auto board = *Board::fromFen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    EXPECT_FALSE(inCheck(board, Color::White));
    EXPECT_FALSE(inCheck(board, Color::Black));
}

TEST(MoveGen, IsAttackedByColor)
{
    // White rook on e5 attacks e-file; black king on e5
    auto board = *Board::fromFen("8/8/8/4kR2/8/8/8/8 w - - 0 1");
    Square e5 = squareOf(File::E, Rank::R5);
    EXPECT_TRUE(isAttacked(board, e5, Color::White));
    EXPECT_FALSE(isAttacked(board, e5, Color::Black));
}

// --- Legal move filtering tests ---

TEST(MoveGen, LegalMovesAtStart)
{
    auto board = Board::fromStartPos();
    auto legal = generateLegalMoves(board);
    EXPECT_EQ(legal.size(), 20u);
}

TEST(MoveGen, LegalMovesCastlingPosition)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    auto legal = generateLegalMoves(board);
    EXPECT_EQ(legal.size(), 25u);
}

TEST(MoveGen, LegalMovesFiltersKingIntoCheck)
{
    // Black rook on d5 attacks d1; white king on e1 cannot go to d1 or d2
    auto board = *Board::fromFen("8/8/8/3r4/8/8/8/4K2R w - - 0 1");
    auto legal = generateLegalMoves(board);
    Square e1 = squareOf(File::E, Rank::R1);
    EXPECT_FALSE(containsMove(legal, e1, squareOf(File::D, Rank::R1)));
    EXPECT_FALSE(containsMove(legal, e1, squareOf(File::D, Rank::R2)));
    EXPECT_TRUE(containsMove(legal, e1, squareOf(File::F, Rank::R1)));
    EXPECT_TRUE(containsMove(legal, e1, squareOf(File::E, Rank::R2)));
}

TEST(MoveGen, LegalMovesPinnedPiece)
{
    // White rook on e2 pinned by black rook on e5 to white king on e1
    auto board = *Board::fromFen("8/8/8/4r3/8/8/4R3/4K3 w - - 0 1");
    auto legal = generateLegalMoves(board);
    Square e2 = squareOf(File::E, Rank::R2);
    // Rook on e2 cannot move off the e-file (would expose king to check)
    EXPECT_FALSE(containsMove(legal, e2, squareOf(File::D, Rank::R2)));
    EXPECT_FALSE(containsMove(legal, e2, squareOf(File::F, Rank::R2)));
    // But can move along e-file
    EXPECT_TRUE(containsMove(legal, e2, squareOf(File::E, Rank::R3)));
}

TEST(MoveGen, LegalMovesCastlingThroughCheck)
{
    // Black rook on f4 attacks transit square f1; KS castle illegal, QS legal
    auto board = *Board::fromFen("8/8/8/8/5r2/8/8/R3K2R w KQ - 0 1");
    auto legal = generateLegalMoves(board);
    Square e1 = squareOf(File::E, Rank::R1);
    // KS castle (e1→g1) should be illegal (f1 attacked)
    EXPECT_FALSE(containsMove(legal, e1, squareOf(File::G, Rank::R1), Castle));
    // QS castle (e1→c1) should be legal
    EXPECT_TRUE(containsMove(legal, e1, squareOf(File::C, Rank::R1), Castle));
}

TEST(MoveGen, LegalMovesCastlingOutOfCheck)
{
    // Black rook on e5 gives check (no e2 pawn blocking); neither castle is legal
    auto board = *Board::fromFen("r3k2r/pppppppp/8/4r3/8/8/PPPP1PPP/R3K2R w KQkq - 0 1");
    auto legal = generateLegalMoves(board);
    Square e1 = squareOf(File::E, Rank::R1);
    EXPECT_FALSE(containsMove(legal, e1, squareOf(File::G, Rank::R1), Castle));
    EXPECT_FALSE(containsMove(legal, e1, squareOf(File::C, Rank::R1), Castle));
}

TEST(MoveGen, LegalMovesCapturesOnly)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    auto legal = generateLegalMoves(board, MoveFilter::CapturesOnly);
    for (const auto& m : legal) {
        EXPECT_TRUE(m.isCapture());
    }
    EXPECT_EQ(legal.size(), 0u);
}

TEST(MoveGen, LegalMovesBlackToMove)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R b KQkq - 0 1");
    auto legal = generateLegalMoves(board);
    EXPECT_EQ(legal.size(), 25u);
}

// --- Game state tests ---

TEST(MoveGen, GameStateOngoing)
{
    auto board = Board::fromStartPos();
    EXPECT_EQ(evaluateGameState(board), GameState::Ongoing);
}

TEST(MoveGen, GameStateCheckmate)
{
    // Black king a8, white queen a7 protected by pawn b6. No escape.
    auto board = *Board::fromFen("k1K5/Q7/1P6/8/8/8/8/8 b - - 0 1");
    EXPECT_EQ(evaluateGameState(board), GameState::Checkmate);
}

TEST(MoveGen, GameStateStalemate)
{
    // Black king a8, white king c7, white queen b6. No legal moves, not in check.
    auto board = *Board::fromFen("k7/2K5/1Q6/8/8/8/8/8 b - - 0 1");
    EXPECT_EQ(evaluateGameState(board), GameState::Stalemate);
}

TEST(MoveGen, GameStateInCheckNotMate)
{
    // Black king e8 in check from rook e2; can escape to d8/f8/f7.
    auto board = *Board::fromFen("4k3/8/8/8/8/8/4R3/4K3 b - - 0 1");
    EXPECT_EQ(evaluateGameState(board), GameState::Ongoing);
}

TEST(MoveGen, GameStateWhiteCheckmated)
{
    // White king e1 checkmated by black queen e2 (protected by rook a2).
    auto board = *Board::fromFen("4k3/8/8/8/8/8/r3q3/4K2R w - - 0 1");
    EXPECT_EQ(evaluateGameState(board), GameState::Checkmate);
}

} // namespace chess
