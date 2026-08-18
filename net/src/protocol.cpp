#include <chess/net/protocol.h>

namespace chess {
namespace net {

namespace {

enum class ClientMsgType : uint8_t {
    Join = 0,
    Move,
    DrawOffer,
    DrawAccept,
    DrawDecline,
    Resign,
    Chat,
    Ping,
};

enum class ServerMsgType : uint8_t {
    Welcome = 0,
    OpponentJoined,
    OpponentLeft,
    Move,
    DrawOffer,
    GameOver,
    Chat,
    Pong,
    Error,
};

} // anonymous namespace

bool serialize(sf::Packet& /*packet*/, const ClientMessage& /*msg*/)
{
    return false; // placeholder
}

bool serialize(sf::Packet& /*packet*/, const ServerMessage& /*msg*/)
{
    return false; // placeholder
}

std::optional<ClientMessage> deserializeClient(sf::Packet& /*packet*/)
{
    return std::nullopt; // placeholder
}

std::optional<ServerMessage> deserializeServer(sf::Packet& /*packet*/)
{
    return std::nullopt; // placeholder
}

std::string debugString(const ClientMessage& /*msg*/)
{
    return ""; // placeholder
}

std::string debugString(const ServerMessage& /*msg*/)
{
    return ""; // placeholder
}

} // namespace net
} // namespace chess
