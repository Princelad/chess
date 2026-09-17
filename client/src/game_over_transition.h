#pragma once

#include <chess/net/messages.h>

#include <algorithm>

namespace chess::client {

// Delay before switching to the game-over screen after the deciding move, so
// the final move's animation and sound get to play out (9.7/9.8).
inline constexpr float kGameOverDelay = 0.55f;

// Minimum transition delay used even when move animation is disabled; keeps the
// final capture/checkmate sound audible.
inline constexpr float kGameOverDelayFloor = kGameOverDelay;

inline float gameOverDelaySec(bool animationEnabled, float animationDuration)
{
    return std::max(animationEnabled ? animationDuration : 0.f,
                    kGameOverDelayFloor);
}

// Arms a pending game-over transition: `arm()` stores the result/reason and
// starts the countdown; `tick(dt)` returns true exactly once (and disarms) once
// the delay elapses. Pure and unit-testable; screens only drive it from update.
class GameOverTransition {
public:
    GameOverTransition() = default;

    void arm(chess::net::GameResult result, chess::net::GameOverReason reason,
             float delaySec)
    {
        result_ = result;
        reason_ = reason;
        timer_ = delaySec;
        armed_ = true;
    }

    bool armed() const { return armed_; }

    // Returns true exactly once when the delay elapses; subsequent calls return
    // false until re-armed.
    bool tick(float dtSec)
    {
        if (!armed_) return false;
        timer_ -= dtSec;
        if (timer_ > 0.f) return false;
        armed_ = false;
        return true;
    }

    chess::net::GameResult result() const { return result_; }
    chess::net::GameOverReason reason() const { return reason_; }

private:
    bool armed_ = false;
    float timer_ = 0.f;
    chess::net::GameResult result_ = chess::net::GameResult::Abort;
    chess::net::GameOverReason reason_ = chess::net::GameOverReason::Disconnection;
};

} // namespace chess::client