#include <chess/db/database.h>

#include <chrono>

namespace chess::db {
namespace {

// Schema revision 1 (phase 10.1): accounts and completed games. The rating
// columns carry the Glicko-2 defaults from the start; phase 10.2 will write
// them, and time_control/start_fen stay empty until 11.1 clocks / 12.1
// variants exist. Bump SchemaVersion and migrate when the layout changes.
constexpr int SchemaVersion = 1;

const char* SchemaSql =
    "CREATE TABLE IF NOT EXISTS players (\n"
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
    ");\n"
    "CREATE TABLE IF NOT EXISTS games (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    white_id INTEGER REFERENCES players(id),\n"
    "    black_id INTEGER REFERENCES players(id),\n"
    "    white_name TEXT NOT NULL,\n"
    "    black_name TEXT NOT NULL,\n"
    "    result INTEGER NOT NULL,\n"
    "    reason INTEGER NOT NULL,\n"
    "    time_control TEXT NOT NULL DEFAULT '',\n"
    "    start_fen TEXT NOT NULL DEFAULT '',\n"
    "    san_moves TEXT NOT NULL DEFAULT '',\n"
    "    played_at INTEGER NOT NULL\n"
    ");\n"
    "CREATE INDEX IF NOT EXISTS idx_games_played_at ON games (played_at);\n"
    "CREATE INDEX IF NOT EXISTS idx_players_rating ON players (rating);\n";

} // namespace

// ── Statement ───────────────────────────────────────────────────────────────

Statement::~Statement() { finalize(); }

bool Statement::requireStatement(const char* what)
{
    if (stmt_ != nullptr)
        return true;
    errorMessage_ = std::string("can't ") + what + " — statement is not prepared";
    return false;
}

bool Statement::prepare(sqlite3* db, const char* sql)
{
    finalize();
    db_ = db;
    if (db_ == nullptr) {
        errorMessage_ = "can't prepare — no database handle";
        return false;
    }
    if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
        errorMessage_ = sqlite3_errmsg(db);
        return false;
    }
    return true;
}

void Statement::finalize()
{
    if (stmt_ != nullptr) {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
    }
    db_ = nullptr;
}

bool Statement::reset()
{
    if (!requireStatement("reset"))
        return false;
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
    errorMessage_.clear();
    return true;
}

Statement::Step Statement::step()
{
    if (!requireStatement("step"))
        return Step::Error;
    int code = sqlite3_step(stmt_);
    if (code == SQLITE_ROW)
        return Step::Ready;
    if (code == SQLITE_DONE)
        return Step::Done;
    errorMessage_ = sqlite3_errmsg(db_);
    return Step::Error;
}

bool Statement::bind(int index, std::int64_t value)
{
    if (!requireStatement("bind"))
        return false;
    if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) {
        errorMessage_ = sqlite3_errmsg(db_);
        return false;
    }
    return true;
}

bool Statement::bind(int index, double value)
{
    if (!requireStatement("bind"))
        return false;
    if (sqlite3_bind_double(stmt_, index, value) != SQLITE_OK) {
        errorMessage_ = sqlite3_errmsg(db_);
        return false;
    }
    return true;
}

bool Statement::bind(int index, const std::string& value)
{
    if (!requireStatement("bind"))
        return false;
    // SQLITE_TRANSIENT makes SQLite copy the buffer, which the caller may
    // freely reuse or destroy right after the call.
    if (sqlite3_bind_text(stmt_, index, value.data(),
                          static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
        errorMessage_ = sqlite3_errmsg(db_);
        return false;
    }
    return true;
}

bool Statement::bindNull(int index)
{
    if (!requireStatement("bind"))
        return false;
    if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) {
        errorMessage_ = sqlite3_errmsg(db_);
        return false;
    }
    return true;
}

int Statement::columnInt(int index) const { return sqlite3_column_int(stmt_, index); }

std::int64_t Statement::columnInt64(int index) const
{
    return sqlite3_column_int64(stmt_, index);
}

double Statement::columnDouble(int index) const
{
    return sqlite3_column_double(stmt_, index);
}

std::string Statement::columnText(int index) const
{
    const auto* text = sqlite3_column_text(stmt_, index);
    return text != nullptr ? reinterpret_cast<const char*>(text) : "";
}

bool Statement::columnIsNull(int index) const
{
    return sqlite3_column_type(stmt_, index) == SQLITE_NULL;
}

// ── Database ────────────────────────────────────────────────────────────────

std::int64_t nowSeconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

Database::~Database() { close(); }

bool Database::open(const std::string& path)
{
    close();
    errorMessage_.clear();

    // FULLMUTEX costs a little concurrency for thread safety; the server is
    // single-threaded but tests and future callers need not care.
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(path.c_str(), &handle_, flags, nullptr) != SQLITE_OK) {
        errorMessage_ = handle_ != nullptr
            ? sqlite3_errmsg(handle_)
            : "cannot open " + path;
        close();
        return false;
    }

    if (!exec("PRAGMA foreign_keys = ON")) {
        std::string message = errorMessage_;
        close();
        errorMessage_ = message;
        return false;
    }

    if (!exec(SchemaSql)) {
        std::string message = errorMessage_;
        close();
        errorMessage_ = message;
        return false;
    }

    if (readUserVersion() == 0 && !writeUserVersion(SchemaVersion)) {
        std::string message = errorMessage_;
        close();
        errorMessage_ = message;
        return false;
    }

    return true;
}

void Database::close()
{
    if (handle_ == nullptr)
        return;
    // close_v2 defers destruction if statements are still alive; managed
    // Statement objects always finalize in their destructor anyway.
    sqlite3_close_v2(handle_);
    handle_ = nullptr;
}

bool Database::exec(const std::string& sql)
{
    errorMessage_.clear();
    if (handle_ == nullptr) {
        errorMessage_ = "database is not open";
        return false;
    }
    char* error = nullptr;
    if (sqlite3_exec(handle_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        errorMessage_ = error != nullptr ? error : "sqlite3_exec failed";
        sqlite3_free(error);
        return false;
    }
    return true;
}

int Database::readUserVersion()
{
    Statement stmt;
    if (!stmt.prepare(handle_, "PRAGMA user_version"))
        return -1;
    if (stmt.step() != Statement::Step::Ready)
        return -1;
    return stmt.columnInt(0);
}

bool Database::writeUserVersion(int version)
{
    return exec("PRAGMA user_version = " + std::to_string(version));
}

// ── Transaction ─────────────────────────────────────────────────────────────

Transaction::~Transaction()
{
    if (open_)
        db_.exec("ROLLBACK");
}

bool Transaction::commit()
{
    if (!open_)
        return false;
    open_ = false;
    return db_.exec("COMMIT");
}

} // namespace chess::db
