#include <gtest/gtest.h>

#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>

#include <chess/net/messages.h>
#include <chess/net/protocol.h>

#include "connection.h"

#include <chrono>
#include <atomic>
#include <memory>
#include <thread>

using chess::client::Connection;
using chess::client::ConnectionState;

class ConnectionTest : public ::testing::Test {
protected:
    struct TestServer {
        sf::TcpListener listener;
        std::unique_ptr<sf::TcpSocket> client;
        unsigned short port = 0;

        bool accept() {
            client = std::make_unique<sf::TcpSocket>();
            return listener.accept(*client) == sf::Socket::Status::Done;
        }

        std::optional<chess::net::ClientMessage> receiveClient() {
            sf::Packet packet;
            client->setBlocking(true);
            if (client->receive(packet) != sf::Socket::Status::Done) return std::nullopt;
            return chess::net::deserializeClient(packet);
        }

        std::optional<chess::net::ServerMessage> receive() {
            sf::Packet packet;
            client->setBlocking(true);
            if (client->receive(packet) != sf::Socket::Status::Done) return std::nullopt;
            return chess::net::deserializeServer(packet);
        }

        bool send(const chess::net::ServerMessage& msg) {
            sf::Packet packet;
            chess::net::serialize(packet, msg);
            return client->send(packet) == sf::Socket::Status::Done;
        }

        void close() {
            if (client) client->disconnect();
            listener.close();
        }
    };

    void SetUp() override {
        server_ = std::make_unique<TestServer>();
        ASSERT_EQ(server_->listener.listen(0), sf::Socket::Status::Done);
        server_->port = server_->listener.getLocalPort();
    }

    void TearDown() override {
        if (server_) server_->close();
    }

    bool waitForConnected(Connection& conn, int timeoutMs = 2000) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (conn.state() == ConnectionState::Connecting &&
               std::chrono::steady_clock::now() < deadline) {
            conn.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        conn.poll();
        return conn.state() == ConnectionState::Connected;
    }

    std::unique_ptr<TestServer> server_;
};

TEST_F(ConnectionTest, ConnectAndDisconnect)
{
    Connection conn;
    conn.connect("127.0.0.1", server_->port);
    EXPECT_EQ(conn.state(), ConnectionState::Connecting);

    ASSERT_TRUE(waitForConnected(conn));
    EXPECT_EQ(conn.state(), ConnectionState::Connected);

    conn.disconnect();
    EXPECT_EQ(conn.state(), ConnectionState::Disconnected);
}

TEST_F(ConnectionTest, ConnectInvalidAddress)
{
    Connection conn;
    conn.connect("not.a.valid.host", 12345);
    EXPECT_EQ(conn.state(), ConnectionState::Disconnected);
    EXPECT_FALSE(conn.error().empty());
}

