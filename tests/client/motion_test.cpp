// 15.5 motion polish pure math: the waiting spinner's tick alphas and the
// board-entrance opacity ramp (both header-only, no RenderTarget needed).
#include "entrance.h"
#include "ui/spinner.h"

#include <gtest/gtest.h>

#include <cmath>

namespace chess::client {
namespace {

// Circular slot distance in [0,1): frac(a - b).
float CircularDist(float a, float b)
{
    float d = a - b;
    d -= std::floor(d);
    return d;
}

// --- waiting spinner ---------------------------------------------------------

TEST(Spinner, HeadTickIsFullAlphaAndTailFloorIsFaint)
{
    // Phase 0: tick 0 is the head at full burn, tick 4 sits opposite on
    // the wheel, tick 7 nearly wraps to 1.0 distance (phases advance in
    // 1/8 slots, so tick 7 rests just under the floor).
    EXPECT_FLOAT_EQ(ui::spinner::tickAlpha(0.f, 0), 1.f);
    EXPECT_FLOAT_EQ(ui::spinner::tickAlpha(0.f, 4), 0.575f);
    EXPECT_NEAR(ui::spinner::tickAlpha(0.f, 7), 0.15f + 0.85f * (0.5f - 0.5f * std::cos(7.f * 3.14159265f / 8.f)), 1e-6f);
}

TEST(Spinner, AlphaIsMonotonicInCircularDistance)
{
    // Brightness is a strictly decreasing function of a tick's circular
    // slot distance to the phase: that's what gives the wheel a clear
    // leading edge (smaller distance => brighter, no ties inside one run).
    for (float phase : {0.f, 0.25f, 0.5f, 0.37f, 0.99f, 3.5f}) {
        for (int i = 0; i < ui::spinner::TickCount; ++i) {
            for (int j = 0; j < ui::spinner::TickCount; ++j) {
                if (i == j) continue;
                const float di = CircularDist(phase,
                    static_cast<float>(i) / ui::spinner::TickCount);
                const float dj = CircularDist(phase,
                    static_cast<float>(j) / ui::spinner::TickCount);
                const float ai = ui::spinner::tickAlpha(phase, i);
                const float aj = ui::spinner::tickAlpha(phase, j);
                if (di < dj) EXPECT_GT(ai, aj);
                if (di > dj) EXPECT_LT(ai, aj);
            }
        }
    }
    // A tick sitting exactly on the phase burns at the head value.
    EXPECT_FLOAT_EQ(ui::spinner::tickAlpha(0.75f, 6), 1.f);
}

TEST(Spinner, UnwrappedPhasesStayInTheirSlot)
{
    // Seconds grow unboundedly; the phase math must amortize them (callers
    // pass phase = seconds / SweepSec; the function takes it from there).
    const float raw = ui::spinner::tickAlpha(1234.75f, 5);
    const float wrapped = ui::spinner::tickAlpha(0.75f, 5);
    EXPECT_NEAR(raw, wrapped, 1e-5f);
}

TEST(Spinner, HeadAdvancesWithThePhase)
{
    // The brightest tick moves forward one slot per 1/8 of a sweep —
    // that's the perceived rotation.
    const auto brightest = [](float phase) {
        float best = -1.f; int bestK = 0;
        for (int k = 0; k < ui::spinner::TickCount; ++k) {
            const float a = ui::spinner::tickAlpha(phase, k);
            if (a > best) { best = a; bestK = k; }
        }
        return bestK;
    };
    for (int slot = 0; slot < 3 * ui::spinner::TickCount; ++slot)
        EXPECT_EQ(brightest(slot / static_cast<float>(ui::spinner::TickCount)),
                  slot % ui::spinner::TickCount);
}

// --- board entrance ----------------------------------------------------------

TEST(Entrance, OpacityBoundaries)
{
    EXPECT_FLOAT_EQ(entranceOpacity(0.f), 0.f);
    EXPECT_FLOAT_EQ(entranceOpacity(-0.4f), 0.f);   // never negative-driven
    EXPECT_FLOAT_EQ(entranceOpacity(kEntranceSec), 1.f);
    EXPECT_FLOAT_EQ(entranceOpacity(10.f), 1.f);    // holds at full after
}

TEST(Entrance, MidRampIsSmoothstepHalf)
{
    // Half the window lands exactly at half opacity (smoothstep).
    EXPECT_FLOAT_EQ(entranceOpacity(kEntranceSec * 0.5f), 0.5f);
    // The ramp is monotonic and stays inside [0, 1].
    float prev = -1.f;
    for (int i = 0; i <= 8; ++i) {
        const float o = entranceOpacity(kEntranceSec * i / 8.f);
        EXPECT_GE(o, prev);
        EXPECT_LE(o, 1.f);
        prev = o;
    }
}

TEST(Entrance, WindowIsAShortNotableFade)
{
    // Short enough to feel like polish, long enough to be seen. Pinned so
    // tuning the value is a deliberate act.
    EXPECT_FLOAT_EQ(kEntranceSec, 0.35f);
}

} // namespace
} // namespace chess::client
