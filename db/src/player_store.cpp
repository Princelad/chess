#include <chess/db/player_store.h>

#include <chess/db/auth.h>
#include <chess/db/glicko2.h>
#include <chess/db/rating_class.h>

namespace chess::db {

namespace {

constexpr const char* SelectColumns =
    "SELECT id, name, rating, rating_deviation, volatility, "
    "wins, losses, draws, created_at, last_seen, last_rated_at FROM players";

constexpr const char* SelectClassColumns =
    "SELECT player_id, class, rating, rating_deviation, volatility, "
    "wins, losses, draws, last_rated_at FROM player_ratings";

// One idle rating period = one day (Glicko-2 periods are arbitrary; daily is
// the usual server convention). Glickman's no-game period grows RD by
// sqrt(φ² + σ²) each period — roughly 1 RD point early on and ~0.15 near the
// cap with σ = 0.06 — so a long absence is the honest way back to the 350
// cap: allow ~13.7 years of idle periods per login pass, with an early exit
// at the cap.
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
    scanRow(stmt, player);
    return player;
}

std::optional<ClassRating> PlayerStore::classRatingFor(
    std::int64_t playerId, const std::string& timeControl)
{
    errorMessage_.clear();
    const std::string ratingClass = classifyTimeControl(timeControl);

    Statement stmt;
    if (!stmt.prepare(db_.handle(),
            std::string(SelectClassColumns)
            + " WHERE player_id = ? AND class = ?")
        || !stmt.bind(1, playerId) || !stmt.bind(2, ratingClass)) {
        errorMessage_ = stmt.errorMessage();
        return std::nullopt;
    }
    auto step = stmt.step();
    if (step == Statement::Step::Error) {
        errorMessage_ = stmt.errorMessage();
        return std::nullopt;
    }
    if (step == Statement::Step::Done) {
        // No rated history in this class: factory defaults (11.1b keeps
        // missing rows implicit instead of materializing them).
        return ClassRating{playerId, ratingClass};
    }
    ClassRating row;
    row.playerId = stmt.columnInt64(0);
    row.ratingClass = stmt.columnText(1);
    row.rating = stmt.columnDouble(2);
    row.ratingDeviation = stmt.columnDouble(3);
    row.volatility = stmt.columnDouble(4);
    row.wins = stmt.columnInt(5);
    row.losses = stmt.columnInt(6);
    row.draws = stmt.columnInt(7);
    row.lastRatedAt = stmt.columnInt64(8);
    return row;
}

void PlayerStore::scanRow(const Statement& stmt, Player& player)
{
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
}

std::vector<Player> PlayerStore::topPlayers(int limit, int minGames)
{
    errorMessage_.clear();
    std::vector<Player> players;
    if (limit <= 0)
        return players;
    Statement stmt;
    if (!stmt.prepare(db_.handle(),
            std::string(SelectColumns)
            + " WHERE wins + losses + draws >= ? "
              "ORDER BY rating DESC, name ASC LIMIT ?")
        || !stmt.bind(1, std::int64_t(minGames))
        || !stmt.bind(2, std::int64_t(limit))) {
        errorMessage_ = stmt.errorMessage();
        return players;
    }
    while (stmt.step() == Statement::Step::Ready) {
        Player player;
        scanRow(stmt, player);
        players.push_back(std::move(player));
    }
    if (!stmt.errorMessage().empty())
        errorMessage_ = stmt.errorMessage();
    return players;
}

std::optional<RatedUpdate> PlayerStore::applyRatedGame(
    std::int64_t whiteId, std::int64_t blackId, chess::net::GameResult result,
    const std::string& timeControl)
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

    const std::string ratingClass = classifyTimeControl(timeControl);

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
    auto whiteClass = classRatingFor(whiteId, timeControl);
    auto blackClass = classRatingFor(blackId, timeControl);
    if (!whiteClass.has_value() || !blackClass.has_value()) {
        errorMessage_ = "rating class row lookup failed";
        return std::nullopt;
    }

    // Both sides read pre-period (pre-update) values, per the paper — now
    // per class (11.1b): a blitz game rates blitz bookkeeping only.
    const auto whitePrime = applyGames(
        Glicko2 {whiteClass->rating, whiteClass->ratingDeviation,
                 whiteClass->volatility},
        {{blackClass->rating, blackClass->ratingDeviation, whiteScore}});
    const auto blackPrime = applyGames(
        Glicko2 {blackClass->rating, blackClass->ratingDeviation,
                 blackClass->volatility},
        {{whiteClass->rating, whiteClass->ratingDeviation, 1.0 - whiteScore}});

