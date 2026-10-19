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

TEST(PlayerStore, RatedGameStampsLastRatedAt)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);
    const auto white = players.registerPlayer("Ada", "pw");
    const auto black = players.registerPlayer("Bob", "pw");
    ASSERT_TRUE(white && black);

    EXPECT_EQ(white->lastRatedAt, 0); // idle growth no-ops for fresh accounts
    const auto update = players.applyRatedGame(
        white->id, black->id, chess::net::GameResult::WhiteWins);
    ASSERT_TRUE(update.has_value());
    const auto now = nowSeconds();
    EXPECT_LE(update->white.lastRatedAt, now);
    EXPECT_GE(update->white.lastRatedAt, now - 5);
    EXPECT_EQ(players.byId(white->id)->lastRatedAt, update->white.lastRatedAt);
}

TEST(PlayerStore, IdleGrowthGrowsDeviationOnly)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);
    const auto white = players.registerPlayer("Ada", "pw");
    const auto black = players.registerPlayer("Bob", "pw");
    ASSERT_TRUE(white && black);

    const auto update = players.applyRatedGame(
        white->id, black->id, chess::net::GameResult::WhiteWins);
    ASSERT_TRUE(update.has_value());
    const double ratedRd = update->white.ratingDeviation;
    ASSERT_LT(ratedRd, 350.0); // one rated period moved RD off the cap

    // Wind the clock back 30 days and grow again on "arrival".
    const int64_t goneBack = update->white.lastRatedAt - 30 * 86400;
    EXPECT_TRUE(db.exec("UPDATE players SET last_rated_at = "
        + std::to_string(goneBack) + " WHERE id = "
        + std::to_string(white->id)));
    const auto now = static_cast<std::int64_t>(update->white.lastRatedAt) + 300;

    const auto fresh = players.applyIdleGrowth(white->id, now);
    ASSERT_TRUE(fresh.has_value());
    EXPECT_GT(fresh->ratingDeviation, ratedRd);   // uncertainty grew
    EXPECT_EQ(fresh->rating, update->white.rating); // rating untouched
    EXPECT_EQ(fresh->volatility, update->white.volatility);
    EXPECT_LT(fresh->ratingDeviation, 350.0);     // 30 steps from ratedRd

    // Persistence: the store itself now serves the grown row.
    EXPECT_DOUBLE_EQ(players.byId(white->id)->ratingDeviation,
                     fresh->ratingDeviation);
}

TEST(PlayerStore, IdleGrowthCapsAtThreeFiftyAndHandlesEdges)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);
    const auto white = players.registerPlayer("Ada", "pw");
    const auto black = players.registerPlayer("Bob", "pw");
    ASSERT_TRUE(white && black);

    // Never rated: growth is a no-op regardless of elapsed time.
    const auto now = nowSeconds();
    auto fresh = players.applyIdleGrowth(white->id, now + 1000 * 86400);
    ASSERT_TRUE(fresh.has_value());
    EXPECT_EQ(fresh->lastRatedAt, 0);
    EXPECT_DOUBLE_EQ(fresh->ratingDeviation, 350.0);

    // Rated once, long idle: RD caps at 350 (never overshoots).
    const auto rated = players.applyRatedGame(
        white->id, black->id, chess::net::GameResult::WhiteWins);
    ASSERT_TRUE(rated.has_value());
    const auto ancient = rated->white.lastRatedAt - 5000 * 86400;
    EXPECT_TRUE(db.exec("UPDATE players SET last_rated_at = "
        + std::to_string(ancient) + " WHERE id = " + std::to_string(white->id)));
    fresh = players.applyIdleGrowth(white->id, now);
    ASSERT_TRUE(fresh.has_value());
    EXPECT_DOUBLE_EQ(fresh->ratingDeviation, 350.0);

    // Sub-period elapsed (less than a full day): untouched.
    EXPECT_TRUE(players.applyIdleGrowth(
        rated->black.id, rated->black.lastRatedAt + 3600).has_value());
    EXPECT_DOUBLE_EQ(players.byId(rated->black.id)->ratingDeviation,
                     rated->black.ratingDeviation);

    // Unknown id reports failure.
    EXPECT_FALSE(players.applyIdleGrowth(424242, now).has_value());
    EXPECT_EQ(players.errorMessage(), "player not found");
}

