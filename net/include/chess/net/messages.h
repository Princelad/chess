#pragma once

#include <string>
#include <variant>

#include <chess/types.h>

namespace chess {
namespace net {

enum class GameResult {
    WhiteWins,
    BlackWins,
    Draw,
    Resignation,
    Abort,
};

enum class GameOverReason {
    Checkmate,
    Stalemate,
    FiftyMove,
    Repetition,
    InsufficientMaterial,
    Resignation,
    Disconnection,
    Abort,
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

} // namespace net
} // namespace chess