    // Upserts the fresh figures into the class row (rows are sparse: a
    // first game in a class materializes it).
    Statement upsert;
    if (!upsert.prepare(db_.handle(),
            "INSERT INTO player_ratings (player_id, class, rating, "
            "rating_deviation, volatility, wins, losses, draws, last_rated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(player_id, class) DO UPDATE SET "
            "rating = excluded.rating, "
            "rating_deviation = excluded.rating_deviation, "
            "volatility = excluded.volatility, wins = excluded.wins, "
            "losses = excluded.losses, draws = excluded.draws, "
            "last_rated_at = excluded.last_rated_at")) {
        errorMessage_ = upsert.errorMessage();
        return std::nullopt;
    }

    // Legacy players-table mirror (only the "unlimited" class writes it):
    // the flat wire (ProfileMsg/WelcomeMsg) and topPlayers keep their
    // pre-11.1b single-number meaning.
    Statement mirror;
    if (ratingClass == "unlimited"
        && !mirror.prepare(db_.handle(),
            "UPDATE players SET rating = ?, rating_deviation = ?, "
            "volatility = ?, wins = ?, losses = ?, draws = ?, "
            "last_rated_at = ? WHERE id = ?")) {
        errorMessage_ = mirror.errorMessage();
        return std::nullopt;
    }

    const std::int64_t now = nowSeconds();
    ClassRating* classRows[2] = {&*whiteClass, &*blackClass};
    const Player* identity[2] = {&*white, &*black};
    const Glicko2 glicko[2] = {whitePrime, blackPrime};
    ClassRating updated[2];

    for (int side = 0; side < 2; ++side) {
        ClassRating& row = *classRows[side];
        const double score = side == 0 ? whiteScore : 1.0 - whiteScore;
        row.wins += score == 1.0 ? 1 : 0;
        row.losses += score == 0.0 ? 1 : 0;
        row.draws += score == 0.5 ? 1 : 0;
        row.rating = glicko[side].rating;
        row.ratingDeviation = glicko[side].deviation;
        row.volatility = glicko[side].volatility;
        row.lastRatedAt = now;
        updated[side] = row;

        upsert.reset();
        if (!upsert.bind(1, row.playerId) || !upsert.bind(2, row.ratingClass)
            || !upsert.bind(3, row.rating)
            || !upsert.bind(4, row.ratingDeviation)
            || !upsert.bind(5, row.volatility)
            || !upsert.bind(6, std::int64_t(row.wins))
            || !upsert.bind(7, std::int64_t(row.losses))
            || !upsert.bind(8, std::int64_t(row.draws))
            || !upsert.bind(9, row.lastRatedAt)
            || upsert.step() != Statement::Step::Done) {
            errorMessage_ = upsert.errorMessage();
            return std::nullopt;
        }

        if (ratingClass == "unlimited") {
            const Player& player = *identity[side];            mirror.reset();
            if (!mirror.bind(1, row.rating)
                || !mirror.bind(2, row.ratingDeviation)
                || !mirror.bind(3, row.volatility)
                || !mirror.bind(4, std::int64_t(row.wins))
                || !mirror.bind(5, std::int64_t(row.losses))
                || !mirror.bind(6, std::int64_t(row.draws))
                || !mirror.bind(7, now)
                || !mirror.bind(8, player.id)
                || mirror.step() != Statement::Step::Done) {
                errorMessage_ = mirror.errorMessage();
                return std::nullopt;
            }
        }
    }

    if (!tx.commit()) {
        errorMessage_ = db_.errorMessage();
        return std::nullopt;
    }
    // Post-update snapshots: the game's class carries the rating figures;
    // name comes from the players row.
    RatedUpdate out;
    out.white = *identity[0];
    out.white.rating = updated[0].rating;
    out.white.ratingDeviation = updated[0].ratingDeviation;
    out.white.volatility = updated[0].volatility;
    out.white.wins = updated[0].wins;
    out.white.losses = updated[0].losses;
    out.white.draws = updated[0].draws;
    out.white.lastRatedAt = updated[0].lastRatedAt;
    out.black = *identity[1];
    out.black.rating = updated[1].rating;
    out.black.ratingDeviation = updated[1].ratingDeviation;
    out.black.volatility = updated[1].volatility;
    out.black.wins = updated[1].wins;
    out.black.losses = updated[1].losses;
    out.black.draws = updated[1].draws;
    out.black.lastRatedAt = updated[1].lastRatedAt;
    return out;
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

