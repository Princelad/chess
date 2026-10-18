#include <chess/db/game_store.h>

namespace chess::db {

namespace {

constexpr const char* SelectColumns =
    "SELECT id, white_id, black_id, white_name, black_name, result, reason, "
    "time_control, start_fen, san_moves, played_at FROM games";

} // namespace

GameStore::GameStore(Database& db) : db_(db) {}

std::optional<std::int64_t> GameStore::insert(const Game& game)
{
    errorMessage_.clear();
    Statement stmt;
    if (!stmt.prepare(db_.handle(),
            "INSERT INTO games (white_id, black_id, white_name, black_name, "
            "result, reason, time_control, start_fen, san_moves, played_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")) {
        errorMessage_ = stmt.errorMessage();
        return std::nullopt;
    }
    if (game.whiteId.has_value())
        stmt.bind(1, *game.whiteId);
    else
        stmt.bindNull(1);
    if (game.blackId.has_value())
        stmt.bind(2, *game.blackId);
    else
        stmt.bindNull(2);
    if (!stmt.bind(3, game.whiteName) || !stmt.bind(4, game.blackName)
        || !stmt.bind(5, std::int64_t(game.result)) || !stmt.bind(6, std::int64_t(game.reason))
        || !stmt.bind(7, game.timeControl) || !stmt.bind(8, game.startFen)
        || !stmt.bind(9, game.sanMoves) || !stmt.bind(10, game.playedAt)) {
        errorMessage_ = stmt.errorMessage();
        return std::nullopt;
    }
    if (stmt.step() != Statement::Step::Done) {
        errorMessage_ = stmt.errorMessage();
        if (errorMessage_.empty())
            errorMessage_ = "game insert failed";
        return std::nullopt;
    }
    return sqlite3_last_insert_rowid(db_.handle());
}

std::optional<Game> GameStore::byId(std::int64_t id)
{
    errorMessage_.clear();
    Statement stmt;
    if (!stmt.prepare(db_.handle(), std::string(SelectColumns) + " WHERE id = ?")
        || !stmt.bind(1, id)) {
        errorMessage_ = stmt.errorMessage();
        return std::nullopt;
    }
    const auto result = stmt.step();
    if (result == Statement::Step::Error) {
        errorMessage_ = stmt.errorMessage();
        return std::nullopt;
    }
    if (result == Statement::Step::Done)
        return std::nullopt;
    Game game;
    scanRow(stmt, game);
    return game;
}

std::vector<Game> GameStore::latest(int limit)
{
    errorMessage_.clear();
    std::vector<Game> games;
    if (limit <= 0)
        return games;
    Statement stmt;
    if (!stmt.prepare(db_.handle(),
            std::string(SelectColumns) + " ORDER BY id DESC LIMIT ?")
        || !stmt.bind(1, std::int64_t(limit))) {
        errorMessage_ = stmt.errorMessage();
        return games;
    }
    while (stmt.step() == Statement::Step::Ready) {
        Game game;
        scanRow(stmt, game);
        games.push_back(std::move(game));
    }
    if (!stmt.errorMessage().empty())
        errorMessage_ = stmt.errorMessage();
    return games;
}

void GameStore::scanRow(const Statement& stmt, Game& game)
{
    game.id = stmt.columnInt64(0);
    game.whiteId = stmt.columnIsNull(1)
        ? std::nullopt : std::optional<std::int64_t>(stmt.columnInt64(1));
    game.blackId = stmt.columnIsNull(2)
        ? std::nullopt : std::optional<std::int64_t>(stmt.columnInt64(2));
    game.whiteName = stmt.columnText(3);
    game.blackName = stmt.columnText(4);
    game.result = stmt.columnInt(5);
    game.reason = stmt.columnInt(6);
    game.timeControl = stmt.columnText(7);
    game.startFen = stmt.columnText(8);
    game.sanMoves = stmt.columnText(9);
    game.playedAt = stmt.columnInt64(10);
}

} // namespace chess::db
