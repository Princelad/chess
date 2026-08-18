#include <chess/board.h>
#include <chess/fen.h>
#include <chess/movegen.h>

#include <gtest/gtest.h>

namespace chess {

namespace {

void perftExpect(Board& board, int depth, uint64_t expected)
{
    EXPECT_EQ(perft(board, depth), expected)
        << "perft(" << depth << ") failed";
}

} // namespace

TEST(Perft, InitialPosition)
{
    auto board = Board::fromStartPos();
    perftExpect(board, 1, 20);
    perftExpect(board, 2, 400);
    perftExpect(board, 3, 8902);
    perftExpect(board, 4, 197281);
    perftExpect(board, 5, 4865609);
}

TEST(Perft, Kiwipete)
{
    auto board = *Board::fromFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    perftExpect(board, 1, 48);
    perftExpect(board, 2, 2039);
    perftExpect(board, 3, 97862);
}

TEST(Perft, Position3)
{
    auto board = *Board::fromFen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    perftExpect(board, 1, 14);
    perftExpect(board, 2, 191);
    perftExpect(board, 3, 2812);
}

TEST(Perft, Position4)
{
    auto board = *Board::fromFen("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    perftExpect(board, 1, 6);
    perftExpect(board, 2, 264);
    perftExpect(board, 3, 9467);
}

TEST(Perft, Position5)
{
    auto board = *Board::fromFen("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
    perftExpect(board, 1, 44);
    perftExpect(board, 2, 1486);
    perftExpect(board, 3, 62379);
}

TEST(Perft, Position6)
{
    auto board = *Board::fromFen("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
    perftExpect(board, 1, 46);
    perftExpect(board, 2, 2079);
    perftExpect(board, 3, 89890);
    perftExpect(board, 4, 3894594);
}

} // namespace chess
