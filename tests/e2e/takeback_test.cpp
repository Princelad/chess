// End-to-end tests for 11.3 Takeback: a request travels to the opponent,
// the accept rolls the shared SAN list back two plies, a decline leaves
// the game untouched, clocks keep flowing after a rollback, and a takeback
// followed by a mid-game drop replays the trimmed history on a token resume.
#include <gtest/gtest.h>

#include <variant>

#include <SFML/Network/TcpSocket.hpp>

#include <chess/net/messages.h>
#include <chess/net/protocol.h>

#include "harness.h"
#include "server.h"

using namespace std::chrono_literals;

class TakebackE2eTest : public chess::e2e::E2eHarness {
protected:
    // Guest join pairing with both rejoin credentials captured (11.2
    // patterns apply unchanged to a takeback's aftermath).
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
        wTokenOut = wWelcome->resumeToken;
        bTokenOut = bWelcome->resumeToken;
    }
};

TEST_F(TakebackE2eTest, AgreedTakebackRollsTheGameBack)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black, "Agata", "Bruno");

    // Three full moves on the table.
    ASSERT_TRUE(playMove(white, "e4"));
    ASSERT_TRUE(playMove(black, "e5"));
    ASSERT_TRUE(playMove(white, "Nf3"));
    ASSERT_TRUE(playMove(black, "Nc6"));
    ASSERT_TRUE(playMove(white, "Bb5"));
    ASSERT_TRUE(playMove(black, "a6"));

    sendMsg(white, chess::net::TakebackRequestMsg{});
    auto ask = await<chess::net::ServerTakebackRequestMsg>(black);
    ASSERT_TRUE(ask.has_value()) << "black sees the forwarded request";

    // Black agrees; both seats learn the trimmed SAN list (six plies minus
    // the last two).
    sendMsg(black, chess::net::TakebackAcceptMsg{});
    auto wAccept = await<chess::net::TakebackAcceptedMsg>(white);
    auto bAccept = await<chess::net::TakebackAcceptedMsg>(black);
    ASSERT_TRUE(wAccept.has_value()) << "white learns of the accepted takeback";
    ASSERT_TRUE(bAccept.has_value()) << "black learns of the accepted takeback";
    ASSERT_EQ(wAccept->sans.size(), 4u);
    EXPECT_EQ(wAccept->sans[3], "Nc6");
    EXPECT_EQ(wAccept->sans, bAccept->sans);

    // Play continues from the rolled-back position (white on the move):
    // the echoes prove the server's board accepted fresh moves.
    ASSERT_TRUE(playMove(white, "Bb5"));
    ASSERT_TRUE(playMove(black, "a6"));
    ASSERT_TRUE(playMove(white, "Bxc6"));
    ASSERT_TRUE(playMove(black, "dxc6"));
    ASSERT_TRUE(await<chess::net::ErrorMsg>(white, 1ms) == std::nullopt)
        << "no server errors after the rollback";
}

TEST_F(TakebackE2eTest, DeclinedTakebackLeavesGameUntouched)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black, "Cyril", "Dalia");

    ASSERT_TRUE(playMove(white, "e4"));
    ASSERT_TRUE(playMove(black, "e5"));
    ASSERT_TRUE(playMove(white, "Nf3"));
    ASSERT_TRUE(playMove(black, "Nc6"));

    sendMsg(white, chess::net::TakebackRequestMsg{});
    auto ask = await<chess::net::ServerTakebackRequestMsg>(black);
    ASSERT_TRUE(ask.has_value());

    sendMsg(black, chess::net::TakebackDeclineMsg{});
    auto decline = await<chess::net::ServerTakebackDeclineMsg>(white);
    ASSERT_TRUE(decline.has_value()) << "white sees the decline";

    // No acceptance ever reaches either seat, and all four plies stay the
    // accepted truth (black's a6 replies to Bb5, i.e. the history stands).
    ASSERT_TRUE(await<chess::net::TakebackAcceptedMsg>(white, 50ms)
                == std::nullopt)
        << "no acceptance after a decline";
    ASSERT_TRUE(await<chess::net::TakebackAcceptedMsg>(black, 50ms)
                == std::nullopt)
        << "no acceptance after a decline";
    ASSERT_TRUE(playMove(white, "Bb5"));
    ASSERT_TRUE(playMove(black, "a6"));
    ASSERT_TRUE(await<chess::net::ErrorMsg>(white, 1ms) == std::nullopt)
        << "play continues without server errors";
}

