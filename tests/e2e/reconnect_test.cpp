// End-to-end tests for 11.2 Reconnect & resume: mid-game drops stay
// recoverable through per-seat resume tokens, the grace window decides on
// absent players, and late token rejoins still see a finished game.
#include <gtest/gtest.h>

#include <filesystem>
#include <variant>

#include <SFML/Network/TcpSocket.hpp>

#include <chess/net/messages.h>
#include <chess/net/protocol.h>

#include "harness.h"
#include "server.h"

using namespace std::chrono_literals;

// DB-backed fixture: the rated scenarios check that results rate and land
// in the archive even after a seat came and went.
class ReconnectE2eTest : public chess::e2e::E2eHarness {
protected:
    void beforeStart() override
    {
        dbPath_ = std::filesystem::temp_directory_path()
            / ("chess_reconnect_e2e_" + std::to_string(++dbCounter) + ".db");
        std::filesystem::remove(dbPath_);
        std::filesystem::remove(dbPath_.string() + "-wal");
        std::filesystem::remove(dbPath_.string() + "-shm");
    }

    void configure(chess::server::ServerConfig& config) override
    {
        config.dbPath = dbPath_.string();
    }

    void afterStop() override
    {
        std::filesystem::remove(dbPath_);
        std::filesystem::remove(dbPath_.string() + "-wal");
        std::filesystem::remove(dbPath_.string() + "-shm");
    }

    // Login flow helper; returns once the fresh profile is acknowledged.
    void loginAccount(sf::TcpSocket& sock, const std::string& name,
                      const std::string& password)
    {
        sendMsg(sock, chess::net::LoginMsg{name, password});
        ASSERT_TRUE(await<chess::net::ProfileMsg>(sock).has_value())
            << "profile for " << name;
    }

    // Join and capture this seat's rejoin credential. The server pairs two
    // queued players, so both joins must be sent before any Welcome flows;
    // the caller then captures each seat's own message.
    void joinCapture(sf::TcpSocket& white, const std::string& wName,
                     sf::TcpSocket& black, const std::string& bName,
                     std::string& wTokenOut, std::string& bTokenOut)
    {
        sendMsg(white, chess::net::JoinMsg{wName});
        sendMsg(black, chess::net::JoinMsg{bName});

        auto wWelcome = await<chess::net::WelcomeMsg>(white);
        auto bWelcome = await<chess::net::WelcomeMsg>(black);
        ASSERT_TRUE(wWelcome.has_value());
        ASSERT_TRUE(bWelcome.has_value());
        EXPECT_EQ(wWelcome->color, chess::Color::White);
        EXPECT_EQ(bWelcome->color, chess::Color::Black);
        EXPECT_FALSE(wWelcome->resumeToken.empty());
        EXPECT_FALSE(bWelcome->resumeToken.empty());
        wTokenOut = wWelcome->resumeToken;
        bTokenOut = bWelcome->resumeToken;
    }

    std::filesystem::path dbPath_;

    static std::atomic<int> dbCounter;
};

std::atomic<int> ReconnectE2eTest::dbCounter{0};

// ── Leave → resume → finish (rated + archived) ─────────────────────────────

