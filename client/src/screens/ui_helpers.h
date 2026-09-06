#pragma once

#include <chess/board.h>
#include <chess/types.h>
#include <SFML/Graphics.hpp>
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
    std::size_t n = maxBytes;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
    return s.substr(0, n > 0 ? n - 1 : 0) + "...";
}

} // namespace chess::client
