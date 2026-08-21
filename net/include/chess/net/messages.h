#pragma once

#include <string>
#include <variant>

#include <chess/types.h>

namespace chess {
namespace net {

enum class GameResult : int {
    WhiteWins,
    BlackWins,
    Draw,
    Resignation,
    Abort,
};

enum class GameOverReason : int {
    Checkmate,
    Stalemate,
    FiftyMove,
    Repetition,
    InsufficientMaterial,
    Resignation,
    Disconnection,
    Abort,
    AgreedDraw,
};

// ── Client → Server ─────────────────────────────────────────────────────────

struct JoinMsg {
    std::string name;
};

struct MoveMsg {
    std::string san;
};

struct DrawOfferMsg {};
struct DrawAcceptMsg {};
struct DrawDeclineMsg {};
struct ResignMsg {};

struct ChatMsg {
    std::string text;
};

struct PingMsg {};

using ClientMessage = std::variant<
    JoinMsg,
    MoveMsg,
    DrawOfferMsg,
    DrawAcceptMsg,
    DrawDeclineMsg,
    ResignMsg,
    ChatMsg,
    PingMsg
>;

// ── Server → Client ─────────────────────────────────────────────────────────

struct WelcomeMsg {
    Color color;
    std::string opponent;
};

struct OpponentJoinedMsg {
    std::string name;
};

struct OpponentLeftMsg {};

struct ServerMoveMsg {
    std::string san;
};

struct ServerDrawOfferMsg {};
struct ServerDrawDeclineMsg {};

struct GameOverMsg {
    GameResult result;
    GameOverReason reason;
};

struct ServerChatMsg {
    std::string name;
    std::string text;
};

struct PongMsg {};

struct ErrorMsg {
    std::string message;
};

using ServerMessage = std::variant<
    WelcomeMsg,
    OpponentJoinedMsg,
    OpponentLeftMsg,
    ServerMoveMsg,
    ServerDrawOfferMsg,
    ServerDrawDeclineMsg,
    GameOverMsg,
    ServerChatMsg,
    PongMsg,
    ErrorMsg
>;

} // namespace net
} // namespace chess
