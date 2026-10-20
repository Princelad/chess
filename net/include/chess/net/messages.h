#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

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

// Game-archive request: the latest completed games, optionally restricted to
// one's own account (mineOnly applies to logged-in players; a guest gets the
// global list server-side). limit 0 = server default; values above 100 clamp
// (the server's clamp mirrors the MaxArchiveEntries wire guard).
struct ArchiveRequestMsg {
    int limit = 50;
    bool mineOnly = false;
};

// Fetch one completed game by archive id; answered with ArchiveGameMsg or
// an ErrorMsg when the id is unknown.
struct ArchiveGetMsg {
    std::int64_t id = 0;
};

// Leaderboard request (10.4): top-N registered accounts by rating. limit 0 or
// negative = server default; the server clamps to its wire maximum. Answered
// in any connection state, like the archive messages.
struct LeaderboardRequestMsg {
    int limit = 50;
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
    LoginMsg,
    ArchiveRequestMsg,
    ArchiveGetMsg,
    LeaderboardRequestMsg
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

// One row of the game archive: enough for a list, not the moves themselves.
struct ArchiveEntryMsg {
    std::int64_t id = 0;
    std::string whiteName;
    std::string blackName;
    int result = 0;      // chess::net::GameResult, range-checked on read
    std::int64_t playedAt = 0;
    bool rated = false;
};

// The list requested with ArchiveRequestMsg. Entries are ordered newest first.
struct ArchiveListMsg {
    std::vector<ArchiveEntryMsg> games;
};

// A single archived game with everything needed for replay/PGN export
// (the SAN list is the move source; reconstruction chains san::fromSan).
struct ArchiveGameMsg {
    std::int64_t id = 0;
    std::string whiteName;
    std::string blackName;
    int result = 0;         // chess::net::GameResult, range-checked on read
    int reason = 0;         // chess::net::GameOverReason, range-checked on read
    std::string timeControl;
    std::string startFen;
    std::string sanMoves;
    std::int64_t playedAt = 0;
    bool rated = false;
};

// One row of the leaderboard (10.4): a registered account as stored in the
// players table. Rank is the entry's position in LeaderboardListMsg.players
// (highest rating first). Only accounts with finished rated games appear.
struct LeaderboardEntryMsg {
    std::string name;
    double rating = 1500.0;
    double deviation = 350.0;
    int wins = 0;
    int losses = 0;
    int draws = 0;
};

// The list requested with LeaderboardRequestMsg, highest rating first.
struct LeaderboardListMsg {
    std::vector<LeaderboardEntryMsg> players;
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
    ProfileMsg,
    ArchiveListMsg,
    ArchiveGameMsg,
    LeaderboardListMsg
>;

} // namespace net
} // namespace chess
