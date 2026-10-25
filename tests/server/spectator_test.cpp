// 12.4 Spectators — Match-level behaviour: move relays carry the watcher
// count, the game end releases the stands, a takeback re-syncs the watchers'
// replay chain, and the stands have a (large) cap. The server-side routing
// (spectate lookup, play-verb refusals for Connected watchers) is covered by
// the e2e suite; here the Client objects are wired straight onto the Match.

#include <gtest/gtest.h>

#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>

#include <chess/net/messages.h>
#include <chess/net/protocol.h>

#include "client.h"
#include "match.h"

#include <memory>
#include <thread>
#include <vector>

namespace {

class SpectatorMatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(listener_.listen(0), sf::Socket::Status::Done);
        port_ = listener_.getLocalPort();

        // White, Black, then one spectator connection per client object the
        // test creates (sockets are consumed in SetUp order).
        acceptThread_ = std::thread([this]() {
            for (auto& slot : serverSocks_) {
                slot = std::make_unique<sf::TcpSocket>();
                [[maybe_unused]] auto s = listener_.accept(*slot);
            }
            listener_.close();
        });

        for (auto& sock : clientSocks_) {
            sock = std::make_unique<sf::TcpSocket>();
            ASSERT_EQ(sock->connect(sf::IpAddress::LocalHostV4, port_),
                      sf::Socket::Status::Done);
        }
        acceptThread_.join();

        white_.socket = std::move(serverSocks_[0]);
        black_.socket = std::move(serverSocks_[1]);
        white_.name = "Alice";
        black_.name = "Bob";

        // Zero grace makes any 11.2 window deterministic; spectators never
        // sit, so this only matters for the end-of-game test.
        match_ = std::make_unique<Match>(white_, black_, std::string(),
                                         std::string(), 0);
    }

    // Binds a fresh watcher connection to the live match, the way
    // server.cpp's handleSpectate does.
    Client& attachSpectator() {
        auto client = std::make_unique<Client>();
        client->socket = std::move(serverSocks_[2 + watchers_.size()]);
        client->name = "Watch" + std::to_string(watchers_.size());
        client->spectating = match_.get();
        Client* raw = client.get();
        match_->addSpectator(*raw);
        watchers_.push_back(std::move(client));
        return *raw;
    }

    // Server-side messages land on the matching client-side socket; the
    // watcher's Client listens on serverSocks_[2], so reads go through
    // clientSocks_[2] (the other end of that connection).
    std::optional<chess::net::ServerMessage> recvWatcher() {
        return recvOnClient(*clientSocks_[2]);
    }

    std::optional<chess::net::ServerMessage> tryRecvWatcher() {
        return tryRecvOnClient(*clientSocks_[2]);
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
    std::unique_ptr<sf::TcpSocket> serverSocks_[3];
    std::unique_ptr<sf::TcpSocket> clientSocks_[3];
    std::thread acceptThread_;
    Client white_;
    Client black_;
    std::unique_ptr<Match> match_;
    std::vector<std::unique_ptr<Client>> watchers_;
};

TEST_F(SpectatorMatchTest, MoveRelayCarriesWatcherCount)
{
    Client& watcher = attachSpectator();
    ASSERT_EQ(match_->spectatorCount(), 1);

    // Before the first relay: the seats have nothing queued.
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});

    for (auto* sock : {clientSocks_[0].get(), clientSocks_[1].get()}) {
        auto msg = recvOnClient(*sock);
        ASSERT_TRUE(msg.has_value());
        auto* move = std::get_if<chess::net::ServerMoveMsg>(&*msg);
        ASSERT_NE(move, nullptr);
        // The count rides the same broadcast the seats get.
        EXPECT_EQ(move->spectators, 1);
    }

    auto msg = recvWatcher();
    ASSERT_TRUE(msg.has_value());
    auto* move = std::get_if<chess::net::ServerMoveMsg>(&*msg);
    ASSERT_NE(move, nullptr);
    EXPECT_EQ(move->san, "e4");
    EXPECT_EQ(move->spectators, 1);
}

