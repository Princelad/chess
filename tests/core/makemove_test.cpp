#include <chess/board.h>
#include <chess/fen.h>
#include <chess/move.h>
#include <chess/movegen.h>

#include <gtest/gtest.h>

namespace chess {

// --- Quiet moves ---

TEST(MakeMove, QuietKnight)
{
    auto board = Board::fromStartPos();
    Square b1 = squareOf(File::B, Rank::R1);
    Square c3 = squareOf(File::C, Rank::R3);
    board.makeMove(move(b1, c3));

    EXPECT_TRUE(board.isEmpty(b1));
    EXPECT_EQ(board.pieceAt(c3), Piece::of(Color::White, PieceType::Knight));
    EXPECT_EQ(board.sideToMove(), Color::Black);
}

TEST(MakeMove, QuietPawnPush)
{
    auto board = Board::fromStartPos();
    Square e2 = squareOf(File::E, Rank::R2);
    Square e3 = squareOf(File::E, Rank::R3);
    board.makeMove(move(e2, e3));

    EXPECT_TRUE(board.isEmpty(e2));
    EXPECT_EQ(board.pieceAt(e3), Piece::of(Color::White, PieceType::Pawn));
}

// --- Capture ---

TEST(MakeMove, Capture)
{
    auto board = *Board::fromFen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    Square d7 = squareOf(File::D, Rank::R7);
    Square e5 = squareOf(File::E, Rank::R5);
    board.makeMove(captureMove(d7, e5));

    EXPECT_TRUE(board.isEmpty(d7));
    EXPECT_EQ(board.pieceAt(e5), Piece::of(Color::Black, PieceType::Pawn));
    EXPECT_EQ(board.halfmoveClock(), 0);
}

// --- Double pawn push ---

TEST(MakeMove, DoublePushSetsEnPassant)
{
    auto board = Board::fromStartPos();
    Square e2 = squareOf(File::E, Rank::R2);
    Square e4 = squareOf(File::E, Rank::R4);
    board.makeMove(doublePushMove(e2, e4));

    EXPECT_TRUE(board.isEmpty(e2));
    EXPECT_EQ(board.pieceAt(e4), Piece::of(Color::White, PieceType::Pawn));
    EXPECT_EQ(board.enPassantSquare(), squareOf(File::E, Rank::R3));
}

TEST(MakeMove, BlackDoublePushSetsEnPassant)
{
    auto board = *Board::fromFen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    Square d7 = squareOf(File::D, Rank::R7);
    Square d5 = squareOf(File::D, Rank::R5);
    board.makeMove(doublePushMove(d7, d5));

    EXPECT_TRUE(board.isEmpty(d7));
    EXPECT_EQ(board.pieceAt(d5), Piece::of(Color::Black, PieceType::Pawn));
    EXPECT_EQ(board.enPassantSquare(), squareOf(File::D, Rank::R6));
}

TEST(MakeMove, EnPassantSquareClearedOnNonDoublePush)
{
    auto board = Board::fromStartPos();
    Square e2 = squareOf(File::E, Rank::R2);
    Square e4 = squareOf(File::E, Rank::R4);
    board.makeMove(doublePushMove(e2, e4));
    EXPECT_EQ(board.enPassantSquare(), squareOf(File::E, Rank::R3));

    // Black plays a non-double-push move
    Square a7 = squareOf(File::A, Rank::R7);
    Square a6 = squareOf(File::A, Rank::R6);
    board.makeMove(move(a7, a6));
    EXPECT_EQ(board.enPassantSquare(), SquareNone);
}

// --- En passant capture ---

TEST(MakeMove, EnPassantCapture)
{
    // White e2-e4, Black d7-d5, White e4xd5 en passant
    auto board = Board::fromStartPos();
    board.makeMove(doublePushMove(squareOf(File::E, Rank::R2), squareOf(File::E, Rank::R4)));
    board.makeMove(doublePushMove(squareOf(File::D, Rank::R7), squareOf(File::D, Rank::R5)));

    Square e4 = squareOf(File::E, Rank::R4);
    Square d5 = squareOf(File::D, Rank::R5);
    board.makeMove(enPassantMove(e4, d5));

    EXPECT_TRUE(board.isEmpty(e4));
    EXPECT_EQ(board.pieceAt(d5), Piece::of(Color::White, PieceType::Pawn));
    // The captured pawn on d5 should be gone (it was replaced by the white pawn)
    // The pawn that was on e4 should be gone
    EXPECT_TRUE(board.isEmpty(squareOf(File::E, Rank::R5)));  // captured pawn's square
}

// --- Promotion ---

TEST(MakeMove, QuietPromotion)
{
    auto board = *Board::fromFen("8/4P3/8/8/8/8/8/4K2k w - - 0 1");
    Square e7 = squareOf(File::E, Rank::R7);
    Square e8 = squareOf(File::E, Rank::R8);
    board.makeMove(promotionMove(e7, e8, PieceType::Queen));

    EXPECT_TRUE(board.isEmpty(e7));
    EXPECT_EQ(board.pieceAt(e8), Piece::of(Color::White, PieceType::Queen));
}

TEST(MakeMove, CapturePromotion)
{
    auto board = *Board::fromFen("3r3k/4P3/8/8/8/8/8/4K3 w - - 0 1");
    Square e7 = squareOf(File::E, Rank::R7);
    Square d8 = squareOf(File::D, Rank::R8);
    board.makeMove(promotionMove(e7, d8, PieceType::Rook));

    EXPECT_TRUE(board.isEmpty(e7));
    EXPECT_EQ(board.pieceAt(d8), Piece::of(Color::White, PieceType::Rook));
}

// --- Castling ---

TEST(MakeMove, WhiteKingSideCastling)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    Square e1 = squareOf(File::E, Rank::R1);
    Square g1 = squareOf(File::G, Rank::R1);
    board.makeMove(castleMove(e1, g1));

