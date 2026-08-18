#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <SFML/Network/Packet.hpp>

#include <chess/net/messages.h>

namespace chess {
namespace net {

constexpr uint8_t ProtocolVersion = 1;

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
