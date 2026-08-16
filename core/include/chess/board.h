#pragma once

#include <array>
#include <string>
#include <string_view>

#include <chess/types.h>

namespace chess {

constexpr int BoardSize = 128;

constexpr bool offBoard(Square sq) noexcept
{
    return (sq & 0x88) != 0;
}

class Board {
public:
    Piece pieceAt(Square sq) const { return m_board[sq]; }
    void setPiece(Square sq, Piece piece) { m_board[sq] = piece; }
    void clearSquare(Square sq) { m_board[sq] = Piece::None(); }
    bool isEmpty(Square sq) const { return m_board[sq].isNone(); }

private:
    std::array<Piece, BoardSize> m_board{};
};

std::string squareToString(Square sq);
Square stringToSquare(std::string_view name);

} // namespace chess
