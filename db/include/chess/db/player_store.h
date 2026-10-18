#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <chess/db/database.h>
#include <chess/net/messages.h>

namespace chess::db {

// A registered account. The rating columns already carry the Glicko-2
// defaults; phase 10.2 fills them on completed rated games. Guests never get
// rows in this table — games reference them by name only.
struct Player {
    std::int64_t id = 0;
    std::string name;
    double rating = 1500.0;
    double ratingDeviation = 350.0;
    double volatility = 0.06;
    int wins = 0;
    int losses = 0;
    int draws = 0;
    std::int64_t createdAt = 0;
    std::int64_t lastSeen = 0;
};

// Post-update snapshots of the two participants of a rated game.
struct RatedUpdate {
    Player white;
    Player black;
};

// SQL-backed account store over a Database.
class PlayerStore {
public:
    explicit PlayerStore(Database& db);

    // Creates an account with a salted password hash. Returns std::nullopt
    // when the name or password fails the shared validation rules or the name
    // is already taken (comparison is case-insensitive).
    std::optional<Player> registerPlayer(const std::string& name, const std::string& password);

    std::optional<Player> byId(std::int64_t id);

    // Case-insensitive lookup, mirroring the UNIQUE COLLATE NOCASE column.
    std::optional<Player> byName(const std::string& name);

    // Verifies a login attempt against the stored salted hash and refreshes
    // last_seen on success.
    bool authenticate(const std::string& name, const std::string& password);

    // Applies one completed rated game atomically: both rows are read,
    // Glicko-2 updates them in one transaction, and wins/losses/draws counters
    // move with the result. Only WhiteWins/BlackWins/Draw are rated.
    // Returns post-update snapshots of both players; std::nullopt when the
    // result is not rated or a player id does not exist.
    std::optional<RatedUpdate> applyRatedGame(
        std::int64_t whiteId, std::int64_t blackId, chess::net::GameResult result);

    const std::string& errorMessage() const { return errorMessage_; }

private:
    std::optional<Player> rowBy(Statement& stmt);

    Database& db_;
    std::string errorMessage_;
};

} // namespace chess::db
