#include <gtest/gtest.h>

#include <chrono>

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
        ASSERT_EQ(listener_.listen(0), sf::Socket::Status::Done);
        port_ = listener_.getLocalPort();

        acceptThread_ = std::thread([this]() {
            whiteSock_ = std::make_unique<sf::TcpSocket>();
            [[maybe_unused]] auto s1 = listener_.accept(*whiteSock_);
            blackSock_ = std::make_unique<sf::TcpSocket>();
            [[maybe_unused]] auto s2 = listener_.accept(*blackSock_);
            listener_.close();
        });

        clientWhite_ = std::make_unique<sf::TcpSocket>();
        ASSERT_EQ(clientWhite_->connect(sf::IpAddress::LocalHostV4, port_), sf::Socket::Status::Done);
        clientBlack_ = std::make_unique<sf::TcpSocket>();
        ASSERT_EQ(clientBlack_->connect(sf::IpAddress::LocalHostV4, port_), sf::Socket::Status::Done);

        acceptThread_.join();

        white_.socket = std::move(whiteSock_);
        black_.socket = std::move(blackSock_);
        white_.name = "Alice";
        black_.name = "Bob";

        // Zero grace makes the 11.2 reconnect window deterministic in unit
        // tests: the seat is absent and the next pollReconnect expires it.
        match_ = std::make_unique<Match>(white_, black_, std::string(),
                                         testGraceMs_);
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
    std::int64_t testGraceMs_ = 0;
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

// 11.2: a disconnect no longer ends the game — the absent seat runs out
// its grace window (clock keeps ticking) while the opponent waits, and a
// token rejoin rebinds the seat. Only a lapsed window or a second
// departure is final.

TEST_F(MatchTest, DisconnectHoldsMatchInReconnectGrace)
{
    match_->handleDisconnect(white_);
    EXPECT_TRUE(match_->isActive());

    // The survivor is told about the window, not the game end.
    auto bMsg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(bMsg.has_value());
    auto* rc = std::get_if<chess::net::OpponentReconnectingMsg>(&*bMsg);
    ASSERT_NE(rc, nullptr);
    EXPECT_EQ(rc->graceSeconds, static_cast<std::int32_t>((testGraceMs_ + 999) / 1000));

    // The absent seat is gone, the seated one still here.
    EXPECT_FALSE(match_->seat(chess::Color::White).present);
    EXPECT_TRUE(match_->seat(chess::Color::Black).present);
    EXPECT_EQ(match_->white(), nullptr);
    EXPECT_EQ(match_->black(), &black_);
}

TEST_F(MatchTest, GraceExpiryEndsGameWithOpponentWin)
{
    match_->handleDisconnect(white_);

    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    match_->pollClock(now);
    match_->pollReconnect(now);
    EXPECT_FALSE(match_->isActive());
    EXPECT_TRUE(match_->reconnecting() == false);

    const auto over = match_->lastGameOver();
    ASSERT_TRUE(over.has_value());
    EXPECT_EQ(over->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Disconnection);

    // The socket may still hold the reconnect notice; skip to the end.
    std::optional<chess::net::GameOverMsg> bOver;
    for (int i = 0; i < 8; ++i) {
        auto m = tryRecvOnClient(*clientBlack_);
        if (m) {
            if (auto* go = std::get_if<chess::net::GameOverMsg>(&*m)) {
                bOver = *go;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(bOver.has_value());
    EXPECT_EQ(bOver->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(bOver->reason, chess::net::GameOverReason::Disconnection);
}

TEST_F(MatchTest, ResumeRebindsSeatAndNotifiesOpponent)
{
    match_->handleDisconnect(white_);

    Client carol; // fresh connection, no socket needed for the match logic
    carol.name = "Carol";
    match_->resume(match_->resumeToken(chess::Color::White), carol);

    EXPECT_TRUE(match_->isActive());
    EXPECT_EQ(match_->white(), &carol);
    EXPECT_TRUE(match_->seat(chess::Color::White).present);
    EXPECT_FALSE(match_->reconnecting());

    // The survivor learns play may continue.
    auto bMsg = tryRecvOnClient(*clientBlack_);
    bool resumed = false;
    while (auto m = tryRecvOnClient(*clientBlack_))
        if (std::holds_alternative<chess::net::OpponentResumedMsg>(*m))
            resumed = true;
    EXPECT_TRUE(resumed);

    // The rebound seat is fully seated: they are the side to move and their
    // move goes through (no "Not your turn").
    match_->handleMessage(carol, chess::net::MoveMsg{"e4"});
    auto bMsg2 = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(bMsg2.has_value());
    auto* move = std::get_if<chess::net::ServerMoveMsg>(&*bMsg2);
    ASSERT_NE(move, nullptr);
    EXPECT_EQ(move->san, "e4");
}

TEST_F(MatchTest, ResumeWithWrongTokenDoesNothing)
{
    match_->handleDisconnect(white_);

    Client carol;
    carol.name = "Carol";
    const auto outcome = match_->resume("no-such-token", carol);
    EXPECT_EQ(outcome, Match::ResumeOutcome::NoSeat);

    // Still absent; grace still armed; the stranger was not seated.
    EXPECT_EQ(match_->white(), nullptr);
    EXPECT_FALSE(match_->seat(chess::Color::White).present);
}

TEST_F(MatchTest, ResumeAlreadyConnectedRefused)
{
    // A live seat's token played back on a second connection is refused.
    Client carol;
    carol.name = "Carol";
    const auto outcome =
        match_->resume(match_->resumeToken(chess::Color::White), carol);
    EXPECT_EQ(outcome, Match::ResumeOutcome::AlreadyConnected);
    EXPECT_EQ(match_->white(), &white_);
}

TEST_F(MatchTest, DoubleDisconnectEndsGameUnratedAbort)
{
    match_->handleDisconnect(white_);
    match_->handleDisconnect(black_);

    EXPECT_FALSE(match_->isActive());
    const auto over = match_->lastGameOver();
    ASSERT_TRUE(over.has_value());
    // Nobody stayed behind: nobody wins. Aborted games are never rated.
    EXPECT_EQ(over->result, chess::net::GameResult::Abort);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Disconnection);
}

TEST_F(MatchTest, FinishedGameMatchesTokenForLateResume)
{
    match_->handleMessage(white_, chess::net::ResignMsg{});
    EXPECT_FALSE(match_->isActive());

    const std::string loserToken = match_->resumeToken(chess::Color::White);
    Client carol;
    carol.name = "Carol";
    EXPECT_EQ(match_->resume(loserToken, carol),
              Match::ResumeOutcome::ReboundOver);
    EXPECT_TRUE(match_->lastGameOver().has_value());
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
