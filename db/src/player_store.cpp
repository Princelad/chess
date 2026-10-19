#include <chess/db/player_store.h>

#include <chess/db/auth.h>
#include <chess/db/glicko2.h>

namespace chess::db {

namespace {

constexpr const char* SelectColumns =
    "SELECT id, name, rating, rating_deviation, volatility, "
    "wins, losses, draws, created_at, last_seen, last_rated_at FROM players";

// One idle rating period = one day (Glicko-2 periods are arbitrary; daily is
// the usual server convention). Glickman's no-game period grows RD by
// sqrt(φ² + σ²) each period ≈ 0.07 RD points with σ = 0.06, so a very long
// absence is the honest way back to the 350 cap: allow ~13 years of idle
// periods per login pass, with an early exit at the cap.
constexpr std::int64_t RatingPeriodSec = 86400;
constexpr int MaxIdlePeriods = 5000;

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
    player.lastRatedAt = stmt.columnInt64(10);
    return player;
}

std::optional<RatedUpdate> PlayerStore::applyRatedGame(
    std::int64_t whiteId, std::int64_t blackId, chess::net::GameResult result)
{
    errorMessage_.clear();

    // Only decisive results and full draws are rated.
    double whiteScore = 0.0;
    switch (result) {
        case chess::net::GameResult::WhiteWins: whiteScore = 1.0; break;
        case chess::net::GameResult::Draw:      whiteScore = 0.5; break;
        case chess::net::GameResult::BlackWins: whiteScore = 0.0; break;
        default:
            errorMessage_ = "game result is not rateable";
            return std::nullopt;
    }

    // One transaction covers both reads and both writes so a game can never
    // be half-rated when the server dies mid-update.
    Transaction tx(db_);
    if (!tx) {
        errorMessage_ = db_.errorMessage();
        return std::nullopt;
    }

    auto white = byId(whiteId);
    auto black = byId(blackId);
    if (!white.has_value() || !black.has_value()) {
        errorMessage_ = white.has_value()
            ? "black player for rated game not found"
            : "white player for rated game not found";
        return std::nullopt;
    }

    // Both sides read pre-period (pre-update) values, per the paper.
    const auto whitePrime = applyGames(
        Glicko2 {white->rating, white->ratingDeviation, white->volatility},
        {{black->rating, black->ratingDeviation, whiteScore}});
    const auto blackPrime = applyGames(
        Glicko2 {black->rating, black->ratingDeviation, black->volatility},
        {{white->rating, white->ratingDeviation, 1.0 - whiteScore}});

    Statement stmt;
    if (!stmt.prepare(db_.handle(),
            "UPDATE players SET rating = ?, rating_deviation = ?, "
            "volatility = ?, wins = ?, losses = ?, draws = ?, "
            "last_rated_at = ? WHERE id = ?")) {
        errorMessage_ = stmt.errorMessage();
        return std::nullopt;
    }
    const std::int64_t now = nowSeconds();
    for (Player* updated : {&*white, &*black}) {
        const bool whiteSide = updated->id == white->id;
        const double score = whiteSide ? whiteScore : 1.0 - whiteScore;
        const int wins = updated->wins + (score == 1.0 ? 1 : 0);
        const int losses = updated->losses + (score == 0.0 ? 1 : 0);
        const int draws = updated->draws + (score == 0.5 ? 1 : 0);
        const auto glicko = whiteSide ? whitePrime : blackPrime;

        stmt.reset();
        if (!stmt.bind(1, glicko.rating) || !stmt.bind(2, glicko.deviation)
            || !stmt.bind(3, glicko.volatility) || !stmt.bind(4, std::int64_t(wins))
            || !stmt.bind(5, std::int64_t(losses))
            || !stmt.bind(6, std::int64_t(draws))
            || !stmt.bind(7, now)
            || !stmt.bind(8, updated->id)
            || stmt.step() != Statement::Step::Done) {
            errorMessage_ = stmt.errorMessage();
            return std::nullopt;
        }
        updated->rating = glicko.rating;
        updated->ratingDeviation = glicko.deviation;
        updated->volatility = glicko.volatility;
        updated->wins = wins;
        updated->losses = losses;
        updated->draws = draws;
        updated->lastRatedAt = now;
    }

    if (!tx.commit()) {
        errorMessage_ = db_.errorMessage();
        return std::nullopt;
    }
    return RatedUpdate {*white, *black};
}

std::optional<Player> PlayerStore::applyIdleGrowth(
    std::int64_t playerId, std::int64_t now)
{
    errorMessage_.clear();
    auto player = byId(playerId);
    if (!player.has_value()) {
        errorMessage_ = "player not found";
        return std::nullopt;
    }

    // Never-rated players already sit at the RD cap: growth is a no-op, not
    // an error (their "last period" is undefined until a first rated game).
    if (player->lastRatedAt <= 0)
        return player;

    std::int64_t elapsed = now - player->lastRatedAt;
    if (elapsed <= 0)
        return player;

    int periods = static_cast<int>(elapsed / RatingPeriodSec);
    if (periods <= 0)
        return player;
    if (periods > MaxIdlePeriods)
        periods = MaxIdlePeriods;

    Glicko2 glicko {player->rating, player->ratingDeviation, player->volatility};
    const double before = glicko.deviation;
    // Every idle step already converges to the 350 cap; the break keeps an
    // idle decade from spinning 100 pointless iterations.
    for (int i = 0; i < periods && glicko.deviation < 350.0; ++i)
        glicko = applyGames(glicko, {});
    if (glicko.deviation == before)
        return player;

    Statement stmt;
    if (!stmt.prepare(db_.handle(),
            "UPDATE players SET rating = ?, rating_deviation = ?, "
            "volatility = ? WHERE id = ?")
        || !stmt.bind(1, glicko.rating) || !stmt.bind(2, glicko.deviation)
        || !stmt.bind(3, glicko.volatility) || !stmt.bind(4, playerId)
        || stmt.step() != Statement::Step::Done) {
        errorMessage_ = stmt.errorMessage();
        return std::nullopt;
    }

    player->rating = glicko.rating;
    player->ratingDeviation = glicko.deviation;
    player->volatility = glicko.volatility;
    return player;
}

} // namespace chess::db
