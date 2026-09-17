#include "game_over_transition.h"

#include <gtest/gtest.h>

namespace chess::client {
namespace {

TEST(GameOverDelay, UsesAnimationDurationWhenAboveFloor)
{
    EXPECT_EQ(gameOverDelaySec(true, 0.7f), 0.7f);
    EXPECT_EQ(gameOverDelaySec(true, 1.5f), 1.5f);
}

TEST(GameOverDelay, FloorsAllDurationsAndWhenDisabled)
{
    EXPECT_EQ(gameOverDelaySec(true, 0.3f), kGameOverDelay);
    EXPECT_EQ(gameOverDelaySec(true, 0.5f), kGameOverDelay);
    EXPECT_EQ(gameOverDelaySec(false, 0.3f), kGameOverDelay);
    EXPECT_EQ(gameOverDelaySec(false, 0.0f), kGameOverDelay);
}

TEST(GameOverTransition, StartsDisarmed)
{
    GameOverTransition t;
    EXPECT_FALSE(t.armed());
    EXPECT_FALSE(t.tick(1.f));
}

TEST(GameOverTransition, FiresOnceAfterDelay)
{
    GameOverTransition t;
    t.arm(chess::net::GameResult::WhiteWins, chess::net::GameOverReason::Checkmate, 0.5f);

    EXPECT_TRUE(t.armed());
    EXPECT_FALSE(t.tick(0.2f));
    EXPECT_TRUE(t.armed());

    EXPECT_TRUE(t.tick(0.4f)); // 0.2 + 0.4 >= 0.5
    EXPECT_FALSE(t.armed());
    EXPECT_FALSE(t.tick(1.f)); // already fired

    EXPECT_EQ(t.result(), chess::net::GameResult::WhiteWins);
    EXPECT_EQ(t.reason(), chess::net::GameOverReason::Checkmate);
}

TEST(GameOverTransition, FiresExactlyAtElapsed)
{
    GameOverTransition t;
    t.arm(chess::net::GameResult::Draw, chess::net::GameOverReason::FiftyMove, 0.25f);
    EXPECT_TRUE(t.tick(0.25f));
    EXPECT_FALSE(t.armed());
}

TEST(GameOverTransition, ArmTwiceRestartsTimer)
{
    GameOverTransition t;
    t.arm(chess::net::GameResult::WhiteWins, chess::net::GameOverReason::Checkmate, 0.5f);
    EXPECT_FALSE(t.tick(0.4f));
    t.arm(chess::net::GameResult::BlackWins, chess::net::GameOverReason::Resignation, 1.f);

    EXPECT_TRUE(t.armed());
    EXPECT_FALSE(t.tick(0.4f)); // 0.6 left; old 0.5 deadline no longer applies
    EXPECT_FALSE(t.tick(0.2f)); // 0.4 left
    EXPECT_TRUE(t.tick(0.5f));  // fires now
    EXPECT_FALSE(t.armed());
    EXPECT_EQ(t.result(), chess::net::GameResult::BlackWins);
    EXPECT_EQ(t.reason(), chess::net::GameOverReason::Resignation);
}

} // namespace
} // namespace chess::client