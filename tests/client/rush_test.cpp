#include "rush.h"

#include <gtest/gtest.h>

namespace rush = chess::client::rush;
using rush::Mode;
using rush::Step;
using rush::shuffledOrder;

TEST(Rush, ModeConstants)
{
    EXPECT_EQ(secondsFor(Mode::ThreeMin), 180);
    EXPECT_EQ(secondsFor(Mode::FiveMin), 300);
    EXPECT_EQ(secondsFor(Mode::Survival), 0);
    EXPECT_TRUE(timed(Mode::ThreeMin));
    EXPECT_TRUE(timed(Mode::FiveMin));
    EXPECT_FALSE(timed(Mode::Survival));

    // 17.1's exact config key names.
    EXPECT_EQ(bestKey(Mode::ThreeMin), "puzzle.rush_3m_best");
    EXPECT_EQ(bestKey(Mode::FiveMin), "puzzle.rush_5m_best");
    EXPECT_EQ(bestKey(Mode::Survival), "puzzle.rush_survival_best");
}

TEST(Rush, ShuffledOrderIsDeterministicAndComplete)
{
    auto a = shuffledOrder(42, 23);
    auto b = shuffledOrder(42, 23);
    ASSERT_EQ(a.size(), 23u);
    EXPECT_EQ(a, b); // same seed = same feed

    // A permutation of 0..count-1: every index exactly once.
    std::vector<bool> seen(23, false);
    for (int v : a) {
        ASSERT_GE(v, 0);
        ASSERT_LT(v, 23);
        EXPECT_FALSE(seen[v]) << "duplicate " << v;
        seen[v] = true;
    }

    // A different seed really shuffles differently.
    auto c = shuffledOrder(43, 23);
    EXPECT_NE(a, c);

    // Degenerate counts stay sane.
    EXPECT_TRUE(shuffledOrder(7, 0).empty());
    auto one = shuffledOrder(7, 1);
    ASSERT_EQ(one.size(), 1u);
    EXPECT_EQ(one[0], 0);
}

TEST(Rush, TimedRunClockOnly)
{
    rush::Run run = rush::Run::start(Mode::ThreeMin, 0);
    EXPECT_FLOAT_EQ(run.remainingSec, 180.f);

    // Fails advance freely; the clock is the constraint.
    EXPECT_FALSE(run.step(Step::Failed));
    EXPECT_FALSE(run.over);
    EXPECT_EQ(run.strikes, 0);
    EXPECT_EQ(run.streak, 0);

    EXPECT_FALSE(run.step(Step::Solved)); // solved never ends the run
    EXPECT_EQ(run.score, 1);
    EXPECT_EQ(run.streak, 1);

    run.tick(180.f);
    EXPECT_TRUE(run.over);
    EXPECT_FLOAT_EQ(run.remainingSec, 0.f);

    // Over runs refuse further steps.
    EXPECT_FALSE(run.step(Step::Solved));
    EXPECT_EQ(run.score, 1);
}

TEST(Rush, SurvivalStrikesEndTheRun)
{
    rush::Run run = rush::Run::start(Mode::Survival, 0);
    EXPECT_FALSE(run.over);
    EXPECT_FLOAT_EQ(run.remainingSec, 0.f);

    run.tick(100.f); // untimed: no-op
    EXPECT_FALSE(run.over);

    EXPECT_FALSE(run.step(Step::Failed)); // strike 1
    EXPECT_FALSE(run.step(Step::Skipped)); // strike 2
    EXPECT_EQ(run.strikes, 2);
    EXPECT_TRUE(run.step(Step::Failed)); // strike 3 = run over
    EXPECT_TRUE(run.over);
    EXPECT_EQ(run.score, 0);
}

TEST(Rush, BeatYourBestBannerTracksLiveScore)
{
    rush::Run run = rush::Run::start(Mode::FiveMin, 5);
    EXPECT_EQ(run.best, 5);
    EXPECT_FALSE(run.beatBest);

    run.step(Step::Solved);
    run.step(Step::Solved);
    run.step(Step::Solved);
    run.step(Step::Solved);
    EXPECT_FALSE(run.beatBest); // 4 == not past 5 yet
    EXPECT_EQ(run.best, 5);

    run.step(Step::Solved);
    EXPECT_FALSE(run.beatBest); // 5 == 5 is a tie, not a beat

    run.step(Step::Solved);
    EXPECT_TRUE(run.beatBest);
    EXPECT_EQ(run.best, 6);
    EXPECT_EQ(run.streak, 6);

    // A fail resets the streak but not the score or the banner.
    run.step(Step::Failed);
    EXPECT_EQ(run.streak, 0);
    EXPECT_EQ(run.score, 6);
    EXPECT_TRUE(run.beatBest);
}
