#pragma once

#include <vector>

#include <chess/board.h>
#include <chess/move.h>

namespace chess {

enum class MoveFilter { All, CapturesOnly };

std::vector<Move> generateMoves(const Board& board, MoveFilter filter = MoveFilter::All);

bool isAttacked(const Board& board, Square sq, Color byColor);
bool inCheck(const Board& board, Color color);

} // namespace chess
