#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <chess/board.h>
#include <chess/move.h>

namespace chess::uci {

struct EngineInfo {
    std::string name;
    std::string author;
};

struct SearchInfo {
    int depth = 0;
    int seldepth = 0;
    int score_cp = 0;
    bool score_is_mate = false;
    int mate_in = 0;
    std::vector<std::string> pv;
    int time_ms = 0;
    uint64_t nodes = 0;
    uint64_t nps = 0;
};

class UciEngine {
public:
    explicit UciEngine(std::string enginePath);
    ~UciEngine();

    UciEngine(const UciEngine&) = delete;
    UciEngine& operator=(const UciEngine&) = delete;

    EngineInfo init(std::chrono::milliseconds timeout = std::chrono::seconds(5));
    void setOption(const std::string& name, const std::string& value);
    void newGame();
    void position(const std::string& fen,
                  const std::vector<std::string>& moves = {});
    void go(int depth = 0, int movetime_ms = 0, bool infinite = false);
    void stop();
    void quit();

    std::optional<Move> waitBestMove(const Board& board,
                                     std::chrono::milliseconds timeout = std::chrono::seconds(30));
    std::optional<Move> tryGetBestMove(const Board& board);

    void onInfo(std::function<void(const SearchInfo&)> cb);
    void onLine(std::function<void(const std::string&)> cb);

    bool isRunning() const;

private:
    void send(const std::string& cmd);
    void readerLoop();
    void closeProcess();

    enum class SyncSignal { None, UciOk, ReadyOk };

    std::string m_enginePath;
    int m_stdinFd = -1;
    int m_stdoutFd = -1;
    int m_pid = -1;

    std::thread m_reader;
    std::atomic<bool> m_running{false};

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::string m_bestmove;
    SyncSignal m_syncSignal = SyncSignal::None;
    std::string m_engineName;
    std::string m_engineAuthor;

    std::mutex m_cbMutex;
    std::function<void(const SearchInfo&)> m_onInfo;
    std::function<void(const std::string&)> m_onLine;
};

} // namespace chess::uci
