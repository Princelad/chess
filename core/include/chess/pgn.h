#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <chess/board.h>
#include <chess/move.h>

namespace chess {

struct PgnGame {
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<Move> moves;
    std::string result;
    std::string startPosition;
};

std::optional<PgnGame> parsePgn(std::string_view pgn);

std::string toPgn(const std::vector<std::pair<std::string, std::string>>& headers,
                  const std::vector<std::string>& SANmoves,
                  std::string_view result,
                  std::string_view startPosition = "");

std::optional<Board> replayMoves(const std::vector<Move>& moves,
                                 std::string_view startPosition = "");

} // namespace chess
