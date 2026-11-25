#include <chess/uci/engine.h>
#include <chess/uci_move.h>
#include <chess/movegen.h>

#include <cerrno>
#include <cstring>
#include <sstream>
#include <thread>

#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace chess::uci {

UciEngine::UciEngine(std::string enginePath)
    : m_enginePath(std::move(enginePath))
{
    signal(SIGPIPE, SIG_IGN);
}

UciEngine::~UciEngine()
{
    if (m_running.load()) {
        quit();
    }
    closeProcess();
}

EngineInfo UciEngine::init(std::chrono::milliseconds timeout)
{
    if (m_running.load() || m_pid > 0) {
        quit();
    }
    int stdinPipe[2];
    int stdoutPipe[2];
    if (pipe(stdinPipe) != 0) {
        return {};
    }
    if (pipe(stdoutPipe) != 0) {
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        return {};
    }

    m_pid = fork();
    if (m_pid < 0) {
        close(stdinPipe[0]); close(stdinPipe[1]);
        close(stdoutPipe[0]); close(stdoutPipe[1]);
        return {};
    }

    if (m_pid == 0) {
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        close(stdinPipe[0]);
        close(stdoutPipe[1]);
        // The engine must see only 0/1/2: an inherited TCP socket keeps the
        // peer connected after the parent disconnects, and an inherited
        // listener keeps the port bound after a crash. FD_SETSIZE is the
        // ceiling that matters — SFML's selector is select()-backed.
        for (int fd = 3; fd < 1024; ++fd) close(fd);
        // Bare names such as "stockfish" must resolve through PATH.
        execlp(m_enginePath.c_str(), m_enginePath.c_str(), static_cast<char*>(nullptr));
        _exit(1);
    }

    close(stdinPipe[0]);
    close(stdoutPipe[1]);
    m_stdinFd = stdinPipe[1];
    m_stdoutFd = stdoutPipe[0];

    m_running = true;
    m_reader = std::thread(&UciEngine::readerLoop, this);

    send("uci");

    EngineInfo info;
    bool failed = false;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        // Death-aware: a reader that exited (process gone) ends the wait
        // immediately instead of running out the whole timeout on the
        // caller's thread — the engine-less client screens and bot spawns
        // used to freeze for the full timeout per attempt.
        const bool ok = m_cv.wait_for(lock, timeout, [this] {
            return m_syncSignal == SyncSignal::UciOk || !m_running.load();
        });
        if (ok && m_running.load()) {
            info.name = m_engineName;
            info.author = m_engineAuthor;
        } else {
            failed = true;
        }
        m_syncSignal = SyncSignal::None;
    }
    // quit() joins the reader, and the reader needs m_mutex to publish its
    // handshake lines: calling quit() under the lock deadlocked both threads
    // whenever the reply arrived just after the timeout.
    if (failed) {
        quit();
        return {};
    }

    send("isready");
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        const bool ok = m_cv.wait_for(lock, timeout, [this] {
            return m_syncSignal == SyncSignal::ReadyOk || !m_running.load();
        });
        m_syncSignal = SyncSignal::None;
        failed = !ok || !m_running.load();
    }
    if (failed) {
        quit();
        return {};
    }

    return info;
}

void UciEngine::setOption(const std::string& name, const std::string& value)
{
    if (value.empty()) {
        send("setoption name " + name);
    } else {
        send("setoption name " + name + " value " + value);
    }
}

void UciEngine::newGame()
{
    send("ucinewgame");
    send("isready");
    std::unique_lock<std::mutex> lock(m_mutex);
    // Death-aware (see init): a dead engine must not stall the caller for
    // the whole timeout — later commands fail fast via pollBestMove.
    m_cv.wait_for(lock, std::chrono::seconds(5), [this] {
        return m_syncSignal == SyncSignal::ReadyOk || !m_running.load();
    });
    m_syncSignal = SyncSignal::None;
}

void UciEngine::position(const std::string& fen,
                         const std::vector<std::string>& moves)
{
    std::string cmd = "position ";
    if (fen == "startpos" || fen.empty()) {
        cmd += "startpos";
    } else {
        cmd += "fen " + fen;
    }
    if (!moves.empty()) {
        cmd += " moves";
        for (const auto& m : moves) {
            cmd += " " + m;
        }
    }
    send(cmd);
}

