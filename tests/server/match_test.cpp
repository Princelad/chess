#include <gtest/gtest.h>

#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>

#include <chess/net/messages.h>
#include <chess/net/protocol.h>

#include "client.h"
#include "match.h"

#include <memory>
#include <thread>

class MatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        listener_.listen(0);
        port_ = listener_.getLocalPort();

        acceptThread_ = std::thread([this]() {
            whiteSock_ = std::make_unique<sf::TcpSocket>();
            listener_.accept(*whiteSock_);
            blackSock_ = std::make_unique<sf::TcpSocket>();
            listener_.accept(*blackSock_);
            listener_.close();
        });

        clientWhite_ = std::make_unique<sf::TcpSocket>();
        clientWhite_->connect(sf::IpAddress::LocalHostV4, port_);
        clientBlack_ = std::make_unique<sf::TcpSocket>();
        clientBlack_->connect(sf::IpAddress::LocalHostV4, port_);

        acceptThread_.join();

        white_.socket = std::move(whiteSock_);
        black_.socket = std::move(blackSock_);
        white_.name = "Alice";
        black_.name = "Bob";

        match_ = std::make_unique<Match>(white_, black_);
    }

    void sendFromClient(sf::TcpSocket& sock, const chess::net::ClientMessage& msg) {
        sf::Packet packet;
        chess::net::serialize(packet, msg);
        [[maybe_unused]] auto s = sock.send(packet);
    }

    std::optional<chess::net::ServerMessage> recvOnClient(sf::TcpSocket& sock) {
        sock.setBlocking(true);
        sf::Packet packet;
        if (sock.receive(packet) != sf::Socket::Status::Done) return std::nullopt;
        return chess::net::deserializeServer(packet);
    }

    std::optional<chess::net::ServerMessage> tryRecvOnClient(sf::TcpSocket& sock) {
        sock.setBlocking(false);
        sf::Packet packet;
        if (sock.receive(packet) != sf::Socket::Status::Done) return std::nullopt;
        return chess::net::deserializeServer(packet);
    }

    sf::TcpListener listener_;
    unsigned short port_ = 0;
    std::unique_ptr<sf::TcpSocket> whiteSock_;
    std::unique_ptr<sf::TcpSocket> blackSock_;
    std::unique_ptr<sf::TcpSocket> clientWhite_;
    std::unique_ptr<sf::TcpSocket> clientBlack_;
    std::thread acceptThread_;
    Client white_;
    Client black_;
    std::unique_ptr<Match> match_;
};

TEST_F(MatchTest, ValidMove)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});

    auto bMsg = recvOnClient(*clientBlack_);
    ASSERT_TRUE(bMsg.has_value());
    auto* move = std::get_if<chess::net::ServerMoveMsg>(&*bMsg);
    ASSERT_NE(move, nullptr);
    EXPECT_EQ(move->san, "e4");
}

TEST_F(MatchTest, WrongTurn)
{
    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});

    auto msg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "Not your turn");
}

TEST_F(MatchTest, IllegalMove)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e5"});

    auto msg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "Illegal move");
}

TEST_F(MatchTest, Resign)
{
    match_->handleMessage(white_, chess::net::ResignMsg{});

    EXPECT_FALSE(match_->isActive());

    auto wMsg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(wMsg.has_value());
    auto* over = std::get_if<chess::net::GameOverMsg>(&*wMsg);
    ASSERT_NE(over, nullptr);
    EXPECT_EQ(over->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Resignation);

    auto bMsg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(bMsg.has_value());
    auto* bOver = std::get_if<chess::net::GameOverMsg>(&*bMsg);
    ASSERT_NE(bOver, nullptr);
    EXPECT_EQ(bOver->result, chess::net::GameResult::BlackWins);
}

TEST_F(MatchTest, DrawOfferAndAccept)
{
    match_->handleMessage(white_, chess::net::DrawOfferMsg{});

    auto bOffer = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(bOffer.has_value());
    EXPECT_TRUE(std::get_if<chess::net::ServerDrawOfferMsg>(&*bOffer));

    match_->handleMessage(black_, chess::net::DrawAcceptMsg{});
    EXPECT_FALSE(match_->isActive());

    auto wOver = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(wOver.has_value());
    auto* over = std::get_if<chess::net::GameOverMsg>(&*wOver);
    ASSERT_NE(over, nullptr);
    EXPECT_EQ(over->result, chess::net::GameResult::Draw);

    auto bOver = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(bOver.has_value());
    auto* bOverMsg = std::get_if<chess::net::GameOverMsg>(&*bOver);
    ASSERT_NE(bOverMsg, nullptr);
    EXPECT_EQ(bOverMsg->result, chess::net::GameResult::Draw);
}

TEST_F(MatchTest, DrawAcceptNoOffer)
{
    match_->handleMessage(black_, chess::net::DrawAcceptMsg{});

    auto msg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "No draw offer pending");
}

TEST_F(MatchTest, ChatRelay)
{
    match_->handleMessage(white_, chess::net::ChatMsg{"hello"});

    auto msg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(msg.has_value());
    auto* chat = std::get_if<chess::net::ServerChatMsg>(&*msg);
    ASSERT_NE(chat, nullptr);
    EXPECT_EQ(chat->name, "Alice");
    EXPECT_EQ(chat->text, "hello");
}

TEST_F(MatchTest, ChatTooLong)
{
    std::string longMsg(501, 'x');
    match_->handleMessage(white_, chess::net::ChatMsg{longMsg});

    auto msg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "Message too long");
}

TEST_F(MatchTest, DisconnectWhite)
{
    match_->handleDisconnect(white_);
    EXPECT_FALSE(match_->isActive());

    auto bMsg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(bMsg.has_value());
    auto* over = std::get_if<chess::net::GameOverMsg>(&*bMsg);
    ASSERT_NE(over, nullptr);
    EXPECT_EQ(over->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Disconnection);
}

TEST_F(MatchTest, PingPong)
{
    match_->handleMessage(white_, chess::net::PingMsg{});

    auto msg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(msg.has_value());
    EXPECT_TRUE(std::get_if<chess::net::PongMsg>(&*msg));
}

TEST_F(MatchTest, MessageAfterGameOver)
{
    match_->handleMessage(white_, chess::net::ResignMsg{});
    EXPECT_FALSE(match_->isActive());

    for (int i = 0; i < 10; ++i) {
        tryRecvOnClient(*clientWhite_);
        tryRecvOnClient(*clientBlack_);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});
    auto msg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "Game is over");
}
