#pragma once

#include <optional>
#include <string>

namespace chess {

// Chess960 (Fischer Random) start positions, using the worldwide-standard
// Reinhard Scharnagl numbering scheme (SP numbers 0..959; the standard chess
// array is SP number 518). Both colors mirror the same back-rank layout.

constexpr int Chess960PositionCount = 960;

// Standard chess start position equivalent to (fromStartPos).
constexpr int Chess960StandardId = 518;

// FEN of start position `id` (0..959). Castling field is X-FEN: traditional
// letters for rooks on corner files, file letters otherwise. Returns nullopt
// for out-of-range ids.
std::optional<std::string> chess960StartFen(int id);

// Random start-position id uniform over 0..959.
int chess960RandomId();

} // namespace chess