TEST_F(SpectatorMatchTest, AfterStopTheCountDrops)
{
    Client& watcher = attachSpectator();
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});
    ASSERT_TRUE(recvWatcher().has_value());
    // Drain the seats' echoes so the next receive is the live relay.
    [[maybe_unused]] auto dw = recvOnClient(*clientSocks_[0]);
    [[maybe_unused]] auto db = recvOnClient(*clientSocks_[1]);

    // SpectateStop (12.4): release the watch; the next relay reflects it.
    EXPECT_TRUE(match_->removeSpectator(watcher));
    watcher.spectating = nullptr;
    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});

    auto whiteMove = tryRecvOnClient(*clientSocks_[0]);
    ASSERT_TRUE(whiteMove.has_value());
    auto* move = std::get_if<chess::net::ServerMoveMsg>(&*whiteMove);
    ASSERT_NE(move, nullptr);
    EXPECT_EQ(move->spectators, 0);
}

TEST_F(SpectatorMatchTest, GameEndReleasesWatchers)
{
    Client& watcher = attachSpectator();

    match_->handleMessage(white_, chess::net::ResignMsg{});

    auto msg = recvWatcher();
    ASSERT_TRUE(msg.has_value());
    auto* over = std::get_if<chess::net::GameOverMsg>(&*msg);
    ASSERT_NE(over, nullptr);
    EXPECT_EQ(over->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Resignation);

    // The game end unbinds the stands itself: no stale spectating pointer
    // may outlive the retained match shell.
    EXPECT_EQ(watcher.spectating, nullptr);
    EXPECT_EQ(match_->spectatorCount(), 0);
}

TEST_F(SpectatorMatchTest, TakebackResyncsWatcherSnapshot)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});
    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});
    // Drain the seats' move echoes so later receives are clean.
    [[maybe_unused]] auto w1 = recvOnClient(*clientSocks_[0]);
    [[maybe_unused]] auto b1 = recvOnClient(*clientSocks_[1]);

    Client& watcher = attachSpectator();

    // White retracts e4+e5 (the classic two-ply frame: asker on the move
    // with ≥2 plies), black accepts.
    match_->handleMessage(white_, chess::net::TakebackRequestMsg{});
    match_->handleMessage(black_, chess::net::TakebackAcceptMsg{});

    // The retracted plies never air as Move broadcasts — the snapshot is
    // the sentinel that keeps the watcher's replayed chain in step.
    auto msg = recvWatcher();
    ASSERT_TRUE(msg.has_value());
    auto* snap = std::get_if<chess::net::SpectateStateMsg>(&*msg);
    ASSERT_NE(snap, nullptr);
    EXPECT_TRUE(snap->sans.empty());
    EXPECT_EQ(snap->whiteName, "Alice");
    EXPECT_EQ(snap->blackName, "Bob");
}

TEST_F(SpectatorMatchTest, SnapshotCarriesCurrentChain)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});
    Client& watcher = attachSpectator();

    auto snap = match_->spectateSnapshot();
    EXPECT_EQ(snap.sans.size(), 1u);
    EXPECT_EQ(snap.sans[0], "e4");
    EXPECT_EQ(snap.whiteName, "Alice");
    EXPECT_EQ(snap.spectators, 1);
}

TEST_F(SpectatorMatchTest, StandsHaveACap)
{
    // addSpectator refuses past Match::MaxSpectators. The extra watchers
    // are pointer-only members (no relays fire while nobody moves), so no
    // sockets are needed for the overflow.
    for (std::size_t i = 0; i < Match::MaxSpectators; ++i) {
        watchers_.push_back(std::make_unique<Client>());
        EXPECT_TRUE(match_->addSpectator(*watchers_.back()))
            << "watcher " << i;
    }
    Client overflow;
    EXPECT_FALSE(match_->addSpectator(overflow));
    EXPECT_EQ(match_->spectatorCount(), Match::MaxSpectators);
}

TEST_F(SpectatorMatchTest, DuplicateBindIsIdempotent)
{
    Client& watcher = attachSpectator();
    // A second spectate of the same game (client re-affirming) must not
    // double-count or duplicate relays.
    EXPECT_TRUE(match_->addSpectator(watcher));
    EXPECT_EQ(match_->spectatorCount(), 1);
}

} // namespace
