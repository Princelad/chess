#include "move_animator.h"

#include <chess/chess960.h>
#include <chess/movegen.h>
#include <chess/san.h>

#include <gtest/gtest.h>

namespace chess::client {
namespace {

TEST(MoveAnimator, EmptyByDefault)
{
    MoveAnimator anim;
    EXPECT_FALSE(anim.active());
    EXPECT_EQ(anim.progress(), 0.f);
    EXPECT_TRUE(anim.empty());
}

TEST(MoveAnimator, ZeroDurationCompletesInstantly)
{
    Board board = Board::fromStartPos();
    auto parsed = chess::san::fromSan(board, "e4");
    ASSERT_TRUE(parsed.has_value());
    MoveAnimator anim;
    anim.start(*parsed, board, 0.f);
    EXPECT_FALSE(anim.active());
    EXPECT_FLOAT_EQ(anim.progress(), 1.f);
}

TEST(MoveAnimator, AnimatesUntilDuration)
{
    Board board = Board::fromStartPos();
    auto parsed = chess::san::fromSan(board, "e4");
    ASSERT_TRUE(parsed.has_value());
    MoveAnimator anim;
    anim.start(*parsed, board, 0.3f);

    EXPECT_TRUE(anim.active());
    EXPECT_FLOAT_EQ(anim.progress(), 0.f);

    anim.update(0.15f);
    EXPECT_TRUE(anim.active());
    const float p1 = anim.progress();
    EXPECT_GT(p1, 0.f);
    EXPECT_LT(p1, 1.f);

    anim.update(0.15f);
    EXPECT_FALSE(anim.active());
    EXPECT_FLOAT_EQ(anim.progress(), 1.f);
}

TEST(MoveAnimator, ProgressClamped)
{
    Board board = Board::fromStartPos();
    auto parsed = chess::san::fromSan(board, "e4");
    ASSERT_TRUE(parsed.has_value());
    MoveAnimator anim;
    anim.start(*parsed, board, 0.3f);
    anim.update(1.0f);  // way past duration
    EXPECT_FLOAT_EQ(anim.progress(), 1.f);
}

TEST(MoveAnimator, SmoothstepMonotonic)
{
    Board board = Board::fromStartPos();
    auto parsed = chess::san::fromSan(board, "e4");
    ASSERT_TRUE(parsed.has_value());
    MoveAnimator anim;
    anim.start(*parsed, board, 0.3f);

    float prev = 0.f;
    for (int i = 1; i <= 3; ++i) {
        anim.update(0.1f);
        float p = anim.progress();
        EXPECT_GT(p, prev);
        prev = p;
    }
}

TEST(MoveAnimator, RestartResets)
{
    Board board = Board::fromStartPos();
    auto parsed = chess::san::fromSan(board, "e4");
    ASSERT_TRUE(parsed.has_value());
    MoveAnimator anim;
    anim.start(*parsed, board, 0.3f);
    anim.update(0.1f);
    EXPECT_TRUE(anim.active());

    anim.start(*parsed, board, 0.2f);
    EXPECT_TRUE(anim.active());
    EXPECT_LT(anim.progress(), 0.5f); // restarted mid-way
}

TEST(MoveAnimator, OneStepForNonCastle)
{
    Board board = Board::fromStartPos();
    auto parsed = chess::san::fromSan(board, "e4");
    ASSERT_TRUE(parsed.has_value());
    MoveAnimator anim;
    anim.start(*parsed, board, 0.3f);
    EXPECT_EQ(anim.steps().size(), 1u);
    EXPECT_EQ(anim.piece(), board.pieceAt(parsed->from));
}

TEST(MoveAnimator, CastlingProducesTwoSteps)
{
    Board b2 = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    auto parsed = chess::san::fromSan(b2, "O-O");
    ASSERT_TRUE(parsed.has_value());
    MoveAnimator anim;
    anim.start(*parsed, b2, 0.3f);
    EXPECT_EQ(anim.steps().size(), 2u);
}

TEST(MoveAnimator, StandardCastleRookSquaresUnchanged)
{
    Board board = *Board::fromFen(
        "r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    auto ks = chess::san::fromSan(board, "O-O");
    ASSERT_TRUE(ks.has_value());
    auto qs = chess::san::fromSan(board, "O-O-O");
    ASSERT_TRUE(qs.has_value());

    MoveAnimator kAnim;
    kAnim.start(*ks, board, 0.3f);
    ASSERT_EQ(kAnim.steps().size(), 2u);
    EXPECT_EQ(kAnim.steps()[1].from, chess::squareOf(chess::File::H, chess::Rank::R1)); // h1
    EXPECT_EQ(kAnim.steps()[1].to, chess::squareOf(chess::File::F, chess::Rank::R1));   // f1

    MoveAnimator qAnim;
    qAnim.start(*qs, board, 0.3f);
    ASSERT_EQ(qAnim.steps().size(), 2u);
    EXPECT_EQ(qAnim.steps()[1].from, chess::squareOf(chess::File::A, chess::Rank::R1)); // a1
    EXPECT_EQ(qAnim.steps()[1].to, chess::squareOf(chess::File::D, chess::Rank::R1));   // d1
}

// FRC sweep (12.1): for every start position, every legal castle animates
// the WING rook (the one flanking the king, wherever it sits — often an
// inner file), landing f-file beside a g-file king or d-file beside a
// c-file king. The old h-file/a-file assumptions are gone.
TEST(MoveAnimator, FrcCastleSweepAnimatesWingRook)
{
    for (int id = 0; id < chess::Chess960PositionCount; ++id) {
        auto fen = chess::chess960StartFen(id);
        ASSERT_TRUE(fen.has_value());
        auto boardOpt = Board::fromFen(*fen);
        ASSERT_TRUE(boardOpt.has_value());
        Board board = *boardOpt;

        const chess::Color mover = board.sideToMove();
        for (const auto& move : chess::generateLegalMoves(board)) {
            if (!move.isCastle())
                continue;
            MoveAnimator anim;
            anim.start(move, board, 0.3f);
            ASSERT_EQ(anim.steps().size(), 2u);

            const bool kingSide =
                static_cast<int>(chess::fileOf(move.to)) >= 6;
            EXPECT_EQ(static_cast<int>(chess::fileOf(move.to)),
                      kingSide ? 6 : 2);

            const auto& rookStep = anim.steps()[1];
            EXPECT_EQ(rookStep.piece.type, chess::PieceType::Rook);
            EXPECT_EQ(rookStep.piece.color, mover);
            EXPECT_EQ(rookStep.piece, board.pieceAt(rookStep.from))
                << "rook step must capture the board's own rook (" << *fen
                << ", castle to " << static_cast<int>(move.to) << ")";
            EXPECT_EQ(static_cast<int>(chess::fileOf(rookStep.to)),
                      kingSide ? 5 : 3);
        }
    }
}

// The king-at-landing-square FRC castle is a legal Move{from == to}: a king
// that STARTS on the g- or c-file casting (only reachable mid-game — from a
// full back rank the rook's landing square is always occupied). The rook
// step must still resolve from the wing, not from a degenerate king slide.
TEST(MoveAnimator, KingAtLandingSquareCastleStillSlidesRook)
{
    // White king started on g1, rook h1, f1 cleared: O-O is from==to.
    auto boardOpt = Board::fromFen("8/8/8/8/8/8/8/6KR w K - 0 1");
    ASSERT_TRUE(boardOpt.has_value());
    auto ks = chess::san::fromSan(*boardOpt, "O-O");
    ASSERT_TRUE(ks.has_value());
    EXPECT_EQ(ks->from, ks->to);
    EXPECT_TRUE(ks->isCastle());
    MoveAnimator kAnim;
    kAnim.start(*ks, *boardOpt, 0.3f);
    ASSERT_EQ(kAnim.steps().size(), 2u);
    EXPECT_EQ(kAnim.steps()[1].from,
              chess::squareOf(chess::File::H, chess::Rank::R1));
    EXPECT_EQ(kAnim.steps()[1].to,
              chess::squareOf(chess::File::F, chess::Rank::R1));
    EXPECT_EQ(kAnim.steps()[1].piece.type, chess::PieceType::Rook);

    // Black mirror started on c8, rook a8, d8 cleared: O-O-O is from==to.
    auto bOpt = Board::fromFen("r1k5/8/8/8/8/8/8/8 b q - 0 1");
    ASSERT_TRUE(bOpt.has_value());
    auto qs = chess::san::fromSan(*bOpt, "O-O-O");
    ASSERT_TRUE(qs.has_value());
    EXPECT_EQ(qs->from, qs->to);
    MoveAnimator qAnim;
    qAnim.start(*qs, *bOpt, 0.3f);
    ASSERT_EQ(qAnim.steps().size(), 2u);
    EXPECT_EQ(qAnim.steps()[1].from,
              chess::squareOf(chess::File::A, chess::Rank::R8));
    EXPECT_EQ(qAnim.steps()[1].to,
              chess::squareOf(chess::File::D, chess::Rank::R8));
}

// A king far from its landing square animates the rook's TRUE landing
// square (f/d), not the king-origin ±1 of the standard layout.
TEST(MoveAnimator, DistantKingCastleLandsRookOnFOrD)
{
    // King b1 castling king-side (rooks flank it): the rook lands on f1,
    // NOT next to the king's old file.
    auto boardOpt = Board::fromFen("8/8/8/8/8/8/8/1K5R w K - 0 1");
    ASSERT_TRUE(boardOpt.has_value());
    auto ks = chess::san::fromSan(*boardOpt, "O-O");
    ASSERT_TRUE(ks.has_value());
    MoveAnimator anim;
    anim.start(*ks, *boardOpt, 0.3f);
    ASSERT_EQ(anim.steps().size(), 2u);
    EXPECT_EQ(anim.steps()[1].to,
              chess::squareOf(chess::File::F, chess::Rank::R1));
    EXPECT_EQ(anim.steps()[1].from,
              chess::squareOf(chess::File::H, chess::Rank::R1));
}

TEST(MoveAnimator, StepPiecesAreCorrect)
{
    Board board = Board::fromStartPos();
    auto parsed = chess::san::fromSan(board, "e4");
    ASSERT_TRUE(parsed.has_value());
    MoveAnimator anim;
    anim.start(*parsed, board, 0.3f);
    EXPECT_EQ(anim.piece().type, chess::PieceType::Pawn);
}

TEST(Smoothstep, EdgeCases)
{
    EXPECT_FLOAT_EQ(MoveAnimator::smoothstep(0.f), 0.f);
    EXPECT_FLOAT_EQ(MoveAnimator::smoothstep(1.f), 1.f);
    EXPECT_GT(MoveAnimator::smoothstep(0.25f), 0.f);
    EXPECT_LT(MoveAnimator::smoothstep(0.25f), 0.25f);
    EXPECT_GT(MoveAnimator::smoothstep(0.75f), 0.75f);
    EXPECT_LT(MoveAnimator::smoothstep(0.75f), 1.f);
}

} // namespace
} // namespace chess::client