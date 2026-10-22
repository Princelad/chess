#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include <SFML/Network/TcpSocket.hpp>

class Match;

// GameOver (11.7): the client finished a match but the match object still
// lives in retention — rematch offers route through it while both seats
// can still answer; JoinMsg/ResumeMsg (fresh connections) are unaffected.
enum class ClientState : int { Connected, Queued, InMatch, GameOver };

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
    // Time-control string agreed at JoinMsg ("5+3" canonical, or empty =
    // unlimited). Pairing matches only equal strings (11.1).
    std::string timeControl;
    // Login-throttle bookkeeping (R2): consecutive LoginMsg failures (wrong
    // password, rejected registration) back off exponentially — armed from
    // the second failure, 1 s doubling capped at 60 s — until the
    // connection succeeds or is dropped. Reset on a successful login.
    int failedLogins = 0;
    std::chrono::steady_clock::time_point loginLockedUntil{};
};
