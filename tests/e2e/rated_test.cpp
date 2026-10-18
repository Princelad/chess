// End-to-end tests for the phase-10.2 account flow and rated games:
// real server over TCP, with a real temp SQLite database.
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>

#include <SFML/Network/TcpSocket.hpp>

#include <chess/net/messages.h>
#include <chess/net/protocol.h>

#include "server.h"

using namespace std::chrono_literals;

namespace {

constexpr auto TestTimeout = 5s;

class RatedE2eTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        dbPath_ = std::filesystem::temp_directory_path()
            / ("chess_rated_e2e_" + std::to_string(++dbCounter) + ".db");
        std::filesystem::remove(dbPath_);
        std::filesystem::remove(dbPath_.string() + "-wal");
        std::filesystem::remove(dbPath_.string() + "-shm");

        shutdownFlag_ = false;
        boundPort_ = 0;
        serverThread_ = std::thread([this]() {
            chess::server::ServerConfig config;
            config.port = 0;
            config.host = "127.0.0.1";
            config.dbPath = dbPath_.string();
            chess::server::runServer(config, shutdownFlag_, &boundPort_);
        });
        while (boundPort_ == 0)
            std::this_thread::sleep_for(1ms);
    }

    void TearDown() override
    {
        chess::server::stopServer(shutdownFlag_);
        if (serverThread_.joinable())
            serverThread_.join();
        std::filesystem::remove(dbPath_);
        std::filesystem::remove(dbPath_.string() + "-wal");
        std::filesystem::remove(dbPath_.string() + "-shm");
    }

    sf::TcpSocket connectClient()
    {
        sf::TcpSocket sock;
        EXPECT_EQ(sock.connect(sf::IpAddress::LocalHostV4, boundPort_),
                  sf::Socket::Status::Done);
        sock.setBlocking(false);
        return sock;
    }

    void sendMsg(sf::TcpSocket& sock, const chess::net::ClientMessage& msg)
    {
        sf::Packet packet;
        chess::net::serialize(packet, msg);
        ASSERT_EQ(sock.send(packet), sf::Socket::Status::Done);
    }

    std::optional<chess::net::ServerMessage> tryRecv(sf::TcpSocket& sock)
    {
        sf::Packet packet;
        if (sock.receive(packet) != sf::Socket::Status::Done)
            return std::nullopt;
        return chess::net::deserializeServer(packet);
    }

    // Polls sock until it sees the wanted message type; ignores others.
    template <typename T>
    std::optional<T> await(sf::TcpSocket& sock, std::chrono::milliseconds timeout = TestTimeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (auto msg = tryRecv(sock)) {
                if (auto* wanted = std::get_if<T>(&*msg))
                    return *wanted;
                // Ignore Progress-bearing siblings (welcome, moves, ...).
            }
            std::this_thread::sleep_for(1ms);
        }
        return std::nullopt;
    }

    std::filesystem::path dbPath_;
    std::atomic<bool> shutdownFlag_ = false;
    std::thread serverThread_;
    unsigned short boundPort_ = 0;

    static std::atomic<int> dbCounter;
};

std::atomic<int> RatedE2eTest::dbCounter{0};

TEST_F(RatedE2eTest, LoginCreatesProfileMessage)
{
    auto sock = connectClient();
    sendMsg(sock, chess::net::LoginMsg{"Ada", "lovelace"});

    auto profile = await<chess::net::ProfileMsg>(sock);
    ASSERT_TRUE(profile.has_value());
    EXPECT_EQ(profile->name, "Ada");
    EXPECT_DOUBLE_EQ(profile->rating, 1500.0);
    EXPECT_DOUBLE_EQ(profile->deviation, 350.0);
    EXPECT_FALSE(profile->isGuest);
}

