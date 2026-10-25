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
    // Game-variant tag agreed at JoinMsg (sanitizeVariant form, 12.1:
    // empty = standard, "chess960"). Pairing matches only equal values; a
    // Chess960 match rolls a random Start Position at pairing.
    std::string variant;
    // Login-throttle bookkeeping (R2): consecutive LoginMsg failures (wrong
    // password, rejected registration) back off exponentially — armed from
    // the second failure, 1 s doubling capped at 60 s — until the
    // connection succeeds or is dropped. Reset on a successful login.
    int failedLogins = 0;
    std::chrono::steady_clock::time_point loginLockedUntil{};
    // Arena membership (12.3): which lobby this connection belongs to and
    // its entrant-row id there. Both empty/cleared on leave/disconnect;
    // `state` stays in its normal values (arena members idle in Connected).
    std::string arenaKey;
    std::int64_t arenaEntrantId = 0;
    // Spectator membership (12.4): the live match this connection is
    // watching (nullptr = not watching). The connection keeps its
    // Connected state while spectating — it owns no seat and answers play
    // verbs with the ordinary in-match errors. Cleared on SpectateStop,
    // disconnect, and by the watched match itself when it ends.
    Match* spectating = nullptr;
};
