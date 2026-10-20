// Unit tests for the 11.1 chess clock math (server/src/clock.h). Pure math
// over injected timestamps — no sockets, no real time.
#include "clock.h"

#include <chess/net/messages.h>

#include <gtest/gtest.h>

namespace chess::server {
namespace {

constexpr std::int64_t operator""_ms(unsigned long long ms)
{
    return static_cast<std::int64_t>(ms);
}

// brings net parse into the local shorthand
using chess::net::parseTimeControl;

TEST(MatchClock, UnlimitedIsInactive)
{
    MatchClock clock;
    EXPECT_FALSE(clock.active());
    clock.start(parseTimeControl(""), Color::White, 0_ms);
    EXPECT_FALSE(clock.active());

    // Passive everywhere: no flags, no ticking.
    EXPECT_EQ(clock.checkFlag(60'000_ms), std::nullopt);
    std::int64_t moverRemaining = 123'456_ms; // untouched by a no-op move
    clock.completedMove(Color::White, 1'000_ms, moverRemaining);
    EXPECT_EQ(moverRemaining, 123'456_ms);
    EXPECT_EQ(clock.remainingMs(Color::Black, 1'000_ms), 0_ms);
}

TEST(MatchClock, BaseTimeChargedOnFirstMove)
{
    MatchClock clock;
    clock.start(parseTimeControl("5+0"), Color::White, 0_ms);
    EXPECT_TRUE(clock.active());
    EXPECT_EQ(clock.onMove(), Color::White);

    // White answers after 10 s: button moves to Black at the same moment
    // and White never drops below base minus elapsed.
    std::int64_t moverRemaining = 0;
    clock.completedMove(Color::White, 10'000_ms, moverRemaining);
    EXPECT_EQ(moverRemaining, 290'000_ms);
    EXPECT_EQ(clock.onMove(), Color::Black);
    EXPECT_EQ(clock.remainingMs(Color::White, 10'000_ms), 290'000_ms);
}

TEST(MatchClock, RemainingTicksForTheSideOnTheMove)
{
    MatchClock clock;
    clock.start(parseTimeControl("5+0"), Color::White, 0_ms);

    // Black still shows base time while White thinks; White ticks down.
    EXPECT_EQ(clock.remainingMs(Color::Black, 20'000_ms), 300'000_ms);
    EXPECT_EQ(clock.remainingMs(Color::White, 20'000_ms), 280'000_ms);
}

TEST(MatchClock, FischerIncrementAppliedPerMove)
{
    MatchClock clock;
    clock.start(parseTimeControl("3+2"), Color::White, 0_ms);

    std::int64_t moverRemaining = 0;
    clock.completedMove(Color::White, 1'500_ms, moverRemaining);
    // 3:00 - 1.5s + 2s increment = 3:00.5 (can exceed base as usual).
    EXPECT_EQ(moverRemaining, 180'500_ms);

    clock.completedMove(Color::Black, 10'000_ms, moverRemaining);
    // Black's own turn started the instant White's move landed (1.5 s), so
    // the charge is 8.5 s: 3:00 - 8.5 s + 2 s increment = 2:53.5.
    EXPECT_EQ(moverRemaining, 173'500_ms);
    EXPECT_EQ(clock.onMove(), Color::White);
}

TEST(MatchClock, FlagFallDetection)
{
    MatchClock clock;
    clock.start(parseTimeControl("1+0"), Color::White, 0_ms);

    EXPECT_EQ(clock.checkFlag(59'999_ms), std::nullopt);
    auto flagged = clock.checkFlag(60'000_ms);
    ASSERT_TRUE(flagged.has_value());
    EXPECT_EQ(flagged, Color::White);

    // The flagged color is the mover; the other side is untouched.
    EXPECT_EQ(clock.remainingMs(Color::Black, 60'000_ms), 60'000_ms);
}

TEST(MatchClock, IncrementSavesFromTheFlag)
{
    MatchClock clock;
    clock.start(parseTimeControl("1+30"), Color::White, 0);

    // White spends 40 of the 60 base seconds; the +30 s increment rescues
    // the near-flag: 20 s left, plus 30 s = 50 s fresh for White.
    std::int64_t moverRemaining = 0;
    clock.completedMove(Color::White, 40'000_ms, moverRemaining);
    EXPECT_EQ(moverRemaining, 50'000_ms);
    EXPECT_EQ(clock.remainingMs(Color::White, 60'000_ms), 50'000_ms);

    // Meanwhile Black still burns his own untouched base: 60 s into his
    // first turn is the flag.
    EXPECT_EQ(clock.checkFlag(99'999_ms), std::nullopt);
    auto flagged = clock.checkFlag(100'000_ms);
    ASSERT_TRUE(flagged.has_value());
    EXPECT_EQ(flagged, Color::Black);
}

TEST(MatchClock, ChargesClampAtZero)
{
    MatchClock clock;
    clock.start(parseTimeControl("1+0"), Color::White, 0_ms);

    // A mover overruns massively: never negative.
    std::int64_t moverRemaining = 0;
    clock.completedMove(Color::White, 120'000_ms, moverRemaining);
    EXPECT_EQ(moverRemaining, 0_ms); // 0 + 0 increment for "1+0"
    EXPECT_EQ(clock.remainingMs(Color::White, 120'000_ms), 0_ms);
}

} // namespace
} // namespace chess::server
