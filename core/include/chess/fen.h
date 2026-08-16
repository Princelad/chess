#pragma once

#include <optional>
#include <string_view>

#include <chess/board.h>

namespace chess {

std::optional<Board> fromFen(std::string_view fen);

} // namespace chess