void UciEngine::go(int depth, int movetime_ms, bool infinite)
{
    // Audit 7.3: a new search invalidates every answer of an older one —
    // a bestmove that arrives after this point belongs to a position the
    // caller has already given up on.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_searchGen;
    }
    std::string cmd = "go";
    if (depth > 0) {
        cmd += " depth " + std::to_string(depth);
    }
    if (movetime_ms > 0) {
        cmd += " movetime " + std::to_string(movetime_ms);
    }
    if (infinite) {
        cmd += " infinite";
    }
    send(cmd);
}

void UciEngine::stop()
{
    send("stop");
}

void UciEngine::quit()
{
    send("quit");
    // Never close the stdout fd here: the reader thread is blocked reading
    // it, and closing it under that read is both a data race on m_stdoutFd
    // and a possible read on a recycled descriptor. closeProcess() lets the
    // child exit first, joins the reader, and only then closes the pipe.
    closeProcess();
}

std::optional<Move> UciEngine::waitBestMove(const Board& board,
                                            std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    // Death-aware (audit: an engine that dies mid-search wakes the waiter
    // instead of running out the whole timeout) and generation-aware
    // (audit 7.3): an answer of an OLDER search never satisfies the wait —
    // the stale slot stays queued until the current search's own reply
    // overwrites it.
    bool found = m_cv.wait_for(lock, timeout, [this] {
        return (!m_bestmove.empty() && m_bestmoveGen == m_searchGen
                && m_searchGen > 0)
            || !m_running.load();
    });

    if (!found || m_bestmove.empty()) {
        return std::nullopt;
    }
    // Found can also mean "the engine died", in which case a queued answer
    // from an older search is still sitting in the slot — never hand it out
    // for the current position (audit 7.3 applies to every consumer).
    if (m_bestmoveGen != m_searchGen || m_searchGen == 0) {
        m_bestmove.clear();
        return std::nullopt;
    }

    std::string uciStr = m_bestmove;
    m_bestmove.clear();

    if (uciStr == "(none)") {
        return std::nullopt;
    }

    return fromUci(board, uciStr);
}

std::optional<Move> UciEngine::tryGetBestMove(const Board& board)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_bestmove.empty()) return std::nullopt;
    // Audit 7.3: an older search's answer is garbage for the current
    // position — discard it on sight instead of handing it to the caller.
    if (m_bestmoveGen != m_searchGen || m_searchGen == 0) {
        m_bestmove.clear();
        return std::nullopt;
    }
    std::string uciStr = m_bestmove;
    m_bestmove.clear();
    if (uciStr == "(none)") return std::nullopt;
    return fromUci(board, uciStr);
}

UciEngine::PollResult UciEngine::pollBestMove(const Board& board, Move& out)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_bestmove.empty()) {
            // Audit 7.3: a stale answer (older search's reply, or an
            // unsolicited one before any go) is dropped so the caller
            // keeps waiting for the current search's own answer.
            if (m_bestmoveGen != m_searchGen || m_searchGen == 0) {
                m_bestmove.clear();
            } else {
                std::string uciStr = m_bestmove;
                m_bestmove.clear();
                if (uciStr != "(none)") {
                    if (auto move = fromUci(board, uciStr)) {
                        out = *move;
                        return PollResult::Move;
                    }
                    // Consumed but unusable: the caller must stop waiting on
                    // this answer — it will never come back (audit 1.3).
                    return PollResult::Rejected;
                }
                return PollResult::Rejected; // "(none)"
            }
        }
    }
    // No answer queued: is the process even alive? (The answer could
    // land between this check and the caller's next poll — harmless.)
    return isRunning() ? PollResult::NoAnswer : PollResult::Dead;
}

void UciEngine::onInfo(std::function<void(const SearchInfo&)> cb)
{
    std::lock_guard<std::mutex> lock(m_cbMutex);
    m_onInfo = std::move(cb);
}

void UciEngine::onLine(std::function<void(const std::string&)> cb)
{
    std::lock_guard<std::mutex> lock(m_cbMutex);
    m_onLine = std::move(cb);
}

bool UciEngine::isRunning() const
{
    return m_running.load();
}

void UciEngine::send(const std::string& cmd)
{
    if (m_stdinFd < 0) return;
    const std::string data = cmd + "\n";
    // A short write would truncate the command and leave the caller waiting
    // out its full timeout for a reply to half a line; EINTR is retried.
    std::size_t off = 0;
    while (off < data.size()) {
        const ssize_t n = ::write(m_stdinFd, data.data() + off,
                                  data.size() - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            return; // EPIPE etc: the reader notices the death
        }
        off += static_cast<std::size_t>(n);
    }
}

