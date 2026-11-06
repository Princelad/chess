// 16.1 unit tests — pure parts + injected driver (never spawns a process).

#include "eval_harness.h"

#include <chess/board.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace chess;
using namespace chess::client;

namespace {

// 5 plies, distinct positions per ply (win%-deltas seed from these later).
const std::vector<std::string> kItalian{"e4", "e5", "Nf3", "Nc6", "Bb5"};

// 4 plies ending in mate (castling/mate SAN coverage).
const std::vector<std::string> kFoolsMate{"f3", "e5", "g4", "Qh4#"};

// 1 ply promotion.
const std::vector<std::string> kPromo{"a8=Q"};

const std::vector<std::string> kThreePlies{"e4", "e5", "Nf3"};

constexpr const char* kStartFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Canned driver. Scores each call 10 * its call index. `gated` makes every
// evaluate BLOCK until releaseGate() or requestStop() (so the test decides
// when a node's search finishes); `freeCalls` lets the first N calls through
// ungated so mid-run cancels land on a known ply. `failOn` scripts failures.
class FakeDriver : public EvalDriver
{
public:
    bool evaluate(const std::string& fen, int movetimeMs, NodeEval& out) override
    {
        (void)fen;
        (void)movetimeMs;
        if (calls.load() >= freeCalls.load() && gated.load()) {
            std::unique_lock<std::mutex> lock(gateMutex);
            gateCv.wait(lock, [this] {
                return released.load() || stopRequested.load();
            });
        }
        const int index = ++calls;
        if (failOn == index) return false;
        out.scoreCp = 10 * index;
        out.isMate = mateOn == index;
        out.mateIn = out.isMate ? 3 : 0;
        out.bestSan = "e4";
        return true;
    }

    void requestStop() override
    {
        stopRequested.store(true);
        gateCv.notify_all();
    }
    void gate()
    {
        gated.store(true);
    }
    void releaseGate()
    {
        released.store(true);
        gateCv.notify_all();
    }

    std::atomic<int> calls{0};
    std::atomic<int> freeCalls{0x7fffffff};
    int failOn = -1;
    int mateOn = -1;
    std::atomic<bool> gated{false};
    std::atomic<bool> released{false};
    std::atomic<bool> stopRequested{false};
    std::condition_variable gateCv;
    std::mutex gateMutex;
};

// Polls until the job reaches `wantState` with at least `wantDone` plies
// scored (or the deadline lapses) and returns the last observed progress.
EvalHarness::Progress awaitState(const EvalHarness& harness, EvalKey key,
                                 EvalState wantState, int wantDone,
                                 int timeoutSec = 5)
{
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSec);
    EvalHarness::Progress p;
    while (std::chrono::steady_clock::now() < deadline) {
        p = harness.progress(key);
        if (p.state == wantState && p.done >= wantDone) return p;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return p;
}

} // namespace

// --- pure: identity ----------------------------------------------------------

TEST(EvalKey, IdentityUsesArchiveIdOrContentHash)
{
    auto archived = evalKeyFor(7, kStartFen, kItalian);
    EXPECT_EQ(archived.archiveId, 7);
    EXPECT_EQ(archived.sanHash, 0u);

    auto hashed = evalKeyFor(0, kStartFen, kItalian);
    EXPECT_EQ(hashed.archiveId, 0);
    EXPECT_NE(hashed.sanHash, 0u);
    EXPECT_TRUE(evalKeyFor(0, kStartFen, kItalian) == hashed);

    // Content identity: a different chain or start position hashes apart.
    EXPECT_NE(evalKeyFor(0, kStartFen, kFoolsMate), hashed);
    EXPECT_NE(evalKeyFor(0, "4k3/8/8/8/8/8/8/4K3 w - - 0 1", kItalian), hashed);

    // Empty-vs-absent tokens stay distinct (the hash is a whole-state key).
    EXPECT_NE(evalGameHash("e4", {""}), evalGameHash("", {"e4"}));

    // The archive shortcut ignores content entirely.
    EXPECT_EQ(evalKeyFor(7, kStartFen, kFoolsMate), archived);
}

// --- pure: position chain ----------------------------------------------------

