// End-to-end tests for 12.4 Spectators: a Connected (non-seated) connection
// binds onto a live game by seat name or the empty "any game" sentinel,
// receives the SAN-chain snapshot and every later move (with the watcher
// count riding each broadcast), play verbs from the stands are refused,
// SpectateStop stops the relay, and the game end releases the stands.

#include <gtest/gtest.h>

#include <optional>
#include <variant>

#include <SFML/Network/TcpSocket.hpp>

#include <chess/net/messages.h>
#include <chess/net/protocol.h>

#include "harness.h"
#include "server.h"

using namespace std::chrono_literals;

class SpectatorE2eBase : public chess::e2e::E2eHarness {
protected:
    // Binds `spectator` onto the pair's game via the spectate verbs,
    // returning the sync snapshot (empty on failure to bind).
    std::optional<chess::net::SpectateStateMsg> spectate(
        sf::TcpSocket& spectator, const std::string& name)
    {
        sendMsg(spectator, chess::net::SpectateMsg{name});
        auto snap = await<chess::net::SpectateStateMsg>(spectator);
        if (snap.has_value()) {
            EXPECT_EQ(snap->whiteName, "Alice");
            EXPECT_EQ(snap->blackName, "Bob");
        }
        return snap;
    }
};

class SpectatorE2eTest : public SpectatorE2eBase {
};


TEST_F(SpectatorE2eTest, WatchByNameSyncsAndFollowsLive)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black, "Alice", "Bob");

    // One landed move before the watcher arrives: the snapshot must carry
    // exactly that chain.
    ASSERT_TRUE(playMove(white, "e4"));

    sf::TcpSocket watcher = connectClient();
    auto bound = spectate(watcher, "Alice");
    ASSERT_TRUE(bound.has_value()) << "spectate binding accepted";
    ASSERT_EQ(bound->sans.size(), 1u);
    EXPECT_EQ(bound->sans[0], "e4");

    // Live play follows: the next move relays to the stands.
    ASSERT_TRUE(playMove(black, "e5"));
    auto watchMove = await<chess::net::ServerMoveMsg>(watcher);
    ASSERT_TRUE(watchMove.has_value());
    EXPECT_EQ(watchMove->san, "e5");
    EXPECT_EQ(watchMove->spectators, 1);

    // The seats' own echoes carry the same count with every move.
    auto seatEcho = await<chess::net::ServerMoveMsg>(white);
    ASSERT_TRUE(seatEcho.has_value());
    EXPECT_EQ(seatEcho->san, "e5");
    EXPECT_EQ(seatEcho->spectators, 1);
}

TEST_F(SpectatorE2eTest, EmptyNamePicksAnyLiveGame)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black, "Alice", "Bob");

    sf::TcpSocket watcher = connectClient();
    sendMsg(watcher, chess::net::SpectateMsg{""});
    auto snap = await<chess::net::SpectateStateMsg>(watcher);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->whiteName, "Alice");
    EXPECT_EQ(snap->blackName, "Bob");
}

TEST_F(SpectatorE2eTest, NoGamesInProgressIsNamed)
{
    sf::TcpSocket watcher = connectClient();
    sendMsg(watcher, chess::net::SpectateMsg{""});
    auto err = await<chess::net::ErrorMsg>(watcher);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->message, "No games in progress");
}

TEST_F(SpectatorE2eTest, NoGameForUnknownNameIsNamed)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black, "Alice", "Bob");

    sf::TcpSocket watcher = connectClient();
    sendMsg(watcher, chess::net::SpectateMsg{"Zed"});
    auto err = await<chess::net::ErrorMsg>(watcher);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->message, "No game in progress for 'Zed'");
}

TEST_F(SpectatorE2eTest, SpectatorCannotPlay)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black, "Alice", "Bob");

    sf::TcpSocket watcher = connectClient();
    spectate(watcher, "Bob");

    // A watcher owns no seat: play verbs answer with the ordinary
    // not-joined error, and the game itself is untouched.
    sendMsg(watcher, chess::net::MoveMsg{"Nf3"});
    auto err = await<chess::net::ErrorMsg>(watcher);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->message, "Join first");

    sendMsg(watcher, chess::net::ResignMsg{});
    auto err2 = await<chess::net::ErrorMsg>(watcher);
    ASSERT_TRUE(err2.has_value());
    EXPECT_EQ(err2->message, "Join first");

    // The game is alive: white still moves free of the stands' noise.
    ASSERT_TRUE(playMove(white, "e4"));
    auto watchMove = await<chess::net::ServerMoveMsg>(watcher);
    ASSERT_TRUE(watchMove.has_value());
    EXPECT_EQ(watchMove->san, "e4");
}

TEST_F(SpectatorE2eTest, StopReleasesTheWatch)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black, "Alice", "Bob");

    sf::TcpSocket watcher = connectClient();
    spectate(watcher, "Alice");

    sendMsg(watcher, chess::net::SpectateStopMsg{});
    // Deterministic unbind probe: while still watching, a ping answers
    // Pong (the spectator chain); once the release has landed, the same
    // ping falls into the Connected fallback ("Join first").
    ASSERT_TRUE(pollUntil([&]() {
        sendMsg(watcher, chess::net::PingMsg{});
        auto m = tryRecv(watcher);
        return m.has_value()
            && std::holds_alternative<chess::net::ErrorMsg>(*m);
    }));

    ASSERT_TRUE(playMove(white, "e4"));
    auto seatEcho = await<chess::net::ServerMoveMsg>(black);
    ASSERT_TRUE(seatEcho.has_value());
    EXPECT_EQ(seatEcho->spectators, 0);

    // One full timeout of quiet on the watcher's socket.
    auto nothing = await<chess::net::ServerMoveMsg>(watcher, 300ms);
    EXPECT_FALSE(nothing.has_value());
}

TEST_F(SpectatorE2eTest, GameOverUnbindsAndInformsTheStands)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black, "Alice", "Bob");

    sf::TcpSocket watcher = connectClient();
    spectate(watcher, "Alice");

    sendMsg(white, chess::net::ResignMsg{});
    auto over = await<chess::net::GameOverMsg>(watcher);
    ASSERT_TRUE(over.has_value());
    EXPECT_EQ(over->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Resignation);

    // The ended game released the stands: with the only match finished, a
    // fresh spectate finds nothing in progress.
    sendMsg(watcher, chess::net::SpectateMsg{""});
    auto err = await<chess::net::ErrorMsg>(watcher);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->message, "No games in progress");
}

TEST_F(SpectatorE2eTest, TakebackResyncsTheWatchedChain)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black, "Alice", "Bob");

    ASSERT_TRUE(playMove(white, "e4"));
    ASSERT_TRUE(playMove(black, "e5"));

    sf::TcpSocket watcher = connectClient();
    spectate(watcher, "Alice");

    // White retracts e4+e5, black accepts.
    sendMsg(white, chess::net::TakebackRequestMsg{});
    sendMsg(black, chess::net::TakebackAcceptMsg{});

    auto snap = await<chess::net::SpectateStateMsg>(watcher);
    ASSERT_TRUE(snap.has_value());
    EXPECT_TRUE(snap->sans.empty());
}
