#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <chess/board.h>
#include <chess/movegen.h>
#include <chess/net/messages.h>
#include <chess/uci/engine.h>

#include "clock.h"

struct Client;

class Match {
public:
    // An empty timeControl keeps the legacy unlimited game; otherwise it
    // must parse ("5+3" style) and both sides are clocked from game start.
    // graceMs is the reconnect window (11.2): a mid-game drop holds the
    // match open that long before the absent seat loses on disconnection.
    Match(Client& white, Client& black, const std::string& timeControl = {},
          std::int64_t graceMs = 30000);

    // Read-only seat snapshot for observers (rated/archive hooks, logs).
    // Presence is a moment-in-time fact; ids/names survive a disconnect.
    struct SeatView {
        std::string name;
        std::optional<std::int64_t> playerId;
        bool isBot = false;
        bool present = false;
        // Rating snapshot at pairing — the figure WelcomeMsg/ResumeStateMsg
        // promised at the table, not a later lookup.
        double rating = 1500.0;
        double deviation = 350.0;
    };
    SeatView seat(chess::Color color) const;

    // The per-seat rejoin credential handed out in WelcomeMsg (11.2).
    const std::string& resumeToken(chess::Color color) const;

    // Pairing-time rating resolution lives outside Match (PlayerStore lookups
    // in server.cpp); the resolved figures are stamped onto the seats so
    // WelcomeMsg and later resume resyncs show exactly the same numbers.
    void seedRatings(const std::pair<double, double>& whiteRd,
                     const std::pair<double, double>& blackRd);

    // Currently connected player of a color, or nullptr while absent —
    // the pre-seats API shape callers know (server loop, hooks).
    Client* white() const;
    Client* black() const;
    Client* opponent(const Client& client) const;

    // Observer notified once after GameOverMsg has been broadcast to the
    // present seats (rated updates, archive writes, ...). Failures inside
    // the observer must not disturb the game.
    using GameOverCallback =
        std::function<void(const Match&, chess::net::GameResult, chess::net::GameOverReason)>;
    void setGameOverCallback(GameOverCallback callback) { gameOverCallback_ = std::move(callback); }

    void handleMessage(Client& sender, const chess::net::ClientMessage& msg);
    void handleDisconnect(const Client& client);

    // Token rejoin (11.2): binds a returning connection to one absent seat
    // of a live game, or delivers the finished game's outcome as a ghost.
    // On Rebound, reboundColorOut tells the server whose state to resync.
    enum class ResumeOutcome { Rebound, ReboundOver, AlreadyConnected, NoSeat };
    ResumeOutcome resume(const std::string& token, Client& newcomer,
                         chess::Color& reboundColorOut);
    // Callers that do not care whose seat rebound (unit tests).
    ResumeOutcome resume(const std::string& token, Client& newcomer);

    // Finishing a game does not destroy it: the last outcome stays available
    // for late token rejoins until retention elapses (server loop cleans up).
    std::optional<chess::net::GameOverMsg> lastGameOver() const;
    bool retentionEnded(std::int64_t nowMs) const;

    bool isActive() const;

    // Canonical SAN moves played so far, in order. Empty until the first
    // move is accepted; used by the game-over hook for the archive row.
    const std::vector<std::string>& sanHistory() const { return sanHistory_; }

    // Agreed time control string ("" = unlimited), for the archive row.
    const std::string& timeControl() const { return timeControl_; }

    // Current position for a full ResumeStateMsg resync (11.2).
    std::string currentFen() const { return board_.toFen(); }

    // Both clocks' remaining time at nowMs for a ResumeStateMsg (0 = unlimited).
    std::pair<std::int32_t, std::int32_t> clockSnapshotMs(std::int64_t nowMs) const;

    // Charges a flag fall if the side to move is out of time at nowMs.
    // Called by the server loop between socket events (a player may pass
    // their turn's last millisecond simply by sitting there).
    void pollClock(std::int64_t nowMs);

    // Ends the game if the reconnect grace window (11.2) lapsed with one
    // seat still absent. Runs beside pollClock on every loop pass.
    void pollReconnect(std::int64_t nowMs);

    // True while an absent seat is inside its grace window; the server
    // loop uses this (beside clockRuns) to shorten its select wait.
    bool reconnecting() const;

    // True while an unlimited clock is not active; the server loop uses
    // this to shorten its select wait so flags fire near-precisely.
    bool clockRuns() const { return clock_.active() && active_; }

    void startBot(chess::Color color, int depth, std::string enginePath);
    void stopBot();
    void pollBotMove();
    bool isBotTurn() const;
    Client* botClient() const;

private:
    // The join-persistent identity of one side. Presence (the live Client
    // object) comes and goes with connections; everything else survives a
    // drop so the rated/archive hooks and token rebinding still work.
    struct Seat {
        Client* present = nullptr;
        std::string name;
        std::optional<std::int64_t> playerId;
        bool isBot = false;
        double rating = 1500.0;
        double deviation = 350.0;
        std::string resumeToken;
    };

    std::size_t seatIndexOf(const Client& client) const;
    bool bothSeatsAbsent() const;
    bool graceArmed() const;

    void handleMove(Client& sender, const chess::net::MoveMsg& msg);
    bool handleMoveSAN(Client& sender, const std::string& san);
    void handleDrawOffer(Client& sender);
    void handleDrawAccept(Client& sender);
    void handleDrawDecline(Client& sender);
    void handleResign(Client& sender);
    void handleChat(Client& sender, const chess::net::ChatMsg& msg);
    void handlePing(Client& sender);

    void endGame(chess::net::GameResult result, chess::net::GameOverReason reason);
    std::pair<chess::net::GameResult, chess::net::GameOverReason> classifyState(chess::GameState state) const;

    std::string timeControl_;
    chess::server::MatchClock clock_;

    chess::Board board_;
    Seat seats_[2]; // [0] = White, [1] = Black
    bool active_ = true;
    bool drawOfferPending_ = false;

    // Reconnect (11.2): one seat absent inside its grace window (both absent
    // ends the game immediately, so at most one seat can be away).
    std::int64_t graceMs_ = 30000;
    std::int64_t graceDeadlineMs_ = 0;
    bool graceRunning_ = false;

    // Finished-game retention for late token rejoins (11.2).
    std::optional<chess::net::GameOverMsg> lastGameOver_;
    std::int64_t finishedAtMs_ = 0;

    std::unique_ptr<chess::uci::UciEngine> botEngine_;
    chess::Color botColor_ = chess::Color::White;
    int botDepth_ = 1;
    bool botThinking_ = false;
    std::vector<std::string> sanHistory_;
    GameOverCallback gameOverCallback_;
};
