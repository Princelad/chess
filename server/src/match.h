#pragma once

#include <functional>
#include <memory>
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
    Match(Client& white, Client& black, const std::string& timeControl = {});

    // Observer notified once after GameOverMsg has been broadcast to both
    // players (used by the server for rated updates, archive writes, ...).
    // Failures inside the observer must not disturb the game.
    using GameOverCallback =
        std::function<void(const Match&, chess::net::GameResult, chess::net::GameOverReason)>;
    void setGameOverCallback(GameOverCallback callback) { gameOverCallback_ = std::move(callback); }

    void handleMessage(Client& sender, const chess::net::ClientMessage& msg);
    void handleDisconnect(const Client& client);

    bool isActive() const;
    Client* white() const;
    Client* black() const;
    Client* opponent(const Client& client) const;

    // Canonical SAN moves played so far, in order. Empty until the first
    // move is accepted; used by the game-over hook for the archive row.
    const std::vector<std::string>& sanHistory() const { return sanHistory_; }

    // Agreed time control string ("" = unlimited), for the archive row.
    const std::string& timeControl() const { return timeControl_; }

    // Charges a flag fall if the side to move is out of time at nowMs.
    // Called by the server loop between socket events (a player may pass
    // their turn's last millisecond simply by sitting there).
    void pollClock(std::int64_t nowMs);

    // True while an unlimited clock is not active; the server loop uses
    // this to shorten its select wait so flags fire near-precisely.
    bool clockRuns() const { return clock_.active() && active_; }

    void startBot(chess::Color color, int depth, std::string enginePath);
    void stopBot();
    void pollBotMove();
    bool isBotTurn() const;
    Client* botClient() const;

private:
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
    Client* white_;
    Client* black_;
    bool active_ = true;
    bool drawOfferPending_ = false;

    std::unique_ptr<chess::uci::UciEngine> botEngine_;
    chess::Color botColor_ = chess::Color::White;
    int botDepth_ = 1;
    bool botThinking_ = false;
    std::vector<std::string> sanHistory_;
    GameOverCallback gameOverCallback_;
};
