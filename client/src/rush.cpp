#include "rush.h"

#include <utility>

namespace chess::client::rush {

int secondsFor(Mode mode)
{
    switch (mode) {
        case Mode::ThreeMin: return 180;
        case Mode::FiveMin: return 300;
        case Mode::Survival: return 0;
    }
    return 0;
}

bool timed(Mode mode)
{
    return mode != Mode::Survival;
}

std::string bestKey(Mode mode)
{
    switch (mode) {
        case Mode::ThreeMin: return "puzzle.rush_3m_best";
        case Mode::FiveMin: return "puzzle.rush_5m_best";
        case Mode::Survival: return "puzzle.rush_survival_best";
    }
    return {};
}

const char* label(Mode mode)
{
    switch (mode) {
        case Mode::ThreeMin: return "3 min";
        case Mode::FiveMin: return "5 min";
        case Mode::Survival: return "Survival";
    }
    return "";
}

std::vector<int> shuffledOrder(std::uint64_t seed, int count)
{
    std::vector<int> order(count);
    for (int i = 0; i < count; ++i)
        order[i] = i;

    // splitmix64 stream: every call advances deterministically from seed.
    std::uint64_t state = seed;
    auto next = [&state]() {
        state += 0x9E3779B97F4A7C15ull;
        std::uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    };

    for (int i = count - 1; i > 0; --i) {
        const std::uint64_t r = next();
        const int j = static_cast<int>(r % static_cast<std::uint64_t>(i + 1));
        std::swap(order[i], order[j]);
    }
    return order;
}

Run Run::start(Mode mode, int previousBest)
{
    Run run;
    run.mode = mode;
    run.best = previousBest;
    if (timed(mode))
        run.remainingSec = static_cast<float>(secondsFor(mode));
    return run;
}

void Run::tick(float dtSec)
{
    if (!timed(mode) || over)
        return;
    remainingSec -= dtSec;
    if (remainingSec <= 0.f) {
        remainingSec = 0.f;
        over = true;
    }
}

bool Run::step(Step what)
{
    if (over)
        return false;

    bool justEnded = false;
    switch (what) {
        case Step::Solved:
            ++score;
            ++streak;
            if (score > best) {
                best = score;
                beatBest = true;
            }
            break;
        case Step::Failed:
        case Step::Skipped:
            streak = 0;
            if (!timed(mode)) {
                ++strikes;
                if (strikes >= kSurvivalStrikes)
                    justEnded = true;
            }
            break;
    }
    if (justEnded) {
        over = true;
        return true;
    }
    return false;
}

} // namespace chess::client::rush
