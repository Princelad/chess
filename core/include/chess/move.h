#pragma once

#include <ostream>
#include <string>

#include <chess/board.h>

namespace chess {

enum MoveFlag : int {
    Quiet      = 0,
    Capture    = 1 << 0,
    DoublePush = 1 << 1,
    Castle     = 1 << 2,
    EnPassant  = 1 << 3,
    Promotion  = 1 << 4,
};

struct Move {
    Square from = SquareNone;
    Square to = SquareNone;
    int flags = Quiet;
    PieceType promotion = PieceType::None;

    constexpr Move() = default;
    constexpr Move(Square from_, Square to_, int flags_ = Quiet) noexcept
        : from(from_), to(to_), flags(flags_) {}

    constexpr bool isNone() const noexcept { return from == SquareNone; }
    constexpr bool isCapture() const noexcept { return (flags & Capture) != 0; }
    constexpr bool isDoublePush() const noexcept { return (flags & DoublePush) != 0; }
    constexpr bool isCastle() const noexcept { return (flags & Castle) != 0; }
    constexpr bool isEnPassant() const noexcept { return (flags & EnPassant) != 0; }
    constexpr bool isPromotion() const noexcept { return (flags & Promotion) != 0; }

    constexpr bool operator==(const Move& other) const noexcept
    {
        return from == other.from && to == other.to && flags == other.flags
            && promotion == other.promotion;
    }
    constexpr bool operator!=(const Move& other) const noexcept
    {
        return !(*this == other);
    }
};

constexpr Move move(Square from, Square to) noexcept { return {from, to}; }
constexpr Move captureMove(Square from, Square to) noexcept { return {from, to, Capture}; }
constexpr Move doublePushMove(Square from, Square to) noexcept { return {from, to, DoublePush}; }
constexpr Move castleMove(Square from, Square to) noexcept { return {from, to, Castle}; }
constexpr Move enPassantMove(Square from, Square to) noexcept
{
    return {from, to, EnPassant | Capture};
}

constexpr Move promotionMove(Square from, Square to, PieceType piece,
                             bool isCapture = false) noexcept
{
    int flags = Promotion | (isCapture ? Capture : 0);
    Move m{from, to, flags};
    m.promotion = piece;
    return m;
}

inline std::string toUci(const Move& m)
{
    std::string result = squareToString(m.from) + squareToString(m.to);
    if (m.isPromotion()) {
        switch (m.promotion) {
            case PieceType::Knight: result += 'n'; break;
            case PieceType::Bishop: result += 'b'; break;
            case PieceType::Rook:   result += 'r'; break;
            case PieceType::Queen:  result += 'q'; break;
            default: break;
        }
    }
    return result;
}

inline std::ostream& operator<<(std::ostream& out, const Move& m)
{
    out << toUci(m);
    if (m.isCapture()) out << "+cap";
    if (m.isDoublePush()) out << "+dp";
    if (m.isCastle()) out << "+castle";
    if (m.isEnPassant()) out << "+ep";
    if (m.isPromotion()) out << "+promo";
    return out;
}

} // namespace chess
