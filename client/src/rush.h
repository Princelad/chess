#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace chess::client::rush {

// ── Puzzle Rush & Survival (17.1) ───────────────────────────────────────────
// Pure run state for the timed puzzle marathons; the screen drives it from
// the 12.2 puzzle flow (solverTurn guard, streak discipline). No I/O here —
// config persistence is the screen's job.

enum class Mode { ThreeMin, FiveMin, Survival };

// Clock lengths in seconds; Survival is untimed (0).
int secondsFor(Mode mode);
bool timed(Mode mode);

// Config key holding the mode's best score (17.1's exact key names).
std::string bestKey(Mode mode);

// Display label for the mode.
const char* label(Mode mode);

// Deterministic feed order for one run: Fisher-Yates over 0..count-1 driven
// by a splitmix64 stream seeded with `seed` (the screen stores the seed in
// config so a session's order is reproducible).
std::vector<int> shuffledOrder(std::uint64_t seed, int count);

// One attempt's outcome as the puzzle flow reports it.
enum class Step { Solved, Failed, Skipped };

struct Run {
    Mode mode = Mode::ThreeMin;
    float remainingSec = 0.f; // timed modes only
    int score = 0;            // puzzles solved this run
    int streak = 0;
    int best = 0;             // previous best score, for the banner
    int strikes = 0;          // Survival only (3 strikes end the run)
    bool over = false;
    bool beatBest = false;    // live "new best" banner state

    static Run start(Mode mode, int previousBest);

    // Clock countdown for timed modes; flips `over` at exactly 0.
    void tick(float dtSec);

    // Advance one puzzle; returns true when the run just ended.
    // Timed modes: fails/skips advance freely (the clock is the constraint).
    // Survival: fails AND skips cost a strike; the 3rd strike ends the run.
    bool step(Step what);

    // Survival's strike budget; 0 in timed modes (strikes never count).
    static constexpr int kSurvivalStrikes = 3;
};

} // namespace chess::client::rush
