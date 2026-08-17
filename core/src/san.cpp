#include <chess/san.h>
#include <chess/movegen.h>

namespace chess {
namespace san {

static const char* pieceLetter(PieceType type)
{
    switch (type) {
        case PieceType::Knight: return "N";
        case PieceType::Bishop: return "B";
        case PieceType::Rook:   return "R";
        case PieceType::Queen:  return "Q";
        case PieceType::King:   return "K";
        default: return "";
    }
}

static const char* promoLetter(PieceType type)
{
    switch (type) {
        case PieceType::Knight: return "N";
        case PieceType::Bishop: return "B";
        case PieceType::Rook:   return "R";
        case PieceType::Queen:  return "Q";
        default: return "";
    }
}

std::string toSan(const Board& board, const Move& m)
{
    if (m.isCastle()) {
        const int destFile = m.to & 7;
        std::string san = (destFile == 6) ? "O-O" : "O-O-O";
        Board b = board;
        b.makeMove(m);
        if (inCheck(b, opposite(board.sideToMove()))) {
            auto legal = generateLegalMoves(b);
            san += legal.empty() ? '#' : '+';
        }
        return san;
    }

    const Piece piece = board.pieceAt(m.from);
    const bool isCapture = m.isCapture();
    const bool isPromo = m.isPromotion();

    std::string san;

    // Piece letter (pawns omit this)
    if (piece.type != PieceType::Pawn) {
        san += pieceLetter(piece.type);
    }

    // Disambiguation for non-pawn, non-king pieces
    if (piece.type != PieceType::Pawn && piece.type != PieceType::King) {
        auto legal = generateLegalMoves(board);
        bool needFile = false;
        bool needRank = false;

        for (const auto& lm : legal) {
            if (lm.to != m.to) continue;
            if (lm.from == m.from) continue;
            const Piece p = board.pieceAt(lm.from);
            if (p.type != piece.type) continue;

            int otherFile = lm.from & 7;
            int myFile = m.from & 7;
            int myRank = (m.from >> 4) & 7;

            if (otherFile == myFile) {
                needRank = true;
            } else {
                needFile = true;
            }
        }

        if (needFile || needRank) {
            if (needFile) {
                san += 'a' + (m.from & 7);
            }
            if (needRank) {
                san += '1' + ((m.from >> 4) & 7);
            }
        }
    }

    // Capture marker
    if (isCapture) {
        if (piece.type == PieceType::Pawn) {
            san += 'a' + (m.from & 7);
        }
        san += 'x';
    }

    // Destination square
    san += squareToString(m.to);

    // Promotion
    if (isPromo) {
        san += '=';
        san += promoLetter(m.promotion);
    }

    // Check / checkmate
    Board b = board;
    b.makeMove(m);
    if (inCheck(b, opposite(board.sideToMove()))) {
        auto legal = generateLegalMoves(b);
        san += legal.empty() ? '#' : '+';
    }

    return san;
}

} // namespace san
} // namespace chess