TEST_F(ConnectionTest, ConnectRefused)
{
    Connection conn;
    conn.connect("127.0.0.1", 1);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while (conn.state() == ConnectionState::Connecting &&
           std::chrono::steady_clock::now() < deadline) {
        conn.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    conn.poll();
    EXPECT_EQ(conn.state(), ConnectionState::Disconnected);
}

TEST_F(ConnectionTest, SendReceive)
{
    Connection conn;
    conn.connect("127.0.0.1", server_->port);
    ASSERT_TRUE(waitForConnected(conn));

    std::thread serverThread([this]() {
        server_->accept();
    });
    serverThread.join();

    conn.send(chess::net::JoinMsg{"Alice"});
    conn.poll();

    auto serverMsg = server_->receiveClient();
    ASSERT_TRUE(serverMsg.has_value());
    auto* join = std::get_if<chess::net::JoinMsg>(&*serverMsg);
    ASSERT_NE(join, nullptr);
    EXPECT_EQ(join->name, "Alice");

    conn.disconnect();
}

TEST_F(ConnectionTest, ReceiveFromServer)
{
    Connection conn;
    conn.connect("127.0.0.1", server_->port);
    ASSERT_TRUE(waitForConnected(conn));

    std::thread serverThread([this]() {
        server_->accept();
        server_->send(chess::net::PongMsg{});
    });

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (!conn.hasMessages() && std::chrono::steady_clock::now() < deadline) {
        conn.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_TRUE(conn.hasMessages());
    auto msg = conn.nextMessage();
    EXPECT_TRUE(std::get_if<chess::net::PongMsg>(&msg));

    serverThread.join();
    conn.disconnect();
}

TEST_F(ConnectionTest, SendWhileDisconnected)
{
    Connection conn;
    conn.send(chess::net::JoinMsg{"test"});
    EXPECT_FALSE(conn.hasMessages());
    EXPECT_EQ(conn.state(), ConnectionState::Disconnected);
}

TEST_F(ConnectionTest, ServerDisconnect)
{
    Connection conn;
    conn.connect("127.0.0.1", server_->port);
    ASSERT_TRUE(waitForConnected(conn));

    std::thread serverThread([this]() {
        server_->accept();
        server_->close();
    });

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (conn.state() == ConnectionState::Connected &&
           std::chrono::steady_clock::now() < deadline) {
        conn.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_EQ(conn.state(), ConnectionState::Disconnected);
    EXPECT_EQ(conn.error(), "Connection lost");

    serverThread.join();
}

TEST_F(ConnectionTest, PingPong)
{
    Connection conn(std::chrono::milliseconds{10}, std::chrono::milliseconds{200});
    conn.connect("127.0.0.1", server_->port);
    ASSERT_TRUE(waitForConnected(conn));

    std::atomic<bool> serverDone{false};
    std::thread serverThread([this, &serverDone]() {
        server_->accept();
        while (!serverDone) {
            auto msg = server_->receiveClient();
            if (!msg) break;
            if (std::get_if<chess::net::PingMsg>(&*msg)) {
                server_->send(chess::net::PongMsg{});
            }
        }
    });

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{1};
    while (std::chrono::steady_clock::now() < deadline) {
        conn.poll();
        if (conn.state() != ConnectionState::Connected) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_EQ(conn.state(), ConnectionState::Connected);

    conn.disconnect();
    serverDone = true;
    server_->close();
    serverThread.join();
}

TEST_F(ConnectionTest, PongTimeout)
{
    Connection conn(std::chrono::milliseconds{10}, std::chrono::milliseconds{50});
    conn.connect("127.0.0.1", server_->port);
    ASSERT_TRUE(waitForConnected(conn));

    std::atomic<bool> serverAccepted{false};
    std::thread serverThread([this, &serverAccepted]() {
        server_->accept();
        serverAccepted = true;
        server_->client->setBlocking(false);
        for (;;) {
            sf::Packet packet;
            auto status = server_->client->receive(packet);
            if (status == sf::Socket::Status::Disconnected ||
                status == sf::Socket::Status::Error) {
                break;
            }
        }
    });

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (conn.state() == ConnectionState::Connected &&
           std::chrono::steady_clock::now() < deadline) {
        conn.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_EQ(conn.state(), ConnectionState::Disconnected);
    EXPECT_EQ(conn.error(), "Connection timed out");

    while (!serverAccepted.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    server_->close();
    serverThread.join();
}

TEST_F(ConnectionTest, JoinAndWelcome)
{
    Connection conn;
    conn.connect("127.0.0.1", server_->port);
    ASSERT_TRUE(waitForConnected(conn));

    std::thread serverThread([this]() {
        server_->accept();
        auto msg = server_->receiveClient();
        ASSERT_TRUE(msg.has_value());
        auto* join = std::get_if<chess::net::JoinMsg>(&*msg);
        ASSERT_NE(join, nullptr);
        EXPECT_EQ(join->name, "Alice");

        server_->send(chess::net::WelcomeMsg{chess::Color::White, "Bob"});
        server_->send(chess::net::OpponentJoinedMsg{"Bob"});
    });

    conn.join("Alice");

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (!conn.hasMessages() &&
           std::chrono::steady_clock::now() < deadline) {
        conn.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_TRUE(conn.hasMessages());
    auto welcome = conn.nextMessage();
    auto* w = std::get_if<chess::net::WelcomeMsg>(&welcome);
    ASSERT_NE(w, nullptr);
    EXPECT_EQ(w->color, chess::Color::White);
    EXPECT_EQ(w->opponent, "Bob");

    deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (!conn.hasMessages() && std::chrono::steady_clock::now() < deadline) {
        conn.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_TRUE(conn.hasMessages());
    auto joined = conn.nextMessage();
    auto* j = std::get_if<chess::net::OpponentJoinedMsg>(&joined);
    ASSERT_NE(j, nullptr);
    EXPECT_EQ(j->name, "Bob");

    serverThread.join();
    conn.disconnect();
}

TEST_F(ConnectionTest, MoveAndChat)
{
    Connection conn;
    conn.connect("127.0.0.1", server_->port);
    ASSERT_TRUE(waitForConnected(conn));

    std::thread serverThread([this]() {
        server_->accept();
        auto join = server_->receiveClient();
        server_->send(chess::net::WelcomeMsg{chess::Color::Black, "Alice"});
        server_->send(chess::net::OpponentJoinedMsg{"Alice"});
        server_->send(chess::net::ServerMoveMsg{"e4"});

        auto move = server_->receiveClient();
        ASSERT_TRUE(move.has_value());
        auto* m = std::get_if<chess::net::ChatMsg>(&*move);
        ASSERT_NE(m, nullptr);
        EXPECT_EQ(m->text, "Hello!");
    });

    conn.join("Bob");

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    int received = 0;
    while (received < 3 && std::chrono::steady_clock::now() < deadline) {
        conn.poll();
        while (conn.hasMessages()) {
            auto msg = conn.nextMessage();
            if (std::get_if<chess::net::WelcomeMsg>(&msg)) ++received;
            if (std::get_if<chess::net::OpponentJoinedMsg>(&msg)) ++received;
            if (std::get_if<chess::net::ServerMoveMsg>(&msg)) ++received;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_EQ(received, 3);

    conn.send(chess::net::ChatMsg{"Hello!"});
    conn.poll();

    serverThread.join();
    conn.disconnect();
}

TEST_F(ConnectionTest, ErrorFromServer)
{
    Connection conn;
    conn.connect("127.0.0.1", server_->port);
    ASSERT_TRUE(waitForConnected(conn));

    std::thread serverThread([this]() {
        server_->accept();
        server_->send(chess::net::ErrorMsg{"Name taken"});
    });

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (!conn.hasMessages() && std::chrono::steady_clock::now() < deadline) {
        conn.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_TRUE(conn.hasMessages());
    auto msg = conn.nextMessage();
    auto* err = std::get_if<chess::net::ErrorMsg>(&msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "Name taken");

    serverThread.join();
    conn.disconnect();
}