TEST_F(RatedE2eTest, WrongPasswordIsAnErrorAndRetryRegistersLogically)
{
    // Unknown name + wrong-looking password register attempt:
    // "Ada" with a non-empty password auto-registers, so wrong-password
    // error flow applies to an existing account. First create Ada.
    auto creator = connectClient();
    sendMsg(creator, chess::net::LoginMsg{"Ada", "lovelace"});
    ASSERT_TRUE(await<chess::net::ProfileMsg>(creator).has_value());

    auto sock = connectClient();
    sendMsg(sock, chess::net::LoginMsg{"Ada", "wrong"});
    auto error = await<chess::net::ErrorMsg>(sock);
    ASSERT_TRUE(error.has_value());

    // Retry with the right password logs in with the stored rating.
    sendMsg(sock, chess::net::LoginMsg{"Ada", "lovelace"});
    auto profile = await<chess::net::ProfileMsg>(sock);
    ASSERT_TRUE(profile.has_value());
    EXPECT_EQ(profile->name, "Ada");
    EXPECT_DOUBLE_EQ(profile->rating, 1500.0);
}

TEST_F(RatedE2eTest, EmptyPasswordOnNewNameKeepsClientInGuestShape)
{
    auto sock = connectClient();
    sendMsg(sock, chess::net::LoginMsg{"NobodyNew", ""});
    auto error = await<chess::net::ErrorMsg>(sock);
    ASSERT_TRUE(error.has_value());

    // The connection is still usable for guest play afterwards.
    sendMsg(sock, chess::net::JoinMsg{"GuestX"});
    auto peer = connectClient();
    sendMsg(peer, chess::net::JoinMsg{"GuestY"});
    auto welcome = await<chess::net::WelcomeMsg>(sock);
    ASSERT_TRUE(welcome.has_value());
    EXPECT_EQ(welcome->color, chess::Color::White);
}

TEST_F(RatedE2eTest, RatedGameUpdatesProfileAfterResign)
{
    auto white = connectClient();
    auto black = connectClient();

    sendMsg(white, chess::net::LoginMsg{"Ada", "pw"});
    sendMsg(black, chess::net::LoginMsg{"Bob", "pw"});
    ASSERT_TRUE(await<chess::net::ProfileMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::ProfileMsg>(black).has_value());

    sendMsg(white, chess::net::JoinMsg{"Ada"});
    sendMsg(black, chess::net::JoinMsg{"Bob"});

    auto wWelcome = await<chess::net::WelcomeMsg>(white);
    auto bWelcome = await<chess::net::WelcomeMsg>(black);
    ASSERT_TRUE(wWelcome.has_value());
    ASSERT_TRUE(bWelcome.has_value());
    EXPECT_TRUE(wWelcome->rated); // both participants are registered
    EXPECT_DOUBLE_EQ(bWelcome->rating, 1500.0); // Ada's rating, seen by Bob
    EXPECT_DOUBLE_EQ(wWelcome->rating, 1500.0); // Bob's rating, seen by Ada

    // White resigns: white loses, black gains. Both get a fresh ProfileMsg
    // right after the GameOverMsg.
    sendMsg(white, chess::net::ResignMsg{});
    ASSERT_TRUE(await<chess::net::GameOverMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::GameOverMsg>(black).has_value());

    auto wProfile = await<chess::net::ProfileMsg>(white);
    auto bProfile = await<chess::net::ProfileMsg>(black);
    ASSERT_TRUE(wProfile.has_value());
    ASSERT_TRUE(bProfile.has_value());
    EXPECT_LT(wProfile->rating, 1500.0);
    EXPECT_GT(bProfile->rating, 1500.0);
    EXPECT_LT(wProfile->deviation, 350.0); // more certain after a rated game
}

TEST_F(RatedE2eTest, GuestGameIsUnrated)
{
    auto white = connectClient();
    auto black = connectClient();

    sendMsg(white, chess::net::JoinMsg{"GuestA"});
    sendMsg(black, chess::net::JoinMsg{"GuestB"});

    auto wWelcome = await<chess::net::WelcomeMsg>(white);
    auto bWelcome = await<chess::net::WelcomeMsg>(black);
    ASSERT_TRUE(wWelcome.has_value());
    ASSERT_TRUE(bWelcome.has_value());
    EXPECT_FALSE(wWelcome->rated);
    EXPECT_FALSE(bWelcome->rated);

    sendMsg(white, chess::net::ResignMsg{});
    ASSERT_TRUE(await<chess::net::GameOverMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::GameOverMsg>(black).has_value());
}

} // namespace
