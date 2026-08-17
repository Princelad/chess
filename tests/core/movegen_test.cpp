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

// --- Threefold repetition tests ---

TEST(MoveGen, ThreefoldRepetitionStartPosNeverRepeats)
{
    auto board = Board::fromStartPos();
    EXPECT_FALSE(threefoldRepetition(board));
}

TEST(MoveGen, ThreefoldRepetitionKnightShuffle)
{
    // 1.Nf3 Nf6 2.Ng1 Ng8 3.Nf3 Nf6 4.Ng1 Ng8
    // After 4...Ng8, the start position has occurred 3 times.
    auto board = Board::fromStartPos();
    Square g1 = squareOf(File::G, Rank::R1);
    Square f3 = squareOf(File::F, Rank::R3);
    Square g8 = squareOf(File::G, Rank::R8);
    Square f6 = squareOf(File::F, Rank::R6);

    // Pair 1
    board.makeMove(move(g1, f3));   // 1.Nf3
    board.makeMove(move(g8, f6));   // 1...Nf6
    board.makeMove(move(f3, g1));   // 2.Ng1
    board.makeMove(move(f6, g8));   // 2...Ng8
    EXPECT_FALSE(threefoldRepetition(board)); // start pos seen twice

    // Pair 2
    board.makeMove(move(g1, f3));   // 3.Nf3
    board.makeMove(move(g8, f6));   // 3...Nf6
    board.makeMove(move(f3, g1));   // 4.Ng1
    board.makeMove(move(f6, g8));   // 4...Ng8
    EXPECT_TRUE(threefoldRepetition(board)); // start pos seen 3 times
}

TEST(MoveGen, ThreefoldRepetitionWithUndo)
{
    // Make moves and undo them; repetition should not persist after undo.
    auto board = Board::fromStartPos();
    Square g1 = squareOf(File::G, Rank::R1);
    Square f3 = squareOf(File::F, Rank::R3);
    Square g8 = squareOf(File::G, Rank::R8);
    Square f6 = squareOf(File::F, Rank::R6);

    board.makeMove(move(g1, f3));
    board.makeMove(move(g8, f6));
    board.makeMove(move(f3, g1));
    board.makeMove(move(f6, g8));
    board.makeMove(move(g1, f3));
    board.makeMove(move(g8, f6));
    board.makeMove(move(f3, g1));
    board.makeMove(move(f6, g8));
    EXPECT_TRUE(threefoldRepetition(board));

    // Undo last move — back to position after 4.Ng1, only 2 occurrences
    board.undoMove(move(f6, g8));
    EXPECT_FALSE(threefoldRepetition(board));
}

TEST(MoveGen, GameStateDrawByRepetition)
{
    auto board = Board::fromStartPos();
    Square g1 = squareOf(File::G, Rank::R1);
    Square f3 = squareOf(File::F, Rank::R3);
    Square g8 = squareOf(File::G, Rank::R8);
    Square f6 = squareOf(File::F, Rank::R6);

    // Two full cycles + one more pair = 3 occurrences of start position
    for (int i = 0; i < 2; ++i) {
        board.makeMove(move(g1, f3));
        board.makeMove(move(g8, f6));
        board.makeMove(move(f3, g1));
        board.makeMove(move(f6, g8));
    }
    EXPECT_EQ(evaluateGameState(board), GameState::Draw);
}

// --- Fifty-move rule tests ---

TEST(MoveGen, FiftyMoveRuleNotTriggered)
{
    auto board = Board::fromStartPos();
    EXPECT_FALSE(fiftyMoveRule(board));
}

TEST(MoveGen, FiftyMoveRuleTriggered)
{
    // Position with halfmove clock at 100 — draw by fifty-move rule
    auto board = *Board::fromFen("8/8/8/8/8/8/8/4K2k w - - 100 50");
    EXPECT_TRUE(fiftyMoveRule(board));
    EXPECT_EQ(evaluateGameState(board), GameState::Draw);
}

