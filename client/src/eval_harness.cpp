#include "eval_harness.h"

#include <chess/san.h>
#include <chess/uci/engine.h>

#include <cstdlib>

namespace chess::client {

std::uint64_t evalGameHash(const std::string& startFen,
                           const std::vector<std::string>& sans)
{
    // FNV-1a, same constants as chat_log.h / avatars.h; 0xFF separates tokens
    // (a zero XOR would be a no-op, so empty-vs-absent tokens stay distinct).
    constexpr std::uint64_t kPrime = 1099511628211ull;
    std::uint64_t hash = 1469598103934665603ull;
    auto mixToken = [&hash](const std::string& token) {
        for (unsigned char c : token) {
            hash ^= c;
            hash *= kPrime;
        }
        hash ^= 0xFFu; // separator
        hash *= kPrime;
    };
    mixToken(startFen);
    for (const auto& san : sans) mixToken(san);
    return hash;
}

std::optional<std::vector<EvalNode>> evalNodes(
    const std::string& startFen, const std::vector<std::string>& sans)
{
    if (sans.empty()) return std::nullopt;

    auto board = startFen.empty() ? Board::fromStartPos()
                                  : Board::fromFen(startFen);
    if (!board) return std::nullopt;

    std::vector<EvalNode> nodes;
    nodes.reserve(sans.size());
    for (const auto& san : sans) {
        if (san.empty()) return std::nullopt;
        EvalNode node;
        node.fen = board->toFen();
        node.san = san;
        auto move = san::fromSan(*board, san);
        if (!move) return std::nullopt;
        board->makeMove(*move);
        nodes.push_back(std::move(node));
    }
    return nodes;
}

// --------------------------------------------------------------------------
// Production driver: one lazily-spawned UciEngine.
// --------------------------------------------------------------------------

class UciEvalDriver : public EvalDriver
{
public:
    ~UciEvalDriver() override
    {
        if (engine_) engine_->quit();
    }

    bool startEngine()
    {
        // Same detection as analysis_screen / local play: env override first.
        const char* envPath = std::getenv("CHESS_ENGINE_PATH");
        std::string enginePath = envPath ? envPath : "stockfish";
        engine_ = std::make_unique<uci::UciEngine>(enginePath);
        auto info = engine_->init();
        if (!engine_->isRunning() && info.name.empty()) {
            engine_.reset();
            return false;
        }
        engine_->setOption("Threads", "1");
        engine_->setOption("Hash", "64");
        engine_->newGame();
        engine_->onInfo([this](const uci::SearchInfo& si) {
            std::lock_guard<std::mutex> lock(infoMutex_);
            lastInfo_ = si;
            gotInfo_ = true;
        });
        return true;
    }

    bool evaluate(const std::string& fen, int movetimeMs, NodeEval& out) override
    {
        if (!engine_ || !engine_->isRunning()) return false;
        // Board parse doubles as the mover seat fromUci needs for bestmove.
        auto board = Board::fromFen(fen);
        if (!board) return false;

        {
            std::lock_guard<std::mutex> lock(infoMutex_);
            lastInfo_ = uci::SearchInfo{};
            gotInfo_ = false;
        }
        engine_->position(fen);
        engine_->go(0, movetimeMs, false);
        auto move = engine_->waitBestMove(
            *board, std::chrono::milliseconds(movetimeMs + 15000));
        if (!move) return false; // timed out / engine died mid-search

        {
            std::lock_guard<std::mutex> lock(infoMutex_);
            // No info line at all (audit: an engine that streams nothing
            // would be recorded as a fake 0.00 "even" eval — its default
            // score_cp is 0). Treat as an invalid node instead; the
            // report/graph refuse unvalid nodes rather than plotting it.
            if (!gotInfo_) return false;
            // Mate scores bypass score_cp on some engines; record both.
            out.scoreCp = lastInfo_.score_cp;
            out.isMate = lastInfo_.score_is_mate;
            out.mateIn = static_cast<int>(lastInfo_.mate_in);
        }
        out.bestSan = san::toSan(*board, *move);
        return true;
    }

