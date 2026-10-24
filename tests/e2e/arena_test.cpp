// End-to-end tests for 12.3 Arena tournaments: two guests entering the
// same arena pair within a server pass into an unrated game whose result
// feeds the standings broadcast, rematches are refused on arena games,
// duplicate guest names and non-members get clean errors, a mid-game
// disconnect reaps the row and the survivor still scores, and the fixed
// quota seals the arena with a final phase-1 table.
#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <variant>

#include <SFML/Network/TcpSocket.hpp>

#include <chess/net/messages.h>
#include <chess/net/protocol.h>

#include "harness.h"
#include "server.h"

using namespace std::chrono_literals;

// Shared standings-drain helper base: fixtures differ only in quota.
class ArenaE2eBase : public chess::e2e::E2eHarness {
protected:
    // Joins both guests into the standard/unlimited arena; the second
    // join lands before the next pass, so the pairing pairs them on it.
    void joinArenaPair(sf::TcpSocket& white, sf::TcpSocket& black,
                       const std::string& wName = "Agata",
                       const std::string& bName = "Bruno")
    {
        sendMsg(white, chess::net::TournamentJoinMsg{wName, "", ""});
        sendMsg(black, chess::net::TournamentJoinMsg{bName, "", ""});
        auto w = await<chess::net::WelcomeMsg>(white);
        auto b = await<chess::net::WelcomeMsg>(black);
        ASSERT_TRUE(w.has_value()) << "white paired into the arena";
        ASSERT_TRUE(b.has_value()) << "black paired into the arena";
        EXPECT_TRUE(w->arena);
        EXPECT_TRUE(b->arena);
        // Arena games never rate.
        EXPECT_FALSE(w->rated);
        EXPECT_FALSE(b->rated);
        EXPECT_EQ(w->opponent, bName);
        EXPECT_EQ(b->opponent, wName);
    }

    // Polls until the socket forwards a TournamentStateMsg satisfying
    // `pred` (intermediate broadcasts are skipped, not failed on).
    std::optional<chess::net::TournamentStateMsg> awaitStandingsWhere(
        sf::TcpSocket& sock,
        const std::function<bool(const chess::net::TournamentStateMsg&)>& pred)
    {
        std::optional<chess::net::TournamentStateMsg> found;
        bool ok = pollUntil([&]() {
            if (auto msg = tryRecv(sock)) {
                if (auto* t = std::get_if<chess::net::TournamentStateMsg>(&*msg);
                    t && pred(*t)) {
                    found = *t;
                    return true;
                }
            }
            return false;
        });
        if (!ok) return std::nullopt;
        return found;
    }
};

class ArenaE2eTest : public ArenaE2eBase {
protected:
    void configure(chess::server::ServerConfig& config) override
    {
        // Two rounds per member: exactly enough to exercise a re-pairing
        // and the final-table flip within one test.
        config.arenaRounds = 2;
    }
};

// Quota 1: once the single game is over, nobody owes another round, so
// the arena pass cannot re-pair the seats out from under the test.
class ArenaLeaveE2eTest : public ArenaE2eBase {
protected:
    void configure(chess::server::ServerConfig& config) override
    {
        config.arenaRounds = 1;
    }
};

