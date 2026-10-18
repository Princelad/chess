#include <chess/db/glicko2.h>

#include <gtest/gtest.h>

namespace chess::db {
namespace {

// The worked example from Glickman's paper (section 5, or
// http://www.glicko.net/glicko/glicko2.pdf §5):
// player 1500/200/0.06 plays three opponents in one rating period:
//   1400/30 → win, 1550/100 → loss, 1700/300 → loss
// Published results: r' = 1464.06, RD' = 151.52, volatility ≈ 0.05999.
TEST(Glicko2, WorkedExampleFromPaper)
{
    const Glicko2 player {1500.0, 200.0, 0.06};
    const auto result = applyGames(player, {
        {1400.0, 30.0, 1.0},
        {1550.0, 100.0, 0.0},
        {1700.0, 300.0, 0.0},
    });
    EXPECT_NEAR(result.rating, 1464.06, 0.05);
    EXPECT_NEAR(result.deviation, 151.52, 0.1);
    EXPECT_NEAR(result.volatility, 0.05999, 0.001);
}

// A win must raise the rating, a loss lower it, deviations shrink.
TEST(Glicko2, WinsRaiseAndLossesLower)
{
    const Glicko2 base {1500.0, 350.0, 0.06};
    const Glicko2 opponent {1500.0, 350.0, 0.06};

    const auto winner = applyGames(base, {{opponent.rating, opponent.deviation, 1.0}});
    const auto loser = applyGames(base, {{opponent.rating, opponent.deviation, 0.0}});

    EXPECT_GT(winner.rating, 1500.0);
    EXPECT_LT(loser.rating, 1500.0);
    EXPECT_NEAR(winner.rating + loser.rating, 3000.0, 0.01); // symmetric
    EXPECT_LT(winner.deviation, 350.0);
    EXPECT_LT(loser.deviation, 350.0);
    EXPECT_NEAR(winner.volatility, loser.volatility, 1e-9);
}

// A draw between equals keeps the rating at 1500.
TEST(Glicko2, DrawBetweenEqualsShiftsNothing)
{
    const Glicko2 base {1500.0, 350.0, 0.06};
    const auto result = applyGames(base, {{1500.0, 350.0, 0.5}});
    EXPECT_NEAR(result.rating, 1500.0, 0.01);
    EXPECT_LT(result.deviation, 350.0);
}

// Strong opponent, surprising win ⇒ larger rating gain.
TEST(Glicko2, UpsetWeighsMoreThanExpectedWin)
{
    const Glicko2 base {1500.0, 200.0, 0.06};
    const auto expected = applyGames(base, {{1500.0, 200.0, 1.0}});
    const auto upset = applyGames(base, {{1700.0, 100.0, 1.0}});
    EXPECT_GT(upset.rating, expected.rating);
}

// Without games in the period, only the deviation grows (capped at 350).
TEST(Glicko2, IdlePeriodGrowsDeviationOnly)
{
    const Glicko2 afterGames {1500.0, 100.0, 0.06};
    const auto idle = applyGames(afterGames, {});
    EXPECT_NEAR(idle.rating, 1500.0, 1e-9);
    EXPECT_GT(idle.deviation, 100.0);
    EXPECT_LE(idle.deviation, 350.0 + 1e-9);
    EXPECT_NEAR(idle.volatility, 0.06, 1e-9);
}

TEST(Glicko2, IdleGrowthSaturatesAtDefaultDeviation)
{
    const Glicko2 fresh {1500.0, 400.0, 0.06}; // already above the cap
    const auto idle = applyGames(fresh, {});
    EXPECT_NEAR(idle.deviation, 350.0, 1e-9);
}

} // namespace
} // namespace chess::db
