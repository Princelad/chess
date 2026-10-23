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

    // Zero grace (11.2): the dropped human still loses the game on the
    // reconnect window's expiry, exactly the pre-11.2 outcome but delayed.
    Match match(human_, bot_, {}, {}, 0);
    match.startBot(chess::Color::Black, 1, enginePath_);

    match.handleDisconnect(human_);
    EXPECT_TRUE(match.isActive());

    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    match.pollReconnect(now);
    EXPECT_FALSE(match.isActive());

    const auto over = match.lastGameOver();
    ASSERT_TRUE(over.has_value());
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Disconnection);
}

TEST_F(BotMatchTest, BotAutoAcceptsTakeback)
{
    human_.state = ClientState::InMatch;
    bot_.state = ClientState::InMatch;

    Match match(human_, bot_);
    match.startBot(chess::Color::Black, 1, enginePath_);

    // e4, wait for the bot's answer; the human is then on the move with
    // exactly one full move to retract. The own echo precedes the bot's
    // reply in the human's queue.
    match.handleMessage(human_, chess::net::MoveMsg{"e4"});
    auto ownEcho = recvWait();
    ASSERT_TRUE(ownEcho.has_value());
    auto* ownMove = std::get_if<chess::net::ServerMoveMsg>(&*ownEcho);
    ASSERT_NE(ownMove, nullptr);
    EXPECT_EQ(ownMove->san, "e4");

    pollUntilBotDone(match);
    EXPECT_TRUE(match.isActive());

    auto botReply = recvWait();
    ASSERT_TRUE(botReply.has_value());
    auto* botMove = std::get_if<chess::net::ServerMoveMsg>(&*botReply);
    ASSERT_NE(botMove, nullptr);
    EXPECT_FALSE(botMove->san.empty());

    match.handleMessage(human_, chess::net::TakebackRequestMsg{});

    // Auto-accept: no forwarded request, and the human learns the rollback
    // straight away. The two plies (the human's e4 and the bot's answer)
    // all disappear: the very first full move gets retracted whole.
    auto msg = recvWait();
    ASSERT_TRUE(msg.has_value());
    auto* accepted = std::get_if<chess::net::TakebackAcceptedMsg>(&*msg);
    ASSERT_NE(accepted, nullptr);
    EXPECT_TRUE(accepted->sans.empty());
    EXPECT_EQ(match.currentFen(), chess::Board::fromStartPos().toFen());
    EXPECT_TRUE(match.sanHistory().empty());
    // The bot is never prompted and never left thinking.
    EXPECT_FALSE(match.isBotTurn());
    EXPECT_TRUE(match.isActive());
}

TEST_F(BotMatchTest, BotMatchTakebackNeedsPlayableFrame)
{
    human_.state = ClientState::InMatch;
    bot_.state = ClientState::InMatch;

    Match match(human_, bot_);
    match.startBot(chess::Color::Black, 1, enginePath_);

    // No plies yet: the frame rule holds even against a bot.
    match.handleMessage(human_, chess::net::TakebackRequestMsg{});
    auto msg = recvWait();
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "No move to take back");
    EXPECT_TRUE(match.isActive());
}