TEST(MoveGen, FiftyMoveRuleResetsOnPawnMove)
{
    // Position with halfmove clock at 100; pawn move resets it
    auto board = *Board::fromFen("8/8/8/8/4P3/8/8/4K2k w - - 100 50");
    EXPECT_TRUE(fiftyMoveRule(board));
    // Make the pawn push — clock resets to 0
    board.makeMove(move(squareOf(File::E, Rank::R4), squareOf(File::E, Rank::R5)));
    EXPECT_FALSE(fiftyMoveRule(board));
}

TEST(MoveGen, FiftyMoveRuleResetsOnCapture)
{
    // Position with halfmove clock at 100; capture resets it
    auto board = *Board::fromFen("8/8/8/4p3/3P4/8/8/4K2k w - - 100 50");
    EXPECT_TRUE(fiftyMoveRule(board));
    // Capture on e5 — clock resets to 0
    board.makeMove(move(squareOf(File::D, Rank::R4), squareOf(File::E, Rank::R5)));
    EXPECT_FALSE(fiftyMoveRule(board));
}

// --- Insufficient material tests ---

TEST(MoveGen, InsufficientMaterialKingVsKing)
{
    auto board = *Board::fromFen("8/8/8/8/8/8/8/4K2k w - - 0 1");
    EXPECT_TRUE(insufficientMaterial(board));
    EXPECT_EQ(evaluateGameState(board), GameState::Draw);
}

TEST(MoveGen, InsufficientMaterialBishopVsKing)
{
    // White bishop on light square
    auto board = *Board::fromFen("8/8/8/8/8/8/5B2/4K2k w - - 0 1");
    EXPECT_TRUE(insufficientMaterial(board));
    EXPECT_EQ(evaluateGameState(board), GameState::Draw);
}

TEST(MoveGen, InsufficientMaterialKnightVsKing)
{
    auto board = *Board::fromFen("8/8/8/8/8/8/5N2/4K2k w - - 0 1");
    EXPECT_TRUE(insufficientMaterial(board));
    EXPECT_EQ(evaluateGameState(board), GameState::Draw);
}

TEST(MoveGen, InsufficientMaterialBishopVsBishopSameColor)
{
    // Both bishops on light squares: c1 (2+0=even) and e3 (4+2=even)
    auto board = *Board::fromFen("8/8/8/8/8/4b3/8/2B1K2k w - - 0 1");
    EXPECT_TRUE(insufficientMaterial(board));
    EXPECT_EQ(evaluateGameState(board), GameState::Draw);
}

TEST(MoveGen, SufficientMaterialBishopVsBishopDifferentColor)
{
    // White bishop on light square, black bishop on dark square
    auto board = *Board::fromFen("8/8/8/8/8/5B2/8/4K1b1 w - - 0 1");
    EXPECT_FALSE(insufficientMaterial(board));
}

TEST(MoveGen, SufficientMaterialRookVsKing)
{
    auto board = *Board::fromFen("8/8/8/8/8/8/8/R3K2k w - - 0 1");
    EXPECT_FALSE(insufficientMaterial(board));
}

TEST(MoveGen, SufficientMaterialKnightAndBishop)
{
    // K+N vs K+B is sufficient
    auto board = *Board::fromFen("8/8/8/8/8/5N2/5b2/4K2k w - - 0 1");
    EXPECT_FALSE(insufficientMaterial(board));
}

// --- isLegalMove tests ---

TEST(MoveGen, IsLegalMoveStartPos)
{
    auto board = Board::fromStartPos();
    Square e2 = squareOf(File::E, Rank::R2);
    Square e4 = squareOf(File::E, Rank::R4);
    EXPECT_TRUE(isLegalMove(board, doublePushMove(e2, e4)));
}

TEST(MoveGen, IsLegalMoveCapture)
{
    auto board = *Board::fromFen("8/8/8/3p4/4P3/8/8/4K3 w - - 0 1");
    Square e4 = squareOf(File::E, Rank::R4);
    Square d5 = squareOf(File::D, Rank::R5);
    EXPECT_TRUE(isLegalMove(board, captureMove(e4, d5)));
}

TEST(MoveGen, IsLegalMoveKingIntoCheck)
{
    // Black rook on d5 attacks d1; king e1 cannot go to d1
    auto board = *Board::fromFen("8/8/8/3r4/8/8/8/4K3 w - - 0 1");
    Square e1 = squareOf(File::E, Rank::R1);
    Square d1 = squareOf(File::D, Rank::R1);
    EXPECT_FALSE(isLegalMove(board, move(e1, d1)));
}

