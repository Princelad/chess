#pragma once

#include <chess/net/messages.h>
#include <chess/pgn.h>

#include <ctime>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace chess::client {

// ── Shared archive formatting helpers (10.3) ────────────────────────────────

// PGN result token for the games-table result value.
inline const char* archiveResultToken(int result)
{
    switch (static_cast<chess::net::GameResult>(result)) {
        case chess::net::GameResult::WhiteWins: return "1-0";
        case chess::net::GameResult::BlackWins: return "0-1";
        case chess::net::GameResult::Draw:      return "1/2-1/2";
        default:                                return "*";
    }
}

// Human-readable result word for list rows ("White wins", ...).
inline const char* archiveResultText(int result)
{
    switch (static_cast<chess::net::GameResult>(result)) {
        case chess::net::GameResult::WhiteWins: return "White wins";
        case chess::net::GameResult::BlackWins: return "Black wins";
        case chess::net::GameResult::Draw:      return "Draw";
        case chess::net::GameResult::Resignation: return "Resigned";
        default:                                return "Aborted";
    }
}

// PGN headers for one archived row + the SAN list split for the consumer.
// Uses the 8.4 PGN writer for the actual move-text formatting.
struct ArchivePgnInput {
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<std::string> sans;
};

inline ArchivePgnInput archivePgnParts(const chess::net::ArchiveGameMsg& game)
{
    ArchivePgnInput parts;
    parts.headers.push_back({"Event",
        game.rated ? "Rated game" : "Casual game"});
    parts.headers.push_back({"Site", "Chess"});
    parts.headers.push_back({"White", game.whiteName});
    parts.headers.push_back({"Black", game.blackName});
    parts.headers.push_back({"Result", archiveResultToken(game.result)});
    if (!game.timeControl.empty())
        parts.headers.push_back({"TimeControl", game.timeControl});
    if (!game.startFen.empty()) {
        parts.headers.push_back({"SetUp", "1"});
        parts.headers.push_back({"FEN", game.startFen});
    }
    std::string san;
    for (char c : game.sanMoves) {
        if (c != ' ')
            san += c;
        else if (!san.empty()) {
            parts.sans.push_back(std::move(san));
            san.clear();
        }
    }
    if (!san.empty())
        parts.sans.push_back(std::move(san));
    return parts;
}

// The date header format required by PGN: "yyyy.mm.dd" (UTC for determinism).
inline std::string archiveDateHeader(std::int64_t playedAt)
{
    const std::time_t t = static_cast<std::time_t>(playedAt);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[16] = {};
    std::snprintf(buf, sizeof(buf), "%04d.%02d.%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return buf;
}

// Full PGN for one archived game (Callers pass the UTC date via the header).
inline std::string gameToPgn(const chess::net::ArchiveGameMsg& game,
                             const std::string& dateHeader)
{
    auto parts = archivePgnParts(game);
    auto& headers = parts.headers;
    headers.insert(headers.begin(),
        {"Date", dateHeader.empty() ? "????.??.??" : dateHeader});
    return toPgn(std::move(headers), parts.sans,
                 archiveResultToken(game.result), game.startFen);
}

// Convenience overload: date derived from the row's own timestamp (UTC).
inline std::string gameToPgn(const chess::net::ArchiveGameMsg& game)
{
    return gameToPgn(game, archiveDateHeader(game.playedAt));
}

} // namespace chess::client