TEST_F(ArenaE2eTest, PairedIntoUnratedArenaGameAndScored)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinArenaPair(white, black);

    // The finished game feeds the standings: W=2 / L=0 with the winner's
    // own row marked self on their socket. White resigned → BLACK won, so
    // Bruno tops the table.
    sendMsg(white, chess::net::ResignMsg{});
    ASSERT_TRUE(await<chess::net::GameOverMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::GameOverMsg>(black).has_value());

    auto wTable = awaitStandingsWhere(white, [](const auto& t) {
        return t.rows.size() == 2;
    });
    auto bTable = awaitStandingsWhere(black, [](const auto& t) {
        return t.rows.size() == 2;
    });
    ASSERT_TRUE(wTable.has_value()) << "winner sees the fresh standings";
    ASSERT_TRUE(bTable.has_value()) << "loser sees the fresh standings";

    // Sorted points-DESC: Bruno (the winner) tops both views.
    for (const auto& table : {wTable, bTable}) {
        EXPECT_EQ(table->phase, 0) << "one round left of the quota";
        EXPECT_EQ(table->roundsTarget, 2);
        ASSERT_EQ(table->rows.size(), 2u);
        EXPECT_EQ(table->rows[0].name, "Bruno");
        EXPECT_EQ(table->rows[0].points, 2);
        EXPECT_EQ(table->rows[0].wins, 1);
        EXPECT_FALSE(table->rows[0].playing);
        EXPECT_EQ(table->rows[1].name, "Agata");
        EXPECT_EQ(table->rows[1].points, 0);
        EXPECT_EQ(table->rows[1].losses, 1);
    }
    EXPECT_FALSE(wTable->rows[0].self) << "Agata's socket marks only her row";
    EXPECT_TRUE(wTable->rows[1].self);
    EXPECT_TRUE(bTable->rows[0].self) << "Bruno's socket marks his row";
    EXPECT_FALSE(bTable->rows[1].self);
}

TEST_F(ArenaE2eTest, FullQuotaEndsArenaWithFinalTable)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinArenaPair(white, black);

    // Round 1: white resigns; both seats end on their game-over screens.
    sendMsg(white, chess::net::ResignMsg{});
    ASSERT_TRUE(await<chess::net::GameOverMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::GameOverMsg>(black).has_value());

    // Round 2 arrives by itself: the arena pass re-pairs the two game-over
    // seats (both owe one more game).
    auto w2 = await<chess::net::WelcomeMsg>(white);
    auto b2 = await<chess::net::WelcomeMsg>(black);
    ASSERT_TRUE(w2.has_value());
    ASSERT_TRUE(b2.has_value());
    EXPECT_TRUE(w2->arena);
    EXPECT_TRUE(b2->arena);

    sendMsg(white, chess::net::ResignMsg{});
    ASSERT_TRUE(await<chess::net::GameOverMsg>(white).has_value());

    auto finalTable = awaitStandingsWhere(white, [](const auto& t) {
        return t.phase == 1;
    });
    ASSERT_TRUE(finalTable.has_value());
    EXPECT_EQ(finalTable->rows[0].wins, 2);
    EXPECT_EQ(finalTable->rows[0].points, 4);
    EXPECT_EQ(finalTable->rows[1].losses, 2);

    // The nomination reaches the loser's socket too (phase 1).
    auto bFinal = awaitStandingsWhere(black, [](const auto& t) {
        return t.phase == 1;
    });
    ASSERT_TRUE(bFinal.has_value());
    EXPECT_EQ(bFinal->rows.size(), 2u);
}

TEST_F(ArenaE2eTest, RematchRefusedOnArenaGames)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinArenaPair(white, black);

    // Mid-game: the refusal beats the "Game not over yet" wording.
    sendMsg(white, chess::net::RematchRequestMsg{});
    auto midGame = await<chess::net::ErrorMsg>(white);
    ASSERT_TRUE(midGame.has_value());
    EXPECT_NE(midGame->message.find("rematch"), std::string::npos);

    // After the game, the rematch hooks are still locked shut: the lobby
    // pairing is the "again" channel.
    sendMsg(white, chess::net::ResignMsg{});
    ASSERT_TRUE(await<chess::net::GameOverMsg>(white).has_value());
    sendMsg(white, chess::net::RematchRequestMsg{});
    auto afterGame = await<chess::net::ErrorMsg>(white);
    ASSERT_TRUE(afterGame.has_value());
    EXPECT_NE(afterGame->message.find("rematch"), std::string::npos);
}