TEST_F(TakebackE2eTest, ResumeAfterTakebackReplaysTrimmedHistory)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    std::string wToken, bToken;
    joinCapture(white, "Ed", black, "Frida", wToken, bToken);

    ASSERT_TRUE(playMove(white, "e4"));
    ASSERT_TRUE(playMove(black, "e5"));
    ASSERT_TRUE(playMove(white, "Nf3"));
    ASSERT_TRUE(playMove(black, "Nc6"));

    sendMsg(white, chess::net::TakebackRequestMsg{});
    ASSERT_TRUE(await<chess::net::ServerTakebackRequestMsg>(black).has_value());
    sendMsg(black, chess::net::TakebackAcceptMsg{});
    auto accept = await<chess::net::TakebackAcceptedMsg>(white);
    ASSERT_TRUE(accept.has_value());
    ASSERT_EQ(accept->sans.size(), 2u);

    // White drops after the takeback and rejoins by token.
    white.disconnect();
    ASSERT_TRUE(await<chess::net::OpponentReconnectingMsg>(black).has_value())
        << "black sees the reconnecting notice";

    sf::TcpSocket rejoin = connectClient();
    sendMsg(rejoin, chess::net::ResumeMsg{wToken});

    auto state = await<chess::net::ResumeStateMsg>(rejoin);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->color, chess::Color::White);
    EXPECT_EQ(state->opponent, "Frida");
    // The takeback is part of history: the resync replays only what remains.
    EXPECT_EQ(state->sans, std::vector<std::string>({"e4", "e5"}));

    ASSERT_TRUE(await<chess::net::OpponentResumedMsg>(black).has_value());
    // Play continues: white is on the move in the rolled-back position.
    ASSERT_TRUE(playMove(rejoin, "Nf3"));
    ASSERT_TRUE(playMove(black, "Nc6"));
}

TEST_F(TakebackE2eTest, ClockGameTakebackKeepsClocksFlowing)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    sendMsg(white, chess::net::JoinMsg{"Gert", "5+0"});
    sendMsg(black, chess::net::JoinMsg{"Heike", "5+0"});
    bool wOk = false, bOk = false;
    ASSERT_TRUE(pollUntil([&]() {
        if (!wOk) {
            auto m = tryRecv(white);
            wOk = m && std::holds_alternative<chess::net::WelcomeMsg>(*m);
        }
        if (!bOk) {
            auto m = tryRecv(black);
            bOk = m && std::holds_alternative<chess::net::WelcomeMsg>(*m);
        }
        return wOk && bOk;
    }));

    ASSERT_TRUE(playMove(white, "e4"));
    ASSERT_TRUE(playMove(black, "e5"));

    // First takeback: both just-played plies disappear (the SAN list ends
    // empty), clocks are not refunded, but the game stays clocked — every
    // later broadcast still carries remaining time, and the loop keeps its
    // 100ms pace (no timeout faux-flag fires over the rollback).
    sendMsg(white, chess::net::TakebackRequestMsg{});
    ASSERT_TRUE(await<chess::net::ServerTakebackRequestMsg>(black).has_value());
    sendMsg(black, chess::net::TakebackAcceptMsg{});
    auto accept = await<chess::net::TakebackAcceptedMsg>(black);
    ASSERT_TRUE(accept.has_value());
    EXPECT_TRUE(accept->sans.empty());

    // White's queue still holds the stale pre-takeback echo (the retracted
    // e5): history now, drained before the post-rollback broadcast check.
    auto stale = await<chess::net::ServerMoveMsg>(white);
    ASSERT_TRUE(stale.has_value());
    EXPECT_EQ(stale->san, "e5");

    // Fresh broadcast after the rollback: white's echoed e4 still carries
    // the 11.1 remaining-time figure, proving the clock survived intact.
    sendMsg(white, chess::net::MoveMsg{"e4"});
    auto echo = await<chess::net::ServerMoveMsg>(white);
    ASSERT_TRUE(echo.has_value());
    EXPECT_EQ(echo->san, "e4");
    EXPECT_GT(echo->remainingMs, 0);
    ASSERT_TRUE(playMove(black, "e5"));

    // A second takeback in the same clocked game, now from the on-move
    // frame again (white after two plies).
    sendMsg(white, chess::net::TakebackRequestMsg{});
    ASSERT_TRUE(await<chess::net::ServerTakebackRequestMsg>(black).has_value());
    sendMsg(black, chess::net::TakebackAcceptMsg{});
    ASSERT_TRUE(await<chess::net::TakebackAcceptedMsg>(black).has_value());

    // And once more the broadcast pattern holds after the second rollback:
    // stale echo out, fresh Nf3 in with live time.
    auto stale2 = await<chess::net::ServerMoveMsg>(white);
    ASSERT_TRUE(stale2.has_value());
    EXPECT_EQ(stale2->san, "e5");
    sendMsg(white, chess::net::MoveMsg{"Nf3"});
    auto echo2 = await<chess::net::ServerMoveMsg>(white);
    ASSERT_TRUE(echo2.has_value());
    EXPECT_EQ(echo2->san, "Nf3");
    EXPECT_GT(echo2->remainingMs, 0);
}

