#include <chess/net/protocol.h>

#include <variant>

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

bool writeClient(sf::Packet& packet, const JoinMsg& msg)
{
    packet << static_cast<uint8_t>(ClientMsgType::Join) << msg.name;
    return static_cast<bool>(packet);
}

bool writeClient(sf::Packet& packet, const MoveMsg& msg)
{
    packet << static_cast<uint8_t>(ClientMsgType::Move) << msg.san;
    return static_cast<bool>(packet);
}

bool writeClient(sf::Packet& packet, DrawOfferMsg)
{
    packet << static_cast<uint8_t>(ClientMsgType::DrawOffer);
    return static_cast<bool>(packet);
}

bool writeClient(sf::Packet& packet, DrawAcceptMsg)
{
    packet << static_cast<uint8_t>(ClientMsgType::DrawAccept);
    return static_cast<bool>(packet);
}

bool writeClient(sf::Packet& packet, DrawDeclineMsg)
{
    packet << static_cast<uint8_t>(ClientMsgType::DrawDecline);
    return static_cast<bool>(packet);
}

bool writeClient(sf::Packet& packet, ResignMsg)
{
    packet << static_cast<uint8_t>(ClientMsgType::Resign);
    return static_cast<bool>(packet);
}

bool writeClient(sf::Packet& packet, const ChatMsg& msg)
{
    packet << static_cast<uint8_t>(ClientMsgType::Chat) << msg.text;
    return static_cast<bool>(packet);
}

bool writeClient(sf::Packet& packet, PingMsg)
{
    packet << static_cast<uint8_t>(ClientMsgType::Ping);
    return static_cast<bool>(packet);
}

bool writeServer(sf::Packet& packet, const WelcomeMsg& msg)
{
    packet << static_cast<uint8_t>(ServerMsgType::Welcome)
           << static_cast<int32_t>(msg.color) << msg.opponent;
    return static_cast<bool>(packet);
}

bool writeServer(sf::Packet& packet, const OpponentJoinedMsg& msg)
{
    packet << static_cast<uint8_t>(ServerMsgType::OpponentJoined) << msg.name;
    return static_cast<bool>(packet);
}

bool writeServer(sf::Packet& packet, OpponentLeftMsg)
{
    packet << static_cast<uint8_t>(ServerMsgType::OpponentLeft);
    return static_cast<bool>(packet);
}

bool writeServer(sf::Packet& packet, const ServerMoveMsg& msg)
{
    packet << static_cast<uint8_t>(ServerMsgType::Move) << msg.san;
    return static_cast<bool>(packet);
}

bool writeServer(sf::Packet& packet, ServerDrawOfferMsg)
{
    packet << static_cast<uint8_t>(ServerMsgType::DrawOffer);
    return static_cast<bool>(packet);
}

bool writeServer(sf::Packet& packet, const GameOverMsg& msg)
{
    packet << static_cast<uint8_t>(ServerMsgType::GameOver)
           << static_cast<int32_t>(msg.result) << static_cast<int32_t>(msg.reason);
    return static_cast<bool>(packet);
}

bool writeServer(sf::Packet& packet, const ServerChatMsg& msg)
{
    packet << static_cast<uint8_t>(ServerMsgType::Chat) << msg.name << msg.text;
    return static_cast<bool>(packet);
}

bool writeServer(sf::Packet& packet, PongMsg)
{
    packet << static_cast<uint8_t>(ServerMsgType::Pong);
    return static_cast<bool>(packet);
}

bool writeServer(sf::Packet& packet, const ErrorMsg& msg)
{
    packet << static_cast<uint8_t>(ServerMsgType::Error) << msg.message;
    return static_cast<bool>(packet);
}

} // anonymous namespace

bool serialize(sf::Packet& packet, const ClientMessage& msg)
{
    packet << ProtocolVersion;
    return std::visit([&](auto&& m) { return writeClient(packet, m); }, msg);
}

bool serialize(sf::Packet& packet, const ServerMessage& msg)
{
    packet << ProtocolVersion;
    return std::visit([&](auto&& m) { return writeServer(packet, m); }, msg);
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
