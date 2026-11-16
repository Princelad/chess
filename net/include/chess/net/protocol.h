#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <SFML/Network/Packet.hpp>

#include <chess/net/messages.h>

namespace chess {
namespace net {

constexpr uint8_t ProtocolVersion = 2;

// The resumed game's SAN-list guard (11.2), shared with the server: the
// wire refuses a count past this, and the server enforces the same
// maximum on its own sanHistory (a move accepted past the cap ends the
// game as a draw), so a genuine reply can never trip the wire guard.
constexpr int32_t MaxResumeMoves = 512;

// ── Serialization ───────────────────────────────────────────────────────────

bool serialize(sf::Packet& packet, const ClientMessage& msg);
bool serialize(sf::Packet& packet, const ServerMessage& msg);

// ── Deserialization ─────────────────────────────────────────────────────────

std::optional<ClientMessage> deserializeClient(sf::Packet& packet);
std::optional<ServerMessage> deserializeServer(sf::Packet& packet);

// ── Debug ───────────────────────────────────────────────────────────────────

std::string debugString(const ClientMessage& msg);
std::string debugString(const ServerMessage& msg);

} // namespace net
} // namespace chess
