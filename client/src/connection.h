#pragma once

#include <chess/net/messages.h>

#include <SFML/Network.hpp>

#include <atomic>
#include <chrono>
#include <deque>
#include <queue>
#include <string>
#include <thread>

namespace chess::client {

enum class ConnectionState { Disconnected, Connecting, Connected };

class Connection {
public:
    static constexpr auto DefaultPingInterval = std::chrono::seconds{30};
    static constexpr auto DefaultPongTimeout = std::chrono::seconds{10};
    static constexpr std::size_t MaxOutboxSize = 128;
    static constexpr std::size_t MaxInboxSize = 256;

    explicit Connection(std::chrono::steady_clock::duration pingInterval = DefaultPingInterval,
                        std::chrono::steady_clock::duration pongTimeout = DefaultPongTimeout);
    ~Connection();
    void connect(const std::string& host, unsigned short port);
    void disconnect();
    void join(const std::string& name);

    void poll();
    bool send(const chess::net::ClientMessage& msg);

    bool hasMessages() const;
    std::size_t messageCount() const;
    chess::net::ServerMessage nextMessage();

    ConnectionState state() const;
    const std::string& error() const;

private:
    void drainOutbox();
    void drainInbox();

    sf::TcpSocket socket_;
    std::queue<chess::net::ClientMessage> outbox_;
    std::deque<chess::net::ServerMessage> inbox_;
    ConnectionState state_ = ConnectionState::Disconnected;
    std::string error_;
    std::chrono::steady_clock::time_point connectStart_;

    std::thread connectThread_;
    std::atomic<bool> connectDone_{false};
    std::atomic<bool> connectSuccess_{false};
    std::string connectError_;

    std::chrono::steady_clock::time_point lastPingSent_;
    std::chrono::steady_clock::time_point lastPongReceived_;
    std::chrono::steady_clock::duration pingInterval_;
    std::chrono::steady_clock::duration pongTimeout_;
};

} // namespace chess::client