TEST(EvalNodes, ChainsOneNodePerPly)
{
    auto nodes = evalNodes("", kItalian);
    ASSERT_TRUE(nodes.has_value());
    ASSERT_EQ(nodes->size(), 5u);

    // First node sits on the standard start.
    EXPECT_EQ((*nodes)[0].fen, kStartFen);
    EXPECT_EQ((*nodes)[0].san, "e4");

    // Second node's fen reflects e4 played (black to move; the ep target
    // e3 is part of the position the mover searches).
    EXPECT_EQ((*nodes)[1].fen,
              "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    EXPECT_EQ((*nodes)[1].san, "e5");
}

TEST(EvalNodes, HandlesMateAndPromotionChains)
{
    auto mate = evalNodes("", kFoolsMate);
    ASSERT_TRUE(mate.has_value());
    EXPECT_EQ(mate->size(), 4u);
    // Piece placement + side to move of the position before the mating queen
    // move (the trailing ep/clock fields are the chain's own bookkeeping).
    EXPECT_EQ((*mate)[3].fen.substr(0, 50),
              "rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b");

    auto promo = evalNodes("4k3/P7/8/8/8/8/8/4K3 w - - 0 1", kPromo);
    ASSERT_TRUE(promo.has_value());
    EXPECT_EQ(promo->size(), 1u);
    EXPECT_EQ((*promo)[0].san, "a8=Q");
}

TEST(EvalNodes, RefusesJunkWhole)
{
    // Empty chain: nothing to evaluate.
    EXPECT_FALSE(evalNodes("", {}).has_value());
    // Unparsable start position.
    EXPECT_FALSE(evalNodes("not a fen", kItalian).has_value());
    // Junk SAN: refused whole, never half-reported.
    auto junk = evalNodes("", std::vector<std::string>{"e4", "e9"});
    EXPECT_FALSE(junk.has_value());
    // Empty SAN token.
    auto blank = evalNodes("", std::vector<std::string>{"e4", ""});
    EXPECT_FALSE(blank.has_value());
}

// --- harness (FakeDriver): run, resume, cancel, failure -----------------------

TEST(EvalHarness, FullRunScoresEveryNodeThenCachesDone)
{
    auto nodes = evalNodes("", kItalian);
    ASSERT_TRUE(nodes.has_value());
    EvalKey key = evalKeyFor(0, "", kItalian);

    auto driver = std::make_unique<FakeDriver>();
    auto* raw = driver.get();
    EvalHarness harness(std::move(driver));

    EXPECT_TRUE(harness.hasEngine());
    harness.start(key, std::move(*nodes), kEvalMovetimeMs);

    auto p = awaitState(harness, key, EvalState::Done, 5);
    EXPECT_EQ(p.state, EvalState::Done);
    EXPECT_EQ(p.done, 5);
    EXPECT_EQ(p.total, 5);
    EXPECT_EQ(raw->calls, 5);

    auto report = harness.report(key);
    ASSERT_TRUE(report);
    ASSERT_EQ(report->evals.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(report->evals[i].valid);
        EXPECT_EQ(report->evals[i].eval.scoreCp, 10 * (i + 1));
        EXPECT_EQ(report->evals[i].eval.bestSan, "e4");
    }
    EXPECT_FALSE(report->failed);

    // Re-start on a done key: nothing recomputes.
    harness.start(key, evalNodes("", kItalian).value(), kEvalMovetimeMs);
    EXPECT_EQ(harness.state(key), EvalState::Done);
    EXPECT_EQ(raw->calls, 5);
}

TEST(EvalHarness, FailureMarksFailedAndResumeSkipsCachedPlies)
{
    auto nodes = evalNodes("", kItalian);
    ASSERT_TRUE(nodes.has_value());
    EvalKey key = evalKeyFor(0, "", kItalian);

    auto driver = std::make_unique<FakeDriver>();
    auto* raw = driver.get();
    raw->failOn = 2; // ply 1's attempt fails first
    EvalHarness harness(std::move(driver));
    harness.start(key, std::move(*nodes), kEvalMovetimeMs);

    auto p = awaitState(harness, key, EvalState::Failed, 1);
    EXPECT_EQ(p.state, EvalState::Failed);
    EXPECT_EQ(p.done, 1);
    EXPECT_EQ(p.total, 5);

    // Partial cache already holds ply 0 only.
    auto partial = harness.report(key);
    ASSERT_TRUE(partial);
    EXPECT_TRUE(partial->evals[0].valid);
    EXPECT_FALSE(partial->evals[1].valid);
    EXPECT_EQ(partial->evals[0].eval.scoreCp, 10);

    // Clear the scripted failure and resume: the scored ply is NOT
    // recomputed, the failed one is retried.
    raw->failOn = -1;
    harness.start(key, evalNodes("", kItalian).value(), kEvalMovetimeMs);
    auto done = awaitState(harness, key, EvalState::Done, 5);
    EXPECT_EQ(done.state, EvalState::Done);
    EXPECT_EQ(done.done, 5);
    // calls: 1 score + 1 failure + 1..4 retries = 6; ply 1 got call 3.
    EXPECT_EQ(raw->calls, 6);
    EXPECT_EQ(harness.report(key)->evals[1].eval.scoreCp, 30);
}

TEST(EvalHarness, StopAbortsInFlightPlyAndResumeFinishes)
{
    auto nodes = evalNodes("", kThreePlies);
    ASSERT_TRUE(nodes.has_value());
    EvalKey key = evalKeyFor(0, "", kThreePlies);

    auto driver = std::make_unique<FakeDriver>();
    auto* raw = driver.get();
    raw->freeCalls = 1; // ply 0 flows; ply 1 blocks until stop/release
    raw->gate();
    EvalHarness harness(std::move(driver));
    harness.start(key, std::move(*nodes), kEvalMovetimeMs);

    // One ply scored, the second in flight.
    auto p = awaitState(harness, key, EvalState::Running, 1);
    EXPECT_EQ(p.done, 1);

    // Cancelling flushes the in-flight node and pauses the job.
    harness.stop();
    EXPECT_EQ(harness.state(key), EvalState::Idle);
    EXPECT_EQ(harness.progress(key).done, 1);

    // Resume without re-scoring ply 0; the rest completes.
    raw->freeCalls.store(0x7fffffff);
    raw->releaseGate();
    harness.start(key, evalNodes("", kThreePlies).value(), kEvalMovetimeMs);
    auto done = awaitState(harness, key, EvalState::Done, 3);
    EXPECT_EQ(done.state, EvalState::Done);
    EXPECT_EQ(done.done, 3);
    // calls: 1 + 1 aborted + 2 resumed.
    EXPECT_EQ(raw->calls, 4);
}

TEST(EvalHarness, StartIgnoresOtherKeysWhileBusy)
{
    auto nodes = evalNodes("", kThreePlies);
    ASSERT_TRUE(nodes.has_value());
    EvalKey keyA = evalKeyFor(0, "", kThreePlies);

    auto driver = std::make_unique<FakeDriver>();
    auto* raw = driver.get();
    raw->freeCalls = 1;
    raw->gate();
    EvalHarness harness(std::move(driver));
    harness.start(keyA, std::move(*nodes), kEvalMovetimeMs);
    (void)awaitState(harness, keyA, EvalState::Running, 1); // busy

    // A different key while a job runs: ignored — no cache entry appears.
    EvalKey keyB{0, 77};
    auto nodesB = evalNodes("", kFoolsMate);
    ASSERT_TRUE(nodesB.has_value());
    harness.start(keyB, std::move(*nodesB), kEvalMovetimeMs);
    EXPECT_EQ(harness.progress(keyB).state, EvalState::Idle); // quiet unknown
    EXPECT_EQ(harness.progress(keyB).total, 0);
    EXPECT_FALSE(harness.report(keyB));

    // stop() then start() puts key B at work; the partial A cache persists.
    harness.stop();
    harness.start(keyB, evalNodes("", kFoolsMate).value(), kEvalMovetimeMs);
    EXPECT_EQ(awaitState(harness, keyB, EvalState::Done, 4).state, EvalState::Done);
    // calls: 1 for A + 1 aborted-by-stop + 4 for B.
    EXPECT_EQ(raw->calls, 6);
    // A's cache still holds its scored ply... its state is paused-until-
    // re-start, so progress stays with its 1 done of 3.
    auto a = harness.progress(keyA);
    EXPECT_EQ(a.state, EvalState::Idle);
    EXPECT_EQ(a.done, 1);
    EXPECT_EQ(a.total, 3);
}

TEST(EvalHarness, UnknownKeysAreQuietAndDefaultCtorNeverSpawns)
{
    // Default (production) harness: no engine until ensureEngine().
    EvalHarness harness;
    EXPECT_FALSE(harness.hasEngine());

    EvalKey unknown{0, 1};
    auto p = harness.progress(unknown);
    EXPECT_EQ(p.state, EvalState::Idle);
    EXPECT_EQ(p.done, 0);
    EXPECT_EQ(p.total, 0);
    EXPECT_FALSE(harness.report(unknown));

    auto nodes = evalNodes("", kItalian);
    ASSERT_TRUE(nodes.has_value());

    // No driver: start() must not spawn nor crash — every 16.* path stays
    // inert until ensureEngine() was called and succeeded.
    harness.start(evalKeyFor(0, "", kItalian), std::move(*nodes),
                  kEvalMovetimeMs);
    EXPECT_EQ(harness.state(evalKeyFor(0, "", kItalian)), EvalState::Idle);
    EXPECT_FALSE(harness.report(evalKeyFor(0, "", kItalian)));
    harness.stop();
}
