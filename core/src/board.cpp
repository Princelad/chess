#include <chess/board.h>

namespace chess {

Board Board::fromStartPos()
{
    Board board;

    const PieceType backRank[8] = {
        PieceType::Rook, PieceType::Knight, PieceType::Bishop,
        PieceType::Queen, PieceType::King,
        PieceType::Bishop, PieceType::Knight, PieceType::Rook,
    };

    for (int file = 0; file < 8; ++file) {
        board.setPiece(squareOf(file, 0), Piece::of(Color::White, backRank[file]));
        board.setPiece(squareOf(file, 1), Piece::of(Color::White, PieceType::Pawn));
        board.setPiece(squareOf(file, 6), Piece::of(Color::Black, PieceType::Pawn));
        board.setPiece(squareOf(file, 7), Piece::of(Color::Black, backRank[file]));
    }

    board.setCastlingRights(AllCastling);
    return board;
}

std::string squareToString(Square sq)
{
    const char file = "abcdefgh"[sq & 7];
    const char rank = '1' + ((sq >> 4) & 7);
    return {file, rank};
}

Square stringToSquare(std::string_view name)
{
    if (name.size() != 2) {
        return SquareNone;
    }
    const char file = name[0];
    const char rank = name[1];
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
        return SquareNone;
    }
    return squareOf(file - 'a', rank - '1');
}

} // namespace chess
