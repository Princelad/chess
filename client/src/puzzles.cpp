#include "puzzles.h"

#include <chess/fen.h>
#include <chess/movegen.h>

#include <fstream>
#include <sstream>

namespace chess::client {

std::optional<Puzzle> puzzleFromPgn(const chess::PgnGame& game)
{
    // The set is composed positions: without the FEN header this is a
    // regular game dump, not a puzzle.
    std::string startFen;
    for (const auto& [key, value] : game.headers) {
        if (key == "FEN" && !value.empty()) {
            startFen = value;
            break;
        }
    }
    if (startFen.empty() || game.moves.empty())
        return std::nullopt;

    auto startBoard = chess::Board::fromFen(startFen);
    if (!startBoard)
        return std::nullopt;

    // parsePgn already resolved the SAN chain against the FEN header, so an
    // admitted game holds a fully legal line from here; replaying it makes
    // the end position inspectable.
    chess::Board board = *startBoard;
    for (const auto& move : game.moves)
        board.makeMove(move);

    // A puzzle is always delivered: the line ends in checkmate and the
    // solver — the start position's side to move — played the final move.
    if (chess::evaluateGameState(board) != chess::GameState::Checkmate)
        return std::nullopt;

    Puzzle puzzle;
    puzzle.startFen = startFen;
    puzzle.solution = game.moves;
    puzzle.solverColor = startBoard->sideToMove();
    if (chess::opposite(board.sideToMove()) != puzzle.solverColor)
        return std::nullopt;

    return puzzle;
}

std::vector<Puzzle> puzzlesFromPgn(const std::vector<chess::PgnGame>& games)
{
    std::vector<Puzzle> puzzles;
    puzzles.reserve(games.size());
    for (const auto& game : games) {
        if (auto puzzle = puzzleFromPgn(game))
            puzzles.push_back(std::move(*puzzle));
    }
    return puzzles;
}

std::vector<Puzzle> loadPuzzlesFromDirs(const std::vector<std::string>& dirs)
{
    for (const auto& dir : dirs) {
        std::ifstream file(dir + "puzzles.pgn");
        if (!file)
            continue;

        std::ostringstream buffer;
        buffer << file.rdbuf();

        return puzzlesFromPgn(chess::parsePgnMulti(buffer.str()));
    }
    return {};
}

} // namespace chess::client