    EXPECT_TRUE(board.isEmpty(e1));
    EXPECT_EQ(board.pieceAt(g1), Piece::of(Color::White, PieceType::King));
    // Rook: h1 → f1
    EXPECT_TRUE(board.isEmpty(squareOf(File::H, Rank::R1)));
    EXPECT_EQ(board.pieceAt(squareOf(File::F, Rank::R1)), Piece::of(Color::White, PieceType::Rook));
}

TEST(MakeMove, WhiteQueenSideCastling)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    Square e1 = squareOf(File::E, Rank::R1);
    Square c1 = squareOf(File::C, Rank::R1);
    board.makeMove(castleMove(e1, c1));

    EXPECT_TRUE(board.isEmpty(e1));
    EXPECT_EQ(board.pieceAt(c1), Piece::of(Color::White, PieceType::King));
    // Rook: a1 → d1
    EXPECT_TRUE(board.isEmpty(squareOf(File::A, Rank::R1)));
    EXPECT_EQ(board.pieceAt(squareOf(File::D, Rank::R1)), Piece::of(Color::White, PieceType::Rook));
}

TEST(MakeMove, BlackKingSideCastling)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R b KQkq - 0 1");
    Square e8 = squareOf(File::E, Rank::R8);
    Square g8 = squareOf(File::G, Rank::R8);
    board.makeMove(castleMove(e8, g8));

    EXPECT_TRUE(board.isEmpty(e8));
    EXPECT_EQ(board.pieceAt(g8), Piece::of(Color::Black, PieceType::King));
    EXPECT_TRUE(board.isEmpty(squareOf(File::H, Rank::R8)));
    EXPECT_EQ(board.pieceAt(squareOf(File::F, Rank::R8)), Piece::of(Color::Black, PieceType::Rook));
}

TEST(MakeMove, BlackQueenSideCastling)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R b KQkq - 0 1");
    Square e8 = squareOf(File::E, Rank::R8);
    Square c8 = squareOf(File::C, Rank::R8);
    board.makeMove(castleMove(e8, c8));

    EXPECT_TRUE(board.isEmpty(e8));
    EXPECT_EQ(board.pieceAt(c8), Piece::of(Color::Black, PieceType::King));
    EXPECT_TRUE(board.isEmpty(squareOf(File::A, Rank::R8)));
    EXPECT_EQ(board.pieceAt(squareOf(File::D, Rank::R8)), Piece::of(Color::Black, PieceType::Rook));
}

// --- Castling rights ---

TEST(MakeMove, KingMoveRevokesCastlingRights)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    Square e1 = squareOf(File::E, Rank::R1);
    Square f1 = squareOf(File::F, Rank::R1);
    board.makeMove(move(e1, f1));

    EXPECT_EQ(board.castlingRights() & (WhiteKingSide | WhiteQueenSide), 0);
    EXPECT_TRUE(canCastle(board.castlingRights(), BlackKingSide));
    EXPECT_TRUE(canCastle(board.castlingRights(), BlackQueenSide));
}

TEST(MakeMove, RookMoveRevokesOwnCastlingRight)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    Square h1 = squareOf(File::H, Rank::R1);
    Square h2 = squareOf(File::H, Rank::R2);
    board.makeMove(move(h1, h2));

    EXPECT_FALSE(canCastle(board.castlingRights(), WhiteKingSide));
    EXPECT_TRUE(canCastle(board.castlingRights(), WhiteQueenSide));
    EXPECT_TRUE(canCastle(board.castlingRights(), BlackKingSide));
    EXPECT_TRUE(canCastle(board.castlingRights(), BlackQueenSide));
}