TEST(PlayerStore, TopPlayersOrdersByRatingThenName)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);
    const auto alice = players.registerPlayer("Alice", "pw");
    const auto bob = players.registerPlayer("Bob", "pw");
    const auto carol = players.registerPlayer("Carol", "pw");
    const auto dave = players.registerPlayer("Dave", "pw");
    ASSERT_TRUE(alice && bob && carol && dave);

    // Symmetric setups (both wins are 1500/350 vs 1500/350) leave Alice and
    // Dave with identical ratings above 1500, Bob and Carol identical below —
    // so the name tie-break alone decides the row order.
    ASSERT_TRUE(players.applyRatedGame(
        alice->id, bob->id, chess::net::GameResult::WhiteWins).has_value());
    ASSERT_TRUE(players.applyRatedGame(
        dave->id, carol->id, chess::net::GameResult::WhiteWins).has_value());

    const auto top = players.topPlayers(10);
    ASSERT_EQ(top.size(), 4u);
    // Winners: shared rating, alphabetical order.
    EXPECT_EQ(top[0].name, "Alice");
    EXPECT_EQ(top[1].name, "Dave");
    // Losers: shared rating, alphabetical order.
    EXPECT_EQ(top[2].name, "Bob");
    EXPECT_EQ(top[3].name, "Carol");
    EXPECT_DOUBLE_EQ(top[0].rating, players.byName("Dave")->rating); // tied
    EXPECT_GT(top[0].rating, top[2].rating);
    EXPECT_EQ(top[0].wins, 1);
    EXPECT_EQ(top[2].losses, 1);
}

TEST(PlayerStore, TopPlayersSkipsFreshAccountsButZeroMinGamesShowsAll)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);
    const auto alice = players.registerPlayer("Unplayed", "pw");
    ASSERT_TRUE(alice.has_value());

    // Never played a rated game: excluded from the default leaderboard.
    EXPECT_TRUE(players.topPlayers(10).empty());

    // minGames = 0 (or lower) lists every registered account.
    const auto all = players.topPlayers(10, 0);
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].name, "Unplayed");

    // minGames = 2 also excludes accounts with exactly one rated game.
    const auto bob = players.registerPlayer("Bob", "pw");
    ASSERT_TRUE(bob.has_value());
    ASSERT_TRUE(players.applyRatedGame(
        alice->id, bob->id, chess::net::GameResult::WhiteWins).has_value());
    EXPECT_TRUE(players.topPlayers(10, 2).empty());
    EXPECT_EQ(players.topPlayers(10, 1).size(), 2u);
}

TEST(PlayerStore, TopPlayersLimitClampsAndEmptyStore)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    PlayerStore players(db);

    // Empty store: no error, empty list.
    EXPECT_TRUE(players.topPlayers(50).empty());
    EXPECT_TRUE(players.errorMessage().empty());

    const auto alice = players.registerPlayer("Alice", "pw");
    const auto bob = players.registerPlayer("Bob", "pw");
    const auto carol = players.registerPlayer("Carol", "pw");
    ASSERT_TRUE(alice && bob && carol);
    ASSERT_TRUE(players.applyRatedGame(
        alice->id, bob->id, chess::net::GameResult::WhiteWins).has_value());
    ASSERT_TRUE(players.applyRatedGame(
        carol->id, bob->id, chess::net::GameResult::WhiteWins).has_value());

    // Alice beats Bob and Dave beats Carol: symmetric games -> Alice/Dave tie
    // above 1500 (name ASC keeps Alice first), Bob/Carol tie below.
    EXPECT_EQ(players.topPlayers(2).size(), 2u);
    const auto single = players.topPlayers(1);
    ASSERT_EQ(single.size(), 1u);
    EXPECT_EQ(single[0].name, "Alice");
    EXPECT_TRUE(players.topPlayers(0).empty());
    EXPECT_TRUE(players.topPlayers(-3).empty());
}

} // namespace
} // namespace chess::db