TEST(MoveGen, IsLegalMovePinnedPiece)
{
    // White rook on e2 pinned by black rook on e5
    auto board = *Board::fromFen("8/8/8/4r3/8/8/4R3/4K3 w - - 0 1");
    Square e2 = squareOf(File::E, Rank::R2);
    Square f2 = squareOf(File::F, Rank::R2);
    EXPECT_FALSE(isLegalMove(board, move(e2, f2)));
}

TEST(MoveGen, EnPassantPinnedDiagonally)
{
    // White king e5, white pawn d5, black pawn e5 is the double-pushed pawn
    // Wait, I need a position where en passant would expose the king.
    // White king on a5, white pawn on b5, black pawn just double-pushed to c5 (EP target c6).
    // If bxc6 en passant, the a5 king would be on the same rank as the black rook on h5 — exposed!
    auto board = *Board::fromFen("8/8/8/RPk5/1P6/8/8/8 b - c6 0 1");
    // Black pawn on c5, en passant target c6. White pawn on b5 can capture en passant: bxc6.
    // But wait, it's black to move. Let me flip: white to move, white pawn captures.
    // Position: 8/8/8/R1kP4/8/8/8/8 w - - 0 1. No EP available.
    // Better: King on b5, pawn on a5, enemy pawn on b4 just double-pushed (a3 EP target).
    // If axb3 EP, then b5 king is on the same file as enemy rook on b8 — exposed!
    auto board2 = *Board::fromFen("r6k/8/8/1P6/Pp6/8/8/8 w - b3 0 1");
    // White pawn a4, black pawn b4 (just double pushed, EP target b3).
    // axb3 en passant: removes pawn on b4, moves pawn from a4 to b3.
    // After the EP, the a-file is clear for black rook on a8 to attack a1... but king is on... hmm let me reconsider.
    // Let me use a cleaner setup:
    // White king on f3, white pawn on e5, black pawn just double-pushed to d5 (EP target d6).
    // If exd6 EP, the e5 pawn leaves f3 exposed to the black bishop on b7 along the a8-h1 diagonal? No...
    // Simplest: white king on d3, white pawn on e5, black rook on h5, black pawn just double-pushed to f5 (EP target f6).
    // If exf6 EP, the e5 pawn leaves d3 exposed to the black rook on h5? No, rook moves along ranks/files.
    // OK let me think clearly. En passant pin: pawn A captures en passant, removing pawn B. After the capture,
    // pawn A moves to a different square than where pawn B was. If pawn B was shielding the king from an
    // attack along a rank or file, removing it (and A not being on that square) exposes the king.
    // Position: White king c3, white pawn d5, black pawn e5 just double-pushed (EP target e6).
    // dxe6 EP: pawn moves from d5 to e6, black pawn on e5 is removed.
    // After EP: white king c3, white pawn e6, black rook on a5 attacks along rank 5 — but d5 is now empty, so a5 rook attacks... no, king is on c3.
    // Let me try: white king c4, white pawn d5, black pawn e5 (EP target e6), black rook on a4.
    // dxe6 EP: pawn goes d5→e6, black pawn on e5 removed.
    // After EP: king on c4, rook on a4 attacks along rank 4 to c4 — CHECK! So dxe6 should be illegal.
    auto board3 = *Board::fromFen("8/8/8/3Pp3/2Rk4/8/8/8 w - e6 0 1");
    // This has: white king c4 (wait, R is on c4 and k is on d4 — that can't be right, two pieces on adjacent squares).
    // Let me fix: "3Pp3/8/8/8/2Rk4/8/8/8" — white pawn d5? No, that's rank 8 for d5...
    // FEN: rank8/rank7/rank6/rank5/rank4/rank3/rank2/rank1
    // I want: white pawn on d5, black pawn on e5, white king on c4, black rook on a4, en passant target e6.
    // Rank 5: 3Pp3 = d5=P(white), e5=p(black). Rank 4: 2Rk4 = c4=R(white? no, R is white rook), d4=k(black king? no).
    // This is getting confused. Let me use a simpler, well-known example:
    // The classic EP pin: white Ke1, white Pe5, black Rh5, black d-pawn just double-pushed to d5.
    // If exd6 EP, the e5 pawn leaves the 5th rank, and Rh5 has a clear path to e1 along rank 5? No, there are other pieces.
    // Classic: Ke5, pe4(double push), Rh1. exd3 EP exposes king to Rh1? No.
    // Let me just use a well-known test position:
    // White king on f3, white pawn on g5, black pawn on h5 just double-pushed (EP target h6).
    // gxh6 EP: pawn moves g5→h6, removes black pawn on h5.
    // After: king f3, pawn h6. Black rook on h8 attacks along h-file — but pawn on h6 blocks. Not a pin.
    // 
    // SIMPLEST: White Ke1, white pawn e5, black pawn d5 (EP target d6), black rook on a5.
    // exd6 EP: pawn moves e5→d6, removes pawn on d5.
    // After: Ke1, pawn d6, Ra5. Ra5 attacks along 5th rank — but e5 is now empty. Does rook reach e1? No, rook is on rank 5, king on rank 1.
    // 
    // OK: White Ke5, white pawn d5, black pawn c5 just double pushed (EP target c6), black rook on a5.
    // dxc6 EP: pawn moves d5→c6, removes black pawn on c5.
    // After: Ke5, pawn c6, Ra5. Ra5 attacks along rank 5 — e5 is now clear of the pawn (it was on d5, moved to c6). But Ke5 is ON rank 5! Ra5→e5 is check!
    // But wait, does the d5 pawn moving away expose the king? The pawn was on d5, king on e5. Removing the pawn from d5 and the black pawn from c5 doesn't directly affect the path from a5 to e5.
    // Actually, Ra5 attacks along rank 5: a5, b5, c5, d5, e5. c5 and d5 had pawns. After dxc6: d5 is empty (pawn moved), c5 is empty (pawn captured). So Ra5→e5 is now open. CHECK!
    // YES! This is the en passant pin. dxc6 should be ILLEGAL because it exposes the white king to the black rook.
    auto board4 = *Board::fromFen("8/8/8/R1pP4/4K3/8/8/8 w - c6 0 1");
    // Rank 5: R1pP4 = a5=R(black), b5=empty, c5=p(black), d5=P(white), e5=K(white, but K should be uppercase)... 
    // Wait, FEN uses lowercase for black, uppercase for white. Let me be careful:
    // a5=R is a WHITE rook? No! In FEN, uppercase = white, lowercase = black.
    // I need: black rook on a5 = 'r', white pawn d5 = 'P', black pawn c5 = 'p', white king e5 = 'K'.
    // Rank 5: r1pP4 — but 'r' is black rook, then 1 empty, 'p' black pawn, 'P' white pawn, 4 empty.
    // That's only: r(a5), 1 empty(b5), p(c5), P(d5), 4 empty(e5-h5). That's 8 squares. But where's the white king?
    // The king should be on e5. So rank 5 = r1pPK3. That's: r(a5), empty(b5), p(c5), P(d5), K(e5), 3 empty(f5-h5) = 8. Yes!
    // But wait, if black rook is on a5 and white king is on e5, and c5/d5 have pawns blocking — is the king already in check? 
    // Ra5 attacks: a5→b5→c5(pawn blocks). So no, the king is safe. After dxc6: d5 empty, c5 empty, Ra5→e5 = check!
    auto epBoard = *Board::fromFen("8/8/8/r1pPK3/8/8/8/8 w - c6 0 1");
    // This should have white to move. dxc6 EP should be generated as pseudo-legal but should NOT be in legal moves.
    auto pseudoLegal = generateMoves(epBoard);
    bool hasEP = false;
    for (const auto& m : pseudoLegal) {
        if (m.isEnPassant() && m.from == squareOf(File::D, Rank::R5) && m.to == squareOf(File::C, Rank::R6)) {
            hasEP = true;
            break;
        }
    }
    EXPECT_TRUE(hasEP); // EP is generated as pseudo-legal
    EXPECT_FALSE(isLegalMove(epBoard, enPassantMove(squareOf(File::D, Rank::R5), squareOf(File::C, Rank::R6))));
}

} // namespace chess
