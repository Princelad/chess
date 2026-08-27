#include <gtest/gtest.h>

#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>

#include <chess/net/messages.h>
#include <chess/net/protocol.h>

#include "client.h"
#include "match.h"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

class BotMatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char* mockPath = std::getenv("MOCK_ENGINE_PATH");
        enginePath_ = mockPath ? mockPath : "mock_engine.py";

        ASSERT_EQ(listener_.listen(0), sf::Socket::Status::Done);
        port_ = listener_.getLocalPort();

        acceptThread_ = std::thread([this]() {
            humanSock_ = std::make_unique<sf::TcpSocket>();
            [[maybe_unused]] auto s = listener_.accept(*humanSock_);
            listener_.close();
        });

        clientSock_ = std::make_unique<sf::TcpSocket>();
        ASSERT_EQ(clientSock_->connect(sf::IpAddress::LocalHostV4, port_), sf::Socket::Status::Done);
        acceptThread_.join();

        human_.socket = std::move(humanSock_);
        human_.name = "Alice";

        bot_.isBot = true;
        bot_.name = "Bot-1";
    }

    void pollUntilBotDone(Match& match, int timeoutMs = 2000) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (match.isBotTurn() && std::chrono::steady_clock::now() < deadline) {
            match.pollBotMove();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    std::optional<chess::net::ServerMessage> tryRecv() {
        clientSock_->setBlocking(false);
        sf::Packet packet;
        if (clientSock_->receive(packet) != sf::Socket::Status::Done) return std::nullopt;
        return chess::net::deserializeServer(packet);
    }

    std::optional<chess::net::ServerMessage> recvWait(int timeoutMs = 2000) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            auto msg = tryRecv();
            if (msg.has_value()) return msg;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return std::nullopt;
    }

    sf::TcpListener listener_;
    unsigned short port_ = 0;
    std::unique_ptr<sf::TcpSocket> humanSock_;
    std::unique_ptr<sf::TcpSocket> clientSock_;
    std::thread acceptThread_;
    Client human_;
    Client bot_;
    std::string enginePath_;
};

TEST_F(BotMatchTest, BotPlaysBlack)
{
    human_.state = ClientState::InMatch;
    bot_.state = ClientState::InMatch;

    Match match(human_, bot_);
    match.startBot(chess::Color::Black, 1, enginePath_);

    EXPECT_FALSE(match.isBotTurn());

    match.handleMessage(human_, chess::net::MoveMsg{"e4"});
    EXPECT_TRUE(match.isBotTurn());

    pollUntilBotDone(match);
    EXPECT_FALSE(match.isBotTurn());

    auto msg = recvWait();
    ASSERT_TRUE(msg.has_value());
    auto* moveMsg = std::get_if<chess::net::ServerMoveMsg>(&*msg);
    ASSERT_NE(moveMsg, nullptr);
    EXPECT_FALSE(moveMsg->san.empty());
}

TEST_F(BotMatchTest, BotPlaysWhite)
{
    human_.state = ClientState::InMatch;
    bot_.state = ClientState::InMatch;

    Match match(human_, bot_);
    match.startBot(chess::Color::White, 1, enginePath_);

    EXPECT_TRUE(match.isBotTurn());

    pollUntilBotDone(match);
    EXPECT_FALSE(match.isBotTurn());

    auto msg = recvWait();
    ASSERT_TRUE(msg.has_value());
    auto* moveMsg = std::get_if<chess::net::ServerMoveMsg>(&*msg);
    ASSERT_NE(moveMsg, nullptr);
    EXPECT_FALSE(moveMsg->san.empty());
}

TEST_F(BotMatchTest, BotEngineFailure)
{
    human_.state = ClientState::InMatch;
    bot_.state = ClientState::InMatch;

    Match match(human_, bot_);
    match.startBot(chess::Color::Black, 1, "/nonexistent/engine");

    EXPECT_FALSE(match.isBotTurn());

    match.handleMessage(human_, chess::net::MoveMsg{"e4"});
    EXPECT_FALSE(match.isBotTurn());

    EXPECT_TRUE(match.isActive());
}

TEST_F(BotMatchTest, BotClient)
{
    human_.state = ClientState::InMatch;
    bot_.state = ClientState::InMatch;

    Match match(human_, bot_);
    EXPECT_EQ(match.botClient(), nullptr);

    match.startBot(chess::Color::Black, 1, enginePath_);
    EXPECT_EQ(match.botClient(), &bot_);
}

TEST_F(BotMatchTest, HumanDisconnectEndsGame)
{
    human_.state = ClientState::InMatch;
    bot_.state = ClientState::InMatch;

    Match match(human_, bot_);
    match.startBot(chess::Color::Black, 1, enginePath_);

    match.handleDisconnect(human_);
    EXPECT_FALSE(match.isActive());
}
