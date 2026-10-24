#include "server.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <SFML/Network/SocketSelector.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>

#include <chess/chess960.h>
#include <chess/db/database.h>
#include <chess/db/game_store.h>
#include <chess/db/player_store.h>
#include <chess/net/protocol.h>

#include "client.h"
#include "log.h"
#include "match.h"
#include "matchmaker.h"
#include "send.h"
#include "tournament.h"

#include <map>

using namespace std::chrono_literals;

static constexpr std::size_t MaxPacketSize = 4096;
static constexpr int MaxBadMessages = 3;
static constexpr std::size_t MaxNameLength = 32;

// Canonicalizes the JoinMsg time-control payload for pairing and the
// archive row: anything unparsable ("5+a", no '+', oversized) is the
// unlimited game, valid forms rebuild as "base+increment" from the parsed
// numbers so the stored string always round-trips through parseTimeControl.
static std::string canonicalTimeControl(const std::string& tc)
{
    if (tc.size() > 16)
        return {};
    const auto parsed = chess::net::parseTimeControl(tc);
    if (parsed.unlimited)
        return {};
    return std::to_string(parsed.baseSeconds) + "+"
           + std::to_string(parsed.incrementSeconds);
}

using ClientVec = std::vector<std::unique_ptr<Client>>;
using MatchVec = std::vector<std::unique_ptr<Match>>;

static ClientVec::iterator disconnectClient(
    ClientVec::iterator it, ClientVec& clients,
    sf::SocketSelector& selector, Matchmaker& matchmaker)
{
    Client& client = **it;
    if (!client.isBot) {
        auto addr = client.socket->getRemoteAddress();
        LOG_INFO("Client disconnected: " + (addr.has_value() ? addr->toString() : "unknown"));
        selector.remove(*client.socket);
    } else {
        LOG_INFO("Bot disconnected: " + client.name);
    }
    if (client.state == ClientState::Queued)
        matchmaker.remove(client);
    else if (client.state == ClientState::InMatch
             || client.state == ClientState::GameOver)
        client.match->handleDisconnect(client);
    return clients.erase(it);
}

// GameResult name for logs (chess::net's own strings live in its
// anonymous namespace).
static const char* gameResultName(chess::net::GameResult result)
{
    switch (result) {
        case chess::net::GameResult::WhiteWins: return "white wins";
        case chess::net::GameResult::BlackWins: return "black wins";
        case chess::net::GameResult::Draw:      return "draw";
        case chess::net::GameResult::Resignation: return "resignation";
        case chess::net::GameResult::Abort:     return "abort";
        default:                                return "unknown";
    }
}

// The connection owning this arena entrant row, or nullptr when the row
// is orphaned (its connection dropped, moved arenas, or cleared itself).
static Client* findArenaClient(ClientVec& clients, const std::string& arenaKey,
                               std::int64_t entrantId)
{
    for (auto& c : clients)
        if (c->arenaKey == arenaKey && c->arenaEntrantId == entrantId)
            return c.get();
    return nullptr;
}

// Arena pairing eligibility for a member's connection: idle-in-lobby
// (Connected) or on a finished game's game-over screen — the retention
// shell detaches right before the row is re-seated. Anything mid-life
// (queued... can't coexist with membership; live/notice states) waits.
static bool isArenaIdle(const Client& client)
{
    if (client.state == ClientState::Connected)
        return true;
    return client.state == ClientState::GameOver && client.match != nullptr
        && !client.match->isActive();
}

// A pairing may re-seat a client whose match still sits in retention
// (their game-over screen). Detach its seats first — the exact discipline
// the 11.7 rematch hook applies before startMatch.
static void detachFinishedMatchFor(Client& client)
{
    if (client.match != nullptr && !client.match->isActive())
        client.match->detachAllSeats();
}

