#pragma once

#include <string>

#include <chess/board.h>
#include <chess/move.h>

namespace chess {
namespace san {

std::string toSan(const Board& board, const Move& m);

} // namespace san
} // namespace chess
