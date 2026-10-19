#include "server.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <memory>
#include <utility>
#include <vector>

#include <SFML/Network/SocketSelector.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>

#include <chess/db/database.h>
#include <chess/db/game_store.h>
#include <chess/db/player_store.h>
#include <chess/net/protocol.h>

#include "client.h"
#include "log.h"
#include "match.h"
#include "matchmaker.h"
#include "send.h"

using namespace std::chrono_literals;

static constexpr std::size_t MaxPacketSize = 4096;
static constexpr int MaxBadMessages = 3;
static constexpr std::size_t MaxNameLength = 32;

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
    else if (client.state == ClientState::InMatch)
        client.match->handleDisconnect(client);
    return clients.erase(it);
}

namespace chess::server {

// ── Auth (phase 10.2) ───────────────────────────────────────────────────────

static void sendProfileTo(Client& client, const chess::db::Player& player)
{
    sendTo(client, chess::net::ProfileMsg{
        player.name, player.rating, player.ratingDeviation, false});
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

    auto existing = players->byName(login.name);
    if (existing.has_value()) {
        if (login.password.empty()) {
            sendTo(client, chess::net::ErrorMsg{
                "Account '" + existing->name + "' is password protected — "
                "enter the password to log in, or pick another name for guest play"});
            return;
        }
        if (!players->authenticate(login.name, login.password)) {
            sendTo(client, chess::net::ErrorMsg{
                "Wrong password for account '" + existing->name + "'"});
            return;
        }
        client.playerId = existing->id;
        client.name = existing->name;
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
        sendTo(client, chess::net::ErrorMsg{players->errorMessage()});
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

    // Rated update on game over (phase 10.2): both participants must be
    // registered; guests and bots keep unrated games. Runs after the
    // GameOverMsg broadcast, inside the currently alive client objects.
    auto onGameOver = [&](const Match& match, chess::net::GameResult result,
                          chess::net::GameOverReason reason) {
        Client* white = match.white();
        Client* black = match.black();

        // Archive (10.3): any game with at least one accepted move is
        // persisted — rated or not, guests get no players-row linkage.
        if (games != nullptr && white != nullptr && black != nullptr
            && !match.sanHistory().empty()) {
            chess::db::Game row;
            row.whiteId = white->playerId;
            row.blackId = black->playerId;
            row.whiteName = white->name;
            row.blackName = black->name;
            row.result = static_cast<int>(result);
            row.reason = static_cast<int>(reason);
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

        // Rated update (phase 10.2): both participants must be registered;
        // guests and bots keep unrated games. Runs after the GameOverMsg
        // broadcast, inside the currently alive client objects.
        if (players == nullptr || white == nullptr || black == nullptr
            || !white->playerId.has_value() || !black->playerId.has_value())
            return;
        auto update = players->applyRatedGame(*white->playerId, *black->playerId, result);
        if (!update.has_value()) {
            LOG_WARN("Rated update failed: " + players->errorMessage());
            return;
        }
        LOG_INFO("Rated " + update->white.name + " -> "
            + std::to_string(static_cast<int>(update->white.rating))
            + " / " + update->black.name + " -> "
            + std::to_string(static_cast<int>(update->black.rating)));
        sendProfileTo(*white, update->white);
        sendProfileTo(*black, update->black);
    };

    // Pairing helper (phase 10.2): creates the match, wires the rated hook
    // and broadcasts the richer WelcomeMsg (opponent rating + rated flag)
    // made from fresh rows; guests fall back to the default 1500 ± 350.
    // Defined after the match container declarations below.

    sf::SocketSelector selector;
    selector.add(listener);

    ClientVec clients;
    MatchVec matches;
    Matchmaker matchmaker;
    std::size_t botNameCounter = 0;

    // Pairing helper (phase 10.2): creates the match, wires the rated hook
    // and broadcasts the richer WelcomeMsg (opponent rating + rated flag)
    // made from fresh rows; guests fall back to the default 1500 ± 350.
    auto startMatch = [&](Client& first, Client& second) -> Match* {
        first.state = ClientState::InMatch;
        second.state = ClientState::InMatch;
        auto match = std::make_unique<Match>(first, second);
        match->setGameOverCallback(onGameOver);
        first.match = match.get();
        second.match = match.get();

        const bool rated = players != nullptr
            && first.playerId.has_value() && second.playerId.has_value();
        auto opponentRow = [&](const Client& c) -> std::pair<double, double> {
            if (players != nullptr && c.playerId.has_value()) {
                auto row = players->byId(*c.playerId);
                if (row.has_value())
                    return {row->rating, row->ratingDeviation};
            }
            return {1500.0, 350.0};
        };
        const auto [firstRating, firstDeviation] = opponentRow(first);
        const auto [secondRating, secondDeviation] = opponentRow(second);

        sendTo(first, chess::net::WelcomeMsg{
            chess::Color::White, second.name, secondRating, secondDeviation, rated});
        sendTo(second, chess::net::WelcomeMsg{
            chess::Color::Black, first.name, firstRating, firstDeviation, rated});
        Match* rawMatch = match.get();
        matches.push_back(std::move(match));
        return rawMatch;
    };

    while (!shutdownFlag.load(std::memory_order_relaxed)) {
        bool hasThinkingBot = false;
        for (const auto& m : matches) {
            if (m->isActive() && m->isBotTurn()) {
                hasThinkingBot = true;
                break;
            }
        }
        auto waitTime = hasThinkingBot ? std::chrono::milliseconds(50) : std::chrono::seconds(1);
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
                        else switch (client.state) {
                        case ClientState::Connected:
                            if (auto* login = std::get_if<chess::net::LoginMsg>(&*msg)) {
                                handleLogin(client, *login, players);
                            } else if (auto* join = std::get_if<chess::net::JoinMsg>(&*msg)) {
                                if (!client.playerId.has_value()) {
                                    // Guest join: name comes from the payload.
                                    if (join->name.empty()) {
                                        sendTo(client, chess::net::ErrorMsg{"Name required"});
                                        break;
                                    }
                                    if (join->name.size() > MaxNameLength) {
                                        sendTo(client, chess::net::ErrorMsg{"Name too long"});
                                        break;
                                    }
                                    client.name = join->name;
                                }
                                // Authenticated clients keep the (canonical)
                                // account name set during LoginMsg; allowing
                                // re-joining after a finished game.
                                client.state = ClientState::Queued;
                                auto pair = matchmaker.enqueue(client);
                                if (pair.has_value()) {
                                    startMatch(*pair->first, *pair->second);
                                } else if (config.botsEnabled) {
                                    auto botCount = std::count_if(clients.begin(), clients.end(),
                                        [](const auto& c) { return c->isBot; });
                                    if (static_cast<std::size_t>(botCount) < config.maxBots) {
                                        auto bot = std::make_unique<Client>();
                                        bot->isBot = true;
                                        bot->name = "Bot-" + std::to_string(++botNameCounter);
                                        bot->state = ClientState::Queued;
                                        bot->lastActivity = std::chrono::steady_clock::now();
                                        Client* botPtr = bot.get();
                                        clients.push_back(std::move(bot));

                                        auto botPair = matchmaker.enqueue(*botPtr);
                                        if (botPair.has_value()) {
                                            auto match = startMatch(*botPair->first, *botPair->second);
                                            chess::Color botColor = botPair->first->isBot ? chess::Color::White : chess::Color::Black;
                                            match->startBot(botColor, config.botDepth, config.botEnginePath);
                                        }
                                    }
                                }
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

        for (auto& match : matches) {
            if (match->isActive()) {
                match->pollBotMove();
            } else {
                if (match->white() && !match->white()->isBot) { match->white()->state = ClientState::Connected; match->white()->match = nullptr; }
                if (match->black() && !match->black()->isBot) { match->black()->state = ClientState::Connected; match->black()->match = nullptr; }
                if (match->white() && match->white()->isBot) { match->white()->state = ClientState::Connected; match->white()->match = nullptr; }
                if (match->black() && match->black()->isBot) { match->black()->state = ClientState::Connected; match->black()->match = nullptr; }
                match->stopBot();
            }
        }

        auto botIt = clients.begin();
        while (botIt != clients.end()) {
            Client& c = **botIt;
            if (c.isBot && c.state != ClientState::InMatch) {
                botIt = clients.erase(botIt);
            } else {
                ++botIt;
            }
        }

        matches.erase(
            std::remove_if(matches.begin(), matches.end(),
                [](const auto& m) { return !m->isActive(); }),
            matches.end());
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
