#include <chess/db/game_store.h>

#include <chess/db/player_store.h>
#include <chess/net/messages.h>

#include <filesystem>
#include <random>
#include <string>

#include <gtest/gtest.h>

namespace chess::db {
namespace {

std::filesystem::path tempPath(const std::string& tag)
{
    static std::mt19937 rng(4321);
    std::string name = "chess_game_store_test_" + tag + "_" + std::to_string(rng());
    return std::filesystem::temp_directory_path() / name;
}

Game makeGame(const std::string& whiteName, const std::string& blackName)
{
    Game game;
    game.whiteName = whiteName;
    game.blackName = blackName;
    game.result = int(chess::net::GameResult::BlackWins);
    game.reason = int(chess::net::GameOverReason::Checkmate);
    game.sanMoves = "e4 e5 Nf3 Nc6 Bb5";
    game.playedAt = nowSeconds();
    return game;
}

TEST(GameStore, GuestGameRoundTrip)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    GameStore games(db);

    Game in = makeGame("Ada", "Bob");
    const auto id = games.insert(in);
    ASSERT_TRUE(id.has_value());
    EXPECT_GT(*id, 0);

    auto stored = games.byId(*id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->whiteName, "Ada");
    EXPECT_EQ(stored->blackName, "Bob");
    EXPECT_FALSE(stored->whiteId.has_value());
    EXPECT_FALSE(stored->blackId.has_value());
    EXPECT_EQ(stored->result, int(chess::net::GameResult::BlackWins));
    EXPECT_EQ(stored->reason, int(chess::net::GameOverReason::Checkmate));
    EXPECT_EQ(stored->sanMoves, "e4 e5 Nf3 Nc6 Bb5");
    EXPECT_EQ(stored->timeControl, "");
    EXPECT_EQ(stored->startFen, "");
    EXPECT_EQ(stored->playedAt, in.playedAt);
}

TEST(GameStore, RegisteredPlayersRoundTrip)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);
    GameStore games(db);

    const auto white = players.registerPlayer("Alice", "pw");
    const auto black = players.registerPlayer("Bob", "pw");
    ASSERT_TRUE(white.has_value() && black.has_value());

    Game in = makeGame("Alice", "Bob");
    in.whiteId = white->id;
    in.blackId = black->id;
    in.timeControl = "900+30"; // placeholder value until 11.1 presets exist
    in.startFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    const auto id = games.insert(in);
    ASSERT_TRUE(id.has_value());
    auto stored = games.byId(*id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->whiteId, white->id);
    EXPECT_EQ(stored->blackId, black->id);
    EXPECT_EQ(stored->timeControl, "900+30");
    EXPECT_EQ(stored->startFen, in.startFen);
}

TEST(GameStore, UnknownPlayerIdViolatesForeignKey)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    GameStore games(db);

    Game in = makeGame("Ada", "Bob");
    in.whiteId = 998877; // no such account
    EXPECT_FALSE(games.insert(in).has_value());
}

TEST(GameStore, LatestOrdersNewestFirst)
{
    const auto path = tempPath("latest");
    {
        Database db;
        ASSERT_TRUE(db.open(path.string()));
        GameStore games(db);
        const auto id0 = games.insert(makeGame("One", "Two"));
        const auto id1 = games.insert(makeGame("Three", "Four"));
        const auto id2 = games.insert(makeGame("Five", "Six"));
        ASSERT_TRUE(id0 && id1 && id2);
    }
    Database db;
    ASSERT_TRUE(db.open(path.string()));
    GameStore games(db);

    auto recent = games.latest(2);
    ASSERT_EQ(recent.size(), 2u);
    EXPECT_EQ(recent[0].whiteName, "Five"); // newest first
    EXPECT_EQ(recent[1].whiteName, "Three");

    EXPECT_TRUE(games.latest(0).empty());
    std::filesystem::remove(path);
}

TEST(GameStore, MissingRowsReportedCleanly)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    GameStore games(db);
    EXPECT_FALSE(games.byId(424242).has_value());
}

} // namespace
} // namespace chess::db
