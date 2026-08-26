#pragma once

#include <optional>
#include <string_view>

#include <chess/board.h>
#include <chess/move.h>

namespace chess {

std::optional<Move> fromUci(const Board& board, std::string_view uci);

} // namespace chess
