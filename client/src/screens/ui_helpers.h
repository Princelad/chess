#pragma once

#include <chess/board.h>
#include <chess/types.h>
#include <SFML/Graphics.hpp>
#include <optional>
#include <string>

namespace chess::client {

inline std::pair<int, int> findKingSquare(const Board& board, Color color)
{
    Piece king = Piece::of(color, PieceType::King);
    for (int file = 0; file < 8; ++file)
        for (int rank = 0; rank < 8; ++rank)
            if (board.pieceAt(squareOf(file, rank)) == king)
                return { file, rank };
    return { 4, color == Color::White ? 0 : 7 };
}

inline std::string safeTruncate(const std::string& s, std::size_t maxBytes)
{
    if (s.size() <= maxBytes) return s;
    // Back off while the first EXCLUDED byte is a UTF-8 continuation, so
    // the kept prefix ends on a whole sequence boundary. No unconditional
    // extra trim: a clean ASCII cut keeps all maxBytes bytes.
    std::size_t n = maxBytes;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
    return s.substr(0, n) + "...";
}

// Audit 5.3: wrap a token-separated chain (the history detail panel's SAN
// record) into display lines. The detail panel cannot scroll and its
// Label draws one sf::Text, so the old single-line setText ran a long
// game's whole chain (a 450-ply game is ~17,000 px) far off the window.
// Lines break on the existing spaces (never mid-token) and hold at most
// wrapChars bytes; past maxLines lines the rest is dropped for a note —
// the Copy button still carries the full record.
inline std::string wrapSanChain(const std::string& text,
                                std::size_t wrapChars,
                                std::size_t maxLines)
{
    std::string out;
    std::size_t lineLen = 0;
    std::size_t lines = 1;
    std::size_t i = 0;
    bool truncated = false;
    while (i < text.size()) {
        std::size_t end = text.find(' ', i);
        if (end == std::string::npos) end = text.size();
        const std::size_t tokenLen = end - i;
        const std::size_t sep = (lineLen > 0) ? 1u : 0u;
        if (lineLen + sep + tokenLen > wrapChars && lineLen > 0) {
            if (lines >= maxLines) {
                truncated = true;
                break;
            }
            out += '\n';
            ++lines;
            lineLen = 0;
        }
        if (lineLen > 0) {
            out += ' ';
            ++lineLen;
        }
        out.append(text, i, tokenLen);
        lineLen += tokenLen;
        i = end + 1;
    }
    if (truncated)
        out += "\n... long game - use Copy for the full record";
    return out;
}

// Audit 5.4: strict port parse. The old call sites did
// static_cast<unsigned short>(std::stoi(text)) inside a catch-all, which
// caught exceptions but still silently WRAPPED out-of-range text
// ("99999" dialed 44639, "-1" dialed 65535) — a typo'd port connected to
// a random wrong port and the failure read like a server problem.
// Digits only, 1..65535, no exceptions at all.
inline std::optional<unsigned short> parsePort(const std::string& text)
{
    if (text.empty() || text.size() > 5) return std::nullopt;
    unsigned value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return std::nullopt;
        value = value * 10 + static_cast<unsigned>(c - '0');
    }
    if (value < 1 || value > 65535) return std::nullopt;
    return static_cast<unsigned short>(value);
}

} // namespace chess::client
