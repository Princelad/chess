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

} // namespace
} // namespace chess::db
