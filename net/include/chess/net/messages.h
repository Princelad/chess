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

// Password login / transparent registration. Guests never send it.
struct LoginMsg {
    std::string name;
    std::string password;
};

using ClientMessage = std::variant<
    JoinMsg,
    MoveMsg,
    DrawOfferMsg,
    DrawAcceptMsg,
    DrawDeclineMsg,
    ResignMsg,
    ChatMsg,
    PingMsg,
    LoginMsg
>;

// ── Server → Client ─────────────────────────────────────────────────────────

struct WelcomeMsg {
    Color color;
    std::string opponent;
    double rating = 1500.0;  // opponent's pre-game rating
    double deviation = 350.0;
    bool rated = false;      // both players registered and the game counts
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

// Identity/own rating; sent after login and re-sent whenever a rated game
// changes the account's rating.
struct ProfileMsg {
    std::string name;
    double rating = 1500.0;
    double deviation = 350.0;
    bool isGuest = true;
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
    ErrorMsg,
    ProfileMsg
>;

} // namespace net
} // namespace chess
