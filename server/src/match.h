#pragma once

#include <utility>

#include <chess/board.h>
#include <chess/movegen.h>
#include <chess/net/messages.h>

struct Client;

class Match {
public:
    Match(Client& white, Client& black);

    void handleMessage(Client& sender, const chess::net::ClientMessage& msg);
    void handleDisconnect(const Client& client);

    bool isActive() const;
    Client* white() const;
    Client* black() const;
    Client* opponent(const Client& client) const;

private:
    void handleMove(Client& sender, const chess::net::MoveMsg& msg);
    void handleDrawOffer(Client& sender);
    void handleDrawAccept(Client& sender);
    void handleDrawDecline(Client& sender);
    void handleResign(Client& sender);
    void handleChat(Client& sender, const chess::net::ChatMsg& msg);
    void handlePing(Client& sender);

    void endGame(chess::net::GameResult result, chess::net::GameOverReason reason);
    std::pair<chess::net::GameResult, chess::net::GameOverReason> classifyState(chess::GameState state) const;

    chess::Board board_;
    Client* white_;
    Client* black_;
    bool active_ = true;
    bool drawOfferPending_ = false;
};