TEST_F(ArenaE2eTest, ArenaNameValidationAndNonMemberRequests)
{
    sf::TcpSocket solo = connectClient();

    // Empty guest name fails just like JoinMsg.
    sendMsg(solo, chess::net::TournamentJoinMsg{"", "", ""});
    auto empty = await<chess::net::ErrorMsg>(solo);
    ASSERT_TRUE(empty.has_value());
    EXPECT_EQ(empty->message, "Name required");

    // A proper solo join gets the initial standings (one row, self).
    sendMsg(solo, chess::net::TournamentJoinMsg{"Agata", "", ""});
    auto soloTable = awaitStandingsWhere(solo, [](const auto& t) {
        return t.rows.size() == 1;
    });
    ASSERT_TRUE(soloTable.has_value());
    EXPECT_TRUE(soloTable->rows[0].self);
    EXPECT_EQ(soloTable->rows[0].name, "Agata");

    // A second guest with the same name is refused (unique inside the
    // arena); a different name is fine.
    sf::TcpSocket impostor = connectClient();
    sf::TcpSocket friend_ = connectClient();
    sendMsg(impostor, chess::net::TournamentJoinMsg{"Agata", "", ""});
    auto dup = await<chess::net::ErrorMsg>(impostor);
    ASSERT_TRUE(dup.has_value());
    EXPECT_NE(dup->message.find("already"), std::string::npos);

    sendMsg(friend_, chess::net::TournamentJoinMsg{"Bruno", "", ""});
    ASSERT_TRUE(
        awaitStandingsWhere(friend_, [](const auto& t) {
            return t.rows.size() == 2;
        }).has_value());

    // A connection without arena membership gets an error, not silence.
    sf::TcpSocket outsider = connectClient();
    sendMsg(outsider, chess::net::TournamentStandingsRequestMsg{});
    auto outsiderErr = await<chess::net::ErrorMsg>(outsider);
    ASSERT_TRUE(outsiderErr.has_value());
    EXPECT_NE(outsiderErr->message.find("arena"), std::string::npos);
}

TEST_F(ArenaLeaveE2eTest, LeaveDropsRowFromNextStandings)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinArenaPair(white, black);

    // Leaving mid-game is refused — finish it first.
    sendMsg(black, chess::net::TournamentLeaveMsg{});
    auto refused = await<chess::net::ErrorMsg>(black);
    ASSERT_TRUE(refused.has_value());
    EXPECT_NE(refused->message.find("Finish"), std::string::npos);

    // After the one-round game, leaving drops the row and the survivor
    // keeps hers (the frozen final table shrinks).
    sendMsg(white, chess::net::ResignMsg{});
    ASSERT_TRUE(await<chess::net::GameOverMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::GameOverMsg>(black).has_value());

    sendMsg(black, chess::net::TournamentLeaveMsg{});
    auto after = awaitStandingsWhere(white, [](const auto& t) {
        return t.rows.size() == 1;
    });
    ASSERT_TRUE(after.has_value());
    // Agata resigned (Bruno won that game) so her remaining row shows a
    // loss, not a win — the dropped row just shrinks the table.
    EXPECT_EQ(after->rows[0].name, "Agata");
    EXPECT_EQ(after->rows[0].points, 0);
    EXPECT_EQ(after->rows[0].losses, 1);
    EXPECT_TRUE(after->rows[0].self);
}

TEST_F(ArenaE2eTest, DisconnectReapsRowAndSurvivorStillScores)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinArenaPair(white, black);

    // White drops mid-game: the row is reaped on the next pass; after the
    // grace window the match flags "opponent wins by disconnection" and
    // the surviving member scores that win even without the row present.
    white.disconnect();
    ASSERT_TRUE(await<chess::net::GameOverMsg>(black).has_value());

    auto survivorTable = awaitStandingsWhere(black, [](const auto& t) {
        for (const auto& row : t.rows)
            if (row.wins > 0) return true;
        return false;
    });
    ASSERT_TRUE(survivorTable.has_value());
    EXPECT_EQ(survivorTable->rows.size(), 1u);
    EXPECT_EQ(survivorTable->rows[0].name, "Bruno");
    EXPECT_EQ(survivorTable->rows[0].wins, 1);
    EXPECT_EQ(survivorTable->rows[0].points, 2);
    EXPECT_TRUE(survivorTable->rows[0].self);
}
