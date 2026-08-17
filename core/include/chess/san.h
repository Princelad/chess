#pragma once

#include <optional>
#include <string>

#include <chess/board.h>
#include <chess/move.h>

namespace chess {
namespace san {

std::string toSan(const Board& board, const Move& m);
std::optional<Move> fromSan(const Board& board, const std::string& san);

} // namespace san
} // namespace chess
