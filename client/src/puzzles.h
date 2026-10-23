#pragma once

#include <chess/board.h>
#include <chess/move.h>
#include <chess/pgn.h>
#include <chess/types.h>

#include <optional>
#include <string>
#include <vector>

namespace chess::client {

// ── Offline puzzle mode (12.2) ──────────────────────────────────────────────

// One composed mate-in-N puzzle: a position plus the full forced line (the
// solver's moves interleaved with the defender's best replies). The solver
// is the side to move at the start position.
struct Puzzle {
    std::string startFen;
    std::vector<chess::Move> solution;
    chess::Color solverColor = chess::Color::None;

    // Solver moves only; replies ride along, so a k-move mate line is
    // 2k-1 plies and ends on the solver's move.
    int mateIn() const { return static_cast<int>((solution.size() + 1) / 2); }
};

// Verifies and converts one parsed game into a puzzle, or nullopt: requires
// a FEN header (the set is composed positions), a legal move chain ending in
// checkmate, and the solver (the start position's side to move) delivering
// that mate. A broken entry never reaches the player.
std::optional<Puzzle> puzzleFromPgn(const chess::PgnGame& game);

// Filters a whole parsed file, keeping only the admitted puzzles (order
// preserved).
std::vector<Puzzle> puzzlesFromPgn(const std::vector<chess::PgnGame>& games);

// Reads every `puzzles.pgn` under the given base directories (first hit
// wins) and returns its verified puzzles; a missing file yields an empty
// set — the screen degrades gracefully rather than erroring.
std::vector<Puzzle> loadPuzzlesFromDirs(const std::vector<std::string>& dirs);

// Pure streak state: a solve raises the current streak (and best), a wrong
// move or a revealed solution zeroes it. Best-streak persistence is the
// screen's job (config key `puzzle.best_streak`).
struct PuzzleStreak {
    int current = 0;
    int best = 0;

    void onSolved()
    {
        ++current;
        if (current > best)
            best = current;
    }

    void onFail() { current = 0; }
};

} // namespace chess::client
