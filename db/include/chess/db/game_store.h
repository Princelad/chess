#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <chess/db/database.h>

namespace chess::db {

// One completed game. Registered players keep their row ids; guests have no
// players table entry, so their ids are empty while the names are always
// stored on the row itself.
struct Game {
    std::int64_t id = 0;
    std::optional<std::int64_t> whiteId;
    std::optional<std::int64_t> blackId;
    std::string whiteName;
    std::string blackName;
    int result = 0;          // chess::net::GameResult
    int reason = 0;          // chess::net::GameOverReason
    std::string timeControl; // e.g. "3+2"; empty until 11.1 clocks exist
    std::string startFen;    // empty for the standard start position
    std::string sanMoves;    // space-separated SAN list; empty without moves
    std::int64_t playedAt = 0;
};

// SQL-backed archive of completed games over a Database (10.3 will wire the
// server to persist into it and build PGN/browser screens on top).
class GameStore {
public:
    explicit GameStore(Database& db);

    // Inserts a completed game row; returns its id on success. Fails when a
    // player id does not exist (foreign key violation).
    std::optional<std::int64_t> insert(const Game& game);

    std::optional<Game> byId(std::int64_t id);

    // The limit most recently completed games, newest first.
    std::vector<Game> latest(int limit);

    // latest(), restricted to games played by one account (as white or
    // black). Guests have no id and thus no per-player view.
    std::vector<Game> latestForPlayer(std::int64_t playerId, int limit);

    const std::string& errorMessage() const { return errorMessage_; }

private:
    static void scanRow(const Statement& stmt, Game& game);

    Database& db_;
    std::string errorMessage_;
};

} // namespace chess::db
