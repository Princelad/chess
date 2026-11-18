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

// Audit 3.2: 4096 could not hold a long game's resume/archive payload —
// ~450+ plies of SAN is ~4.6 KB, so a reconnect (or a history browse of a
// long game) hard-disconnected the client with "Packet too large" and the
// rated seat was lost. 16384 keeps the sanity limit while fitting any
// realistic game (~4000 plies of disambiguated SAN); both sides must keep
// this value in sync.
static constexpr std::size_t MaxPacketSize = 16384;

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
    connectCancel_ = false;

    auto addrs = std::make_shared<std::vector<sf::IpAddress>>(std::move(addresses));

    // Audit: the thread used to connect with a 3 s BLOCKING timeout per
    // address, so disconnect() joining it froze the UI for up to 3 s
    // (more with several resolved addresses). The connect itself is
    // non-blocking now and the wait runs in ~50 ms selector slices, so a
    // cancel request is honored within one slice.
    connectThread_ = std::thread([this, addrs, port]() {
        socket_.setBlocking(false);
        // Overall deadline replaces the old per-address 3 s blocking
        // timeout: the handshake phase never outlives it.
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(3000);
        for (const auto& ip : *addrs) {
            if (connectCancel_.load(std::memory_order_relaxed)) return;
            auto status = socket_.connect(ip, port);
            if (status == sf::Socket::Status::Done) {
                connectSuccess_ = true;
                connectDone_ = true;
                return;
            }
            if (status == sf::Socket::Status::NotReady) {
                sf::SocketSelector selector;
                selector.add(socket_, sf::SocketSelector::Send);
                while (!connectCancel_.load(std::memory_order_relaxed)) {
                    if (std::chrono::steady_clock::now() >= deadline)
                        break;
                    if (!selector.wait(sf::milliseconds(50))) continue;
                    // Writable means the handshake returned. connect()
                    // cannot be re-called here — SFML disconnects the
                    // socket first — so completion reads like SFML's own
                    // blocking-timeout path: a connected peer address is
                    // the success check.
                    if (socket_.getRemoteAddress().has_value()) {
                        connectSuccess_ = true;
                        connectDone_ = true;
                        return;
                    }
                    break; // writable but refused: try the next address
                }
                if (connectCancel_.load(std::memory_order_relaxed)) return;
            }
            if (std::chrono::steady_clock::now() >= deadline) break;
            socket_.disconnect();
        }
        connectError_ = "Connection failed";
        connectDone_ = true;
    });
}

void Connection::disconnect()
{
    connectCancel_ = true;
    if (connectThread_.joinable()) connectThread_.join();
    socket_.disconnect();
    while (!outbox_.empty()) outbox_.pop();
    // Audit 5.1: the in-flight partial packet must not outlive the socket
    // either. drainOutbox parks the front message's serialized packet here
    // across Partial/NotReady sends (the SFML partial-send contract), and a
    // mid-packet drop used to leave the stale bytes behind — the next
    // connect()'s first drainOutbox() then skipped serializing the fresh
    // front message (JoinMsg/ResumeMsg), completed the stale packet instead,
    // and popped the fresh message un-sent: the server never saw the join or
    // resume and the seat was falsely aborted by the ~30 s give-up.
    pendingSend_.reset();
    inbox_.clear();
    state_ = ConnectionState::Disconnected;
    error_.clear();
}

void Connection::join(const std::string& name, const std::string& timeControl,
                      const std::string& variant)
{
    send(chess::net::JoinMsg{name, timeControl, variant});
}

void Connection::resume(const std::string& token)
{
    send(chess::net::ResumeMsg{token});
}

void Connection::spectate(const std::string& name)
{
    send(chess::net::SpectateMsg{name});
}

void Connection::spectateStop()
{
    send(chess::net::SpectateStopMsg{});
}

Connection::~Connection()
{
    // Honor a pending connect so exit never waits out the handshake.
    connectCancel_ = true;
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
        // Audit 3.x-adjacent: the heartbeat rides the same outbox cap as
        // user traffic — the old direct push could grow the queue without
        // bound while the link congested (drainOutbox would catch up, but
        // the cap is the honest admission that the link is behind).
        if (outbox_.size() < MaxOutboxSize)
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

void Connection::unreadMessage(ServerMessage msg)
{
    inbox_.push_front(std::move(msg));
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
        // SFML's partial-send contract: once send() reports Partial, the
        // SAME packet object must complete the send — a re-serialized
        // packet resets the stream position and the receiver reads a
        // corrupt framing. The front message's packet therefore lives in
        // pendingSend_ until it goes out whole.
        if (!pendingSend_) {
            pendingSend_.emplace();
            if (!serialize(*pendingSend_, outbox_.front())) {
                pendingSend_.reset();
                outbox_.pop();
                error_ = "Serialization failed";
                state_ = ConnectionState::Disconnected;
                return;
            }
        }
        auto status = socket_.send(*pendingSend_);
        if (status == sf::Socket::Status::Done) {
            pendingSend_.reset();
            outbox_.pop();
        } else if (status == sf::Socket::Status::Disconnected ||
                   status == sf::Socket::Status::Error) {
            error_ = "Connection lost";
            state_ = ConnectionState::Disconnected;
            return;
        } else {
            return; // Partial (or NotReady): resume with the same packet
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
                badMessages_ = 0;
                if (std::get_if<chess::net::PongMsg>(&*msg)) {
                    lastPongReceived_ = std::chrono::steady_clock::now();
                }
                inbox_.push_back(std::move(*msg));
                if (inbox_.size() > MaxInboxSize) {
                    error_ = "Inbox overflow";
                    state_ = ConnectionState::Disconnected;
                    return;
                }
            } else {
                badMessages_++;
                if (badMessages_ >= MaxBadMessages) {
                    error_ = "Too many malformed packets";
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