    void requestStop() override
    {
        if (engine_) engine_->stop();
    }

private:
    std::unique_ptr<uci::UciEngine> engine_;
    std::mutex infoMutex_;
    uci::SearchInfo lastInfo_;
    bool gotInfo_ = false;
};

// --------------------------------------------------------------------------
// Harness
// --------------------------------------------------------------------------

EvalHarness::EvalHarness(std::unique_ptr<EvalDriver> driver)
    : driver_(std::move(driver)), enginePresent_(true)
{
}

EvalHarness::~EvalHarness()
{
    stop();
}

bool EvalHarness::ensureEngine()
{
    if (driver_) return enginePresent_;

    auto candidate = std::make_unique<UciEvalDriver>();
    if (!candidate->startEngine()) return false;
    driver_ = std::move(candidate);
    enginePresent_ = true;
    return true;
}

void EvalHarness::start(EvalKey key, std::vector<EvalNode> nodes,
                        int movetimeMs)
{
    int firstPly = -1; // >= 0 = spawn needed
    {
        std::lock_guard<std::mutex> lock(m_);
        if (!driver_) return; // never spawn from the UI path (16.1 contract)
        if (running_) return; // one job at a time; stop() to switch keys

        auto& job = jobs_[key];
        if (!job) job = std::make_unique<Job>();
        if (!job->report) {
            auto report = std::make_shared<EvalReport>();
            report->nodes = std::move(nodes);
            report->evals.assign(report->nodes.size(), NodeResult{});
            job->report = std::move(report);
        }
        // Same key re-start: nodes are identical by identity definition —
        // only the unscored tail is (re)run.
        firstPly = 0;
        for (const auto& pe : job->report->evals)
            if (pe.valid) ++firstPly;
        if (firstPly >= static_cast<int>(job->report->nodes.size())) {
            firstPly = -1; // already done
            return;
        }

        job->failed = false;
        currentKey_ = key;
        running_ = true;
        cancel_ = false;
    }

    // Join the previous (idle) worker OUTSIDE the lock — it needs m_ for its
    // final statement, so joining under m_ would deadlock. Safe: single
    // caller thread, and the stale thread is never running here.
    if (firstPly >= 0) {
        if (worker_.joinable()) worker_.join();
        worker_ = std::thread(&EvalHarness::workerLoop, this, key, firstPly,
                              movetimeMs);
    }
}

void EvalHarness::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_);
        cancel_ = true;
        if (driver_) driver_->requestStop(); // flush the in-flight search
    }
    if (worker_.joinable()) worker_.join();
    std::lock_guard<std::mutex> lock(m_);
    cancel_ = false;
}

EvalHarness::Progress EvalHarness::progress(EvalKey key) const
{
    std::lock_guard<std::mutex> lock(m_);
    auto it = jobs_.find(key);
    if (it == jobs_.end() || !it->second->report)
        return {};

    const Job& job = *it->second;
    const int total = static_cast<int>(job.report->evals.size());
    int done = 0;
    for (const auto& pe : job.report->evals)
        if (pe.valid) ++done;

    if (running_ && currentKey_ == key)
        return {EvalState::Running, done, total};
    if (job.failed) return {EvalState::Failed, done, total};
    if (total > 0 && done == total) return {EvalState::Done, done, total};
    return {EvalState::Idle, done, total};
}

std::shared_ptr<const EvalReport> EvalHarness::report(EvalKey key) const
{
    std::lock_guard<std::mutex> lock(m_);
    auto it = jobs_.find(key);
    if (it == jobs_.end() || !it->second->report) return nullptr;
    // A consistent copy: the worker mutates its own vector in place — sharing
    // the pointer would be a data race with the render loop's read.
    return std::make_shared<const EvalReport>(*it->second->report);
}

void EvalHarness::workerLoop(EvalKey key, int firstPly, int movetimeMs)
{
    for (int index = firstPly;; ++index) {
        std::string fen;
        {
            std::lock_guard<std::mutex> lock(m_);
            if (cancel_ || !driver_) break;
            auto& job = *jobs_.at(key);
            if (index >= static_cast<int>(job.report->nodes.size())) break;
            fen = job.report->nodes[index].fen;
        }

        NodeEval pe;
        bool ok = driver_->evaluate(fen, movetimeMs, pe);

        {
            std::lock_guard<std::mutex> lock(m_);
            if (cancel_) break; // in-flight node result discarded on stop
            auto& job = *jobs_.at(key);
            if (!ok) {
                job.failed = true;
                break;
            }
            NodeResult nr;
            nr.eval = pe;
            nr.valid = true;
            job.report->evals[index] = nr;
        }
    }
    std::lock_guard<std::mutex> lock(m_);
    running_ = false;
}

} // namespace chess::client
