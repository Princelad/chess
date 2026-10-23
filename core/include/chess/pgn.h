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

// Splits a multi-game PGN file and parses each game independently. Games
// that fail to parse (malformed headers, illegal move chains) are skipped
// rather than failing the whole file. A game is a header line (`[` …)
// followed by movetext; the next `[`-line after movetext begins a new one.
std::vector<PgnGame> parsePgnMulti(std::string_view pgn);

std::string toPgn(std::vector<std::pair<std::string, std::string>> headers,
                  const std::vector<std::string>& SANmoves,
                  std::string_view result,
                  std::string_view startPosition = "");

std::optional<Board> replayMoves(const std::vector<Move>& moves,
                                 std::string_view startPosition = "");

} // namespace chess
