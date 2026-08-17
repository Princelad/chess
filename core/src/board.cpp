#include <chess/board.h>
#include <chess/move.h>

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

void Board::makeMove(const Move& m)
{
    pushSnapshot();

    const Color side = m_state.sideToMove;
    const Piece movingPiece = pieceAt(m.from);

    // --- Move piece(s) based on flags ---

    if (m.isCastle()) {
        // Move king
        setPiece(m.to, movingPiece);
        clearSquare(m.from);

        // Move rook — derive from king destination file
        const int destFile = m.to & 7;
        const int rankBase = m.to & 0xF0;
        if (destFile == 6) {
            // King-side: rook h→f
            setPiece(rankBase + 5, pieceAt(rankBase + 7));
            clearSquare(rankBase + 7);
        } else {
            // Queen-side: rook a→d
            setPiece(rankBase + 3, pieceAt(rankBase + 0));
            clearSquare(rankBase + 0);
        }
    } else if (m.isEnPassant()) {
        // Move pawn to target, remove captured pawn (same rank as from, same file as to)
        setPiece(m.to, movingPiece);
        clearSquare(m.from);
        clearSquare(squareOf(m.to & 7, (m.from >> 4) & 7));
    } else if (m.isPromotion()) {
        // Move pawn to target, replace with promoted piece
        setPiece(m.to, Piece(side, m.promotion));
        clearSquare(m.from);
    } else {
        // Quiet or capture — just move the piece
        setPiece(m.to, movingPiece);
        clearSquare(m.from);
    }

    // --- Update castling rights ---

    int newRights = m_state.castlingRights;

    // King moved
    if (movingPiece.type == PieceType::King) {
        if (side == Color::White) {
            newRights &= ~(WhiteKingSide | WhiteQueenSide);
        } else {
            newRights &= ~(BlackKingSide | BlackQueenSide);
        }
    }

    // Rook moved from home square
    if (movingPiece.type == PieceType::Rook) {
        if (m.from == squareOf(7, 0))  newRights &= ~WhiteKingSide;
        if (m.from == squareOf(0, 0))  newRights &= ~WhiteQueenSide;
        if (m.from == squareOf(7, 7))  newRights &= ~BlackKingSide;
        if (m.from == squareOf(0, 7))  newRights &= ~BlackQueenSide;
    }

    // Capture on rook home square
    if (m.isCapture() && !m.isEnPassant()) {
        if (m.to == squareOf(7, 0))  newRights &= ~WhiteKingSide;
        if (m.to == squareOf(0, 0))  newRights &= ~WhiteQueenSide;
        if (m.to == squareOf(7, 7))  newRights &= ~BlackKingSide;
        if (m.to == squareOf(0, 7))  newRights &= ~BlackQueenSide;
    }

    m_state.castlingRights = newRights;

    // --- En passant square ---

    if (m.isDoublePush()) {
        const int pushDir = (side == Color::White) ? 1 : -1;
        m_state.enPassant = squareOf(m.from & 7, ((m.from >> 4) & 7) + pushDir);
    } else {
        m_state.enPassant = SquareNone;
    }

    // --- Clocks ---

    if (movingPiece.type == PieceType::Pawn || m.isCapture()) {
        m_state.halfmoveClock = 0;
    } else {
        ++m_state.halfmoveClock;
    }

    if (side == Color::Black) {
        ++m_state.fullmoveNumber;
    }

    // --- Side to move ---

    m_state.sideToMove = opposite(side);
}

void Board::undoMove(const Move& /*m*/)
{
    popSnapshot();
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

bool Board::isRepetition(int count) const
{
    int matches = 1; // current position counts as one occurrence
    for (std::size_t i = 0; i < m_history.size(); ++i) {
        const auto& snap = m_history[i];
        if (m_board != snap.pieces) continue;
        if (m_state.sideToMove != snap.state.sideToMove) continue;
        if (m_state.castlingRights != snap.state.castlingRights) continue;
        if (m_state.enPassant != snap.state.enPassant) continue;
        ++matches;
        if (matches >= count) return true;
    }
    return false;
}

} // namespace chess
