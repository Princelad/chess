#include "puzzles.h"

#include <chess/fen.h>
#include <chess/movegen.h>
#include <chess/pgn.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace chess;
using namespace chess::client;

namespace {

// A mate-in-1 row verified during development (also lives in the asset).
constexpr const char* MateIn1Pgn =
    "[SetUp \"1\"]\n"
    "[FEN \"6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1\"]\n"
    "\n"
    "1. Ra8# 1-0\n";

// Annotated but broken entries the admission filter must refuse.
constexpr const char* NoFenPgn =
    "[Event \"regular game\"]\n"
    "\n"
    "1. e4 e5 2. Nf3 * \n";

constexpr const char* NotMatePgn =
    "[SetUp \"1\"]\n"
    "[FEN \"6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1\"]\n"
    "\n"
    "1. Ra1 * \n";

std::vector<Puzzle> parse(const char* pgn)
{
    return puzzlesFromPgn(parsePgnMulti(pgn));
}

} // namespace

TEST(PuzzlePgn, ParseMultiSplitsAdjacentGames)
{
    std::string two = std::string(MateIn1Pgn) +
        "[SetUp \"1\"]\n"
        "[FEN \"k5r1/8/8/8/8/8/7r/K7 b - - 0 1\"]\n"
        "\n"
        "1... Rg1# 0-1\n";
    auto games = parsePgnMulti(two);
    ASSERT_EQ(games.size(), 2u);
    EXPECT_EQ(games[0].moves.size(), 1u);
    EXPECT_EQ(games[1].moves.size(), 1u);

    auto puzzles = puzzlesFromPgn(games);
    ASSERT_EQ(puzzles.size(), 2u);
    EXPECT_EQ(puzzles[0].solverColor, Color::White);
    EXPECT_EQ(puzzles[1].solverColor, Color::Black);
}

TEST(PuzzlePgn, ParseMultiSkipsMalformedGame)
{
    std::string file = std::string(
        "[FEN \"6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1\"]\n\n"
        "1. IloveQg5 * \n") + MateIn1Pgn;
    auto games = parsePgnMulti(file);
    ASSERT_EQ(games.size(), 1u); // broken game skipped, rest lives on
    EXPECT_EQ(games.front().moves.size(), 1u);
}

TEST(PuzzlePgn, AdmitsVerifiedSolvedLine)
{
    auto puzzles = parse(MateIn1Pgn);
    ASSERT_EQ(puzzles.size(), 1u);
    const Puzzle& p = puzzles.front();
    EXPECT_EQ(p.startFen, "6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1");
    EXPECT_EQ(p.mateIn(), 1);
    ASSERT_EQ(p.solution.size(), 1u);
    EXPECT_TRUE(p.solution.front().isPromotion() == false);
    EXPECT_EQ(p.solution.front().from, squareOf(0, 0));
    EXPECT_EQ(p.solution.front().to, squareOf(0, 7));
}

TEST(PuzzlePgn, RejectsNonPuzzleGames)
{
    EXPECT_TRUE(parse(NoFenPgn).empty());    // no FEN header
    EXPECT_TRUE(parse(NotMatePgn).empty());  // legal but not mate
    EXPECT_TRUE(parse("").empty());          // nothing at all
}

TEST(PuzzlePgn, MateInCountsSolverMoves)
{
    // Mate in 2 (queen check forces the reply, mate follows).
    auto puzzles = parse(
        "[SetUp \"1\"]\n"
        "[FEN \"4k3/3p4/1Q6/8/7N/B7/8/2K5 w - - 0 1\"]\n"
        "\n"
        "1. Qd6 Kd8 2. Qb8# 1-0\n");
    ASSERT_EQ(puzzles.size(), 1u);
    EXPECT_EQ(puzzles.front().mateIn(), 2);
    EXPECT_EQ(puzzles.front().solution.size(), 3u);
    // Solver owns even plies; the reply ply belongs to the defender.
    Board b = *Board::fromFen(puzzles.front().startFen);
    for (std::size_t i = 0; i < puzzles.front().solution.size(); ++i) {
        if (i % 2 == 0)
            EXPECT_EQ(b.sideToMove(), puzzles.front().solverColor) << "ply " << i;
        b.makeMove(puzzles.front().solution[i]);
    }
    EXPECT_EQ(evaluateGameState(b), GameState::Checkmate);
}

TEST(PuzzleStreak, PureMath)
{
    PuzzleStreak s;
    EXPECT_EQ(s.current, 0);
    EXPECT_EQ(s.best, 0);

    s.onSolved();
    s.onSolved();
    s.onSolved();
    EXPECT_EQ(s.current, 3);
    EXPECT_EQ(s.best, 3);

    s.onFail();
    EXPECT_EQ(s.current, 0);
    EXPECT_EQ(s.best, 3); // best survives failures

    s.onSolved();
    EXPECT_EQ(s.current, 1);
    EXPECT_EQ(s.best, 3);
}

TEST(PuzzleLoad, ReadsFirstAvailableDir)
{
    auto dir = std::filesystem::temp_directory_path() /
               ("chess_puzzle_test_" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);

    EXPECT_TRUE(loadPuzzlesFromDirs({dir.string() + "/"}).empty());

    const std::string path = (dir / "puzzles.pgn").string();
    {
        std::ofstream out(path);
        out << MateIn1Pgn;
    }
    auto puzzles = loadPuzzlesFromDirs({dir.string() + "/"});
    ASSERT_EQ(puzzles.size(), 1u);
    EXPECT_EQ(puzzles.front().mateIn(), 1);

    std::filesystem::remove_all(dir);
}

TEST(PuzzleLoad, MissingDirsYieldNothing)
{
    EXPECT_TRUE(loadPuzzlesFromDirs(
        {"no/such/dir/", "still/nothing/"}).empty());
}

TEST(PuzzleAsset, BundledSetLoadsAndIsSound)
{
#ifdef CHESS_ASSETS_DIR
    auto puzzles = loadPuzzlesFromDirs({CHESS_ASSETS_DIR "/"});
    ASSERT_FALSE(puzzles.empty());
    EXPECT_GE(puzzles.size(), 20u);

    int blackSolvers = 0;
    bool sawMateIn2 = false;
    for (const auto& p : puzzles) {
        ASSERT_FALSE(p.startFen.empty());
        ASSERT_FALSE(p.solution.empty());
        EXPECT_GE(p.mateIn(), 1);
        EXPECT_LE(p.mateIn(), 3);
        EXPECT_TRUE(p.mateIn() >= 1 && p.mateIn() <= 3);
        if (p.mateIn() >= 2) sawMateIn2 = true;
        if (p.solverColor == Color::Black) ++blackSolvers;

        // Independent replay: legal chain ending in the solver's mate.
        auto board = Board::fromFen(p.startFen);
        ASSERT_TRUE(board);
        EXPECT_EQ(board->sideToMove(), p.solverColor);
        // Odd plies belong to the defender, even plies to the solver.
        for (std::size_t i = 0; i < p.solution.size(); ++i) {
            if (i % 2 == 0)
                EXPECT_EQ(board->sideToMove(), p.solverColor);
            board->makeMove(p.solution[i]);
        }
        // Final mover must be the solver: side to move flipped.
        EXPECT_NE(board->sideToMove(), p.solverColor);
        EXPECT_EQ(evaluateGameState(*board), GameState::Checkmate);
    }
    EXPECT_TRUE(sawMateIn2);
    EXPECT_GE(blackSolvers, 3); // both colors represented
#else
    GTEST_FAIL() << "CHESS_ASSETS_DIR not defined";
#endif
}
