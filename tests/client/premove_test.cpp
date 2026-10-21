// Unit tests for the pure premove resolver (11.4): queued raw from/to
// squares resolve against the live position at flush time — legality
// decides everything else (en passant, castling, promotions, drops).
#include <gtest/gtest.h>

#include <chess/board.h>
#include <chess/move.h>
#include <chess/movegen.h>

#include "premove.h"

namespace chess {
namespace client {
namespace {

std::optional<Move> resolve(const char* fen, int fromFile, int fromRank,
                            int toFile, int toRank)
{
    auto board = Board::fromFen(fen);
    if (!board.has_value()) {
        ADD_FAILURE() << "bad test FEN: " << fen;
        return std::nullopt;
    }
    return resolvePremove(*board,
                          Premove{fromFile, fromRank, toFile, toRank});
}

TEST(PremoveTest, QuietPawnMoveResolves)
{
    auto board = Board::fromStartPos();
    auto move = resolvePremove(board, Premove{4, 1, 4, 3});
    ASSERT_TRUE(move.has_value());
    EXPECT_TRUE(move->isDoublePush());
}

TEST(PremoveTest, CaptureResolvesWhenTargetOccupied)
{
    // Position after 1.e4 d5: the queued exd5 becomes a plain capture.
    auto move = resolve(
        "rnbqkbnr/pppppppp/8/3p4/4P3/8/PPPPPPPP/RNBQKBNR w kq - 0 2",
        4, 3, 3, 4);
    ASSERT_TRUE(move.has_value());
    EXPECT_TRUE(move->isCapture());
    EXPECT_FALSE(move->isPromotion());
}

TEST(PremoveTest, CaptureFrameOntoEmptySquareDies)
{
    // White pawn premoved exd5 while d5 stayed empty: no matching legal
    // move, the premove is dropped at flush.
    auto move = resolve(
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPPPPPP/RNBQKBNR w kq - 0 2",
        4, 3, 3, 4);
    EXPECT_FALSE(move.has_value());
}

TEST(PremoveTest, LandingOnOwnPieceDies)
{
    // King premoved to d1, where the queen already stands.
    auto move = resolve(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        4, 0, 3, 0);
    EXPECT_FALSE(move.has_value());
}

TEST(PremoveTest, PinnedPiecePremovesAllDie)
{
    // The knight on e4 is the pawn-shield of the e1 king against the e8
    // rook: every knight move leaves the file, so none is legal.
    auto move = resolve(
        "k3r3/8/8/8/4n3/8/8/4K3 w - - 0 1",
        4, 3, 6, 4);
    EXPECT_FALSE(move.has_value());
}

TEST(PremoveTest, AlongThePinLineResolves)
{
    // A queen on the same file slides it (alive, backed by the knight).
    auto move = resolve(
        "k3r3/8/8/8/4n3/8/6K1/4Q3 w - - 0 1",
        4, 0, 4, 1);
    ASSERT_TRUE(move.has_value());
    EXPECT_EQ(move->from, squareOf(File::E, Rank::R1));
    EXPECT_EQ(move->to, squareOf(File::E, Rank::R2));
}

TEST(PremoveTest, CastlingFrameResolves)
{
    auto move = resolve(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQK2R w KQkq - 0 1",
        4, 0, 6, 0);
    ASSERT_TRUE(move.has_value());
    EXPECT_TRUE(move->isCastle());
}

TEST(PremoveTest, PromotionAutoQueens)
{
    // Pawn a7 rising to a8: the flush part promotes to the queen.
    auto move = resolve(
        "8/P1k5/8/8/8/8/6K1/8 w - - 0 1",
        0, 6, 0, 7);
    ASSERT_TRUE(move.has_value());
    EXPECT_TRUE(move->isPromotion());
    EXPECT_EQ(move->promotion, PieceType::Queen);
}

TEST(PremoveTest, SameSquareDiesImmediately)
{
    auto board = Board::fromStartPos();
    EXPECT_FALSE(
        resolvePremove(board, Premove{4, 0, 4, 0}).has_value());
}

} // namespace
} // namespace client
} // namespace chess
