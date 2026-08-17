#include <chess/board.h>

#include <gtest/gtest.h>

namespace chess {

TEST(State, Defaults)
{
    Board board;
    EXPECT_EQ(board.sideToMove(), Color::White);
    EXPECT_EQ(board.castlingRights(), NoCastling);
    EXPECT_EQ(board.enPassantSquare(), SquareNone);
    EXPECT_EQ(board.halfmoveClock(), 0);
    EXPECT_EQ(board.fullmoveNumber(), 1);
}

TEST(State, SettersAndCastlingFlags)
{
    Board board;
    board.setSideToMove(Color::Black);
    board.setCastlingRights(WhiteKingSide | BlackQueenSide);
    board.setEnPassantSquare(squareOf(File::E, Rank::R3));
    board.setHalfmoveClock(47);
    board.setFullmoveNumber(12);

    EXPECT_EQ(board.sideToMove(), Color::Black);
    EXPECT_EQ(board.castlingRights(), WhiteKingSide | BlackQueenSide);
    EXPECT_TRUE(canCastle(board.castlingRights(), WhiteKingSide));
    EXPECT_FALSE(canCastle(board.castlingRights(), WhiteQueenSide));
    EXPECT_TRUE(canCastle(board.castlingRights(), BlackQueenSide));
    EXPECT_FALSE(canCastle(board.castlingRights(), BlackKingSide));
    EXPECT_EQ(board.enPassantSquare(), squareOf(File::E, Rank::R3));
    EXPECT_EQ(board.halfmoveClock(), 47);
    EXPECT_EQ(board.fullmoveNumber(), 12);
}

TEST(State, HistorySnapshotRoundTrip)
{
    Board board;
    const Square e1 = squareOf(File::E, Rank::R1);
    board.setPiece(e1, Piece::of(Color::White, PieceType::King));
    board.setCastlingRights(AllCastling);
    board.setSideToMove(Color::Black);
    board.pushSnapshot();
    EXPECT_EQ(board.snapshotCount(), 1u);

    board.clearSquare(e1);
    board.setCastlingRights(NoCastling);
    board.setSideToMove(Color::White);

    board.popSnapshot();
    EXPECT_EQ(board.snapshotCount(), 0u);
    EXPECT_EQ(board.pieceAt(e1), Piece::of(Color::White, PieceType::King));
    EXPECT_EQ(board.castlingRights(), AllCastling);
    EXPECT_EQ(board.sideToMove(), Color::Black);
}

TEST(State, SnapshotStoresFullPositionCopy)
{
    Board board;
    board.setPiece(squareOf(File::A, Rank::R1), Piece::of(Color::White, PieceType::Rook));
    board.setPiece(squareOf(File::H, Rank::R8), Piece::of(Color::Black, PieceType::Rook));
    board.pushSnapshot();

    const Snapshot& snap = board.snapshotAt(0);
    EXPECT_EQ(snap.pieces[squareOf(File::A, Rank::R1)], Piece::of(Color::White, PieceType::Rook));
    EXPECT_EQ(snap.pieces[squareOf(File::H, Rank::R8)], Piece::of(Color::Black, PieceType::Rook));
    EXPECT_EQ(snap.state.sideToMove, Color::White);
    EXPECT_EQ(snap.state.castlingRights, NoCastling);
}

} // namespace chess
