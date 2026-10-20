// Shared end-to-end harness (extracted in 11.2): every e2e fixture spins
// up a real server on an OS-assigned loopback port, drives it over TCP
// sockets with non-blocking receives, and (optionally) points it at a
// temp SQLite database. Fixtures customize the ServerConfig through
// configure() and do their own bookkeeping in beforeStart()/afterStop().
#pragma once

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <thread>

#include <SFML/Network/TcpSocket.hpp>

#include <chess/net/messages.h>
#include <chess/net/protocol.h>

#include "server.h"

namespace chess {
namespace e2e {

using namespace std::chrono_literals;

inline constexpr auto TestTimeout = 5s;

class E2eHarness : public ::testing::Test {
protected:
    void SetUp() override
    {
        shutdownFlag_ = false;
        boundPort_ = 0;
        beforeStart();

        serverThread_ = std::thread([this]() {
            chess::server::ServerConfig config;
            config.port = 0;
            config.host = "127.0.0.1";
            // Short reconnect grace (11.2) for all e2e flows: plain
            // disconnect semantics ("wins on disconnection") still hold but
            // resolve within a few loop passes instead of 30 real seconds.
            config.reconnectGraceMs = testGraceMs_;
            configure(config);
            // boundPort_ is written before the event loop starts.
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
        afterStop();
    }

    virtual void configure(chess::server::ServerConfig&) {}
    virtual void beforeStart() {}
    virtual void afterStop() {}

    // Reconnect grace injected into the server (ms); the token-resume tests
    // override this where a longer observable window is needed.
    std::int64_t testGraceMs_ = 400;

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
        EXPECT_EQ(sock.send(packet), sf::Socket::Status::Done);
    }

    std::optional<chess::net::ServerMessage> tryRecv(sf::TcpSocket& sock)
    {
        // The one-packet-per-call version loses the rare TCP packets that
        // arrive fragmented (bytes are consumed and dropped on timeout
        // under load). Ride through Partial so a later receive completes it.
        sf::Packet packet;
        while (true) {
            auto status = sock.receive(packet);
            if (status == sf::Socket::Status::Done)
                return chess::net::deserializeServer(packet);
            if (status == sf::Socket::Status::Partial) {
                std::this_thread::sleep_for(1ms);
                continue;
            }
            return std::nullopt;
        }
    }

    template <typename Pred>
    bool pollUntil(Pred pred, std::chrono::milliseconds timeout = TestTimeout)
    {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (pred()) return true;
            std::this_thread::sleep_for(1ms);
        }
        return false;
    }

    // Polls sock until it sees the wanted message type; ignores others.
    template <typename T>
    std::optional<T> await(sf::TcpSocket& sock,
                           std::chrono::milliseconds timeout = TestTimeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (auto msg = tryRecv(sock)) {
                if (auto* wanted = std::get_if<T>(&*msg))
                    return *wanted;
                // Ignore progress-bearing siblings (welcome, moves, ...).
            }
            std::this_thread::sleep_for(1ms);
        }
        return std::nullopt;
    }

    void joinBoth(sf::TcpSocket& white, sf::TcpSocket& black,
                  const std::string& wName = "White",
                  const std::string& bName = "Black")
    {
        sendMsg(white, chess::net::JoinMsg{wName});
        sendMsg(black, chess::net::JoinMsg{bName});

        bool wWelcome = false, bWelcome = false;
        ASSERT_TRUE(pollUntil([&]() {
            if (!wWelcome) {
                auto m = tryRecv(white);
                if (m) {
                    auto* w = std::get_if<chess::net::WelcomeMsg>(&*m);
                    EXPECT_NE(w, nullptr);
                    if (w) {
                        EXPECT_EQ(w->color, chess::Color::White);
                        EXPECT_EQ(w->opponent, bName);
                    }
                    wWelcome = true;
                }
            }
            if (!bWelcome) {
                auto m = tryRecv(black);
                if (m) {
                    auto* w = std::get_if<chess::net::WelcomeMsg>(&*m);
                    EXPECT_NE(w, nullptr);
                    if (w) {
                        EXPECT_EQ(w->color, chess::Color::Black);
                        EXPECT_EQ(w->opponent, wName);
                    }
                    bWelcome = true;
                }
            }
            return wWelcome && bWelcome;
        }));
    }

    // Sends one SAN move and waits until the server echoes it back to the
    // sender. This synchronizes play with server processing: the next move
    // is never sent before the previous one was applied, so the select
    // loop's cross-socket drain order cannot reorder move applications.
    // The sender's socket may still hold the previous move's own stale
    // echo, so non-matching ServerMoveMsg entries are skipped; ErrorMsg
    // (e.g. out of turn) fails fast.
    bool playMove(sf::TcpSocket& sender, const std::string& san)
    {
        sendMsg(sender, chess::net::MoveMsg{san});
        const auto deadline = std::chrono::steady_clock::now() + TestTimeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (auto msg = tryRecv(sender)) {
                if (auto* move = std::get_if<chess::net::ServerMoveMsg>(&*msg)) {
                    if (move->san == san)
                        return true;
                }
                else if (std::holds_alternative<chess::net::ErrorMsg>(*msg))
                    return false;
            }
            std::this_thread::sleep_for(1ms);
        }
        return false;
    }

    std::atomic<bool> shutdownFlag_ = false;
    std::thread serverThread_;
    unsigned short boundPort_ = 0;
};

} // namespace e2e
} // namespace chess
