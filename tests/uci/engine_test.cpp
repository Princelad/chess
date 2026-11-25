#include <gtest/gtest.h>

#include <chess/uci/engine.h>
#include <chess/uci_move.h>
#include <chess/movegen.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <poll.h>
#include <unistd.h>

using namespace chess;
using namespace std::chrono_literals;

static std::string mockEnginePath()
{
    const char* env = std::getenv("MOCK_ENGINE_PATH");
    if (env) return env;
    return "mock_engine.py";
}

// Audit mocks live beside the main mock (same directory).
static std::string mockEngineSibling(const char* name)
{
    std::string dir = mockEnginePath();
    const auto slash = dir.find_last_of('/');
    return (slash == std::string::npos) ? std::string(name)
                                        : dir.substr(0, slash + 1) + name;
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

TEST(UciEngine, BareNameResolvesThroughPath)
{
    const char* mockPath = std::getenv("MOCK_ENGINE_PATH");
    if (!mockPath) return;

    std::string path(mockPath);
    const auto slash = path.find_last_of('/');
    if (slash == std::string::npos) return;
    std::string dir = path.substr(0, slash);

    const char* oldPath = std::getenv("PATH");
    std::string savedPath = oldPath ? oldPath : "";
    std::string testPath = dir + (oldPath ? ":" + savedPath : "");
    setenv("PATH", testPath.c_str(), 1);

    uci::UciEngine engine("mock_engine.py");
    auto info = engine.init(3s);

    if (oldPath) setenv("PATH", savedPath.c_str(), 1);
    else unsetenv("PATH");

    EXPECT_TRUE(engine.isRunning());
    EXPECT_EQ(info.name, "MockEngine 1.0");
    engine.quit();
}

TEST(UciEngine, StockfishBareNameHandshakeWhenInstalled)
{
    if (std::system("command -v stockfish >/dev/null 2>&1") != 0) return;

    uci::UciEngine engine("stockfish");
    auto info = engine.init(5s);
    EXPECT_TRUE(engine.isRunning());
    EXPECT_FALSE(info.name.empty());
    engine.quit();
}

// Round-4 audit: the handshake waits used to run out their whole timeout
// when the engine process died (missing path, exec failure) — every
// engine-less client screen froze for the full 5 s per attempt, and the
// eval worker waited movetime + 15 s against a dead process. The reader's
// death must end the wait.
TEST(UciEngine, DeadEngineHandshakeReturnsQuickly)
{
    uci::UciEngine engine("/nonexistent/engine/binary");
    const auto start = std::chrono::steady_clock::now();
    auto info = engine.init(5s);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_TRUE(info.name.empty());
    EXPECT_FALSE(engine.isRunning());
    // The old behavior burned the full 5 s timeout here.
    EXPECT_LT(elapsed, std::chrono::seconds(3));
}

TEST(UciEngine, DeadEngineWaitBestMoveReturnsQuickly)
{
    uci::UciEngine engine("/nonexistent/engine/binary");
    engine.init(5s);
    Board board = Board::fromStartPos();
    const auto start = std::chrono::steady_clock::now();
    auto move = engine.waitBestMove(board, 10s);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_FALSE(move.has_value());
    EXPECT_LT(elapsed, std::chrono::seconds(3));
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

// Audit 7.3: a bestmove must never surface as the answer of a search it
// does not belong to. The mock sends an unsolicited bestmove before any
// go (stamped generation 0) — tryGetBestMove must discard it, and the
// first search's own late reply must still arrive.
TEST(UciEngine, UnsolicitedBestmoveBeforeAnyGoIsDiscarded)
{
    uci::UciEngine engine(mockEngineSibling("mock_engine_stale.py"));
    auto info = engine.init(3s);
    ASSERT_FALSE(info.name.empty());

    Board b = Board::fromStartPos();
    // The unsolicited pre-go answer is in the queue here.
    EXPECT_FALSE(engine.tryGetBestMove(b).has_value());

    engine.position("startpos");
    engine.go(1);
    auto move = engine.waitBestMove(b, 3s);
    ASSERT_TRUE(move.has_value());
    EXPECT_EQ(*move, *chess::fromUci(b, "e2e4"));
    engine.quit();
}

// Audit 7.3: a reply that outlives its consumer's timeout (the eval
// harness's stall scenario) must not satisfy the retry's wait — the
// retry's go bumps the generation, so the stale answer is discarded and
// the new search's own reply is consumed instead.
TEST(UciEngine, LateAnswerDoesNotPoisonTheRetry)
{
    uci::UciEngine engine(mockEngineSibling("mock_engine_stale.py"));
    auto info = engine.init(3s);
    ASSERT_FALSE(info.name.empty());

    Board b = Board::fromStartPos();
    engine.position("startpos");
    engine.go(1);
    // The mock answers the first go after 0.5 s — this wait gives up first.
    EXPECT_FALSE(engine.waitBestMove(b, 100ms).has_value());

    // Retry: the late e2e4 is now queued under the OLD generation.
    engine.position("startpos");
    engine.go(1);
    auto move = engine.waitBestMove(b, 3s);
    ASSERT_TRUE(move.has_value());
    EXPECT_EQ(*move, *chess::fromUci(b, "a2a3"));
    engine.quit();
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

// Round-11 audit: init()'s timeout branch called quit() while still holding
// m_mutex. quit() joins the reader, and the reader needs that same mutex to
// publish the late uciok — both threads waited forever. The timeout must
// release the lock before tearing down.
TEST(UciEngine, LateHandshakeTimeoutDoesNotDeadlock)
{
    uci::UciEngine engine(mockEngineSibling("mock_engine_slow.py"));
    const auto start = std::chrono::steady_clock::now();
    auto info = engine.init(100ms);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_TRUE(info.name.empty());
    EXPECT_FALSE(engine.isRunning());
    EXPECT_LT(elapsed, std::chrono::seconds(3));
}

// Round-11 audit: closeProcess() joined the reader before the SIGTERM/SIGKILL
// fallback, so an engine that ignores quit (and never closes stdout) hung
// the caller forever. Teardown must be bounded by the kill.
TEST(UciEngine, QuitKillsAnUnresponsiveEngine)
{
    uci::UciEngine engine(mockEngineSibling("mock_engine_mute.py"));
    const auto start = std::chrono::steady_clock::now();
    auto info = engine.init(100ms);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_TRUE(info.name.empty());
    EXPECT_FALSE(engine.isRunning());
    EXPECT_LT(elapsed, std::chrono::seconds(3));
}

// Round-11 audit: the execlp'd child inherited every open fd. An inherited
// TCP socket outlives the parent's disconnect() (the server keeps a phantom
// client) and an inherited listener keeps the port bound after a crash.
TEST(UciEngine, EngineDoesNotInheritOpenFileDescriptors)
{
    if (!isEngineAvailable()) return;

    int pipeFds[2];
    ASSERT_EQ(::pipe(pipeFds), 0);
    {
        uci::UciEngine engine(mockEnginePath());
        ASSERT_FALSE(engine.init(3s).name.empty());
        ASSERT_EQ(::close(pipeFds[1]), 0);

        pollfd pfd;
        pfd.fd = pipeFds[0];
        pfd.events = POLLIN;
        pfd.revents = 0;
        // EOF (readable on a pipe nobody writes to) only happens when the
        // child closed its inherited copy of the write end.
        EXPECT_GT(::poll(&pfd, 1, 500), 0);
        ::close(pipeFds[0]);
    }
}

// Round-11 audit: waitBestMove() treated "the engine died" as success even
// when the queued answer belonged to an older search — the eval harness
// could record the previous node's move for the current position. Audit 7.3
// applies to every consumer, not just tryGetBestMove/pollBestMove.
TEST(UciEngine, StaleAnswerIsNotReturnedWhenTheEngineDies)
{
    uci::UciEngine engine(mockEngineSibling("mock_engine_late_die.py"));
    ASSERT_FALSE(engine.init(3s).name.empty());

    Board b = Board::fromStartPos();
    engine.position("startpos");
    engine.go(1);
    // The mock answers after 0.5 s; this wait gives up first.
    EXPECT_FALSE(engine.waitBestMove(b, 50ms).has_value());

    // The late e2e4 lands under generation 1, then the mock exits.
    std::this_thread::sleep_for(700ms);
    engine.go(1); // generation 2; the child is already gone
    EXPECT_FALSE(engine.waitBestMove(b, 2s).has_value());
}
