#include "server.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>

#include <SFML/Network/SocketSelector.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>

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

unsigned short runServer(const ServerConfig& config, std::atomic<bool>& shutdownFlag,
                         unsigned short* boundPortOut)
{
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

    sf::SocketSelector selector;
    selector.add(listener);

    ClientVec clients;
    MatchVec matches;
    Matchmaker matchmaker;
    std::size_t botNameCounter = 0;

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

                        switch (client.state) {
                        case ClientState::Connected:
                            if (auto* join = std::get_if<chess::net::JoinMsg>(&*msg)) {
                                if (!client.name.empty()) {
                                    sendTo(client, chess::net::ErrorMsg{"Already joined"});
                                    break;
                                }
                                if (join->name.empty()) {
                                    sendTo(client, chess::net::ErrorMsg{"Name required"});
                                    break;
                                }
                                if (join->name.size() > MaxNameLength) {
                                    sendTo(client, chess::net::ErrorMsg{"Name too long"});
                                    break;
                                }
                                client.name = join->name;
                                client.state = ClientState::Queued;
                                auto pair = matchmaker.enqueue(client);
                                if (pair.has_value()) {
                                    pair->first->state = ClientState::InMatch;
                                    pair->second->state = ClientState::InMatch;
                                    auto match = std::make_unique<Match>(*pair->first, *pair->second);
                                    pair->first->match = match.get();
                                    pair->second->match = match.get();
                                    sendTo(*pair->first, chess::net::WelcomeMsg{chess::Color::White, pair->second->name});
                                    sendTo(*pair->second, chess::net::WelcomeMsg{chess::Color::Black, pair->first->name});
                                    matches.push_back(std::move(match));
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
                                            botPair->first->state = ClientState::InMatch;
                                            botPair->second->state = ClientState::InMatch;
                                            auto match = std::make_unique<Match>(*botPair->first, *botPair->second);
                                            botPair->first->match = match.get();
                                            botPair->second->match = match.get();
                                            sendTo(*botPair->first, chess::net::WelcomeMsg{chess::Color::White, botPair->second->name});
                                            sendTo(*botPair->second, chess::net::WelcomeMsg{chess::Color::Black, botPair->first->name});

                                            chess::Color botColor = botPair->first->isBot ? chess::Color::White : chess::Color::Black;
                                            match->startBot(botColor, config.botDepth, config.botEnginePath);
                                            matches.push_back(std::move(match));
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