    // Per class (11.1b): every owned row idles independently. One query
    // scans the (sparse) class rows for the player while a second statement
    // writes grown rows back.
    constexpr const char* ClassQuery =
        "SELECT rating, rating_deviation, volatility, class, last_rated_at, "
        "wins, losses, draws FROM player_ratings WHERE player_id = ?";
    Statement query;
    Statement write;
    const bool twoStatements =
        query.prepare(db_.handle(), ClassQuery)
        && write.prepare(db_.handle(),
            "UPDATE player_ratings SET rating = ?, rating_deviation = ?, "
            "volatility = ?, last_rated_at = ? WHERE player_id = ? AND class = ?");
    if (!twoStatements) {
        errorMessage_ = query.errorMessage().empty() ? write.errorMessage()
                                                     : query.errorMessage();
        return std::nullopt;
    }

    // The flat wire has no class field, so the login profile shows the
    // newest-rated class's figures; unlimited rows additionally mirror into
    // the legacy players columns. Track the freshest row while growing.
    std::int64_t newestTime = -1;
    ClassRating newestRow;
    bool unlimitedGrew = false;
    Glicko2 unlimitedAfter {};

    if (!query.bind(1, playerId)) {
        errorMessage_ = query.errorMessage();
        return std::nullopt;
    }
    while (query.step() == Statement::Step::Ready) {
        ClassRating row;
        row.playerId = playerId;
        row.rating = query.columnDouble(0);
        row.ratingDeviation = query.columnDouble(1);
        row.volatility = query.columnDouble(2);
        row.ratingClass = query.columnText(3);
        row.lastRatedAt = query.columnInt64(4);
        row.wins = query.columnInt(5);
        row.losses = query.columnInt(6);
        row.draws = query.columnInt(7);

        const bool isUnlimited = row.ratingClass == "unlimited";
        double afterRating = row.rating;
        if (row.lastRatedAt > 0) {
            std::int64_t elapsed = now - row.lastRatedAt;
            if (elapsed > 0) {
                int periods = static_cast<int>(elapsed / RatingPeriodSec);
                if (periods > 0) {
                    if (periods > MaxIdlePeriods)
                        periods = MaxIdlePeriods;
                    Glicko2 glicko {row.rating, row.ratingDeviation,
                                    row.volatility};
                    // Every idle step already converges to the 350 cap; the
                    // break keeps an idle decade from spinning 100 pointless
                    // iterations.
                    for (int i = 0; i < periods && glicko.deviation < 350.0;
                         ++i)
                        glicko = applyGames(glicko, {});
                    afterRating = glicko.rating;
                    if (glicko.deviation != row.ratingDeviation
                        || glicko.rating != row.rating
                        || glicko.volatility != row.volatility) {
                        write.reset();
                        if (!write.bind(1, glicko.rating)
                            || !write.bind(2, glicko.deviation)
                            || !write.bind(3, glicko.volatility)
                            || !write.bind(4, now)
                            || !write.bind(5, playerId)
                            || !write.bind(6, row.ratingClass)
                            || write.step() != Statement::Step::Done) {
                            errorMessage_ = write.errorMessage();
                            return std::nullopt;
                        }
                        row.rating = glicko.rating;
                        row.ratingDeviation = glicko.deviation;
                        row.volatility = glicko.volatility;
                        row.lastRatedAt = now;
                        if (isUnlimited) {
                            unlimitedGrew = true;
                            unlimitedAfter = glicko;
                        }
                    }
                }
            }
        }
        if (row.lastRatedAt > newestTime) {
            newestTime = row.lastRatedAt;
            newestRow = row;
        }
    }
    if (!query.errorMessage().empty()) {
        errorMessage_ = query.errorMessage();
        return std::nullopt;
    }

    if (unlimitedGrew) {
        // The legacy mirror follows the unlimited class row: same figures,
        // same freshness, so flat-wire readers (topPlayers) keep honest
        // numbers without any wire change.
        Statement legacy;
        if (!legacy.prepare(db_.handle(),
                "UPDATE players SET rating = ?, rating_deviation = ?, "
                "volatility = ?, last_rated_at = ? WHERE id = ?")
            || !legacy.bind(1, unlimitedAfter.rating)
            || !legacy.bind(2, unlimitedAfter.deviation)
            || !legacy.bind(3, unlimitedAfter.volatility)
            || !legacy.bind(4, now)
            || !legacy.bind(5, playerId)
            || legacy.step() != Statement::Step::Done) {
            errorMessage_ = legacy.errorMessage();
            return std::nullopt;
        }
    }

    // Flat display: the newest-rated class's figures land on the returned
    // Player (its own last_rated_at included), identity from the players row.
    if (newestTime >= 0) {
        player->rating = newestRow.rating;
        player->ratingDeviation = newestRow.ratingDeviation;
        player->volatility = newestRow.volatility;
        player->wins = newestRow.wins;
        player->losses = newestRow.losses;
        player->draws = newestRow.draws;
        player->lastRatedAt = newestRow.lastRatedAt;
    }
    return player;
}

} // namespace chess::db
