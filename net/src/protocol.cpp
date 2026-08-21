#include <chess/net/protocol.h>

#include <type_traits>

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
    DrawDecline,
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
           << static_cast<int>(msg.color) << msg.opponent;
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

bool writeServer(sf::Packet& packet, ServerDrawDeclineMsg)
{
    packet << static_cast<uint8_t>(ServerMsgType::DrawDecline);
    return static_cast<bool>(packet);
}

bool writeServer(sf::Packet& packet, const GameOverMsg& msg)
{
    packet << static_cast<uint8_t>(ServerMsgType::GameOver)
           << static_cast<int>(msg.result) << static_cast<int>(msg.reason);
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

bool validColor(int v)
{
    return v == 0 || v == 1;
}

bool validResult(int v)
{
    return v >= 0 && v <= static_cast<int>(GameResult::Abort);
}

bool validReason(int v)
{
    return v >= 0 && v <= static_cast<int>(GameOverReason::AgreedDraw);
}

const char* resultStr(GameResult r)
{
    switch (r) {
        case GameResult::WhiteWins:    return "WhiteWins";
        case GameResult::BlackWins:    return "BlackWins";
        case GameResult::Draw:         return "Draw";
        case GameResult::Resignation:  return "Resignation";
        case GameResult::Abort:        return "Abort";
        default:                       return "Unknown";
    }
}

const char* reasonStr(GameOverReason r)
{
    switch (r) {
        case GameOverReason::Checkmate:           return "Checkmate";
        case GameOverReason::Stalemate:           return "Stalemate";
        case GameOverReason::FiftyMove:           return "FiftyMove";
        case GameOverReason::Repetition:          return "Repetition";
        case GameOverReason::InsufficientMaterial: return "InsufficientMaterial";
        case GameOverReason::Resignation:         return "Resignation";
        case GameOverReason::Disconnection:       return "Disconnection";
        case GameOverReason::Abort:               return "Abort";
        case GameOverReason::AgreedDraw:           return "AgreedDraw";
        default:                                  return "Unknown";
    }
}

const char* colorStr(Color c)
{
    switch (c) {
        case Color::White: return "White";
        case Color::Black: return "Black";
        default:           return "Unknown";
    }
}

std::string escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\':  out += "\\\\"; break;
            case '"':   out += "\\\""; break;
            case '\n':  out += "\\n"; break;
            case '\t':  out += "\\t"; break;
            default:    out += c; break;
        }
    }
    return out;
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
            int color = 0;
            packet >> color >> msg.opponent;
            if (!static_cast<bool>(packet)) return std::nullopt;
            if (!validColor(color)) return std::nullopt;
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
        case ServerMsgType::DrawDecline:
            return ServerDrawDeclineMsg{};
        case ServerMsgType::GameOver: {
            GameOverMsg msg;
            int result = 0, reason = 0;
            packet >> result >> reason;
            if (!static_cast<bool>(packet)) return std::nullopt;
            if (!validResult(result) || !validReason(reason)) return std::nullopt;
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

std::string debugString(const ClientMessage& msg)
{
    return std::visit([](auto&& m) -> std::string {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, JoinMsg>)
            return "Join{name=\"" + escape(m.name) + "\"}";
        else if constexpr (std::is_same_v<T, MoveMsg>)
            return "Move{san=\"" + escape(m.san) + "\"}";
        else if constexpr (std::is_same_v<T, DrawOfferMsg>)
            return "DrawOffer{}";
        else if constexpr (std::is_same_v<T, DrawAcceptMsg>)
            return "DrawAccept{}";
        else if constexpr (std::is_same_v<T, DrawDeclineMsg>)
            return "DrawDecline{}";
        else if constexpr (std::is_same_v<T, ResignMsg>)
            return "Resign{}";
        else if constexpr (std::is_same_v<T, ChatMsg>)
            return "Chat{text=\"" + escape(m.text) + "\"}";
        else if constexpr (std::is_same_v<T, PingMsg>)
            return "Ping{}";
    }, msg);
}

std::string debugString(const ServerMessage& msg)
{
    return std::visit([](auto&& m) -> std::string {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, WelcomeMsg>)
            return "Welcome{color=" + std::string(colorStr(m.color)) + ", opponent=\"" + escape(m.opponent) + "\"}";
        else if constexpr (std::is_same_v<T, OpponentJoinedMsg>)
            return "OpponentJoined{name=\"" + escape(m.name) + "\"}";
        else if constexpr (std::is_same_v<T, OpponentLeftMsg>)
            return "OpponentLeft{}";
        else if constexpr (std::is_same_v<T, ServerMoveMsg>)
            return "Move{san=\"" + escape(m.san) + "\"}";
        else if constexpr (std::is_same_v<T, ServerDrawOfferMsg>)
            return "DrawOffer{}";
        else if constexpr (std::is_same_v<T, ServerDrawDeclineMsg>)
            return "DrawDecline{}";
        else if constexpr (std::is_same_v<T, GameOverMsg>)
            return "GameOver{" + std::string(resultStr(m.result)) + ", " + std::string(reasonStr(m.reason)) + "}";
        else if constexpr (std::is_same_v<T, ServerChatMsg>)
            return "Chat{name=\"" + escape(m.name) + "\", text=\"" + escape(m.text) + "\"}";
        else if constexpr (std::is_same_v<T, PongMsg>)
            return "Pong{}";
        else if constexpr (std::is_same_v<T, ErrorMsg>)
            return "Error{message=\"" + escape(m.message) + "\"}";
    }, msg);
}

} // namespace net
} // namespace chess
