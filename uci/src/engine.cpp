#include <chess/uci/engine.h>
#include <chess/uci_move.h>
#include <chess/movegen.h>

#include <cstring>
#include <sstream>
#include <thread>

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
    if (pipe(stdinPipe) != 0 || pipe(stdoutPipe) != 0) {
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
        execl(m_enginePath.c_str(), m_enginePath.c_str(), static_cast<char*>(nullptr));
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
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        bool ok = m_cv.wait_for(lock, timeout, [this] {
            return m_syncSignal == SyncSignal::UciOk;
        });
        if (!ok) {
            quit();
            return {};
        }
        info.name = m_engineName;
        info.author = m_engineAuthor;
        m_syncSignal = SyncSignal::None;
    }

    send("isready");
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        bool ok = m_cv.wait_for(lock, timeout, [this] {
            return m_syncSignal == SyncSignal::ReadyOk;
        });
        m_syncSignal = SyncSignal::None;
        if (!ok) {
            quit();
            return {};
        }
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
    m_cv.wait_for(lock, std::chrono::seconds(5), [this] {
        return m_syncSignal == SyncSignal::ReadyOk;
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

    if (m_stdoutFd >= 0) {
        ::close(m_stdoutFd);
        m_stdoutFd = -1;
    }

    if (m_reader.joinable()) {
        m_reader.join();
    }

    closeProcess();
}

std::optional<Move> UciEngine::waitBestMove(const Board& board,
                                            std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    bool found = m_cv.wait_for(lock, timeout, [this] {
        return !m_bestmove.empty();
    });

    if (!found || m_bestmove.empty()) {
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
    std::string uciStr = m_bestmove;
    m_bestmove.clear();
    if (uciStr == "(none)") return std::nullopt;
    return fromUci(board, uciStr);
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
    std::string data = cmd + "\n";
    ::write(m_stdinFd, data.c_str(), data.size());
}

void UciEngine::readerLoop()
{
    std::string line;
    char c;
    while (true) {
        ssize_t n = ::read(m_stdoutFd, &c, 1);
        if (n <= 0) {
            m_running = false;
            m_cv.notify_all();
            return;
        }

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
}

void UciEngine::closeProcess()
{
    if (m_stdinFd >= 0) { ::close(m_stdinFd); m_stdinFd = -1; }
    if (m_stdoutFd >= 0) { ::close(m_stdoutFd); m_stdoutFd = -1; }
    if (m_pid > 0) {
        int status;
        pid_t result = waitpid(m_pid, &status, WNOHANG);
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
}

} // namespace chess::uci
