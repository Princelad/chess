// End-to-end tests for 11.7 Rematch: a post-game offer travels to the
// opponent of the finished match while both seats are still attached,
// acceptance rebuilds a fresh game with colors swapped (no rejoin), a
// decline (and the offer's self-cancel / expiry) notifies the requester,
// bots auto-accept, and an expired retention window answers from the
// lobby state with "session expired".
#include <gtest/gtest.h>

#include <variant>

#include <SFML/Network/TcpSocket.hpp>

#include <chess/net/messages.h>
#include <chess/net/protocol.h>

#include "harness.h"
#include "server.h"

using namespace std::chrono_literals;

class RematchE2eTest : public chess::e2e::E2eHarness {
protected:
    // The finished match lives exactly for the reconnect grace window
    // (retention = grace, 11.2): the rematch offer conversation must fit
    // inside it, so this fixture runs a longer window than the default
    // 400ms while keeping every other e2e semantic intact.
    void beforeStart() override { testGraceMs_ = 2500; }

    // Pairs two guests, plays a whole game (white resigns), and both
    // sockets consume their GameOverMsg. Returns the welcome pair so the
    // rematch test can compare the fresh credentials against them.
    void playAndFinish(sf::TcpSocket& white, sf::TcpSocket& black,
                       chess::net::WelcomeMsg& wWelcomeOut,
                       chess::net::WelcomeMsg& bWelcomeOut,
                       const std::string& wName = "Agata",
                       const std::string& bName = "Bruno")
    {
        sendMsg(white, chess::net::JoinMsg{wName});
        sendMsg(black, chess::net::JoinMsg{bName});
        auto first = await<chess::net::WelcomeMsg>(white);
        auto second = await<chess::net::WelcomeMsg>(black);
        ASSERT_TRUE(first.has_value());
        ASSERT_TRUE(second.has_value());
        wWelcomeOut = *first;
        bWelcomeOut = *second;

        sendMsg(white, chess::net::ResignMsg{});
        ASSERT_TRUE(await<chess::net::GameOverMsg>(white).has_value());
        ASSERT_TRUE(await<chess::net::GameOverMsg>(black).has_value());
    }
};

TEST_F(RematchE2eTest, AcceptStartsFreshGameWithSwappedColors)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    chess::net::WelcomeMsg wOld, bOld;
    playAndFinish(white, black, wOld, bOld);

    sendMsg(white, chess::net::RematchRequestMsg{});
    auto offer = await<chess::net::ServerRematchRequestMsg>(black);
    ASSERT_TRUE(offer.has_value()) << "black sees the forwarded offer";

    sendMsg(black, chess::net::RematchAcceptMsg{});
    auto wNew = await<chess::net::WelcomeMsg>(white);
    auto bNew = await<chess::net::WelcomeMsg>(black);
    ASSERT_TRUE(wNew.has_value()) << "white learns of the rematched game";
    ASSERT_TRUE(bNew.has_value()) << "black learns of the rematched game";

    // Colors swap for the rematch (the server's fresh match seats the
    // former black as White).
    EXPECT_EQ(wOld.color, chess::Color::White);
    EXPECT_EQ(wNew->color, chess::Color::Black);
    EXPECT_EQ(bNew->color, chess::Color::White);
    EXPECT_EQ(wNew->opponent, "Bruno");
    EXPECT_EQ(bNew->opponent, "Agata");
    EXPECT_EQ(wNew->timeControl, wOld.timeControl);
    EXPECT_EQ(wNew->rated, wOld.rated);
    EXPECT_FALSE(wNew->resumeToken.empty());
    EXPECT_NE(wNew->resumeToken, wOld.resumeToken) << "fresh seat token";
    EXPECT_NE(bNew->resumeToken, bOld.resumeToken) << "fresh seat token";

    // The rematched game is live: the new white (Bruno's socket) moves
    // first, and Agata's reply is accepted on her swapped black seat.
    ASSERT_TRUE(playMove(black, "e4"));
    ASSERT_TRUE(playMove(white, "e5"));
}

TEST_F(RematchE2eTest, DeclineNotifiesRequesterAndDissolvesOffer)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    chess::net::WelcomeMsg wOld, bOld;
    playAndFinish(white, black, wOld, bOld);

    sendMsg(white, chess::net::RematchRequestMsg{});
    ASSERT_TRUE(await<chess::net::ServerRematchRequestMsg>(black).has_value());

    sendMsg(black, chess::net::RematchDeclineMsg{});
    auto decline = await<chess::net::ServerRematchDeclineMsg>(white);
    ASSERT_TRUE(decline.has_value()) << "white sees the decline";
    EXPECT_FALSE(decline->withdrawn) << "a plain decline is not a withdraw";
    ASSERT_TRUE(await<chess::net::ServerRematchDeclineMsg>(black, 50ms)
                == std::nullopt)
        << "the decliner gets no echo";

    // The offer died with the decline: a fresh request is possible again.
    sendMsg(white, chess::net::RematchRequestMsg{});
    ASSERT_TRUE(await<chess::net::ServerRematchRequestMsg>(black).has_value())
        << "a second offer after a decline forwards again";
}

