#include <chess/board.h>

#include <gtest/gtest.h>

namespace chess {

namespace {

int countPieces(const Board& board, Color color)
{
    int count = 0;
    for (int sq = 0; sq < BoardSize; ++sq) {
        if (offBoard(sq)) {
            continue;
        }
        const Piece piece = board.pieceAt(sq);
        if (!piece.isNone() && piece.color == color) {
            ++count;
        }
    }
    return count;
}

} // namespace

TEST(StartPosition, PieceCounts)
{
    const Board board = Board::fromStartPos();
    EXPECT_EQ(countPieces(board, Color::White), 16);
    EXPECT_EQ(countPieces(board, Color::Black), 16);
}

TEST(StartPosition, KingsOnE1AndE8)
{
    const Board board = Board::fromStartPos();
    EXPECT_EQ(board.pieceAt(squareOf(File::E, Rank::R1)), Piece::of(Color::White, PieceType::King));
    EXPECT_EQ(board.pieceAt(squareOf(File::E, Rank::R8)), Piece::of(Color::Black, PieceType::King));
}

TEST(StartPosition, BackRanks)
{
    const Board board = Board::fromStartPos();
    const PieceType backRank[8] = {
        PieceType::Rook, PieceType::Knight, PieceType::Bishop,
        PieceType::Queen, PieceType::King,
        PieceType::Bishop, PieceType::Knight, PieceType::Rook,
    };
    for (int file = 0; file < 8; ++file) {
        EXPECT_EQ(board.pieceAt(squareOf(file, 0)),
                  Piece::of(Color::White, backRank[file]));
        EXPECT_EQ(board.pieceAt(squareOf(file, 7)),
                  Piece::of(Color::Black, backRank[file]));
    }
}

TEST(StartPosition, Pawns)
{
    const Board board = Board::fromStartPos();
    for (int file = 0; file < 8; ++file) {
        EXPECT_EQ(board.pieceAt(squareOf(file, 1)),
                  Piece::of(Color::White, PieceType::Pawn));
        EXPECT_EQ(board.pieceAt(squareOf(file, 6)),
                  Piece::of(Color::Black, PieceType::Pawn));
    }
}

TEST(StartPosition, State)
{
    const Board board = Board::fromStartPos();
    EXPECT_EQ(board.sideToMove(), Color::White);
    EXPECT_EQ(board.castlingRights(), AllCastling);
    EXPECT_EQ(board.enPassantSquare(), SquareNone);
    EXPECT_EQ(board.halfmoveClock(), 0);
    EXPECT_EQ(board.fullmoveNumber(), 1);
}

} // namespace chess
