#include "match.h"

#include <iostream>
#include <string>

#include <chess/movegen.h>
#include <chess/san.h>
#include <chess/net/protocol.h>

#include "client.h"

static bool sendTo(Client& client, const chess::net::ServerMessage& msg)
{
    sf::Packet packet;
    chess::net::serialize(packet, msg);
    return client.socket->send(packet) == sf::Socket::Status::Done;
}

Match::Match(Client& white, Client& black)
    : board_(chess::Board::fromStartPos())
    , white_(&white)
    , black_(&black)
{
}

void Match::handleMessage(Client& sender, const chess::net::ClientMessage& msg)
{
    if (!active_) {
        sendTo(sender, chess::net::ErrorMsg{"Game is over"});
        return;
    }

    std::visit([this, &sender](auto&& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, chess::net::MoveMsg>)
            handleMove(sender, m);
        else if constexpr (std::is_same_v<T, chess::net::DrawOfferMsg>)
            handleDrawOffer(sender);
        else if constexpr (std::is_same_v<T, chess::net::DrawAcceptMsg>)
            handleDrawAccept(sender);
        else if constexpr (std::is_same_v<T, chess::net::DrawDeclineMsg>)
            handleDrawDecline(sender);
        else if constexpr (std::is_same_v<T, chess::net::ResignMsg>)
            handleResign(sender);
        else if constexpr (std::is_same_v<T, chess::net::ChatMsg>)
            handleChat(sender, m);
        else if constexpr (std::is_same_v<T, chess::net::PingMsg>)
            handlePing(sender);
    }, msg);
}

void Match::handleMove(Client& sender, const chess::net::MoveMsg& msg)
{
    if (&sender != (board_.sideToMove() == chess::Color::White ? white_ : black_)) {
        sendTo(sender, chess::net::ErrorMsg{"Not your turn"});
        return;
    }

    auto move = chess::san::fromSan(board_, msg.san);
    if (!move.has_value() || !chess::isLegalMove(board_, *move)) {
        sendTo(sender, chess::net::ErrorMsg{"Illegal move"});
        return;
    }

    std::string san = chess::san::toSan(board_, *move);
    board_.makeMove(*move);

    chess::net::ServerMoveMsg serverMove{san};
    sendTo(*white_, serverMove);
    sendTo(*black_, serverMove);

    drawOfferPending_ = false;

    auto state = chess::evaluateGameState(board_);
    if (state != chess::GameState::Ongoing) {
        auto [result, reason] = classifyState();
        endGame(result, reason);
    }
}

void Match::handleDrawOffer(Client& sender)
{
    drawOfferPending_ = true;
    sendTo(*opponent(sender), chess::net::ServerDrawOfferMsg{});
}

void Match::handleDrawAccept(Client& sender)
{
    if (!drawOfferPending_) {
        sendTo(sender, chess::net::ErrorMsg{"No draw offer pending"});
        return;
    }
    endGame(chess::net::GameResult::Draw, chess::net::GameOverReason::Abort);
}

void Match::handleDrawDecline(Client&)
{
    drawOfferPending_ = false;
}

void Match::handleResign(Client& sender)
{
    chess::net::GameResult result = (&sender == white_)
        ? chess::net::GameResult::BlackWins
        : chess::net::GameResult::WhiteWins;
    endGame(result, chess::net::GameOverReason::Resignation);
}

void Match::handleChat(Client& sender, const chess::net::ChatMsg& msg)
{
    sendTo(*opponent(sender), chess::net::ServerChatMsg{sender.name, msg.text});
}

void Match::handlePing(Client& sender)
{
    sendTo(sender, chess::net::PongMsg{});
}

void Match::handleDisconnect(Client& client)
{
    if (!active_)
        return;

    Client* survivor = opponent(client);
    if (survivor) {
        chess::net::GameResult result = (&client == white_)
            ? chess::net::GameResult::BlackWins
            : chess::net::GameResult::WhiteWins;
        endGame(result, chess::net::GameOverReason::Disconnection);
    } else {
        endGame(chess::net::GameResult::Abort, chess::net::GameOverReason::Abort);
    }
}

bool Match::isActive() const { return active_; }

Client* Match::opponent(const Client& client) const
{
    if (&client == white_) return black_;
    if (&client == black_) return white_;
    return nullptr;
}

void Match::endGame(chess::net::GameResult result, chess::net::GameOverReason reason)
{
    active_ = false;
    chess::net::GameOverMsg gameOver{result, reason};
    sendTo(*white_, gameOver);
    sendTo(*black_, gameOver);
    std::cout << "[INFO] Game over: result=" << static_cast<int>(result)
              << " reason=" << static_cast<int>(reason) << "\n";
}

std::pair<chess::net::GameResult, chess::net::GameOverReason> Match::classifyState() const
{
    auto state = chess::evaluateGameState(board_);
    switch (state) {
        case chess::GameState::Checkmate: {
            chess::Color loser = board_.sideToMove();
            return {
                loser == chess::Color::White ? chess::net::GameResult::BlackWins
                                             : chess::net::GameResult::WhiteWins,
                chess::net::GameOverReason::Checkmate
            };
        }
        case chess::GameState::Stalemate:
            return {chess::net::GameResult::Draw, chess::net::GameOverReason::Stalemate};
        case chess::GameState::Draw:
            if (chess::fiftyMoveRule(board_))
                return {chess::net::GameResult::Draw, chess::net::GameOverReason::FiftyMove};
            if (chess::threefoldRepetition(board_))
                return {chess::net::GameResult::Draw, chess::net::GameOverReason::Repetition};
            if (chess::insufficientMaterial(board_))
                return {chess::net::GameResult::Draw, chess::net::GameOverReason::InsufficientMaterial};
            return {chess::net::GameResult::Draw, chess::net::GameOverReason::Abort};
        default:
            return {chess::net::GameResult::Abort, chess::net::GameOverReason::Abort};
    }
}
