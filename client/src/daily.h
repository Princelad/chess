#pragma once

#include <cstdint>

namespace chess::client::daily {

// ── Daily puzzle + streaks (17.2) ───────────────────────────────────────────
// Pure date/streak math; the screen owns config persistence and the chrono
// call that produces today's day number. No I/O here.

// Days since 1970-01-01 (civil calendar, leap-year correct). Cross-checked
// against std::chrono::sys_days in the tests.
int dayNumber(int year, int month, int day);

// Deterministic daily pick: mixes the day number (splitmix64 family) and
// shards into the 12.2 puzzle list. poolSize <= 0 yields 0.
int dailyIndex(int dayNumber, int poolSize);

// Streak bookkeeping for a solve happening on `today`:
//   lastDay == today      -> unchanged (already solved today)
//   lastDay == today - 1  -> currentStreak + 1 (consecutive day)
//   otherwise             -> 1 (never solved, or a skipped day reset it)
int solveStreak(int lastDay, int currentStreak, int today);

// What the header displays BEFORE today's solve: the stored streak only
// counts while the chain is still alive (solved today or yesterday); a
// skipped day displays 0 until a fresh solve restarts the chain.
int displayStreak(int lastDay, int storedStreak, int today);

// The daily was completed on `today`.
bool solvedToday(int lastDay, int today);

} // namespace chess::client::daily