TEST_F(ReconnectE2eTest, ResumeRebindsSeatFinishesRatedAndArchives)
{
    auto white = connectClient();
    auto black = connectClient();
    loginAccount(white, "Ada", "pw");
    loginAccount(black, "Bob", "pw");

    std::string wToken, bToken;
    joinCapture(white, "Ada", black, "Bob", wToken, bToken);
    // Tokens are per seat: one cannot stand in for the other.
    EXPECT_NE(wToken, bToken);

    ASSERT_TRUE(playMove(white, "f3"));

    // White drops mid-game.
    white.disconnect();

    auto rc = await<chess::net::OpponentReconnectingMsg>(black);
    ASSERT_TRUE(rc.has_value());
    EXPECT_EQ(rc->graceSeconds,
              static_cast<std::int32_t>((testGraceMs_ + 999) / 1000));

    // Fresh connection, no join, no login: the token alone reclaims the seat.
    auto rejoin = connectClient();
    sendMsg(rejoin, chess::net::ResumeMsg{wToken});

    auto state = await<chess::net::ResumeStateMsg>(rejoin);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->color, chess::Color::White);
    EXPECT_EQ(state->opponent, "Bob");
    EXPECT_EQ(state->sans, std::vector<std::string>{"f3"});
    EXPECT_TRUE(state->rated); // Ada + Bob, both registered
    // Unlimited game (no time control joined with): clocks stay off.
    EXPECT_EQ(state->whiteRemainingMs, 0);
    EXPECT_EQ(state->blackRemainingMs, 0);

    // And the survivor is told play may continue.
    ASSERT_TRUE(await<chess::net::OpponentResumedMsg>(black).has_value());
    // The resume screen still goes by the captured credential of the OTHER
    // seat: replaying Bob's token on the live seat must not replace him.
    {
        auto stranger = connectClient();
        sendMsg(stranger, chess::net::ResumeMsg{bToken});
        auto refused = await<chess::net::ErrorMsg>(stranger);
        EXPECT_TRUE(refused.has_value());
    }

    // Finish: 1. f3 e5 2. g4 Qh4# — the rejoiner (rejoined as White) loses.
    ASSERT_TRUE(playMove(black, "e5"));
    ASSERT_TRUE(playMove(rejoin, "g4"));
    sendMsg(black, chess::net::MoveMsg{"Qh4#"});

    auto over = await<chess::net::GameOverMsg>(black);
    ASSERT_TRUE(over.has_value());
    EXPECT_EQ(over->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Checkmate);

    // Rated flow ran to completion across the drop: current profiles land
    // on the rejoiner ("Ada", fresh rating below 1500) and the survivor.
    auto adaProfile = await<chess::net::ProfileMsg>(rejoin);
    ASSERT_TRUE(adaProfile.has_value());
    EXPECT_EQ(adaProfile->name, "Ada");
    EXPECT_FALSE(adaProfile->isGuest);
    EXPECT_LT(adaProfile->rating, 1500.0);

    auto bobProfile = await<chess::net::ProfileMsg>(black);
    ASSERT_TRUE(bobProfile.has_value());
    EXPECT_EQ(bobProfile->name, "Bob");
    EXPECT_FALSE(bobProfile->isGuest);
    EXPECT_GT(bobProfile->rating, 1500.0);

    // One archived row: the finished rated game with the full SAN history.
    sendMsg(rejoin, chess::net::ArchiveRequestMsg{10, false});
    auto list = await<chess::net::ArchiveListMsg>(rejoin);
    ASSERT_TRUE(list.has_value());
    ASSERT_EQ(list->games.size(), 1u);
    const auto& row = list->games.front();
    EXPECT_EQ(row.whiteName, "Ada");
    EXPECT_EQ(row.blackName, "Bob");
    EXPECT_EQ(row.result, static_cast<int>(chess::net::GameResult::BlackWins));
}

// ── Grace expiry: the survivor wins and the result still rates ─────────────

TEST_F(ReconnectE2eTest, GraceExpiryEndsGameAndRatesSurvivor)
{
    auto white = connectClient();
    auto black = connectClient();
    loginAccount(white, "Ada", "pw");
    loginAccount(black, "Bob", "pw");
    sendMsg(white, chess::net::JoinMsg{"Ada"});
    sendMsg(black, chess::net::JoinMsg{"Bob"});
    ASSERT_TRUE(await<chess::net::WelcomeMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::WelcomeMsg>(black).has_value());

    ASSERT_TRUE(playMove(white, "e4"));
    white.disconnect();

    // While the window runs, the match is alive: no GameOverMsg yet.
    {
        auto early = await<chess::net::GameOverMsg>(black, 200ms);
        // The window itself is 400ms: at the 200ms point the game must
        // still be open (tolerated nullopt), but it cannot be over yet.
        if (early.has_value())
            ADD_FAILURE() << "game ended before the grace window lapsed";
    }

    // The absent player's clock keeps running and the window lapses: the
    // survivor wins by disconnection.
    auto over = await<chess::net::GameOverMsg>(black, 2s);
    ASSERT_TRUE(over.has_value());
    EXPECT_EQ(over->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Disconnection);

    auto bobProfile = await<chess::net::ProfileMsg>(black);
    ASSERT_TRUE(bobProfile.has_value());
    EXPECT_GT(bobProfile->rating, 1500.0); // rating even though Ada never returned
}

