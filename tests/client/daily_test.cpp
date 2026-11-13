#include "daily.h"

#include <gtest/gtest.h>

#include <chrono>

namespace daily = chess::client::daily;

TEST(Daily, DayNumberKnownAnchors)
{
    // Anchors verified against an independent ordinal-day calculation.
    EXPECT_EQ(daily::dayNumber(1970, 1, 1), 0);
    EXPECT_EQ(daily::dayNumber(1970, 1, 2), 1);
    EXPECT_EQ(daily::dayNumber(1970, 2, 28), 58);
    EXPECT_EQ(daily::dayNumber(1970, 3, 1), 59); // 1970 is NOT a leap year
    EXPECT_EQ(daily::dayNumber(1972, 2, 29), 789); // 1972 IS a leap year
    EXPECT_EQ(daily::dayNumber(1999, 12, 31), 10956);
    EXPECT_EQ(daily::dayNumber(2000, 1, 1), 10957);
    EXPECT_EQ(daily::dayNumber(2000, 2, 29), 11016); // 2000 IS a leap year
    EXPECT_EQ(daily::dayNumber(1900, 3, 1), -25508); // 1900 was NOT leap
    EXPECT_EQ(daily::dayNumber(2024, 2, 29), 19782);
    EXPECT_EQ(daily::dayNumber(2026, 11, 12), 20769);
    EXPECT_EQ(daily::dayNumber(2026, 12, 31), 20818);
    EXPECT_EQ(daily::dayNumber(2100, 3, 1), 47541); // 2100 is NOT leap
}

TEST(Daily, DayNumberAdvancesByOne)
{
    // Consecutive calendar days are consecutive day numbers, including the
    // February/March boundary in a leap and a non-leap year.
    EXPECT_EQ(daily::dayNumber(2024, 3, 1) - daily::dayNumber(2024, 2, 28), 2);
    EXPECT_EQ(daily::dayNumber(2025, 3, 1) - daily::dayNumber(2025, 2, 28), 1);
    EXPECT_EQ(daily::dayNumber(2026, 12, 31) - daily::dayNumber(2027, 1, 1),
              -1);
}

TEST(Daily, DailyIndexIsDeterministicPerDay)
{
    // Same day, same pick; the pick is a valid index.
    const int d = daily::dayNumber(2026, 11, 12);
    EXPECT_EQ(daily::dailyIndex(d, 23), daily::dailyIndex(d, 23));
    EXPECT_GE(daily::dailyIndex(d, 23), 0);
    EXPECT_LT(daily::dailyIndex(d, 23), 23);

    // The next day picks again within bounds (usually differently).
    const int next = daily::dailyIndex(d + 1, 23);
    EXPECT_GE(next, 0);
    EXPECT_LT(next, 23);

    // Degenerate pool sizes stay sane.
    EXPECT_EQ(daily::dailyIndex(d, 1), 0);
    EXPECT_EQ(daily::dailyIndex(d, 0), 0);
    EXPECT_EQ(daily::dailyIndex(d, -5), 0);
}

TEST(Daily, SolveStreakChainRules)
{
    const int today = daily::dayNumber(2026, 11, 12);
    const int yesterday = today - 1;
    const int threeDaysAgo = today - 3;

    // First ever solve starts the chain at 1.
    EXPECT_EQ(daily::solveStreak(-1, 0, today), 1);
    // Consecutive day extends.
    EXPECT_EQ(daily::solveStreak(yesterday, 4, today), 5);
    // Solving again the same day changes nothing.
    EXPECT_EQ(daily::solveStreak(today, 5, today), 5);
    // A skipped day resets (even a long-dormant chain).
    EXPECT_EQ(daily::solveStreak(threeDaysAgo, 9, today), 1);
}

TEST(Daily, DisplayStreakDiesAfterAGap)
{
    const int today = daily::dayNumber(2026, 11, 12);

    // Solved today or yesterday: the stored chain is alive.
    EXPECT_EQ(daily::displayStreak(today, 5, today), 5);
    EXPECT_EQ(daily::displayStreak(today - 1, 5, today), 5);
    // Any older last solve displays 0 until a fresh solve restarts it.
    EXPECT_EQ(daily::displayStreak(today - 2, 5, today), 0);
    EXPECT_EQ(daily::displayStreak(-1, 5, today), 0);

    // The never-solved default chain.
    EXPECT_EQ(daily::displayStreak(-1, 0, today), 0);
}

TEST(Daily, SolvedTodayPredicate)
{
    const int today = daily::dayNumber(2026, 11, 12);
    EXPECT_TRUE(daily::solvedToday(today, today));
    EXPECT_FALSE(daily::solvedToday(today - 1, today));
    EXPECT_FALSE(daily::solvedToday(-1, today));
}
