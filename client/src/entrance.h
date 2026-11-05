#pragma once

#include <algorithm>
#include <cmath>

namespace chess::client {

// 15.5 board entrance: on a freshly opened game screen the piece layer
// fades in over kEntranceSec. Opt-in via config `animation.entrance`
// (default off, config only — no Settings row). Rebuild paths (resume,
// takeback, rematch-adjacent setGame calls) never re-arm it: the timers
// live in the screens and only the constructors start them.
inline constexpr float kEntranceSec = 0.35f;

// Pure: piece-layer opacity at `elapsed` seconds into the entrance.
// Smoothstep ramp — 0 before it starts, 1 from kEntranceSec onwards.
inline float entranceOpacity(float elapsed)
{
    if (elapsed <= 0.f) return 0.f;
    const float t = std::min(elapsed, kEntranceSec) / kEntranceSec;
    return t * t * (3.f - 2.f * t);
}

} // namespace chess::client
