#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include <SFML/Network/TcpSocket.hpp>

class Match;

enum class ClientState : int { Connected, Queued, InMatch };

struct Client {
    std::unique_ptr<sf::TcpSocket> socket;
    std::string name;
    std::chrono::steady_clock::time_point lastActivity;
    ClientState state = ClientState::Connected;
    Match* match = nullptr;
    int badMessages = 0;
    bool isBot = false;
    // Account id when authenticated via LoginMsg; empty for guests and bots.
    std::optional<std::int64_t> playerId = std::nullopt;
};
