#include <chess/db/database.h>
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
    std::string name = "chess_db_test_" + tag + "_" + std::to_string(rng());
    return std::filesystem::temp_directory_path() / name;
}

TEST(Database, OpenMemoryAndClose)
{
    Database db;
    EXPECT_TRUE(db.open(":memory:"));
    EXPECT_TRUE(db.isOpen());
    EXPECT_TRUE(db.errorMessage().empty());
    db.close();
    EXPECT_FALSE(db.isOpen());
}

TEST(Database, ReopenExistingFileKeepsData)
{
    const auto path = tempPath("reopen");
    {
        Database db;
        ASSERT_TRUE(db.open(path.string()));
        ASSERT_TRUE(db.exec("INSERT INTO games "
                            "(white_name, black_name, result, reason, played_at) "
                            "VALUES ('Ada', 'Bob', 0, 0, 1)"));
    }
    {
        Database db;
        ASSERT_TRUE(db.open(path.string()));
        Statement stmt;
        ASSERT_TRUE(stmt.prepare(db.handle(), "SELECT COUNT(*) FROM games"));
        ASSERT_EQ(stmt.step(), Statement::Step::Ready);
        EXPECT_EQ(stmt.columnInt(0), 1);
    }
    std::filesystem::remove(path);
}

TEST(Database, SchemaUsesDefaults)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.exec("INSERT INTO games "
                        "(white_name, black_name, result, reason, played_at) "
                        "VALUES ('Ada', 'Bob', 0, 0, 1)"));
    Statement stmt;
    ASSERT_TRUE(stmt.prepare(
        db.handle(), "SELECT time_control, start_fen, white_id FROM games"));
    ASSERT_EQ(stmt.step(), Statement::Step::Ready);
    EXPECT_EQ(stmt.columnText(0), "");
    EXPECT_EQ(stmt.columnText(1), "");
    EXPECT_TRUE(stmt.columnIsNull(2));
}

TEST(Database, ExecReportsBadSql)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    EXPECT_FALSE(db.exec("CREATE IT ALL"));
    EXPECT_FALSE(db.errorMessage().empty());
}

TEST(Database, TransactionRollsBack)
{
    const auto path = tempPath("rollback");
    {
        Database db;
        ASSERT_TRUE(db.open(path.string()));
        // Early destruction without commit() rolls back the INSERT.
        {
            Transaction tx(db);
            ASSERT_TRUE(db.exec("INSERT INTO games "
                                "(white_name, black_name, result, reason, played_at) "
                                "VALUES ('Ada', 'Bob', 0, 0, 1)"));
        }
    }
    Database db;
    ASSERT_TRUE(db.open(path.string()));
    Statement stmt;
    ASSERT_TRUE(stmt.prepare(db.handle(), "SELECT COUNT(*) FROM games"));
    ASSERT_EQ(stmt.step(), Statement::Step::Ready);
    EXPECT_EQ(stmt.columnInt(0), 0);
    std::filesystem::remove(path);
}

TEST(Database, TransactionCommits)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    {
        Transaction tx(db);
        ASSERT_TRUE(db.exec("INSERT INTO games "
                            "(white_name, black_name, result, reason, played_at) "
                            "VALUES ('Ada', 'Bob', 0, 0, 1)"));
        EXPECT_TRUE(tx.commit());
    }
    Statement stmt;
    ASSERT_TRUE(stmt.prepare(db.handle(), "SELECT COUNT(*) FROM games"));
    ASSERT_EQ(stmt.step(), Statement::Step::Ready);
    EXPECT_EQ(stmt.columnInt(0), 1);
}

TEST(Database, SchemaVersionStamp)
{
    Database db;
    ASSERT_TRUE(db.open(":memory:"));
    Statement stmt;
    ASSERT_TRUE(stmt.prepare(db.handle(), "PRAGMA user_version"));
    ASSERT_EQ(stmt.step(), Statement::Step::Ready);
    EXPECT_EQ(stmt.columnInt(0), 3);
}

TEST(Database, MigrationAddsLastRatedColumn)
{
    // Simulate a revision-1 database (pre-10.2 layout), then reopen: the
    // migration must add the column without touching existing rows.
    const auto path = tempPath("migrate");
    std::filesystem::remove(path); // a prior aborted run leaves rev-1 debris
    {
        Database db;
        if (!db.open(path.string()))
            GTEST_FAIL() << "first open failed: " << db.errorMessage();
        PlayerStore players(db);
        ASSERT_TRUE(players.registerPlayer("Ada", "pw").has_value());
    }
    // Revision-1 shape: rebuild the players table without last_rated_at and
    // downgrade the stamp so the next open() re-runs the migration. The
    // rebuild keeps the original constraints (CREATE TABLE AS SELECT would
    // strip the primary key, which the revision-3 seed's FK parser rejects
    // — real revision-1 files kept theirs, so the sim does too).
    {
        Database db;
        ASSERT_TRUE(db.open(path.string()));
        ASSERT_TRUE(db.exec("CREATE TABLE players_old (\n"
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
            "    name TEXT NOT NULL UNIQUE COLLATE NOCASE,\n"
            "    password_hash TEXT NOT NULL,\n"
            "    rating REAL NOT NULL DEFAULT 1500.0,\n"
            "    rating_deviation REAL NOT NULL DEFAULT 350.0,\n"
            "    volatility REAL NOT NULL DEFAULT 0.06,\n"
            "    wins INTEGER NOT NULL DEFAULT 0,\n"
            "    losses INTEGER NOT NULL DEFAULT 0,\n"
            "    draws INTEGER NOT NULL DEFAULT 0,\n"
            "    created_at INTEGER NOT NULL,\n"
            "    last_seen INTEGER NOT NULL\n"
            ")"));
        ASSERT_TRUE(db.exec("INSERT INTO players_old SELECT id, name, "
            "password_hash, rating, rating_deviation, volatility, wins, "
            "losses, draws, created_at, last_seen FROM players"));
        ASSERT_TRUE(db.exec("DROP TABLE players"));
        ASSERT_TRUE(db.exec("ALTER TABLE players_old RENAME TO players"));
        ASSERT_TRUE(db.exec("PRAGMA user_version = 1"));
    }
    {
        Database db;
        ASSERT_TRUE(db.open(path.string()));
        Statement stmt;
        ASSERT_TRUE(stmt.prepare(db.handle(), "PRAGMA user_version"));
        ASSERT_EQ(stmt.step(), Statement::Step::Ready);
        EXPECT_EQ(stmt.columnInt(0), 3);

        PlayerStore players(db);
        const auto ada = players.byName("Ada");
        ASSERT_TRUE(ada.has_value());
        EXPECT_EQ(ada->lastRatedAt, 0); // revision-1 rows start un-rated
        // The revision-3 seed materialized the account's unlimited class
        // row from the flat columns (factory state for a fresh account).
        const auto row = players.classRatingFor(ada->id, "");
        ASSERT_TRUE(row.has_value());
        EXPECT_EQ(row->ratingClass, "unlimited");
        EXPECT_DOUBLE_EQ(row->rating, 1500.0);
        EXPECT_EQ(row->wins, 0);
    }
    std::filesystem::remove(path);
}

} // namespace
} // namespace chess::db
