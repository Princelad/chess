#include <chess/fen.h>

#include <gtest/gtest.h>

namespace chess {

namespace {

bool sameBoard(const Board& a, const Board& b)
{
    if (a.sideToMove() != b.sideToMove()) return false;
    if (a.castlingRights() != b.castlingRights()) return false;
    if (a.enPassantSquare() != b.enPassantSquare()) return false;
    if (a.halfmoveClock() != b.halfmoveClock()) return false;
    if (a.fullmoveNumber() != b.fullmoveNumber()) return false;
    for (int sq = 0; sq < BoardSize; ++sq) {
        if (offBoard(sq)) {
            continue;
        }
        if (a.pieceAt(sq) != b.pieceAt(sq)) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST(Fen, StartPosition)
{
    auto board = fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    ASSERT_TRUE(board.has_value());
    EXPECT_TRUE(sameBoard(*board, Board::fromStartPos()));
}

TEST(Fen, BoardFromFenMatchesFreeFunction)
{
    auto viaMember = Board::fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    auto viaFree = fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    ASSERT_TRUE(viaMember.has_value());
    ASSERT_TRUE(viaFree.has_value());
    EXPECT_TRUE(sameBoard(*viaMember, *viaFree));
}

TEST(Fen, KiwipetePosition)
{
    auto board = fromFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    ASSERT_TRUE(board.has_value());
    EXPECT_EQ(board->sideToMove(), Color::White);
    EXPECT_EQ(board->castlingRights(), AllCastling);
    EXPECT_EQ(board->enPassantSquare(), SquareNone);
    EXPECT_EQ(board->pieceAt(squareOf(File::A, Rank::R8)), Piece::of(Color::Black, PieceType::Rook));
    EXPECT_EQ(board->pieceAt(squareOf(File::E, Rank::R8)), Piece::of(Color::Black, PieceType::King));
    EXPECT_EQ(board->pieceAt(squareOf(File::B, Rank::R6)), Piece::of(Color::Black, PieceType::Knight));
    EXPECT_EQ(board->pieceAt(squareOf(File::D, Rank::R5)), Piece::of(Color::White, PieceType::Pawn));
    EXPECT_EQ(board->pieceAt(squareOf(File::E, Rank::R5)), Piece::of(Color::White, PieceType::Knight));
}

TEST(Fen, EnPassantAndClocks)
{
    auto board = fromFen("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq e6 0 4");
    ASSERT_TRUE(board.has_value());
    EXPECT_EQ(board->sideToMove(), Color::White);
    EXPECT_EQ(board->castlingRights(), AllCastling);
    EXPECT_EQ(board->enPassantSquare(), squareOf(File::E, Rank::R6));
    EXPECT_EQ(board->halfmoveClock(), 0);
    EXPECT_EQ(board->fullmoveNumber(), 4);
    EXPECT_EQ(board->pieceAt(squareOf(File::C, Rank::R6)), Piece::of(Color::Black, PieceType::Knight));
    EXPECT_EQ(board->pieceAt(squareOf(File::C, Rank::R4)), Piece::of(Color::White, PieceType::Bishop));
}

TEST(Fen, BlackToMoveLimitedCastlingEmptyBoard)
{
    auto board = fromFen("8/8/8/8/8/8/8/8 b Qq - 12 40");
    ASSERT_TRUE(board.has_value());
    EXPECT_EQ(board->sideToMove(), Color::Black);
    EXPECT_EQ(board->castlingRights(), WhiteQueenSide | BlackQueenSide);
    EXPECT_EQ(board->halfmoveClock(), 12);
    EXPECT_EQ(board->fullmoveNumber(), 40);
    for (int sq = 0; sq < BoardSize; ++sq) {
        if (!offBoard(sq)) {
            EXPECT_TRUE(board->isEmpty(sq));
        }
    }
}

TEST(Fen, RejectsMalformedStrings)
{
    const char* invalid[] = {
        "",
        "w",
        "8/8/8/8/8/8/8/8",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 ",
        " rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR  w KQkq - 0 1",
        "9/8/8/8/8/8/8/8 w KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNRR w KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR/ w KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkqX - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KKQ - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq e9 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq e2 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - abc 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - -1 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0",
    };
    for (const char* fen : invalid) {
        EXPECT_FALSE(fromFen(fen).has_value()) << "expected rejection: " << fen;
    }
}

} // namespace chess