namespace chess::server {

// ── Auth (phase 10.2) ───────────────────────────────────────────────────────

static void sendProfileTo(Client& client, const chess::db::Player& player)
{
    sendTo(client, chess::net::ProfileMsg{
        player.name, player.rating, player.ratingDeviation, false});
}

// ── Token resume (11.2) ─────────────────────────────────────────────────────

// Full resync for a rejoining seat: everything Replay needs to rebuild —
// seat identity, final board via the SAN chain, fresh clock figures —
// sampled at a single timestamp.
static void sendResumeState(Client& rejoiner, const Match& match,
                            chess::Color myColor, std::int64_t nowMs)
{
    const auto mine = match.seat(myColor);
    const auto other = match.seat(
        myColor == chess::Color::White ? chess::Color::Black : chess::Color::White);
    chess::net::ResumeStateMsg msg;
    msg.color = myColor;
    msg.opponent = other.name;
    msg.rating = other.rating;
    msg.deviation = other.deviation;
    msg.rated = mine.playerId.has_value() && other.playerId.has_value();
    msg.timeControl = match.timeControl();
    msg.fen = match.currentFen();
    msg.sans = match.sanHistory();
    auto clocks = match.clockSnapshotMs(nowMs);
    msg.whiteRemainingMs = clocks.first;
    msg.blackRemainingMs = clocks.second;
    sendTo(rejoiner, msg);
}

// Refreshes the rejoiner's account profile from storage: rated results may
// have landed while the seat was away, and the client caches the old figure.
static void sendFreshProfile(Client& rejoiner, const Match& match,
                             chess::Color seatColor, chess::db::PlayerStore* players)
{
    const auto seat = match.seat(seatColor);
    if (players == nullptr || !seat.playerId.has_value()) return;
    auto row = players->byId(*seat.playerId);
    if (row.has_value())
        sendProfileTo(rejoiner, *row);
}

// Routes ResumeMsg{token} into the match that owns that seat token.
static void handleResume(Client& newcomer, const chess::net::ResumeMsg& msg,
                         MatchVec& matches, chess::db::PlayerStore* players)
{
    for (auto& match : matches) {
        chess::Color rebound = chess::Color::White;
        auto outcome = match->resume(msg.token, newcomer, rebound);
        if (outcome == Match::ResumeOutcome::NoSeat)
            continue; // tokens are unique per seat; keep searching
        if (outcome == Match::ResumeOutcome::AlreadyConnected) {
            sendTo(newcomer, chess::net::ErrorMsg{"That seat is already connected"});
            return;
        }

        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        sendResumeState(newcomer, *match, rebound, nowMs);
        sendFreshProfile(newcomer, *match, rebound, players);

        if (outcome == Match::ResumeOutcome::ReboundOver) {
            // The game ended while the seat was away: replay the outcome so
            // the client still lands on its result screen.
            auto over = match->lastGameOver();
            if (over)
                sendTo(newcomer, *over);
        } else {
            // Live game: seated again for the message routing.
            newcomer.state = ClientState::InMatch;
            newcomer.match = match.get();
        }
        return;
    }
    sendTo(newcomer, chess::net::ErrorMsg{"No game to resume — session expired"});
}

static void handleLogin(Client& client, const chess::net::LoginMsg& login,
                        chess::db::PlayerStore* players)
{
    if (players == nullptr) {
        sendTo(client, chess::net::ErrorMsg{"This server has no account support"});
        return;
    }
    if (login.name.empty()) {
        sendTo(client, chess::net::ErrorMsg{"Name required"});
        return;
    }
    if (login.name.size() > MaxNameLength) {
        sendTo(client, chess::net::ErrorMsg{"Name too long"});
        return;
    }

    // Login throttle (R2): consecutive failures on a connection back off
    // exponentially (1 s doubling, 60 s cap) so password guessing by byte
    // count is slowed to a crawl. The lock only delays new attempts — no
    // error carries the account's existence.
    const auto now = std::chrono::steady_clock::now();
    if (now < client.loginLockedUntil) {
        const auto wait = std::chrono::duration_cast<std::chrono::seconds>(
            client.loginLockedUntil - now).count() + 1;
        sendTo(client, chess::net::ErrorMsg{
            "Too many failed logins — try again in "
            + std::to_string(wait) + " seconds"});
        return;
    }

    auto failLogin = [&client](const std::string& why) {
        ++client.failedLogins;
        // A single failure never locks: one typo followed by the correct
        // password must stay instant. The backoff arms from the second
        // consecutive failure (1 s doubling, 60 s cap).
        if (client.failedLogins >= 2) {
            // Clamp the exponent before shifting: the failure count is
            // unbounded, so the raw shift would run past int width (UB) on a
            // spammed connection — six steps of doubling (64 s) already hit
            // the 60 s cap, so nothing past six can be distinguishing anyway.
            const int exponent = std::min(client.failedLogins - 2, 6);
            const auto stepMs = std::min<std::int64_t>(
                std::int64_t(1000) << exponent, 60000);
            client.loginLockedUntil = std::chrono::steady_clock::now()
                + std::chrono::milliseconds(stepMs);
        }
        sendTo(client, chess::net::ErrorMsg{why});
    };

    auto existing = players->byName(login.name);
    if (existing.has_value()) {
        if (login.password.empty()) {
            sendTo(client, chess::net::ErrorMsg{
                "Account '" + existing->name + "' is password protected — "
                "enter the password to log in, or pick another name for guest play"});
            return;
        }
        if (!players->authenticate(login.name, login.password)) {
            failLogin("Wrong password for account '" + existing->name + "'");
            return;
        }
        client.playerId = existing->id;
        client.name = existing->name;
        client.failedLogins = 0;
        client.loginLockedUntil = {};
        // Glicko-2 idle periods grow RD since the last rated game; the
        // profile is sent from the freshly grown row.
        auto fresh = players->applyIdleGrowth(existing->id, chess::db::nowSeconds());
        if (fresh.has_value())
            existing = *fresh;
        sendProfileTo(client, *existing);
        LOG_INFO("Client logged in: " + existing->name);
        return;
    }

    // Unknown name + password = transparent account registration.
    if (login.password.empty()) {
        sendTo(client, chess::net::ErrorMsg{
            "Enter a password to create an account with this name, "
            "or leave it empty to play as an unrated guest"});
        return;
    }
    auto created = players->registerPlayer(login.name, login.password);
    if (!created.has_value()) {
        failLogin(players->errorMessage());
        return;
    }
    client.playerId = created->id;
    client.name = created->name;
    sendProfileTo(client, *created);
    LOG_INFO("Client registered: " + created->name);
}

// Archive rows → wire entries (rated flag = both participants registered).
static std::vector<chess::net::ArchiveEntryMsg> toArchiveEntries(
    const std::vector<chess::db::Game>& games)
{
    std::vector<chess::net::ArchiveEntryMsg> entries;
    entries.reserve(games.size());
    for (const auto& game : games) {
        chess::net::ArchiveEntryMsg entry;
        entry.id = game.id;
        entry.whiteName = game.whiteName;
        entry.blackName = game.blackName;
        entry.result = game.result;
        entry.playedAt = game.playedAt;
        entry.rated = game.whiteId.has_value() && game.blackId.has_value();
        entries.push_back(std::move(entry));
    }
    return entries;
}

// Leaderboard rows → wire entries (10.4): name, rating and the rated-game
// counters; rank is the entry's position in the list.
static std::vector<chess::net::LeaderboardEntryMsg> toLeaderboardEntries(
    const std::vector<chess::db::Player>& players)
{
    std::vector<chess::net::LeaderboardEntryMsg> entries;
    entries.reserve(players.size());
    for (const auto& player : players) {
        chess::net::LeaderboardEntryMsg entry;
        entry.name = player.name;
        entry.rating = player.rating;
        entry.deviation = player.ratingDeviation;
        entry.wins = player.wins;
        entry.losses = player.losses;
        entry.draws = player.draws;
        entries.push_back(std::move(entry));
    }
    return entries;
}

unsigned short runServer(const ServerConfig& config, std::atomic<bool>& shutdownFlag,
                         unsigned short* boundPortOut){
    auto ipOpt = sf::IpAddress::fromString(config.host);
    if (!ipOpt.has_value()) {
        LOG_ERROR("Invalid address: " + config.host);
        return 0;
    }

    sf::TcpListener listener;
    auto status = listener.listen(config.port, *ipOpt);
    if (status != sf::Socket::Status::Done) {
        LOG_ERROR("Failed to bind listener on " + config.host + ":" + std::to_string(config.port));
        return 0;
    }
    unsigned short boundPort = listener.getLocalPort();
    if (boundPortOut) *boundPortOut = boundPort;
    LOG_INFO("Listening on " + config.host + ":" + std::to_string(boundPort));

    // Persistent storage (phase 10.1). A hard failure is preferable to a
    // silently unrated server when the operator asked for a database.
    chess::db::Database database;
    std::optional<chess::db::PlayerStore> playerStoreOpt;
    std::optional<chess::db::GameStore> gameStoreOpt;
    if (!config.dbPath.empty()) {
        if (!database.open(config.dbPath)) {
            LOG_ERROR("Failed to open database " + config.dbPath + ": "
                + database.errorMessage());
            return 0;
        }
        playerStoreOpt.emplace(database);
        gameStoreOpt.emplace(database);
        LOG_INFO("Database opened: " + config.dbPath);
    }
    chess::db::PlayerStore* players =
        playerStoreOpt.has_value() ? &*playerStoreOpt : nullptr;
    chess::db::GameStore* games =
        gameStoreOpt.has_value() ? &*gameStoreOpt : nullptr;

    ClientVec clients;
    MatchVec matches;
    Matchmaker matchmaker;
    std::size_t botNameCounter = 0;

    // Arenas (12.3): one running tournament per (timeControl, variant)
    // pair, keyed "tc|variant" — the same key Match carries on its games.
    // An Ended arena stays in the map to keep serving its final table
    // until the last member leaves; a fresh join reopens it.
    std::map<std::string, Tournament> arenas;

    // One personalized standings copy for `recipient` (only their own row
    // carries the self flag). Also the arena's phase driver: when every
    // member played the full quota mid-broadcast, the phase flips to
    // Ended and this broadcast IS the final table.
    auto sendArenaSnapshot = [&](Tournament& arena, Client& recipient) {
        // Final-table driver: the moment the last quota lands (mid-
        // broadcast), the phase flips to Ended and this snapshot IS the
        // final table — no member is pairable any more, so the lobby
        // would otherwise run open forever.
        if (arena.phase() == ArenaPhase::Open && arena.everyoneFinished())
            arena.endArena();
        auto rows = arena.entrants();
        std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
            const int pa = arenaPoints(a);
            const int pb = arenaPoints(b);
            if (pa != pb) return pa > pb;
            return a.name < b.name;
        });
        chess::net::TournamentStateMsg state;
        state.phase = arena.phase() == ArenaPhase::Ended ? 1 : 0;
        state.roundsTarget = arena.roundsTarget();
        for (const auto& row : rows) {
            chess::net::TournamentRowMsg wire;
            wire.name = row.name;
            wire.points = arenaPoints(row);
            wire.wins = row.wins;
            wire.losses = row.losses;
            wire.draws = row.draws;
            wire.self = row.id == recipient.arenaEntrantId
                && recipient.arenaKey == arena.key();
            wire.playing = row.inGame;
            state.rows.push_back(std::move(wire));
        }
        sendTo(recipient, state);
    };

    // Fresh standings for every live member of the arena (joins, leaves,
    // finished games). Empty arenas have nobody left: the loop skips
    // through, and the pass sweep erases the shell afterwards.
    auto broadcastArena = [&](Tournament& arena) {
        for (auto& c : clients)
            if (c->arenaKey == arena.key())
                sendArenaSnapshot(arena, *c);
    };

    // Rated update on game over (phase 10.2): both participants must be
    // registered; guests and bots keep unrated games. Runs after the
    // GameOverMsg broadcast to present seats, while seats are still attached.
    auto onGameOver = [&](const Match& match, chess::net::GameResult result,
                          chess::net::GameOverReason reason) {
        // Seat snapshots (11.2), not live connections: a departed player
        // still owns their half of the record and the rated update.
        const auto white = match.seat(chess::Color::White);
        const auto black = match.seat(chess::Color::Black);

        // Archive (10.3): any game with at least one accepted move is
        // persisted — rated or not, guests get no players-row linkage.
        if (games != nullptr && !match.sanHistory().empty()) {
            chess::db::Game row;
            row.whiteId = white.playerId;
            row.blackId = black.playerId;
            row.whiteName = white.name;
            row.blackName = black.name;
            row.result = static_cast<int>(result);
            row.reason = static_cast<int>(reason);
            row.timeControl = match.timeControl();
            row.startFen = match.startFen();  // empty = standard start (12.1)
            std::string sanJoined;
            for (const auto& san : match.sanHistory()) {
                if (!sanJoined.empty()) sanJoined += ' ';
                sanJoined += san;
            }
            row.sanMoves = std::move(sanJoined);
            row.playedAt = std::time(nullptr);
            if (!games->insert(row).has_value())
                LOG_WARN("Failed to archive game: " + games->errorMessage());
            else
                LOG_INFO("Game archived (plies=" + std::to_string(match.sanHistory().size()) + ")");
        }

        // Arena scoring (12.3): a tournament game never rates — the lobby
        // standings absorb it instead. Rows are found by seat name; a seat
        // that already left (their connection dropped, row reaped) simply
        // makes no row, so the survivor still gets their point.
        if (match.isArena()) {
            auto arenaIt = arenas.find(match.arenaKey());
            if (arenaIt != arenas.end()
                && arenaIt->second.applyResult(white.name, black.name, result)) {
                LOG_INFO("Arena result applied: " + white.name + " vs "
                    + black.name + " (" + std::string(gameResultName(result)) + ")");
                broadcastArena(arenaIt->second);
            }
            return;
        }

        // Rated update (phase 10.2, per class since 11.1b): both
        // participants must be registered; guests and bots keep unrated
        // games. The time control keys the rating class.
        if (players == nullptr || !white.playerId.has_value()
            || !black.playerId.has_value())
            return;
        auto update = players->applyRatedGame(*white.playerId, *black.playerId,
                                              result, match.timeControl());
        if (!update.has_value()) {
            LOG_WARN("Rated update failed: " + players->errorMessage());
            return;
        }
        LOG_INFO("Rated " + update->white.name + " -> "
            + std::to_string(static_cast<int>(update->white.rating))
            + " / " + update->black.name + " -> "
            + std::to_string(static_cast<int>(update->black.rating)));
        // Profiles reach whoever is still here; departed seats see the fresh
        // row the next time they log in.
        if (match.white()) sendProfileTo(*match.white(), update->white);
        if (match.black()) sendProfileTo(*match.black(), update->black);
    };

    sf::SocketSelector selector;
    selector.add(listener);

    // Pairing helper (phase 10.2): creates the match, wires the rated hook
    // and broadcasts the richer WelcomeMsg (opponent rating + rated flag)
    // made from fresh rows; guests fall back to the default 1500 ± 350.
    // Held as a std::function because the rematch callback (11.7) is
    // mutually recursive with it: an accept pairs the same two clients
    // again with swapped colors and needs startMatch, and every fresh
    // match in turn needs that hook wired. The trailing `arena` flag
    // (12.3) marks a tournament pairing: never rated, the game carries
    // its arena key, and its rematch gates refuse inside Match itself.
    std::function<Match*(Client&, Client&, bool arena)> startMatch;

    // Rematch hook (11.7): only fires from a finished match with both
    // seats attached on their game-over screens. Colors swap (startMatch's
    // first client takes White), the finished match keeps only its
    // retention shell, and a rematched bot restarts against its new color.
    auto onRematch = [&](Match& finished) {
        Client* white = finished.white();
        Client* black = finished.black();
        if (!white || !black) {
            LOG_WARN("Rematch fired with an absent seat — dropped");
            return;
        }
        Client* bot = white->isBot ? white : (black->isBot ? black : nullptr);
        const bool botSeatWhiteNext = bot == black;
        finished.detachAllSeats();
        Match* match = startMatch(*black, *white, false);
        if (match != nullptr && bot != nullptr)
            match->startBot(botSeatWhiteNext ? chess::Color::White
                                             : chess::Color::Black,
                            config.botDepth, config.botEnginePath);
    };

    startMatch = [&](Client& first, Client& second, bool arena) -> Match* {
        first.state = ClientState::InMatch;
        second.state = ClientState::InMatch;
        // Chess960 (12.1): every fresh pairing rolls a random Start
        // Position number and carries its FEN through Match + both
        // WelcomeMsgs — including rematches, which deliberately regenerate
        // (fresh position, same variant).
        std::string startFen;
        if (!first.variant.empty()) {
            auto fen = chess960StartFen(chess960RandomId());
            if (fen.has_value())
                startFen = *fen;
        }
        auto match = std::make_unique<Match>(first, second, first.timeControl,
                                             startFen,
                                             config.reconnectGraceMs);
        // Arena game (12.3): the lobby key travels with the game so the
        // game-over hook finds the right standings table, and Match itself
        // refuses the rematch trio.
        if (arena)
            match->setArenaKey(first.timeControl + "|" + first.variant);
        match->setGameOverCallback(onGameOver);
        match->setRematchCallback(onRematch);
        first.match = match.get();
        second.match = match.get();

        const bool rated = !arena && players != nullptr
            && first.playerId.has_value() && second.playerId.has_value();
        auto opponentRow = [&](const Client& c,
                               const std::string& timeControl)
            -> std::pair<double, double> {
            if (players != nullptr && c.playerId.has_value()) {
                // Per class (11.1b): the match's time control selects the
                // rating figures the opponents see on the welcome line.
                if (auto row =
                        players->classRatingFor(*c.playerId, timeControl);
                    row.has_value())
                    return {row->rating, row->ratingDeviation};
            }
            return {1500.0, 350.0};
        };
        const auto [firstRating, firstDeviation] =
            opponentRow(first, first.timeControl);
        const auto [secondRating, secondDeviation] =
            opponentRow(second, second.timeControl);
        // Stamps the resolved pairing figures onto the seats: welcome and
        // later resume resyncs then agree on the same numbers.
        match->seedRatings({firstRating, firstDeviation}, {secondRating, secondDeviation});

        sendTo(first, chess::net::WelcomeMsg{
            chess::Color::White, second.name, secondRating, secondDeviation, rated,
            first.timeControl, match->resumeToken(chess::Color::White),
            first.variant, std::move(startFen), arena});
        // Black's seat must see the same rolled position — a copy, since
        // first's message already consumed the value via std::move above.
        sendTo(second, chess::net::WelcomeMsg{
            chess::Color::Black, first.name, firstRating, firstDeviation, rated,
            first.timeControl, match->resumeToken(chess::Color::Black),
            first.variant, match->startFen(), arena});
        Match* rawMatch = match.get();
        matches.push_back(std::move(match));
        return rawMatch;
    };

    while (!shutdownFlag.load(std::memory_order_relaxed)) {
        bool hasThinkingBot = false;
        bool hasRunningClock = false;
        for (const auto& m : matches) {
            if (!m->isActive())
                continue;
            if (m->isBotTurn())
                hasThinkingBot = true;
            if (m->clockRuns() || m->reconnecting())
                hasRunningClock = true;
        }
        // The loop samples clocks and reconnect grace deadlines on each pass,
        // so shrink the select wait whenever either is running: flags and
        // grace expiries fire within ~100ms.
        auto waitTime = hasThinkingBot ? std::chrono::milliseconds(50)
            : hasRunningClock ? std::chrono::milliseconds(100)
            : std::chrono::seconds(1);
        // The polls run every pass even when the wait expired with no
        // events — a flag may fall (or a grace lapse) for a player simply
        // sitting still.
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        for (auto& match : matches) {
            match->pollClock(nowMs);
            match->pollReconnect(nowMs);
        }

        for (auto& match : matches) {
            if (match->isActive()) {
                match->pollBotMove();
            } else {
                // Finished matches keep the game-over seats (11.7); they
                // linger only for the token-resume retention window with
                // the bot already quiesced.
                match->stopBot();
            }
        }

        // Retention cleanup (11.2/11.7): finished matches age out of the
        // token-resume window; game-over seats still attached (rematch
        // offers unanswered) return to the Connected lobby state quietly —
        // their next rematch attempt answers "no recent game" from there.
        // Runs on every pass, before the select: an idle server must still
        // age out matches on the clock of the sweep itself.
        for (auto it = matches.begin(); it != matches.end();) {
            Match& m = **it;
            if (!m.retentionEnded(nowMs)) {
                ++it;
                continue;
            }
            for (auto& c : clients) {
                if (c->match == &m) {
                    c->state = ClientState::Connected;
                    c->match = nullptr;
                }
            }
            it = matches.erase(it);
        }

        auto botIt = clients.begin();
        while (botIt != clients.end()) {
            Client& c = **botIt;
            // A bot seats a rematch offer only while its finished match is
            // still retained (the human may still ask); match == nullptr
            // after the retention sweep releases it for real.
            const bool botSeatedForRematch =
                c.state == ClientState::GameOver && c.match != nullptr;
            if (c.isBot && c.state != ClientState::InMatch && !botSeatedForRematch) {
                botIt = clients.erase(botIt);
            } else {
                ++botIt;
            }
        }

        if (!selector.wait(waitTime))
            continue;

        if (selector.isReady(listener)) {
            auto newClient = std::make_unique<Client>();
            newClient->socket = std::make_unique<sf::TcpSocket>();
            newClient->lastActivity = std::chrono::steady_clock::now();

            if (listener.accept(*newClient->socket) == sf::Socket::Status::Done) {
                if (clients.size() >= config.maxClients) {
                    LOG_WARN("Max clients reached (" + std::to_string(config.maxClients) + "), rejecting");
                    newClient->socket->disconnect();
                    continue;
                }
                auto addr = newClient->socket->getRemoteAddress();
                LOG_INFO("Client connected from " + (addr.has_value() ? addr->toString() : "unknown"));
                if (!selector.add(*newClient->socket)) {
                    LOG_WARN("Failed to add client to selector");
                    newClient->socket->disconnect();
                    continue;
                }
                clients.push_back(std::move(newClient));
            } else {
                LOG_WARN("Failed to accept connection");
            }
        }

        // Bot spawns requested during the pass below are deferred until the
        // client loop left iteration range: pushing into `clients` while
        // iterating it reallocates the vector and invalidates the loop's
        // own iterator (a fresh bot is the classic trigger — one join, two
        // Clients).
        std::vector<std::pair<std::string, std::string>> botSpawnQueue;
        // ^ {time control, variant} requested by the human the bot pairs
        // with (12.1): the spawn loop already runs post-loop, so the pair
        // must survive that hop.
        for (auto it = clients.begin(); it != clients.end(); ) {
            Client& client = **it;
            if (client.isBot) {
                ++it;
                continue;
            }
            if (selector.isReady(*client.socket)) {
                sf::Packet packet;
                auto recvStatus = client.socket->receive(packet);

                if (recvStatus == sf::Socket::Status::Done) {
                    client.lastActivity = std::chrono::steady_clock::now();

                    if (packet.getDataSize() > MaxPacketSize) {
                        LOG_WARN("Packet too large (" + std::to_string(packet.getDataSize()) + " bytes), disconnecting");
                        it = disconnectClient(it, clients, selector, matchmaker);
                        continue;
                    }

                    auto msg = chess::net::deserializeClient(packet);
                    if (msg.has_value()) {
                        LOG_INFO("Received: " + chess::net::debugString(*msg));

                        // Archive browsing is answered in every state (it
                        // never disturbs queues or running matches). Same
                        // for the leaderboard (10.4), another read-only
                        // store query answered from any connection state.
                        if (auto* archiveReq =
                                std::get_if<chess::net::ArchiveRequestMsg>(&*msg)) {
                            constexpr int DefaultArchiveLimit = 50;
                            // Matches the MaxArchiveEntries wire guard: a
                            // larger clamp would make the server send lists
                            // the client deserializer rejects as hostile.
                            constexpr int MaxArchiveLimit = 100;
                            int limit = archiveReq->limit <= 0
                                ? DefaultArchiveLimit : archiveReq->limit;
                            limit = std::min(limit, MaxArchiveLimit);
                            chess::net::ArchiveListMsg list;
                            if (games != nullptr) {
                                if (archiveReq->mineOnly && client.playerId.has_value())
                                    list.games = toArchiveEntries(
                                        games->latestForPlayer(*client.playerId, limit));
                                else
                                    list.games = toArchiveEntries(games->latest(limit));
                            }
                            sendTo(client, list);
                        }
                        else if (auto* leaderboardReq = std::get_if<
                                     chess::net::LeaderboardRequestMsg>(&*msg)) {
                            constexpr int DefaultLeaderboardLimit = 50;
                            constexpr int MaxLeaderboardLimit = 100;
                            int limit = leaderboardReq->limit <= 0
                                ? DefaultLeaderboardLimit : leaderboardReq->limit;
                            limit = std::min(limit, MaxLeaderboardLimit);
                            if (players == nullptr)
                                sendTo(client, chess::net::ErrorMsg{
                                    "This server has no leaderboard"});
                            else {
                                chess::net::LeaderboardListMsg list;
                                list.players =
                                    toLeaderboardEntries(players->topPlayers(limit));
                                sendTo(client, list);
                            }
                        }
                        else if (auto* archiveGet =
                                std::get_if<chess::net::ArchiveGetMsg>(&*msg)) {
                            if (games == nullptr) {
                                sendTo(client, chess::net::ErrorMsg{
                                    "This server has no game archive"});
                            } else {
                                auto row = games->byId(archiveGet->id);
                                if (!row.has_value()) {
                                    sendTo(client, chess::net::ErrorMsg{
                                        "Game not found in the archive"});
                                } else {
                                    chess::net::ArchiveGameMsg record;
                                    record.id = row->id;
                                    record.whiteName = row->whiteName;
                                    record.blackName = row->blackName;
                                    record.result = row->result;
                                    record.reason = row->reason;
                                    record.timeControl = row->timeControl;
                                    record.startFen = row->startFen;
                                    record.sanMoves = row->sanMoves;
                                    record.playedAt = row->playedAt;
                                    record.rated =
                                        row->whiteId.has_value() && row->blackId.has_value();
                                    sendTo(client, record);
                                }
                            }
                        }
                        else if (auto* arenaJoin =
                                std::get_if<chess::net::TournamentJoinMsg>(&*msg)) {
                            // Arena join (12.3): refused while seated in a
                            // live game (the lobby return happens after it);
                            // answered from every other state — Connected
                            // (new member), Queued (drops the matchmaking
                            // queue on the way in) and GameOver (re-affirm
                            // from the return-to-arena path).
                            if (client.state == ClientState::InMatch) {
                                sendTo(client, chess::net::ErrorMsg{
                                    "Finish the game first"});
                                break;
                            }

                            // Guests carry their name in the payload (same
                            // validation as JoinMsg); accounts keep the
                            // login name and must not be overridable.
                            std::string playerName;
                            bool nameOk = false;
                            if (client.playerId.has_value()) {
                                playerName = client.name;
                                nameOk = true;
                            } else if (arenaJoin->name.empty()) {
                                sendTo(client, chess::net::ErrorMsg{"Name required"});
                            } else if (arenaJoin->name.size() > MaxNameLength) {
                                sendTo(client, chess::net::ErrorMsg{"Name too long"});
                            } else if (players != nullptr
                                       && players->byName(arenaJoin->name).has_value()) {
                                sendTo(client, chess::net::ErrorMsg{
                                    "Name '" + arenaJoin->name + "' belongs to an account — "
                                    "log in with its password or pick another name"});
                            } else {
                                playerName = arenaJoin->name;
                                nameOk = true;
                            }
                            if (!nameOk)
                                break;

                            // On the way out of the matchmaking queue (if
                            // any): the arena owns this connection now.
                            if (client.state == ClientState::Queued) {
                                matchmaker.remove(client);
                                client.state = ClientState::Connected;
                            }

                            client.timeControl =
                                canonicalTimeControl(arenaJoin->timeControl);
                            const std::string variant =
                                chess::net::sanitizeVariant(arenaJoin->variant);
                            const std::string key =
                                client.timeControl + "|" + variant;

                            // Moving over from a different arena? Leave it
                            // quietly first.
                            if (!client.arenaKey.empty() && client.arenaKey != key) {
                                auto oldIt = arenas.find(client.arenaKey);
                                if (oldIt != arenas.end()
                                    && oldIt->second.leave(client.arenaEntrantId))
                                    broadcastArena(oldIt->second);
                                client.arenaKey.clear();
                                client.arenaEntrantId = 0;
                            }

                            auto arenaIt = arenas.find(key);
                            if (arenaIt == arenas.end()) {
                                // Tournament's roundsTarget is fixed at
                                // creation: try_emplace keeps an existing
                                // arena's original quota untouched.
                                arenaIt = arenas.try_emplace(
                                    key, key, config.arenaRounds).first;
                            }
                            Tournament& arena = arenaIt->second;

                            if (client.arenaKey == key
                                && arena.has(client.arenaEntrantId)) {
                                // Re-affirmation (return-to-arena member):
                                // the row survives with its score, no
                                // quota resets, just the fresh snapshot.
                                sendArenaSnapshot(arena, client);
                                break;
                            }

                            std::string joinError;
                            auto rowId = arena.join(playerName, joinError);
                            if (!rowId.has_value()) {
                                sendTo(client, chess::net::ErrorMsg{joinError});
                                break;
                            }
                            client.name = playerName;
                            client.arenaKey = key;
                            client.arenaEntrantId = *rowId;
                            broadcastArena(arena);
                            LOG_INFO("Arena join: " + client.name
                                + " (" + key + ", rows="
                                + std::to_string(arena.entrants().size()) + ")");
                        }
                        else if (std::get_if<chess::net::TournamentLeaveMsg>(&*msg)) {
                            if (client.state == ClientState::InMatch) {
                                sendTo(client, chess::net::ErrorMsg{
                                    "Finish the game first"});
                                break;
                            }
                            if (!client.arenaKey.empty()
                                && client.arenaEntrantId != 0) {
                                auto arenaIt = arenas.find(client.arenaKey);
                                if (arenaIt != arenas.end()
                                    && arenaIt->second.leave(client.arenaEntrantId))
                                    broadcastArena(arenaIt->second);
                            }
                            client.arenaKey.clear();
                            client.arenaEntrantId = 0;
                        }
                        else if (std::get_if<
                                     chess::net::TournamentStandingsRequestMsg>(
                                     &*msg)) {
                            // Snapshot for the requester only (the other
                            // members already have the live table).
                            auto arenaIt = arenas.find(client.arenaKey);
                            if (client.arenaKey.empty()
                                || client.arenaEntrantId == 0
                                || arenaIt == arenas.end()
                                || !arenaIt->second.has(client.arenaEntrantId)) {
                                sendTo(client, chess::net::ErrorMsg{
                                    "This connection is not in an arena"});
                            } else {
                                sendArenaSnapshot(arenaIt->second, client);
                            }
                        }
                        else switch (client.state) {
                        case ClientState::Connected:
                            if (auto* login = std::get_if<chess::net::LoginMsg>(&*msg)) {
                                handleLogin(client, *login, players);
                            } else if (auto* join = std::get_if<chess::net::JoinMsg>(&*msg)) {                                if (!client.playerId.has_value()) {
                                    // Guest join: name comes from the payload.
                                    if (join->name.empty()) {
                                        sendTo(client, chess::net::ErrorMsg{"Name required"});
                                        break;
                                    }
                                    if (join->name.size() > MaxNameLength) {
                                        sendTo(client, chess::net::ErrorMsg{"Name too long"});
                                        break;
                                    }
                                    // An account is gated by its password, but a
                                    // guest name matching one would still put
                                    // impostor chat and archive rows beside that
                                    // player's real rating — reject it.
                                    if (players != nullptr) {
                                        auto account = players->byName(join->name);
                                        if (account.has_value()) {
                                            sendTo(client, chess::net::ErrorMsg{
                                                "Name '" + account->name + "' belongs to an account — "
                                                "log in with its password or pick another name"});
                                            break;
                                        }
                                    }
                                    client.name = join->name;
                                }
                                // Authenticated clients keep the (canonical)
                                // account name set during LoginMsg; allowing
                                // re-joining after a finished game.
                                client.timeControl =
                                    canonicalTimeControl(join->timeControl);
                                // 12.1: the variant tag is sanitized once at
                                // join; pairing and Start-Position rolling
                                // both key off this string.
                                client.variant =
                                    chess::net::sanitizeVariant(join->variant);
                                client.state = ClientState::Queued;
                                auto pair = matchmaker.enqueue(client);
                                if (pair.has_value()) {
                                    startMatch(*pair->first, *pair->second, false);
                                } else if (config.botsEnabled) {
                                    auto botCount = std::count_if(clients.begin(), clients.end(),
                                        [](const auto& c) { return c->isBot; });
                                    if (static_cast<std::size_t>(botCount) < config.maxBots) {
                                        // Deferred: pushing a new bot into
                                        // `clients` here would reallocate the
                                        // vector mid-iteration and invalidate
                                        // this very loop's iterator (the bot
                                        // spawn runs after the loop instead).
                                        botSpawnQueue.emplace_back(
                                            client.timeControl, client.variant);
                                    }
                                }
                            } else if (std::get_if<chess::net::ResumeMsg>(&*msg)) {
                                // Token rejoin (11.2) — checked before the
                                // generic "Join first" fallback below.
                                handleResume(client,
                                    std::get<chess::net::ResumeMsg>(*msg),
                                    matches, players);
                            } else if (std::get_if<chess::net::RematchRequestMsg>(
                                           &*msg)) {
                                // Rematch (11.7) after the retention sweep
                                // reset the game-over seat: "Join first"
                                // would be cryptic.
                                sendTo(client, chess::net::ErrorMsg{
                                    "No game to rematch — session expired"});
                            } else {
                                sendTo(client, chess::net::ErrorMsg{"Join first"});
                            }
                            break;
                        case ClientState::Queued:
                            if (std::get_if<chess::net::PingMsg>(&*msg)) {
                                sendTo(client, chess::net::PongMsg{});
                            } else {
                                sendTo(client, chess::net::ErrorMsg{"Waiting for match"});
                            }
                            break;
                        case ClientState::InMatch:
                        case ClientState::GameOver:
                            // Game-over seats (11.7) stay members of their
                            // finished match for the retention window: the
                            // rematch trio routes through the match, and
                            // Match::handleMessage clears anything else it
                            // refuses.
                            client.match->handleMessage(client, *msg);
                            break;
                        }
                    } else {
                        client.badMessages++;
                        LOG_WARN("Failed to deserialize message (bad " + std::to_string(client.badMessages) + "/" + std::to_string(MaxBadMessages) + ")");
                        if (client.badMessages >= MaxBadMessages) {
                            LOG_WARN("Too many bad messages, disconnecting");
                            it = disconnectClient(it, clients, selector, matchmaker);
                            continue;
                        }
                    }
                } else {
                    it = disconnectClient(it, clients, selector, matchmaker);
                    continue;
                }
            }
            ++it;
        }

        // Spawn the bots this pass requested (post-loop, so the pushes
        // cannot disturb an in-flight iteration). Every queued enqueue
        // resolves against whatever is waiting — its requesting human has
        // already been queued, so the pairing outcome matches the old
        // mid-loop form.
        for (const auto& [botTimeControl, botVariant] : botSpawnQueue) {
            auto botCount = std::count_if(clients.begin(), clients.end(),
                [](const auto& c) { return c->isBot; });
            if (static_cast<std::size_t>(botCount) >= config.maxBots)
                break;
            auto bot = std::make_unique<Client>();
            bot->isBot = true;
            bot->name = "Bot-" + std::to_string(++botNameCounter);
            bot->state = ClientState::Queued;
            // Bots mirror the waiting human's time control (11.1) and game
            // variant (12.1: a Chess960 request pairs a Chess960 bot game).
            bot->timeControl = botTimeControl;
            bot->variant = botVariant;
            bot->lastActivity = std::chrono::steady_clock::now();
            Client* botPtr = bot.get();
            clients.push_back(std::move(bot));

            auto botPair = matchmaker.enqueue(*botPtr);
            if (botPair.has_value()) {
                auto match = startMatch(*botPair->first, *botPair->second, false);
                chess::Color botColor = botPair->first->isBot ? chess::Color::White : chess::Color::Black;
                match->startBot(botColor, config.botDepth, config.botEnginePath);
            }
        }

        // ── Arena pass (12.3) ────────────────────────────────────────────
        // Runs every loop pass: reaps rows without a living connection
        // (disconnect paths don't need special handling), reaps empty
        // arenas, and pairs the idle members of every running arena.
        for (auto arenaIt = arenas.begin(); arenaIt != arenas.end();) {
            Tournament& arena = arenaIt->second;

            // Orphan rows: no connection carries (arenaKey, row id) — a
            // disconnect or a move-over left this one behind. Rows are
            // snapshotted before the removals (leave() erases in place).
            bool reaped = false;
            {
                std::vector<std::int64_t> ids;
                for (const auto& row : arena.entrants())
                    ids.push_back(row.id);
                for (const auto id : ids)
                    if (findArenaClient(clients, arena.key(), id) == nullptr)
                        reaped = arena.leave(id) || reaped;
            }

            if (arena.entrants().empty()) {
                arenaIt = arenas.erase(arenaIt);
                continue;
            }
            if (reaped)
                broadcastArena(arena);

            if (arena.phase() == ArenaPhase::Open) {
                // Idle members: quota unmet, not seated, connection live
                // and not in the middle of anything. A GameOver seat still
                // holding a finished match's retention shell is fair game —
                // its seats detach first (the rematch hook's discipline).
                std::vector<ArenaEntrant> idle;
                for (const auto& row : arena.entrants()) {
                    if (arenaGames(row) >= arena.roundsTarget() || row.inGame)
                        continue;
                    const Client* c =
                        findArenaClient(clients, arena.key(), row.id);
                    if (c == nullptr)
                        continue;
                    if (!isArenaIdle(*c))
                        continue;
                    idle.push_back(row);
                }

                bool seated = false;
                for (const auto& [leftIdx, rightIdx] : arenaPairIdle(idle)) {
                    Client* left = findArenaClient(
                        clients, arena.key(), idle[leftIdx].id);
                    Client* right = findArenaClient(
                        clients, arena.key(), idle[rightIdx].id);
                    if (left == nullptr || right == nullptr || left == right)
                        continue;
                    detachFinishedMatchFor(*left);
                    detachFinishedMatchFor(*right);
                    Match* match = startMatch(*left, *right, true);
                    if (match == nullptr)
                        continue;
                    // Seat the rows for the next pass: both are in game,
                    // and neither may be paired against the other again
                    // immediately.
                    auto markRow = [&](std::int64_t rowId, std::int64_t opponentId) {
                        if (auto* row = arena.entrant(rowId)) {
                            row->inGame = true;
                            row->lastOpponentId = opponentId;
                        }
                    };
                    markRow(idle[leftIdx].id, idle[rightIdx].id);
                    markRow(idle[rightIdx].id, idle[leftIdx].id);
                    seated = true;
                }

                // A pairing broadcast announces the fresh inGame flags —
                // the lobby shows who is playing. Quiet passes (everyone
                // seated or unpairable) don't resend the table.
                if (seated)
                    broadcastArena(arena);
            }

            ++arenaIt;
        }

        if (config.timeout > 0) {
            auto now = std::chrono::steady_clock::now();
            for (auto it = clients.begin(); it != clients.end(); ) {
                Client& client = **it;
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - client.lastActivity).count();
                if (elapsed > config.timeout) {
                    LOG_INFO("Client idle timeout (" + std::to_string(elapsed) + "s), disconnecting");
                    it = disconnectClient(it, clients, selector, matchmaker);
                    continue;
                }
                ++it;
            }
        }
    }

    LOG_INFO("Shutting down with " + std::to_string(clients.size()) + " client(s) connected");
    for (auto& client : clients)
        if (!client->isBot && client->socket)
            client->socket->disconnect();
    clients.clear();
    matches.clear();
    listener.close();
    return boundPort;
}

void stopServer(std::atomic<bool>& shutdownFlag)
{
    shutdownFlag.store(true, std::memory_order_relaxed);
}

} // namespace chess::server
