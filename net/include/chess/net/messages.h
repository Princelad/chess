#pragma once

#include <cstdint>
#include <string>
#include <string_view>
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
    Timeout, // flag fall (11.1); appended last for protocol v2 compatibility
};

// Time control in the wire form ("5+3"). Empty = the unlimited game,
// exactly what every pre-clock message still produces.
struct TimeControl {
    std::int64_t baseSeconds = 0;
    std::int64_t incrementSeconds = 0;
    bool unlimited = true;
};

// Parses the JoinMsg/WelcomeMsg form: "base+increment" (minutes). Anything
// malformed — empty, no '+', non-digit parts — is the unlimited game,
// never trust the payload.
TimeControl parseTimeControl(std::string_view tc);

// Game-variant tag carried by JoinMsg/WelcomeMsg (12.1). Exactly
// "chess960" selects the Chess960 Start Position game; anything else —
// including a hostile or future-unknown string — is the standard game,
// never trust the payload.
constexpr const char* Chess960Variant = "chess960";
std::string sanitizeVariant(std::string_view variant);

// ── Client → Server ─────────────────────────────────────────────────────────

// The time control requested at pairing ("5+3" style, or empty for the
// legacy unlimited game) plus the game variant (sanitizeVariant form).
// Pairing only matches clients equal on both strings.
struct JoinMsg {
    std::string name;
    std::string timeControl;
    std::string variant;  // empty = standard (12.1; trailing, append-only)
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

// Token-based rejoin (11.2): sent on a fresh connection to reclaim one's
// seat in a live game after a mid-game drop. The token arrives per seat in
// WelcomeMsg; anything else unlocks nothing.
struct ResumeMsg {
    std::string token;
};

// Takeback (11.3): a player asks to retract the last full move (their own
// ply plus the opponent's reply), returning the board to the position where
// they went on move. Heard only while the requester is exactly in that
// frame (on the move, >= 2 plies, everyone still seated); the asked seat
// answers accept/decline — a server bot answers itself. A pending request
// dissolves on the next accepted move, a decline (or the requester's own
// self-cancel), a disconnect, or the game ending.
struct TakebackRequestMsg {};
struct TakebackAcceptMsg {};
struct TakebackDeclineMsg {};

// Rematch (11.7): a post-game offer to play the same pairing again with
// colors swapped, on the connection that just finished. Heard only while
// the finished match still sits in its retention window and the opponent's
// seat is present; the asked seat answers accept/decline — a server bot
// answers itself. There is no separate "accepted" server message: the
// fresh WelcomeMsg of the newly created match is the acceptance signal for
// both seats. A pending offer dissolves on a decline (or the requester's
// own self-cancel), either seat's departure, or the retention window
// lapsing.
struct RematchRequestMsg {};
struct RematchAcceptMsg {};
struct RematchDeclineMsg {};

// Arena (12.3): join the tournament lobby for one (timeControl, variant)
// combination. The name rides along only for guest connections (the same
// validation JoinMsg applies server-side); logged-in players leave it
// empty — the server keeps the account name. Joining an arena already
// joined by this connection is an idempotent re-affirmation; joining a
// different arena silently moves this connection over.
struct TournamentJoinMsg {
    std::string name;
    std::string timeControl;
    std::string variant;
};

// Stop pairing me in the arena: the row (and its scores) drops and no
// further pairing includes this connection. Refused while a game is still
// being played — finish it first.
struct TournamentLeaveMsg {};

// Standings refresh for the arena the connection belongs to. Answered in
// any connection state, like the archive/leaderboard reads; a connection
// without arena membership gets an ErrorMsg.
struct TournamentStandingsRequestMsg {};

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
    LeaderboardRequestMsg,
    ResumeMsg,
    TakebackRequestMsg,
    TakebackAcceptMsg,
    TakebackDeclineMsg,
    RematchRequestMsg,
    RematchAcceptMsg,
    RematchDeclineMsg,
    TournamentJoinMsg,
    TournamentLeaveMsg,
    TournamentStandingsRequestMsg
>;

// ── Server → Client ─────────────────────────────────────────────────────────

struct WelcomeMsg {
    Color color;
    std::string opponent;
    double rating = 1500.0;   // opponent's pre-game rating
    double deviation = 350.0;
    bool rated = false;       // both players registered and the game counts
    std::string timeControl;  // "5+3" or empty = unlimited (11.1)
    // Per-seat rejoin credential (11.2): a mid-game drop is recovered on a
    // fresh connection by sending it back in ResumeMsg.
    std::string resumeToken;
    // Game variant (12.1, sanitizeVariant form) and the Chess960 start
    // position the server rolled at pairing; both empty = standard start.
    // Trailing appended fields — legacy-shaped packets fail to deserialize
    // as intended.
    std::string variant;
    std::string startFen;
    // Arena game (12.3): the pairing came from a tournament lobby rather
    // than the matchmaking queue. Arena games never rate; the client's
    // game-over screen swaps its rematch controls for "return to arena".
    // Trailing appended — legacy-shaped packets fail to deserialize.
    bool arena = false;
};

struct OpponentJoinedMsg {
    std::string name;
};

struct OpponentLeftMsg {};

struct ServerMoveMsg {
    std::string san;
    // Remaining time of the player now on the move, in milliseconds.
    // 0 = no clock (unlimited game). 11.1: synced with every move broadcast.
    std::int32_t remainingMs = 0;
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
};// One row of the game archive: enough for a list, not the moves themselves.
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

// Full resync after a token rejoin (11.2): the seat is re-bound and the
// client rebuilds the board by replaying `sans` (chaining san::fromSan from
// `fen`, always the standard start position today). The clock figures are
// both sides' remaining time sampled at the resync (0 = no clock).
struct ResumeStateMsg {
    Color color = Color::White;
    std::string opponent;
    double rating = 1500.0;
    double deviation = 350.0;
    bool rated = false;
    std::string timeControl;
    std::string fen;
    std::vector<std::string> sans;
    std::int32_t whiteRemainingMs = 0;
    std::int32_t blackRemainingMs = 0;
};

// The opponent's connection dropped mid-game; the match lives for this many
// seconds awaiting their token rejoin (11.2). The seat's clock keeps running
// meanwhile — a flag fall inside the grace window still loses on time.
struct OpponentReconnectingMsg {
    std::int32_t graceSeconds = 0;
};

// The rejoining seat is bound again: clear the reconnecting notice and play on.
struct OpponentResumedMsg {};

// ── Takeback (11.3) ─────────────────────────────────────────────────────────

// Forwarded to the seat that must agree; empty, the interesting payload is
// the answer below.
struct ServerTakebackRequestMsg {};

// A takeback went through: `sans` is the full remaining move list (the two
// retracted plies are gone). Both clients rebuild by replaying the list
// from the start position — the exact reconstruction used for token
// resumes — and the requester ends up on the move. Clocks are not refunded:
// the time the two plies spent stays spent, and the re-established turn
// simply continues running from where it was.
struct TakebackAcceptedMsg {
    std::vector<std::string> sans;
};

// Sent to the requester only: the asked seat declined, or the requester
// self-cancelled their own pending request.
struct ServerTakebackDeclineMsg {};

// ── Rematch (11.7) ─────────────────────────────────────────────────────────

// Forwarded to the seat that must agree — the other player of the game that
// just ended, still seated on its game-over screen. The fresh WelcomeMsg of
// the rematched game (same time control, swapped colors) is the acceptance
// signal; no separate "accepted" message exists.
struct ServerRematchRequestMsg {};

// Sent to the requester only: the asked seat declined outright (withdrawn
// = false), or the offer was cancelled — the requester themself left or
// dropped (withdrawn = true, the self-cancel path).
struct ServerRematchDeclineMsg {
    bool withdrawn = false;
};

// ── Arena (12.3) ────────────────────────────────────────────────────────────

// One row of the arena standings. Points run W=2 / D=1 / L=0; rows are
// ordered points-DESC, then name ASC (server-side sort, deterministic).
// `self` marks the recipient's own row; `playing` marks a row whose player
// is seated in a live game right now.
struct TournamentRowMsg {
    std::string name;
    int points = 0;
    int wins = 0;
    int losses = 0;
    int draws = 0;
    bool self = false;
    bool playing = false;
};

// Full standings snapshot: broadcast to every arena member after joins,
// leaves and finished games, and answered single-copy to
// TournamentStandingsRequestMsg. phase: 0 = running, 1 = final (the fixed
// rounds-per-player quota is met; the lobby shows the end table).
struct TournamentStateMsg {
    int phase = 0;
    int roundsTarget = 0;
    std::vector<TournamentRowMsg> rows;
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
    LeaderboardListMsg,
    ResumeStateMsg,
    OpponentReconnectingMsg,
    OpponentResumedMsg,
    ServerTakebackRequestMsg,
    TakebackAcceptedMsg,
    ServerTakebackDeclineMsg,
    ServerRematchRequestMsg,
    ServerRematchDeclineMsg,
    TournamentStateMsg
>;

} // namespace net
} // namespace chess