// ── Both gone: immediate unrated abort ─────────────────────────────────────

TEST_F(ReconnectE2eTest, DoubleDisconnectAbortsUnrated)
{
    auto white = connectClient();
    auto black = connectClient();
    loginAccount(white, "Ada", "pw");
    loginAccount(black, "Bob", "pw");
    sendMsg(white, chess::net::JoinMsg{"Ada"});
    sendMsg(black, chess::net::JoinMsg{"Bob"});
    ASSERT_TRUE(await<chess::net::WelcomeMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::WelcomeMsg>(black).has_value());

    ASSERT_TRUE(playMove(white, "e4"));

    // Sync on the reconnect notice first: this proves the server armed the
    // grace window, so Black's drop below is guaranteed to be drained while
    // the window is still open (a >window stall between the two drops would
    // otherwise turn the scene into a legit grace expiry).
    white.disconnect();
    ASSERT_TRUE(await<chess::net::OpponentReconnectingMsg>(black).has_value());
    black.disconnect();

    // Nothing to observe on a dead socket; instead check side effects: no
    // rated update (re-login shows the factory rating) and no failure.
    auto checker = connectClient();
    loginAccount(checker, "Ada", "pw");
    sendMsg(checker, chess::net::ArchiveRequestMsg{10, false});
    auto list = await<chess::net::ArchiveListMsg>(checker);
    ASSERT_TRUE(list.has_value());
    // A one-move aborted game is still archived (10.3 keeps moves-aborts),
    // but the outcome is an abort nobody won.
    ASSERT_FALSE(list->games.empty());
    EXPECT_EQ(list->games.front().result,
              static_cast<int>(chess::net::GameResult::Abort));
}

// ── Wrong token refused ────────────────────────────────────────────────────

TEST_F(ReconnectE2eTest, WrongTokenRefused)
{
    auto stranger = connectClient();
    sendMsg(stranger, chess::net::ResumeMsg{std::string(48, 'a')});
    auto refused = await<chess::net::ErrorMsg>(stranger, 500ms);
    ASSERT_TRUE(refused.has_value());
}

// ── Late resume after the game already ended ───────────────────────────────

// Separate fixture: the retention window equals the grace (11.2), and this
// scenario waits past a game end before resuming, so the server is started
// with a wider window than the other tests.
class ResumeRetentionE2eTest : public ReconnectE2eTest {
protected:
    void configure(chess::server::ServerConfig& config) override
    {
        ReconnectE2eTest::configure(config);
        config.reconnectGraceMs = 2500;
    }
};

TEST_F(ResumeRetentionE2eTest, ResumeAfterGameOverReplaysOutcome)
{
    auto white = connectClient();
    auto black = connectClient();
    sendMsg(white, chess::net::JoinMsg{"White"});
    sendMsg(black, chess::net::JoinMsg{"Black"});
    auto wWelcome = await<chess::net::WelcomeMsg>(white);
    auto bWelcome = await<chess::net::WelcomeMsg>(black);
    ASSERT_TRUE(wWelcome.has_value() && bWelcome.has_value());

    // Fool's mate lands while White is still connected.
    ASSERT_TRUE(playMove(white, "f3"));
    ASSERT_TRUE(playMove(black, "e5"));
    ASSERT_TRUE(playMove(white, "g4"));
    sendMsg(black, chess::net::MoveMsg{"Qh4#"});

    // Now White leaves; the game is already over, so the disconnect does
    // not resurrect it — the seat is gone, the match is in retention.
    white.disconnect();
    ASSERT_TRUE(await<chess::net::GameOverMsg>(black).has_value());

    // The late token rejoin replays the final state + outcome.
    auto rejoin = connectClient();
    sendMsg(rejoin, chess::net::ResumeMsg{wWelcome->resumeToken});

    auto state = await<chess::net::ResumeStateMsg>(rejoin);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->color, chess::Color::White);
    ASSERT_EQ(state->sans.size(), 4u);
    EXPECT_EQ(state->sans.back(), "Qh4#");

    auto over = await<chess::net::GameOverMsg>(rejoin);
    ASSERT_TRUE(over.has_value());
    EXPECT_EQ(over->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Checkmate);
}
