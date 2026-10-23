#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <chess/types.h>

namespace chess {

struct Move;  // forward declaration — defined in move.h

constexpr int BoardSize = 128;

constexpr bool offBoard(Square sq) noexcept
{
    return (sq & 0x88) != 0;
}

enum CastlingRights : int {
    NoCastling     = 0,
    WhiteKingSide  = 1,
    WhiteQueenSide = 2,
    BlackKingSide  = 4,
    BlackQueenSide = 8,
    AllCastling    = WhiteKingSide | WhiteQueenSide | BlackKingSide | BlackQueenSide,
};

constexpr bool canCastle(int rights, CastlingRights side) noexcept
{
    return (rights & static_cast<int>(side)) != 0;
}

// Index of a castling right inside State::castlingRooksFrom.
// Order follows the bit values: WhiteKs, WhiteQs, BlackKs, BlackQs.
constexpr int castlingRightIndex(CastlingRights right) noexcept
{
    switch (right) {
        case WhiteKingSide:  return 0;
        case WhiteQueenSide: return 1;
        case BlackKingSide:  return 2;
        case BlackQueenSide: return 3;
    }
    return 0;
}

// Back-rank file of a granted-but-not-yet-traditionally-placed rook to support
// Chess960 castling (rooks start on arbitrary back-rank files; the king still
// finishes on the g/c-file and the rook on the f/d-file, per FRC rules).
struct State {
    Color sideToMove = Color::White;
    int castlingRights = NoCastling;
    // Origin square (per color) of the rook owning each castling right. Only meaningful
    // for granted rights; defaults mirror the standard geometry (h1/a1/h8/a8).
    std::array<Square, 4> castlingRooksFrom = {
        squareOf(7, 0),
        squareOf(0, 0),
        squareOf(7, 7),
        squareOf(0, 7),
    };
    Square enPassant = SquareNone;
    int halfmoveClock = 0;
    int fullmoveNumber = 1;
};

struct Snapshot {
    std::array<Piece, BoardSize> pieces{};
    State state;
};

class Board {
public:
    static Board fromStartPos();
    static std::optional<Board> fromFen(std::string_view fen);

    std::string toFen() const;

    void makeMove(const Move& m);
    void undoMove(const Move& m);

    Piece pieceAt(Square sq) const { return m_board[sq]; }
    void setPiece(Square sq, Piece piece) { m_board[sq] = piece; }
    void clearSquare(Square sq) { m_board[sq] = Piece::None(); }
    bool isEmpty(Square sq) const { return m_board[sq].isNone(); }

    Color sideToMove() const { return m_state.sideToMove; }
    int castlingRights() const { return m_state.castlingRights; }
    Square enPassantSquare() const { return m_state.enPassant; }
    int halfmoveClock() const { return m_state.halfmoveClock; }
    int fullmoveNumber() const { return m_state.fullmoveNumber; }

    const State& state() const { return m_state; }

    // Origin square of the rook that owns `right` (valid for granted rights).
    Square castlingRookSquare(CastlingRights right) const
    {
        return m_state.castlingRooksFrom[castlingRightIndex(right)];
    }
    void setCastlingRookSquare(CastlingRights right, Square sq)
    {
        m_state.castlingRooksFrom[castlingRightIndex(right)] = sq;
    }

    void setSideToMove(Color color) { m_state.sideToMove = color; }
    void setCastlingRights(int rights) { m_state.castlingRights = rights; }
    void setEnPassantSquare(Square sq) { m_state.enPassant = sq; }
    void setHalfmoveClock(int n) { m_state.halfmoveClock = n; }
    void setFullmoveNumber(int n) { m_state.fullmoveNumber = n; }

    std::size_t snapshotCount() const { return m_history.size(); }
    const Snapshot& snapshotAt(std::size_t index) const { return m_history[index]; }

    bool isRepetition(int count) const;

    void pushSnapshot()
    {
        Snapshot snap;
        snap.pieces = m_board;
        snap.state = m_state;
        m_history.push_back(snap);
    }

    void popSnapshot()
    {
        m_board = m_history.back().pieces;
        m_state = m_history.back().state;
        m_history.pop_back();
    }

private:
    std::array<Piece, BoardSize> m_board{};
    State m_state;
    std::vector<Snapshot> m_history;
};

std::string squareToString(Square sq);
Square stringToSquare(std::string_view name);

} // namespace chess
