#include <chess/board.h>
#include <chess/movegen.h>
#include <chess/san.h>

#include <gtest/gtest.h>

namespace chess {
namespace {

// Helper: resolve a SAN string and make the move. Fails the test on invalid SAN.
static void play(Board& board, const char* san)
{
    auto m = san::fromSan(board, san);
    ASSERT_TRUE(m.has_value()) << "fromSan failed for: " << san;
    board.makeMove(*m);
}

TEST(Endgame, FoolsMate)
{
    auto board = Board::fromStartPos();

    play(board, "f3");
    play(board, "e5");
    play(board, "g4");
    play(board, "Qh4#");

    EXPECT_EQ(evaluateGameState(board), GameState::Checkmate);
}

TEST(Endgame, ScholarsMate)
{
    auto board = Board::fromStartPos();

    play(board, "e4");
    play(board, "e5");
    play(board, "Bc4");
    play(board, "Nc6");
    play(board, "Qh5");
    play(board, "Nf6");
    play(board, "Qxf7#");

    EXPECT_EQ(evaluateGameState(board), GameState::Checkmate);
}

TEST(Endgame, BackRankMate)
{
    // White Ra1 delivers back-rank mate: Kg8 blocked by own pawns on f7/g7/h7
    auto board = *Board::fromFen("6k1/5ppp/8/8/8/8/5PPP/R3K2R w KQ - 0 1");

    play(board, "Ra8#");

    EXPECT_EQ(evaluateGameState(board), GameState::Checkmate);
}

TEST(Endgame, DrawByRepetitionPlaythrough)
{
    auto board = Board::fromStartPos();

    play(board, "Nf3");
    play(board, "Nf6");
    play(board, "Ng1");
    play(board, "Ng8");
    play(board, "Nf3");
    play(board, "Nf6");
    play(board, "Ng1");
    play(board, "Ng8");

    EXPECT_EQ(evaluateGameState(board), GameState::Draw);
}

TEST(Endgame, DrawByFiftyMovePlaythrough)
{
    auto board = Board::fromStartPos();

    // Advance to a position where the halfmove clock is at 99,
    // then make a quiet non-pawn move to reach 100.
    play(board, "Nf3");
    play(board, "Nf6");

    board.setHalfmoveClock(99);

    play(board, "Ng1");  // clock increments to 100
    play(board, "Ng8");  // clock increments to 101

    EXPECT_EQ(evaluateGameState(board), GameState::Draw);
}

} // namespace
} // namespace chess
