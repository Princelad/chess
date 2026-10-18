#include <chess/db/player_store.h>

#include <chess/db/auth.h>

namespace chess::db {

namespace {

constexpr const char* SelectColumns =
    "SELECT id, name, rating, rating_deviation, volatility, "
    "wins, losses, draws, created_at, last_seen FROM players";

} // namespace

PlayerStore::PlayerStore(Database& db) : db_(db) {}

std::optional<Player> PlayerStore::registerPlayer(
    const std::string& name, const std::string& password)
{
    errorMessage_.clear();
    if (!validAccountName(name) || !validPassword(password)) {
        errorMessage_ = "invalid account name or password";
        return std::nullopt;
    }

    const std::string hash = hashPassword(password);
    const auto now = nowSeconds();

    Statement stmt;
    if (!stmt.prepare(db_.handle(),
            "INSERT INTO players (name, password_hash, rating, rating_deviation, "
            "volatility, created_at, last_seen) "
            "VALUES (?, ?, 1500.0, 350.0, 0.06, ?, ?)")) {
        errorMessage_ = stmt.errorMessage();
        return std::nullopt;
    }
    if (!stmt.bind(1, name) || !stmt.bind(2, hash)
        || !stmt.bind(3, now) || !stmt.bind(4, now)) {
        errorMessage_ = stmt.errorMessage();
        return std::nullopt;
    }
    if (stmt.step() != Statement::Step::Done) {
        sqlite3* connection = db_.handle();
        errorMessage_ = sqlite3_errcode(connection) == SQLITE_CONSTRAINT
            ? "account name is already taken"
            : stmt.errorMessage();
        return std::nullopt;
    }

    const auto id = sqlite3_last_insert_rowid(db_.handle());
    auto registered = byId(id);
    if (!registered.has_value())
        errorMessage_ = "registerPlayer: inserted row could not be read back";
    return registered;
}

std::optional<Player> PlayerStore::byId(std::int64_t id)
{
    errorMessage_.clear();
    Statement stmt;
    if (!stmt.prepare(db_.handle(), std::string(SelectColumns) + " WHERE id = ?")
        || !stmt.bind(1, id)) {
        errorMessage_ = stmt.errorMessage();
        return std::nullopt;
    }
    return rowBy(stmt);
}

std::optional<Player> PlayerStore::byName(const std::string& name)
{
    errorMessage_.clear();
    Statement stmt;
    // The column itself carries COLLATE NOCASE, so '=' already ignores case.
    if (!stmt.prepare(db_.handle(), std::string(SelectColumns) + " WHERE name = ?")
        || !stmt.bind(1, name)) {
        errorMessage_ = stmt.errorMessage();
        return std::nullopt;
    }
    return rowBy(stmt);
}

bool PlayerStore::authenticate(const std::string& name, const std::string& password)
{
    errorMessage_.clear();
    if (!validAccountName(name) || !validPassword(password)) {
        errorMessage_ = "invalid account name or password";
        return false;
    }

    // Case-insensitive match, consistent with byName().
    Statement stmt;
    if (!stmt.prepare(db_.handle(),
            "SELECT id, password_hash FROM players WHERE name = ?")
        || !stmt.bind(1, name)) {
        errorMessage_ = stmt.errorMessage();
        return false;
    }
    if (stmt.step() != Statement::Step::Ready) {
        // Not found (or a rare statement failure — LastError is empty for DONE).
        errorMessage_ = "player not found";
        return false;
    }
    const std::int64_t id = stmt.columnInt64(0);
    const std::string storedHash = stmt.columnText(1);

    if (!verifyPassword(password, storedHash)) {
        errorMessage_ = "wrong password";
        return false;
    }

    Statement touch;
    if (!touch.prepare(db_.handle(), "UPDATE players SET last_seen = ? WHERE id = ?")
        || !touch.bind(1, nowSeconds()) || !touch.bind(2, id)
        || touch.step() != Statement::Step::Done) {
        errorMessage_ = touch.errorMessage();
        return false;
    }
    return true;
}

std::optional<Player> PlayerStore::rowBy(Statement& stmt)
{
    const auto result = stmt.step();
    if (result == Statement::Step::Error) {
        errorMessage_ = stmt.errorMessage();
        return std::nullopt;
    }
    if (result == Statement::Step::Done)
        return std::nullopt; // no such row

    Player player;
    player.id = stmt.columnInt64(0);
    player.name = stmt.columnText(1);
    player.rating = stmt.columnDouble(2);
    player.ratingDeviation = stmt.columnDouble(3);
    player.volatility = stmt.columnDouble(4);
    player.wins = stmt.columnInt(5);
    player.losses = stmt.columnInt(6);
    player.draws = stmt.columnInt(7);
    player.createdAt = stmt.columnInt64(8);
    player.lastSeen = stmt.columnInt64(9);
    return player;
}

} // namespace chess::db
