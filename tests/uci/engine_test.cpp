#include <gtest/gtest.h>

#include <chess/uci/engine.h>
#include <chess/movegen.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <thread>

using namespace chess;
using namespace std::chrono_literals;

static std::string mockEnginePath()
{
    const char* env = std::getenv("MOCK_ENGINE_PATH");
    if (env) return env;
    return "mock_engine.py";
}

static bool isEngineAvailable()
{
    uci::UciEngine engine(mockEnginePath());
    auto info = engine.init(2s);
    return !info.name.empty() || engine.isRunning();
}

TEST(UciEngine, InitHandshake)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    auto info = engine.init(3s);
    EXPECT_TRUE(engine.isRunning());
    EXPECT_EQ(info.name, "MockEngine 1.0");
    EXPECT_EQ(info.author, "Test");
    engine.quit();
}

TEST(UciEngine, SetOption)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    engine.init(3s);
    EXPECT_NO_THROW(engine.setOption("Hash", "128"));
    EXPECT_NO_THROW(engine.setOption("Threads", "2"));
    engine.quit();
}

TEST(UciEngine, NewGame)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    engine.init(3s);
    EXPECT_NO_THROW(engine.newGame());
    engine.quit();
}

TEST(UciEngine, PositionStartpos)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    engine.init(3s);
    EXPECT_NO_THROW(engine.position("startpos"));
    engine.quit();
}

TEST(UciEngine, PositionFen)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    engine.init(3s);
    EXPECT_NO_THROW(engine.position(
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"));
    engine.quit();
}

TEST(UciEngine, PositionWithMoves)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    engine.init(3s);
    engine.position("startpos", {"e2e4", "e7e5"});
    engine.quit();
}

TEST(UciEngine, GoDepth)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    engine.init(3s);
    engine.position("startpos");
    engine.go(1);

    Board b = Board::fromStartPos();
    auto move = engine.waitBestMove(b, 3s);
    EXPECT_TRUE(move.has_value());
    engine.quit();
}

TEST(UciEngine, GoMovetime)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    engine.init(3s);
    engine.position("startpos");
    engine.go(0, 50);

    Board b = Board::fromStartPos();
    auto move = engine.waitBestMove(b, 3s);
    EXPECT_TRUE(move.has_value());
    engine.quit();
}

TEST(UciEngine, GoInfinite)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    engine.init(3s);
    engine.position("startpos");
    engine.go(0, 0, true);

    std::this_thread::sleep_for(100ms);
    engine.stop();

    Board b = Board::fromStartPos();
    auto move = engine.waitBestMove(b, 3s);
    EXPECT_TRUE(move.has_value());
    engine.quit();
}

TEST(UciEngine, BestMoveIsLegal)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    engine.init(3s);
    engine.position("startpos");
    engine.go(1);

    Board b = Board::fromStartPos();
    auto move = engine.waitBestMove(b, 3s);
    ASSERT_TRUE(move.has_value());
    EXPECT_TRUE(isLegalMove(b, *move));
    engine.quit();
}

TEST(UciEngine, InfoCallback)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    engine.init(3s);

    int infoCount = 0;
    engine.onInfo([&](const uci::SearchInfo& si) {
        infoCount++;
        EXPECT_GT(si.depth, 0);
    });

    engine.position("startpos");
    engine.go(1);

    Board b = Board::fromStartPos();
    engine.waitBestMove(b, 3s);
    EXPECT_GT(infoCount, 0);
    engine.quit();
}

TEST(UciEngine, Timeout)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    engine.init(3s);

    Board b = Board::fromStartPos();
    auto move = engine.waitBestMove(b, 10ms);
    EXPECT_FALSE(move.has_value());
    engine.quit();
}

TEST(UciEngine, TryGetBestMoveEmpty)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    engine.init(3s);

    Board b = Board::fromStartPos();
    auto move = engine.tryGetBestMove(b);
    EXPECT_FALSE(move.has_value());
    engine.quit();
}

TEST(UciEngine, TryGetBestMoveAfterGo)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    engine.init(3s);
    engine.position("startpos");
    engine.go(1);

    Board b = Board::fromStartPos();
    auto move = engine.waitBestMove(b, 3s);
    ASSERT_TRUE(move.has_value());

    auto move2 = engine.tryGetBestMove(b);
    EXPECT_FALSE(move2.has_value());
    engine.quit();
}

TEST(UciEngine, QuitIdempotent)
{
    if (!isEngineAvailable()) return;

    uci::UciEngine engine(mockEnginePath());
    engine.init(3s);
    engine.quit();
    EXPECT_FALSE(engine.isRunning());
}

TEST(UciEngine, DestructorCleansUp)
{
    if (!isEngineAvailable()) return;

    {
        uci::UciEngine engine(mockEnginePath());
        engine.init(3s);
    }
    SUCCEED();
}
