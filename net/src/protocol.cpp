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

std::optional<ClientMessage> deserializeClient(sf::Packet& packet)
{
    uint8_t version = 0;
    packet >> version;
    if (!static_cast<bool>(packet) || version != ProtocolVersion)
        return std::nullopt;

    uint8_t tag = 0;
    packet >> tag;
    if (!static_cast<bool>(packet))
        return std::nullopt;

    switch (static_cast<ClientMsgType>(tag)) {
        case ClientMsgType::Join: {
            JoinMsg msg;
            packet >> msg.name;
            if (!static_cast<bool>(packet)) return std::nullopt;
            return msg;
        }
        case ClientMsgType::Move: {
            MoveMsg msg;
            packet >> msg.san;
            if (!static_cast<bool>(packet)) return std::nullopt;
            return msg;
        }
        case ClientMsgType::DrawOffer:
            return DrawOfferMsg{};
        case ClientMsgType::DrawAccept:
            return DrawAcceptMsg{};
        case ClientMsgType::DrawDecline:
            return DrawDeclineMsg{};
        case ClientMsgType::Resign:
            return ResignMsg{};
        case ClientMsgType::Chat: {
            ChatMsg msg;
            packet >> msg.text;
            if (!static_cast<bool>(packet)) return std::nullopt;
            return msg;
        }
        case ClientMsgType::Ping:
            return PingMsg{};
        default:
            return std::nullopt;
    }
}

std::optional<ServerMessage> deserializeServer(sf::Packet& packet)
{
    uint8_t version = 0;
    packet >> version;
    if (!static_cast<bool>(packet) || version != ProtocolVersion)
        return std::nullopt;

    uint8_t tag = 0;
    packet >> tag;
    if (!static_cast<bool>(packet))
        return std::nullopt;

    switch (static_cast<ServerMsgType>(tag)) {
        case ServerMsgType::Welcome: {
            WelcomeMsg msg;
            int32_t color = 0;
            packet >> color >> msg.opponent;
            if (!static_cast<bool>(packet)) return std::nullopt;
            msg.color = static_cast<Color>(color);
            return msg;
        }
        case ServerMsgType::OpponentJoined: {
            OpponentJoinedMsg msg;
            packet >> msg.name;
            if (!static_cast<bool>(packet)) return std::nullopt;
            return msg;
        }
        case ServerMsgType::OpponentLeft:
            return OpponentLeftMsg{};
        case ServerMsgType::Move: {
            ServerMoveMsg msg;
            packet >> msg.san;
            if (!static_cast<bool>(packet)) return std::nullopt;
            return msg;
        }
        case ServerMsgType::DrawOffer:
            return ServerDrawOfferMsg{};
        case ServerMsgType::GameOver: {
            GameOverMsg msg;
            int32_t result = 0, reason = 0;
            packet >> result >> reason;
            if (!static_cast<bool>(packet)) return std::nullopt;
            msg.result = static_cast<GameResult>(result);
            msg.reason = static_cast<GameOverReason>(reason);
            return msg;
        }
        case ServerMsgType::Chat: {
            ServerChatMsg msg;
            packet >> msg.name >> msg.text;
            if (!static_cast<bool>(packet)) return std::nullopt;
            return msg;
        }
        case ServerMsgType::Pong:
            return PongMsg{};
        case ServerMsgType::Error: {
            ErrorMsg msg;
            packet >> msg.message;
            if (!static_cast<bool>(packet)) return std::nullopt;
            return msg;
        }
        default:
            return std::nullopt;
    }
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
