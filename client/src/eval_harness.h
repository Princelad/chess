#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <chess/board.h>

namespace chess::client {

// --- 16.1 eval harness -------------------------------------------------------
// Batch evaluation of every position of a finished game in a background
// thread. The production driver talks to one UCI engine spawned lazily on
// demand; everything the CI suite touches is pure or driver-injected — the
// harness never spawns a process from tests.

// Game identity: an archive id when the game is archived (nonzero), else an
// FNV-1a hash over the start FEN + SAN chain (local/offline games, which have
// no server row). Two identical anonymous games are deliberately one identity
// (same content = same evals, cache reuse is the point).
struct EvalKey
{
    std::int64_t archiveId = 0;
    std::uint64_t sanHash = 0; // 0 only when archiveId != 0

    friend constexpr bool operator==(const EvalKey& a, const EvalKey& b)
    {
        return a.archiveId == b.archiveId && a.sanHash == b.sanHash;
    }
    friend constexpr bool operator!=(const EvalKey& a, const EvalKey& b)
    {
        return !(a == b);
    }
    // Map key ordering (archiveId, then hash — no cross-field collisions are
    // meaningful beyond ordering stability).
    friend constexpr bool operator<(const EvalKey& a, const EvalKey& b)
    {
        return a.archiveId != b.archiveId ? a.archiveId < b.archiveId
                                          : a.sanHash < b.sanHash;
    }
};

// FNV-1a over the start FEN followed by each SAN token (same hash family as
// chat_log/avatars).
std::uint64_t evalGameHash(const std::string& startFen,
                           const std::vector<std::string>& sans);

// Identity for a finished game; the hash path is used when archiveId == 0.
inline EvalKey evalKeyFor(std::int64_t archiveId, const std::string& startFen,
                          const std::vector<std::string>& sans)
{
    if (archiveId != 0) return {archiveId, 0};
    return {0, evalGameHash(startFen, sans)};
}

// One position of a finished game.
struct EvalNode
{
    std::string fen; // the position BEFORE the SAN move (mover to move)
    std::string san; // the SAN token of the move played
};

// Chains startFen + SAN tokens into one node per ply (an empty startFen means
// the standard start). Returns nullopt on the first unresolvable position or
// move — a game with junk SAN is refused whole, never half-reported.
std::optional<std::vector<EvalNode>> evalNodes(
    const std::string& startFen, const std::vector<std::string>& sans);

// Score for one node, from the MOVER's seat (the side to move in node.fen —
// 16.2's win%-loss deltas read exactly that). bestSan is the engine's own
// choice SAN when known (empty otherwise). mateIn is SIGNED: positive the
// mover mates in N, negative the mover is getting mated in N. (Named
// NodeEval, not PlyEval — analysis_screen.h owns PlyEval already.)
struct NodeEval
{
    int scoreCp = 0;
    bool isMate = false;
    int mateIn = 0;    // signed: >0 mover mates in N, <0 mover is mated in N
    std::string bestSan;

    bool operator==(const NodeEval& other) const
    {
        return scoreCp == other.scoreCp && isMate == other.isMate
            && mateIn == other.mateIn && bestSan == other.bestSan;
    }
};

// One evaluated position; valid stays false until the driver scored it.
struct NodeResult
{
    NodeEval eval;
    bool valid = false;
};

struct EvalReport
{
    std::vector<EvalNode> nodes; // one per ply
    std::vector<NodeResult> evals; // parallel to nodes
    bool failed = false;         // the engine died mid-batch (resumable)
};

// The engine-facing abstraction. Concrete impls: UciEvalDriver (production,
// eval_harness.cpp) and the test suite's canned drivers (no process spawns).
class EvalDriver
{
public:
    virtual ~EvalDriver() = default;

    // Scores fen with the given per-node time budget from the mover's seat.
    // False = engine failure (the ply stays unscored and is retried on a
    // later start).
    virtual bool evaluate(const std::string& fen, int movetimeMs,
                          NodeEval& out) = 0;

    // Polls-into-stop hook: production UciDriver forwards engine stop() so a
    // cancelled job flushes promptly instead of idling out the movetime.
    virtual void requestStop() {}
};

enum class EvalState
{
    /// Nothing cached for this key yet (or a paused job with unscored plies).
    Idle,
    /// A worker is filling the report right now.
    Running,
    /// Every ply scored.
    Done,
    /// The last run failed (driver refusal); resumable via start().
    Failed,
};

// App-owned batch evaluator: one engine process, one worker thread at a
// time, per-run cache keyed by EvalKey. The render loop only ever polls
// (progress()/report() never block) — the 8.2 discipline.
class EvalHarness
{
public:
    // Production: no engine is spawned until ensureEngine() is called.
    EvalHarness() = default;
    // Test: a caller-provided driver stands in for the engine; hasEngine()
    // reports true immediately and no process can ever be spawned.
    explicit EvalHarness(std::unique_ptr<EvalDriver> driver);
    ~EvalHarness();

    EvalHarness(const EvalHarness&) = delete;
    EvalHarness& operator=(const EvalHarness&) = delete;

    // True only after a successful ensureEngine() (or an injected driver).
    // Must NEVER spawn anything — callers hide all 16.* UI on false.
    bool hasEngine() const { return enginePresent_; }

    // Spawns the engine once, on demand. False = no usable engine found.
    bool ensureEngine();

    // Starts (or resumes) evaluation of the given game. Repeated starts on
    // the KEY being evaluated or already done are no-ops; a different key is
    // ignored while a job runs (call stop() first). Resumes from the first
    // unscored ply — cached results never recompute.
    void start(EvalKey key, std::vector<EvalNode> nodes, int movetimeMs);

    // Cancels the running job (join is bounded by one node otherwise);
    // evaluated plies stay cached. Safe when idle.
    void stop();

    struct Progress
    {
        EvalState state = EvalState::Idle;
        int done = 0;
        int total = 0;
    };

    // Polled from update(): never blocks.
    Progress progress(EvalKey key) const;
    EvalState state(EvalKey key) const { return progress(key).state; }

    // Snapshot of the report as of this instant (a consistent copy — the
    // worker keeps filling its own). Null while the key is unknown.
    std::shared_ptr<const EvalReport> report(EvalKey key) const;

private:
    struct Job
    {
        std::shared_ptr<EvalReport> report;
        bool failed = false;
    };

    void workerLoop(EvalKey key, int firstPly, int movetimeMs);

    std::unique_ptr<EvalDriver> driver_;
    bool enginePresent_ = false;

    mutable std::mutex m_;
    std::map<EvalKey, std::unique_ptr<Job>> jobs_;
    EvalKey currentKey_{};
    bool running_ = false;
    bool cancel_ = false; // guarded by m_
    std::thread worker_;
};

// Per-position time budget: a constant for 16.1 (tuning revisited at the
// 16.3 report probe).
inline constexpr int kEvalMovetimeMs = 400;

} // namespace chess::client
