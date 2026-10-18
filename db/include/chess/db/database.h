#pragma once

#include <cstdint>
#include <string>

#include <sqlite3.h>

namespace chess::db {

// A managed SQL statement. Preparation and execution never throw; every
// failure leaves a human-readable message in errorMessage().
class Statement {
public:
    enum class Step { Ready, Done, Error };

    Statement() = default;
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    // Prepares the (single) statement in sql. db must outlive this statement.
    bool prepare(sqlite3* db, const char* sql);
    void finalize();

    // Resets execution and clears prior bindings.
    bool reset();

    // Advances the statement and returns its outcome.
    Step step();

    // bind() takes 1-based parameter positions.
    bool bind(int index, std::int64_t value);
    bool bind(int index, double value);
    bool bind(int index, const std::string& value);
    bool bindNull(int index);

    // column accessors are valid between a Ready step() and the next step().
    int columnInt(int index) const;
    std::int64_t columnInt64(int index) const;
    double columnDouble(int index) const;
    std::string columnText(int index) const;
    bool columnIsNull(int index) const;

    const std::string& errorMessage() const { return errorMessage_; }

private:
    bool requireStatement(const char* what);

    sqlite3* db_ = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
    std::string errorMessage_;
};

// Wall-clock seconds since the epoch, for timestamp columns.
std::int64_t nowSeconds();

// RAII SQLite connection. open() also creates the schema; it is safe to call
// open()/close() repeatedly. Non-copyable, non-movable — stores keep a plain
// reference to it.
class Database {
public:
    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Opens path (":memory:" for an in-memory database), applies pragmas and
    // creates the phase-10 schema when necessary. Idempotent for existing
    // files of the same schema version. Returns false on failure.
    bool open(const std::string& path);
    void close();

    bool isOpen() const { return handle_ != nullptr; }
    sqlite3* handle() const { return handle_; }

    // Executes zero or more semicolon-separated statements.
    bool exec(const std::string& sql);

    const std::string& errorMessage() const { return errorMessage_; }

private:
    int readUserVersion();
    bool writeUserVersion(int version);

    sqlite3* handle_ = nullptr;
    std::string errorMessage_;
};

// RAII transaction: BEGIN on construction, rollback on destruction unless
// commit() succeeded. Enforces commit only when every earlier statement ran
// inside the open transaction.
class Transaction {
public:
    explicit Transaction(Database& db) : db_(db) { open_ = db_.exec("BEGIN"); }
    ~Transaction();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    // Commits the transaction; returns false and rolls back on failure.
    bool commit();

    explicit operator bool() const { return open_; }

private:
    Database& db_;
    bool open_ = true;
};

} // namespace chess::db