TEST(MakeMove, CaptureOnRookHomeRevokesRights)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    // White captures on h8 (black rook home)
    Square g6 = squareOf(File::G, Rank::R6);
    Square h8 = squareOf(File::H, Rank::R8);
    // Set up a white queen at g6 to capture
    board.setPiece(g6, Piece::of(Color::White, PieceType::Queen));
    board.makeMove(captureMove(g6, h8));

    EXPECT_FALSE(canCastle(board.castlingRights(), BlackKingSide));
    EXPECT_TRUE(canCastle(board.castlingRights(), BlackQueenSide));
    EXPECT_TRUE(canCastle(board.castlingRights(), WhiteKingSide));
    EXPECT_TRUE(canCastle(board.castlingRights(), WhiteQueenSide));
}

// --- Side to move ---

TEST(MakeMove, SideToMoveToggles)
{
    auto board = Board::fromStartPos();
    EXPECT_EQ(board.sideToMove(), Color::White);

    board.makeMove(move(squareOf(File::E, Rank::R2), squareOf(File::E, Rank::R3)));
    EXPECT_EQ(board.sideToMove(), Color::Black);

    board.makeMove(move(squareOf(File::E, Rank::R7), squareOf(File::E, Rank::R6)));
    EXPECT_EQ(board.sideToMove(), Color::White);
}

// --- Halfmove clock ---

TEST(MakeMove, HalfmoveClockResetsOnPawnMove)
{
    auto board = Board::fromStartPos();
    board.setHalfmoveClock(10);

    board.makeMove(move(squareOf(File::E, Rank::R2), squareOf(File::E, Rank::R3)));
    EXPECT_EQ(board.halfmoveClock(), 0);
}

TEST(MakeMove, HalfmoveClockResetsOnCapture)
{
    auto board = *Board::fromFen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 5 1");
    board.setHalfmoveClock(10);

    board.makeMove(captureMove(squareOf(File::D, Rank::R7), squareOf(File::E, Rank::R5)));
    EXPECT_EQ(board.halfmoveClock(), 0);
}

TEST(MakeMove, HalfmoveClockIncrementsOnNonPawnNonCapture)
{
    auto board = Board::fromStartPos();
    board.setHalfmoveClock(5);

    board.makeMove(move(squareOf(File::B, Rank::R1), squareOf(File::C, Rank::R3)));
    EXPECT_EQ(board.halfmoveClock(), 6);
}

// --- Fullmove number ---

TEST(MakeMove, FullmoveNumberIncrementsAfterBlack)
{
    auto board = Board::fromStartPos();
    EXPECT_EQ(board.fullmoveNumber(), 1);

    board.makeMove(move(squareOf(File::E, Rank::R2), squareOf(File::E, Rank::R3)));
    EXPECT_EQ(board.fullmoveNumber(), 1);  // still white's turn conceptually

    board.makeMove(move(squareOf(File::E, Rank::R7), squareOf(File::E, Rank::R6)));
    EXPECT_EQ(board.fullmoveNumber(), 2);
}

// --- Snapshot pushed for undo ---

TEST(MakeMove, PushesSnapshot)
{
    auto board = Board::fromStartPos();
    EXPECT_EQ(board.snapshotCount(), 0u);

    board.makeMove(move(squareOf(File::E, Rank::R2), squareOf(File::E, Rank::R3)));
    EXPECT_EQ(board.snapshotCount(), 1u);
}

// --- Restore via snapshot (smoke test for undo infrastructure) ---

TEST(MakeMove, SnapshotPreservesPreviousState)
{
    auto board = Board::fromStartPos();
    board.makeMove(move(squareOf(File::E, Rank::R2), squareOf(File::E, Rank::R3)));

    const Snapshot& snap = board.snapshotAt(0);
    EXPECT_EQ(snap.pieces[squareOf(File::E, Rank::R2)], Piece::of(Color::White, PieceType::Pawn));
    EXPECT_TRUE(snap.pieces[squareOf(File::E, Rank::R3)].isNone());
    EXPECT_EQ(snap.state.sideToMove, Color::White);
    EXPECT_EQ(snap.state.castlingRights, AllCastling);
}

// --- Round-trip: makeMove then popSnapshot restores original ---

TEST(MakeMove, RoundTripRestore)
{
    auto board = Board::fromStartPos();
    std::string originalFen = board.toFen();

    board.makeMove(move(squareOf(File::E, Rank::R2), squareOf(File::E, Rank::R3)));
    board.popSnapshot();

    EXPECT_EQ(board.toFen(), originalFen);
}

} // namespace chess
