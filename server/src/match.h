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
    // An empty startFen (12.1) is the standard start position; otherwise it
    // must parse into a legal board (Chess960 SP). graceMs is the reconnect
    // window (11.2): a mid-game drop holds the match open that long before
    // the absent seat loses on disconnection.
    Match(Client& white, Client& black, const std::string& timeControl = {},
          const std::string& startFen = {}, std::int64_t graceMs = 30000);

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

    // Arena games (12.3): the game carries its arena's key ("tc|variant",
    // same form the server's lobby map uses) so the game-over hook can
    // update the right standings, and rematch offers never fire — the
    // arena's_next pairing IS the "again" channel.
    void setArenaKey(std::string key);
    bool isArena() const { return !arenaKey_.empty(); }
    const std::string& arenaKey() const { return arenaKey_; }

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

    // Takeback (11.3): true while a request is out for the asked seat to
    // agree. One at a time; dissolved by the next accepted move, a decline,
    // a disconnect of either seat, or the game ending.
    bool takebackPending() const { return takebackPending_.has_value(); }

    // Rematch (11.7), the post-game counterpart: true while an offer is out
    // on a FINISHED match (retention window) for the asked seat to agree.
    // Dissolved by a decline, the requester's self-cancel, either seat's
    // departure, or the retention lapse erasing the match.
    bool rematchPending() const { return rematchPending_.has_value(); }

    // Fired exactly once per accepted rematch, while both seats are still
    // attached in their game-over seats: the server swaps colors, dissolves
    // this match's seats and pairs the same clients into a fresh match.
    using RematchCallback = std::function<void(Match&)>;
    void setRematchCallback(RematchCallback callback)
    {
        rematchCallback_ = std::move(callback);
    }

    // Voluntary departure from a finished match's game-over seat (the
    // client requeues or resumes elsewhere). The seat is released; a
    // pending rematch offer dissolves with a decline notice to the
    // requester (withdrawn when the requester is the one leaving).
    void leave(const Client& client);

    // Clears every seat's presence and any pending rematch without
    // notification (11.7): the server calls this when a rematch just
    // re-seated both clients into a fresh match — this one lingers for
    // its token-resume retention only, and stale seat pointers must not
    // survive clients that moved on.
    void detachAllSeats();

    // Agreed time control string ("" = unlimited), for the archive row.
    const std::string& timeControl() const { return timeControl_; }

    // The exact starting FEN the game was played from (12.1): empty = the
    // standard start position. Feeds the archive row and every SAN-chain
    // replay (takeback rollback); the board itself is built from it.
    const std::string& startFen() const { return startFen_; }

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
    // True once the finished match's retention window has lapsed but the
    // server loop's sweep has not torn the game-over seats down yet; the
    // rematch handlers fold this stale frame into "session expired".
    bool retentionLapsed() const;

    void handleMove(Client& sender, const chess::net::MoveMsg& msg);
    bool handleMoveSAN(Client& sender, const std::string& san);
    void handleDrawOffer(Client& sender);
    void handleDrawAccept(Client& sender);
    void handleDrawDecline(Client& sender);
    void handleTakebackRequest(Client& sender);
    void handleTakebackAccept(Client& sender);
    void handleTakebackDecline(Client& sender);
    void handleRematchRequest(Client& sender);
    void handleRematchAccept(Client& sender);
    void handleRematchDecline(Client& sender);
    void handleResign(Client& sender);
    void handleChat(Client& sender, const chess::net::ChatMsg& msg);
    void handlePing(Client& sender);

    void endGame(chess::net::GameResult result, chess::net::GameOverReason reason);
    std::pair<chess::net::GameResult, chess::net::GameOverReason> classifyState(chess::GameState state) const;
    void applyTakeback(std::size_t requesterIdx);

    std::string timeControl_;
    // The game's starting FEN (12.1), empty for the standard start; drives
    // every SAN-chain reconstruction (takeback rollback) beside the board.
    std::string startFen_;
    chess::server::MatchClock clock_;

    chess::Board board_;
    Seat seats_[2]; // [0] = White, [1] = Black
    bool active_ = true;
    bool drawOfferPending_ = false;

    // Takeback (11.3): the pending request's asker seat index while the
    // offer is out.
    std::optional<std::size_t> takebackPending_;

    // Rematch (11.7): the pending offer's asker seat index, valid only on
    // a finished match (retention window). The fresh match on acceptance
    // is built by the server-side rematch callback, never here.
    std::optional<std::size_t> rematchPending_;

    // Arena membership (12.3): set by the server at pairing for tournament
    // games. Empty = a normal queue/rating game.
    std::string arenaKey_;

    // Releases the pending rematch with a decline notice to the offer's
    // asker; withdrawn=true when the asker themself is the one leaving.
    void dissolveRematch(bool withdrawn);

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
    RematchCallback rematchCallback_;
};
