#pragma once

namespace chess {

enum class Color : int {
    White = 0,
    Black = 1,
    None  = 2,
};

constexpr Color opposite(Color color) noexcept
{
    switch (color) {
        case Color::White: return Color::Black;
        case Color::Black: return Color::White;
        case Color::None:  return Color::None;
    }
    return Color::None;
}

enum class PieceType : int {
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King,
};

struct Piece {
    Color color = Color::None;
    PieceType type = PieceType::Pawn;

    constexpr Piece() = default;
    constexpr Piece(Color c, PieceType t) noexcept : color(c), type(t) {}

    static constexpr Piece None() noexcept { return {}; }
    static constexpr Piece of(Color c, PieceType t) noexcept { return {c, t}; }

    constexpr bool isNone() const noexcept { return color == Color::None; }

    constexpr bool operator==(const Piece& other) const noexcept
    {
        return color == other.color && type == other.type;
    }
    constexpr bool operator!=(const Piece& other) const noexcept
    {
        return !(*this == other);
    }
};

enum class File : int { A = 0, B, C, D, E, F, G, H };
enum class Rank : int { R1 = 0, R2, R3, R4, R5, R6, R7, R8 };

using Square = int;

constexpr Square SquareNone = -1;

constexpr Square squareOf(File file, Rank rank) noexcept
{
    return static_cast<int>(rank) * 16 + static_cast<int>(file);
}

constexpr Square squareOf(int file, int rank) noexcept
{
    return rank * 16 + file;
}

constexpr File fileOf(Square sq) noexcept { return static_cast<File>(sq & 7); }
constexpr Rank rankOf(Square sq) noexcept { return static_cast<Rank>((sq >> 4) & 7); }

} // namespace chess
