#include <chess/db/database.h>

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
    EXPECT_EQ(stmt.columnInt(0), 1);
}

} // namespace
} // namespace chess::db
