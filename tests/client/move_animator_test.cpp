#include "move_animator.h"

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