void UciEngine::readerLoop()
{
    std::string line;
    char c;
    while (m_running.load()) {
        // Bounded poll instead of a blocking read: teardown must not depend
        // on the child closing stdout (a grandchild can keep it open), and
        // the fd must never be closed under a blocked read.
        pollfd pfd;
        pfd.fd = m_stdoutFd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        if (::poll(&pfd, 1, 100) <= 0) continue;

        const ssize_t n = ::read(m_stdoutFd, &c, 1);
        if (n < 0) continue; // EINTR
        if (n == 0) break;   // EOF: the engine closed stdout

        if (c == '\n') {
            if (line.empty()) continue;
            if (line.back() == '\r') line.pop_back();

            {
                std::lock_guard<std::mutex> cbLock(m_cbMutex);
                if (m_onLine) {
                    m_onLine(line);
                }
            }

            if (line.compare(0, 9, "bestmove ") == 0) {
                std::string uciMove = line.substr(9);
                auto space = uciMove.find(' ');
                if (space != std::string::npos) {
                    uciMove = uciMove.substr(0, space);
                }
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_bestmove = uciMove;
                    // Audit 7.3: stamp the answer with the search it
                    // belongs to — a reply that arrives after a newer go
                    // is stamped with the OLD generation and consumers
                    // discard it.
                    m_bestmoveGen = m_searchGen;
                }
                m_cv.notify_all();
            } else if (line.compare(0, 5, "info ") == 0) {
                SearchInfo si;
                std::istringstream iss(line);
                std::string token;
                while (iss >> token) {
                    if (token == "depth" && iss >> si.depth) {}
                    else if (token == "seldepth" && iss >> si.seldepth) {}
                    else if (token == "score") {
                        std::string type;
                        if (iss >> type) {
                            if (type == "cp") iss >> si.score_cp;
                            else if (type == "mate") {
                                si.score_is_mate = true;
                                iss >> si.mate_in;
                            }
                        }
                    }
                    else if (token == "time" && iss >> si.time_ms) {}
                    else if (token == "nodes" && iss >> si.nodes) {}
                    else if (token == "nps" && iss >> si.nps) {}
                    else if (token == "pv") {
                        std::string mv;
                        while (iss >> mv) si.pv.push_back(mv);
                    }
                }
                std::lock_guard<std::mutex> cbLock(m_cbMutex);
                if (m_onInfo) {
                    m_onInfo(si);
                }
            } else if (line.compare(0, 3, "id ") == 0) {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (line.compare(0, 8, "id name ") == 0) {
                    m_engineName = line.substr(8);
                } else if (line.compare(0, 10, "id author ") == 0) {
                    m_engineAuthor = line.substr(10);
                }
            } else if (line == "uciok") {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_syncSignal = SyncSignal::UciOk;
                m_cv.notify_all();
            } else if (line == "readyok") {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_syncSignal = SyncSignal::ReadyOk;
                m_cv.notify_all();
            }

            line.clear();
        } else {
            line += c;
        }
    }
    m_running = false;
    m_cv.notify_all();
}

void UciEngine::closeProcess()
{
    // Close stdin first: EOF makes a well-behaved engine exit, which ends
    // the reader's blocking read. The stdout fd must NOT be closed while
    // the reader may still be reading it — that raced on m_stdoutFd and
    // could read a recycled descriptor.
    if (m_stdinFd >= 0) { ::close(m_stdinFd); m_stdinFd = -1; }
    // Reap the child BEFORE joining the reader: a child that ignores quit
    // never closes stdout, so joining first would hang forever and the
    // SIGTERM/SIGKILL fallback below would be unreachable.
    if (m_pid > 0) {
        int status = 0;
        pid_t result = waitpid(m_pid, &status, WNOHANG);
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(200);
        while (result == 0 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            result = waitpid(m_pid, &status, WNOHANG);
        }
        if (result == 0) {
            kill(m_pid, SIGTERM);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            result = waitpid(m_pid, &status, WNOHANG);
            if (result == 0) {
                kill(m_pid, SIGKILL);
                waitpid(m_pid, &status, 0);
            }
        }
        m_pid = -1;
    }
    // Audit 1.3 follow-up: the reader must be joined here, not only in
    // quit() — a Match destroyed with an already-dead engine skips quit()
    // (m_running is false) and used to destroy a still-joinable reader
    // thread, which calls std::terminate and takes the server down.
    // Clearing m_running wakes the poll loop, so the join is bounded even
    // if something still holds the child's stdout open.
    m_running = false;
    if (m_reader.joinable())
        m_reader.join();
    if (m_stdoutFd >= 0) { ::close(m_stdoutFd); m_stdoutFd = -1; }
}

} // namespace chess::uci
