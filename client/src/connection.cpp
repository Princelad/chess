#include "connection.h"

#include <chess/net/protocol.h>

#include <cassert>
#include <memory>
#include <vector>

using chess::net::ClientMessage;
using chess::net::ServerMessage;
using chess::net::serialize;
using chess::net::deserializeServer;

namespace chess::client {

static constexpr std::size_t MaxPacketSize = 4096;
static constexpr auto ConnectTimeout = sf::seconds(3);

Connection::Connection(std::chrono::steady_clock::duration pingInterval,
                       std::chrono::steady_clock::duration pongTimeout)
    : pingInterval_(pingInterval), pongTimeout_(pongTimeout)
{
}

void Connection::connect(const std::string& host, unsigned short port)
{
    disconnect();

    std::vector<sf::IpAddress> addresses;
    if (auto ip = sf::IpAddress::fromString(host)) {
        addresses.push_back(*ip);
    } else {
        auto resolved = sf::Dns::resolve(host);
        if (!resolved || resolved->empty()) {
            error_ = "Cannot resolve: " + host;
            return;
        }
        addresses = std::move(*resolved);
    }

    state_ = ConnectionState::Connecting;
    connectDone_ = false;
    connectSuccess_ = false;
    connectError_.clear();
    connectStart_ = std::chrono::steady_clock::now();

    auto addrs = std::make_shared<std::vector<sf::IpAddress>>(std::move(addresses));

    connectThread_ = std::thread([this, addrs, port]() {
        socket_.setBlocking(true);
        for (const auto& ip : *addrs) {
            auto status = socket_.connect(ip, port, ConnectTimeout);
            if (status == sf::Socket::Status::Done) {
                socket_.setBlocking(false);
                connectSuccess_ = true;
                connectDone_ = true;
                return;
            }
            socket_.disconnect();
        }
        connectError_ = "Connection failed";
        connectDone_ = true;
    });
}

void Connection::disconnect()
{
    if (connectThread_.joinable()) connectThread_.join();
    socket_.disconnect();
    while (!outbox_.empty()) outbox_.pop();
    inbox_.clear();
    state_ = ConnectionState::Disconnected;
    error_.clear();
}

void Connection::join(const std::string& name)
{
    send(chess::net::JoinMsg{name});
}

Connection::~Connection()
{
    if (connectThread_.joinable()) connectThread_.join();
}

void Connection::poll()
{
    if (state_ == ConnectionState::Connecting) {
        if (connectDone_) {
            if (connectThread_.joinable()) connectThread_.join();
            if (connectSuccess_) {
                state_ = ConnectionState::Connected;
                lastPingSent_ = std::chrono::steady_clock::now();
                lastPongReceived_ = lastPingSent_;
            } else {
                error_ = std::move(connectError_);
                state_ = ConnectionState::Disconnected;
            }
        }
        return;
    }

    if (state_ != ConnectionState::Connected) return;

    auto now = std::chrono::steady_clock::now();

    if (now - lastPingSent_ >= pingInterval_) {
        outbox_.push(chess::net::PingMsg{});
        lastPingSent_ = now;
    }

    if (now - lastPongReceived_ > pingInterval_ + pongTimeout_) {
        error_ = "Connection timed out";
        state_ = ConnectionState::Disconnected;
        return;
    }

    drainOutbox();
    if (state_ == ConnectionState::Connected) drainInbox();
}

bool Connection::send(const ClientMessage& msg)
{
    if (state_ != ConnectionState::Connected) return false;
    if (outbox_.size() >= MaxOutboxSize) return false;
    outbox_.push(msg);
    return true;
}

bool Connection::hasMessages() const
{
    return !inbox_.empty();
}

std::size_t Connection::messageCount() const
{
    return inbox_.size();
}

ServerMessage Connection::nextMessage()
{
    assert(!inbox_.empty());
    auto msg = std::move(inbox_.front());
    inbox_.pop_front();
    return msg;
}

ConnectionState Connection::state() const
{
    return state_;
}

const std::string& Connection::error() const
{
    return error_;
}

void Connection::drainOutbox()
{
    while (!outbox_.empty()) {
        sf::Packet packet;
        if (!serialize(packet, outbox_.front())) {
            outbox_.pop();
            error_ = "Serialization failed";
            state_ = ConnectionState::Disconnected;
            return;
        }
        auto status = socket_.send(packet);
        if (status == sf::Socket::Status::Done) {
            outbox_.pop();
        } else if (status == sf::Socket::Status::Disconnected ||
                   status == sf::Socket::Status::Error) {
            error_ = "Connection lost";
            state_ = ConnectionState::Disconnected;
            return;
        } else {
            return;
        }
    }
}

void Connection::drainInbox()
{
    std::size_t count = 0;
    for (; count < MaxDrainPerPoll; ++count) {
        sf::Packet packet;
        auto status = socket_.receive(packet);
        if (status == sf::Socket::Status::Done) {
            if (packet.getDataSize() > MaxPacketSize) {
                error_ = "Packet too large";
                state_ = ConnectionState::Disconnected;
                return;
            }
            auto msg = deserializeServer(packet);
            if (msg) {
                if (std::get_if<chess::net::PongMsg>(&*msg)) {
                    lastPongReceived_ = std::chrono::steady_clock::now();
                }
                inbox_.push_back(std::move(*msg));
                if (inbox_.size() > MaxInboxSize) {
                    error_ = "Inbox overflow";
                    state_ = ConnectionState::Disconnected;
                    return;
                }
            }
        } else if (status == sf::Socket::Status::Disconnected ||
                   status == sf::Socket::Status::Error) {
            error_ = "Connection lost";
            state_ = ConnectionState::Disconnected;
            return;
        } else {
            return;
        }
    }
}

} // namespace chess::client