TEST_F(RematchE2eTest, RequesterSelfCancelReadsAsWithdrawn)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    chess::net::WelcomeMsg wOld, bOld;
    playAndFinish(white, black, wOld, bOld);

    sendMsg(white, chess::net::RematchRequestMsg{});
    ASSERT_TRUE(await<chess::net::ServerRematchRequestMsg>(black).has_value());
    sendMsg(white, chess::net::RematchDeclineMsg{});
    auto notice = await<chess::net::ServerRematchDeclineMsg>(white);
    ASSERT_TRUE(notice.has_value());
    EXPECT_TRUE(notice->withdrawn) << "the asker's own decline is a cancel";
}

TEST_F(RematchE2eTest, LiveGameRematchRequestFailsFast)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black, "Cyril", "Dalia");

    sendMsg(white, chess::net::RematchRequestMsg{});
    auto err = await<chess::net::ErrorMsg>(white);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->message, "Game not over yet");
}

TEST_F(RematchE2eTest, AwayOpponentFailsFast)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    chess::net::WelcomeMsg wOld, bOld;
    playAndFinish(white, black, wOld, bOld);

    // White drops after the game: the game-over seat is released without
    // notices (nothing pending). Black then cannot queue an offer.
    white.disconnect();
    sendMsg(black, chess::net::RematchRequestMsg{});
    auto err = await<chess::net::ErrorMsg>(black);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->message, "The opponent is away");
}

// Retention = the reconnect grace window (11.2); this fixture keeps it at
// 300ms so the expiry is observable within the test. The override must sit
// in beforeStart() — testGraceMs_ is already consumed by the time the test
// body runs.
class RematchWindowExpiredE2eTest : public RematchE2eTest {
protected:
    void beforeStart() override { testGraceMs_ = 300; }
};

TEST_F(RematchWindowExpiredE2eTest, RetentionLapseAnswersFromLobbyState)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    chess::net::WelcomeMsg wOld, bOld;
    playAndFinish(white, black, wOld, bOld);

    // Let the grace sweep erase the finished match and return both
    // game-over seats to the Connected lobby state.
    std::this_thread::sleep_for(600ms);

    sendMsg(white, chess::net::RematchRequestMsg{});
    auto err = await<chess::net::ErrorMsg>(white);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->message, "No game to rematch — session expired");

    // The queue still works from that state: both can rejoin normally.
    sendMsg(white, chess::net::JoinMsg{"Emil"});
    sendMsg(black, chess::net::JoinMsg{"Falk"});
    ASSERT_TRUE(await<chess::net::WelcomeMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::WelcomeMsg>(black).has_value());
}

class BotRematchE2eTest : public chess::e2e::E2eHarness {
protected:
    void beforeStart() override { testGraceMs_ = 2500; }
    void configure(chess::server::ServerConfig& config) override
    {
        config.botsEnabled = true;
        // Same procedural mock the bot match tests use (MOCK_ENGINE_PATH
        // is injected by the ctest registration).
        if (const char* mockPath = std::getenv("MOCK_ENGINE_PATH"))
            config.botEnginePath = mockPath;
    }
};

TEST_F(BotRematchE2eTest, BotAutoAcceptsRematchWithSwappedColors)
{
    sf::TcpSocket human = connectClient();
    sendMsg(human, chess::net::JoinMsg{"Solouser"});
    auto welcome = await<chess::net::WelcomeMsg>(human);
    ASSERT_TRUE(welcome.has_value()) << "server pairs the solo human with a bot";

    sendMsg(human, chess::net::ResignMsg{});
    ASSERT_TRUE(await<chess::net::GameOverMsg>(human).has_value());

    // A bot never needs the round trip: the request is accepted inside
    // the handler and the fresh match's welcome is the answer.
    sendMsg(human, chess::net::RematchRequestMsg{});
    auto rematch = await<chess::net::WelcomeMsg>(human);
    ASSERT_TRUE(rematch.has_value()) << "the bot accepted instantly";
    EXPECT_NE(rematch->color, welcome->color) << "colors swap on rematch";
    EXPECT_EQ(rematch->opponent, welcome->opponent) << "same bot opponent";
    EXPECT_EQ(rematch->timeControl, welcome->timeControl);
}
