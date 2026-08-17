#pragma once

#include <vector>

#include <chess/board.h>
#include <chess/move.h>

namespace chess {

enum class MoveFilter { All, CapturesOnly };

enum class GameState { Ongoing, Checkmate, Stalemate, Draw };

std::vector<Move> generateMoves(const Board& board, MoveFilter filter = MoveFilter::All);
std::vector<Move> generateLegalMoves(const Board& board, MoveFilter filter = MoveFilter::All);

GameState evaluateGameState(const Board& board);

bool threefoldRepetition(const Board& board);

bool fiftyMoveRule(const Board& board);

bool insufficientMaterial(const Board& board);

bool isAttacked(const Board& board, Square sq, Color byColor);
bool inCheck(const Board& board, Color color);

bool isLegalMove(const Board& board, const Move& m);

} // namespace chess
