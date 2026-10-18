#include <chess/db/player_store.h>

#include <filesystem>
#include <random>
#include <string>

#include <gtest/gtest.h>

namespace chess::db {
namespace {

std::filesystem::path tempPath(const std::string& tag)
{
    static std::mt19937 rng(1234);
    std::string name = "chess_player_store_test_" + tag + "_" + std::to_string(rng());
    return std::filesystem::temp_directory_path() / name;
}

TEST(PlayerStore, RegisterAndReadBack)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);

    auto alice = players.registerPlayer("Alice","s3cret");
    ASSERT_TRUE(alice.has_value());
    EXPECT_EQ(alice->name, "Alice");
    EXPECT_GT(alice->id, 0);
    // Glicko-2 defaults until 10.2 starts writing them:
    EXPECT_DOUBLE_EQ(alice->rating, 1500.0);
    EXPECT_DOUBLE_EQ(alice->ratingDeviation, 350.0);
    EXPECT_DOUBLE_EQ(alice->volatility, 0.06);
    EXPECT_EQ(alice->wins, 0);
    EXPECT_EQ(alice->losses, 0);
    EXPECT_EQ(alice->draws, 0);
    EXPECT_GT(alice->createdAt, 0);
    EXPECT_LE(alice->createdAt, alice->lastSeen);
}

TEST(PlayerStore, RegisterRejectsInvalidInput)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);

    EXPECT_FALSE(players.registerPlayer("", "pw").has_value());
    EXPECT_FALSE(players.registerPlayer("Alice", "").has_value());
    EXPECT_FALSE(players.registerPlayer(std::string(33, 'a'), "pw").has_value());
}

TEST(PlayerStore, NameTakenCaseInsensitive)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);

    ASSERT_TRUE(players.registerPlayer("Alice", "pw").has_value());
    EXPECT_FALSE(players.registerPlayer("ALICE", "other").has_value());

    // Yet lookups by either case reach the same account:
    auto found = players.byName("alice");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "Alice");
}

TEST(PlayerStore, ByLookupMisses)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);

    EXPECT_FALSE(players.byId(987654).has_value());
    EXPECT_FALSE(players.byName("nobody").has_value());
}

TEST(PlayerStore, AuthenticateRoundTrip)
{
    const auto path = tempPath("auth");
    {
        Database db;
        ASSERT_TRUE(db.open(path.string()));
        PlayerStore players(db);
        ASSERT_TRUE(players.registerPlayer("Alice", "s3cret").has_value());

        EXPECT_TRUE(players.authenticate("Alice", "s3cret"));
        EXPECT_TRUE(players.authenticate("aLiCe", "s3cret")); // case-insensitive name
        EXPECT_FALSE(players.authenticate("Alice", "wrong"));
        EXPECT_FALSE(players.authenticate("Nobody", "s3cret"));
        EXPECT_FALSE(players.authenticate("Alice", ""));
    }
    {
        // last_seen was refreshed and survives a reopen.
        Database db;
        ASSERT_TRUE(db.open(path.string()));
        PlayerStore players(db);
        auto alice = players.byName("Alice");
        ASSERT_TRUE(alice.has_value());
        EXPECT_GE(alice->lastSeen, alice->createdAt);
    }
    std::filesystem::remove(path);
}

TEST(PlayerStore, GuestPasswordNotRequiredForAnything)
{
    // Guests (unregistered names) simply have no row to authenticate;
    // the store only ever knows about accounts.
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);
    EXPECT_FALSE(players.authenticate("Guest", ""));
}

TEST(PlayerStore, RatedWinMovesCountersAndRatings)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);
    const auto white = players.registerPlayer("Alice", "pw");
    const auto black = players.registerPlayer("Bob", "pw");
    ASSERT_TRUE(white && black);

    const auto update = players.applyRatedGame(
        white->id, black->id, chess::net::GameResult::WhiteWins);
    ASSERT_TRUE(update.has_value());

    EXPECT_EQ(update->white.wins, 1);
    EXPECT_EQ(update->white.losses, 0);
    EXPECT_GT(update->white.rating, 1500.0);   // rating rose
    EXPECT_LT(update->white.ratingDeviation, 350.0); // gained confidence
    EXPECT_LT(update->black.rating, 1500.0);
    EXPECT_EQ(update->black.losses, 1);

    // Persisted: read back both rows.
    const auto whiteAfter = players.byName("alice");
    const auto blackAfter = players.byName("BOB");
    ASSERT_TRUE(whiteAfter && blackAfter);
    EXPECT_DOUBLE_EQ(whiteAfter->rating, update->white.rating);
    EXPECT_DOUBLE_EQ(blackAfter->rating, update->black.rating);
    EXPECT_EQ(whiteAfter->wins, 1);
}

TEST(PlayerStore, RatedDrawCountsBoth)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);
    const auto white = players.registerPlayer("Alice", "pw");
    const auto black = players.registerPlayer("Bob", "pw");
    ASSERT_TRUE(white && black);

    const auto update = players.applyRatedGame(
        white->id, black->id, chess::net::GameResult::Draw);
    ASSERT_TRUE(update.has_value());
    EXPECT_EQ(update->white.draws, 1);
    EXPECT_EQ(update->black.draws, 1);
    // Equals with equal deviations keep 1500.
    EXPECT_DOUBLE_EQ(update->white.rating, 1500.0);
    EXPECT_DOUBLE_EQ(update->black.rating, 1500.0);
}

TEST(PlayerStore, AbortedResultIsNeverRated)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);
    const auto white = players.registerPlayer("Alice", "pw");
    const auto black = players.registerPlayer("Bob", "pw");
    ASSERT_TRUE(white && black);

    EXPECT_FALSE(players.applyRatedGame(
        white->id, black->id, chess::net::GameResult::Abort).has_value());
    EXPECT_FALSE(players.applyRatedGame(42, black->id,
        chess::net::GameResult::WhiteWins).has_value());
    EXPECT_FALSE(players.applyRatedGame(white->id, 42,
        chess::net::GameResult::WhiteWins).has_value());

    // Untouched by the failed attempts:
    EXPECT_DOUBLE_EQ(players.byName("Alice")->rating, 1500.0);
    EXPECT_DOUBLE_EQ(players.byName("Bob")->rating, 1500.0);
}

} // namespace
} // namespace chess::db
