// End-to-end tests for the phase-10.2 account flow and rated games:
// real server over TCP, with a real temp SQLite database.
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>
#include <variant>

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
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
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

TEST_F(RatedE2eTest, GuestJoinCannotTakeAnAccountName)
{
    // An account is gated by its password, so guests must not reuse its
    // name: chat/archive attribution and player cards would be spoofable.
    auto creator = connectClient();
    sendMsg(creator, chess::net::LoginMsg{"Ada", "pw"});
    ASSERT_TRUE(await<chess::net::ProfileMsg>(creator).has_value());

    // Same name case-insensitively (the account lookup is NOCASE) → refused.
    auto guest = connectClient();
    sendMsg(guest, chess::net::JoinMsg{"ada"});
    auto err = await<chess::net::ErrorMsg>(guest);
    ASSERT_TRUE(err.has_value());
    EXPECT_NE(err->message.find("account"), std::string::npos);

    // Any other name queues as before (paired here from a second socket).
    sendMsg(guest, chess::net::JoinMsg{"GuestA"});
    auto peer = connectClient();
    sendMsg(peer, chess::net::JoinMsg{"GuestB"});
    auto welcome = await<chess::net::WelcomeMsg>(guest);
    ASSERT_TRUE(welcome.has_value());
    EXPECT_FALSE(welcome->rated); // still an unrated guest game
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

TEST_F(RatedE2eTest, DisconnectingClientLosesAndGameIsRated)
{
    // Ranked loss on disconnect: no resignation message is ever sent — the
    // server's disconnect path assigns the defeat to the dropped side and
    // rates it like any decisive result.
    auto white = connectClient();
    auto black = connectClient();

    sendMsg(white, chess::net::LoginMsg{"Ria", "pw"});
    sendMsg(black, chess::net::LoginMsg{"Ken", "pw"});
    ASSERT_TRUE(await<chess::net::ProfileMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::ProfileMsg>(black).has_value());

    sendMsg(white, chess::net::JoinMsg{"Ria"});
    sendMsg(black, chess::net::JoinMsg{"Ken"});
    ASSERT_TRUE(await<chess::net::WelcomeMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::WelcomeMsg>(black).has_value());

    ASSERT_TRUE(playMove(white, "f3"));

    // White drops the connection outright.
    white.disconnect();
    auto bOver = await<chess::net::GameOverMsg>(black);
    ASSERT_TRUE(bOver.has_value());
    EXPECT_EQ(bOver->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(bOver->reason, chess::net::GameOverReason::Disconnection);

    auto bProfile = await<chess::net::ProfileMsg>(black);
    ASSERT_TRUE(bProfile.has_value());
    EXPECT_GT(bProfile->rating, 1500.0);
    EXPECT_LT(bProfile->deviation, 350.0);

    // The dropped client logs in again fresh and sees the defeat applied.
    auto rejoin = connectClient();
    sendMsg(rejoin, chess::net::LoginMsg{"Ria", "pw"});
    auto rProfile = await<chess::net::ProfileMsg>(rejoin);
    ASSERT_TRUE(rProfile.has_value());
    EXPECT_LT(rProfile->rating, 1500.0);
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

    // One move was played? The guests never moved, so resigning immediately
    // from the start position leaves sanHistory() empty archive-wise: NO row.
    auto archivist = connectClient();
    sendMsg(archivist, chess::net::ArchiveRequestMsg{50, false});
    auto list = await<chess::net::ArchiveListMsg>(archivist);
    ASSERT_TRUE(list.has_value());
    EXPECT_TRUE(list->games.empty());
}

TEST_F(RatedE2eTest, ArchivePersistsAndServesCompletedGames)
{
    auto white = connectClient();
    auto black = connectClient();

    sendMsg(white, chess::net::LoginMsg{"Ada", "pw"});
    sendMsg(black, chess::net::LoginMsg{"Bob", "pw"});
    ASSERT_TRUE(await<chess::net::ProfileMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::ProfileMsg>(black).has_value());

    sendMsg(white, chess::net::JoinMsg{"Ada"});
    sendMsg(black, chess::net::JoinMsg{"Bob"});
    ASSERT_TRUE(await<chess::net::WelcomeMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::WelcomeMsg>(black).has_value());

    // Fool's mate: 1. f3 e5 2. g4 Qh4# — each move awaits its echo so the
    // server has applied it before the next one is sent.
    ASSERT_TRUE(playMove(white, "f3"));
    ASSERT_TRUE(playMove(black, "e5"));
    ASSERT_TRUE(playMove(white, "g4"));
    ASSERT_TRUE(playMove(black, "Qh4#"));
    ASSERT_TRUE(await<chess::net::GameOverMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::GameOverMsg>(black).has_value());

    // Per-player list view as Ada (was white).
    auto archivist = connectClient();
    sendMsg(archivist, chess::net::LoginMsg{"Ada", "pw"});
    ASSERT_TRUE(await<chess::net::ProfileMsg>(archivist).has_value());
    sendMsg(archivist, chess::net::ArchiveRequestMsg{50, true});
    auto mine = await<chess::net::ArchiveListMsg>(archivist);
    ASSERT_TRUE(mine.has_value());
    ASSERT_EQ(mine->games.size(), 1u);
    EXPECT_EQ(mine->games[0].whiteName, "Ada");
    EXPECT_EQ(mine->games[0].blackName, "Bob");
    EXPECT_TRUE(mine->games[0].rated);
    EXPECT_EQ(mine->games[0].result, static_cast<int>(chess::net::GameResult::BlackWins));

    // Guest view: mineOnly degrades to the global list.
    auto before = connectClient();
    sendMsg(before, chess::net::ArchiveRequestMsg{10, true});
    auto global = await<chess::net::ArchiveListMsg>(before);
    ASSERT_TRUE(global.has_value());
    ASSERT_EQ(global->games.size(), 1u);

    // Record fetch by id.
    sendMsg(before, chess::net::ArchiveGetMsg{global->games[0].id});
    auto record = await<chess::net::ArchiveGameMsg>(before);
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->id, global->games[0].id);
    EXPECT_EQ(record->whiteName, "Ada");
    EXPECT_EQ(record->blackName, "Bob");
    EXPECT_EQ(record->result, static_cast<int>(chess::net::GameResult::BlackWins));
    EXPECT_EQ(record->reason, static_cast<int>(chess::net::GameOverReason::Checkmate));
    EXPECT_EQ(record->sanMoves, "f3 e5 g4 Qh4#");
    EXPECT_TRUE(record->rated);
    EXPECT_GT(record->playedAt, 0);

    // Unknown id and unknown-name players are cleanly reported.
    sendMsg(before, chess::net::ArchiveGetMsg{987654});
    auto missing = await<chess::net::ErrorMsg>(before);
    ASSERT_TRUE(missing.has_value());

    // Bob's own list also sees the game from the other side.
    auto bobSock = connectClient();
    sendMsg(bobSock, chess::net::LoginMsg{"Bob", "pw"});
    ASSERT_TRUE(await<chess::net::ProfileMsg>(bobSock).has_value());
    sendMsg(bobSock, chess::net::ArchiveRequestMsg{50, true});
    auto bobList = await<chess::net::ArchiveListMsg>(bobSock);
    ASSERT_TRUE(bobList.has_value());
    ASSERT_EQ(bobList->games.size(), 1u);
    EXPECT_EQ(bobList->games[0].blackName, "Bob");
}

TEST_F(RatedE2eTest, LeaderboardListsRatedAccountsInAnyState)
{
    // Fool's mate won by black: Ada (black) beats Bob (white).
    auto white = connectClient();
    auto black = connectClient();
    sendMsg(white, chess::net::LoginMsg{"Bob", "pw"});
    sendMsg(black, chess::net::LoginMsg{"Ada", "pw"});
    ASSERT_TRUE(await<chess::net::ProfileMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::ProfileMsg>(black).has_value());
    sendMsg(white, chess::net::JoinMsg{"Ada"});
    sendMsg(black, chess::net::JoinMsg{"Bob"});
    ASSERT_TRUE(await<chess::net::WelcomeMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::WelcomeMsg>(black).has_value());
    ASSERT_TRUE(playMove(white, "f3"));
    ASSERT_TRUE(playMove(black, "e5"));
    ASSERT_TRUE(playMove(white, "g4"));
    ASSERT_TRUE(playMove(black, "Qh4#"));
    ASSERT_TRUE(await<chess::net::GameOverMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::GameOverMsg>(black).has_value());

    // A fresh connection in the pre-login Connected state gets the board —
    // the handler answers in any connection state. Ada (winner) first.
    auto observer = connectClient();
    sendMsg(observer, chess::net::LeaderboardRequestMsg{10});
    auto list = await<chess::net::LeaderboardListMsg>(observer);
    ASSERT_TRUE(list.has_value());
    ASSERT_EQ(list->players.size(), 2u);
    EXPECT_EQ(list->players[0].name, "Ada");
    EXPECT_GT(list->players[0].rating, 1500.0);
    EXPECT_EQ(list->players[0].wins, 1);
    EXPECT_EQ(list->players[0].losses, 0);
    EXPECT_EQ(list->players[1].name, "Bob");
    EXPECT_LT(list->players[1].rating, 1500.0);
    EXPECT_EQ(list->players[1].losses, 1);

    // Queued players get the answer too (the request never disturbs the
    // queue): a guest join then a leaderboard request.
    auto queued = connectClient();
    sendMsg(queued, chess::net::JoinMsg{"IdleGuest"});
    sendMsg(queued, chess::net::LeaderboardRequestMsg{10});
    auto queuedList = await<chess::net::LeaderboardListMsg>(queued);
    ASSERT_TRUE(queuedList.has_value());
    EXPECT_EQ(queuedList->players.size(), 2u);

    // Limit 0 falls back to a real answer (server default), not an error.
    sendMsg(observer, chess::net::LeaderboardRequestMsg{0});
    auto defaulted = await<chess::net::LeaderboardListMsg>(observer);
    ASSERT_TRUE(defaulted.has_value());
    EXPECT_EQ(defaulted->players.size(), 2u);
}

TEST_F(RatedE2eTest, LeaderboardHidesGuestOnlySourcesAndFreshAccounts)
{
    // No rated games yet: even with accounts registered, the leaderboard is
    // empty (fresh factory rows never rank).
    auto first = connectClient();
    sendMsg(first, chess::net::LoginMsg{"Ada", "pw"});
    ASSERT_TRUE(await<chess::net::ProfileMsg>(first).has_value());
    sendMsg(first, chess::net::LeaderboardRequestMsg{10});
    auto empty = await<chess::net::LeaderboardListMsg>(first);
    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE(empty->players.empty());
}

TEST_F(RatedE2eTest, ArchiveRequestBeyondClampStillDeserializes)
{
    // The archive server clamp (100) must match the ListMsg wire guard so an
    // over-large request never produces a list the client rejects. One rated
    // game feeds the archive: 1. f3 e5, then white resigns.
    auto white = connectClient();
    auto black = connectClient();
    sendMsg(white, chess::net::LoginMsg{"Ada", "pw"});
    sendMsg(black, chess::net::LoginMsg{"Bob", "pw"});
    ASSERT_TRUE(await<chess::net::ProfileMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::ProfileMsg>(black).has_value());
    sendMsg(white, chess::net::JoinMsg{"Ada"});
    sendMsg(black, chess::net::JoinMsg{"Bob"});
    ASSERT_TRUE(await<chess::net::WelcomeMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::WelcomeMsg>(black).has_value());
    ASSERT_TRUE(playMove(white, "f3"));
    ASSERT_TRUE(playMove(black, "e5"));
    sendMsg(white, chess::net::ResignMsg{});
    ASSERT_TRUE(await<chess::net::GameOverMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::GameOverMsg>(black).has_value());

    auto observer = connectClient();
    sendMsg(observer, chess::net::ArchiveRequestMsg{500, false});
    auto list = await<chess::net::ArchiveListMsg>(observer);
    ASSERT_TRUE(list.has_value());
    ASSERT_EQ(list->games.size(), 1u);
    EXPECT_EQ(list->games[0].whiteName, "Ada");
    EXPECT_TRUE(list->games[0].rated);
}

} // namespace
