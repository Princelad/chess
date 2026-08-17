#include <chess/san.h>
#include <chess/movegen.h>

#include <cctype>

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

std::optional<Move> fromSan(const Board& board, const std::string& san)
{
    if (san.empty()) return std::nullopt;

    std::string s = san;

    // Strip trailing +/#
    while (!s.empty() && (s.back() == '+' || s.back() == '#')) {
        s.pop_back();
    }

    // Parse promotion
    PieceType promoType = PieceType::None;
    if (s.size() >= 2 && s[s.size() - 2] == '=') {
        char pc = s.back();
        switch (pc) {
            case 'Q': promoType = PieceType::Queen;  break;
            case 'R': promoType = PieceType::Rook;   break;
            case 'B': promoType = PieceType::Bishop; break;
            case 'N': promoType = PieceType::Knight; break;
            default: return std::nullopt;
        }
        s.pop_back(); // piece letter
        s.pop_back(); // '='
    }

    if (s.size() < 2) return std::nullopt;

    // Castling (check before destination parsing since O-O has no square suffix)
    if (s == "O-O" || s == "O-O-O") {
        Color side = board.sideToMove();
        Square from = squareOf(File::E, side == Color::White ? Rank::R1 : Rank::R8);
        Square to = (s == "O-O")
            ? squareOf(File::G, side == Color::White ? Rank::R1 : Rank::R8)
            : squareOf(File::C, side == Color::White ? Rank::R1 : Rank::R8);
        return castleMove(from, to);
    }

    // Destination square = last 2 chars
    std::string destStr = s.substr(s.size() - 2);
    int destFile = destStr[0] - 'a';
    int destRank = destStr[1] - '1';
    if (destFile < 0 || destFile > 7 || destRank < 0 || destRank > 7)
        return std::nullopt;
    Square dest = squareOf(destFile, destRank);

    std::string prefix = s.substr(0, s.size() - 2);

    // Determine piece type and disambiguation
    PieceType pieceType = PieceType::Pawn;
    int disambigFile = -1;  // -1 = not specified
    int disambigRank = -1;
    bool isCapture = false;

    if (!prefix.empty()) {
        char first = prefix[0];
        if (std::isupper(static_cast<unsigned char>(first))) {
            // Piece letter
            switch (first) {
                case 'K': pieceType = PieceType::King;   break;
                case 'Q': pieceType = PieceType::Queen;  break;
                case 'R': pieceType = PieceType::Rook;   break;
                case 'B': pieceType = PieceType::Bishop; break;
                case 'N': pieceType = PieceType::Knight; break;
                default: return std::nullopt;
            }
            prefix = prefix.substr(1);
        } else if (std::islower(static_cast<unsigned char>(first))) {
            // Pawn capture: file letter + 'x'
            if (prefix.size() >= 2 && prefix[1] == 'x') {
                isCapture = true;
                disambigFile = prefix[0] - 'a';
                prefix = prefix.substr(2);
            } else {
                // Shouldn't happen in valid SAN but handle gracefully
                return std::nullopt;
            }
        } else if (first == 'x') {
            isCapture = true;
            prefix = prefix.substr(1);
        }
    }

    // Parse remaining disambiguation
    for (char c : prefix) {
        if (c == 'x') {
            isCapture = true;
        } else if (c >= 'a' && c <= 'h') {
            disambigFile = c - 'a';
        } else if (c >= '1' && c <= '8') {
            disambigRank = c - '1';
        } else {
            return std::nullopt;
        }
    }

    // Generate legal moves and find matching
    auto legal = generateLegalMoves(board);
    std::optional<Move> result;
    int matchCount = 0;

    for (const auto& m : legal) {
        // Destination
        if (m.to != dest) continue;

        // Piece type
        Piece piece = board.pieceAt(m.from);
        if (piece.type != pieceType) continue;

        // Disambiguation
        int fromFile = m.from & 7;
        int fromRank = (m.from >> 4) & 7;
        if (disambigFile >= 0 && fromFile != disambigFile) continue;
        if (disambigRank >= 0 && fromRank != disambigRank) continue;

        // Capture
        if (isCapture && !m.isCapture()) continue;
        if (!isCapture && m.isCapture()) continue;

        // Promotion
        if (promoType != PieceType::None) {
            if (!m.isPromotion() || m.promotion != promoType) continue;
        } else {
            if (m.isPromotion()) continue;
        }

        result = m;
        ++matchCount;
        if (matchCount > 1) return std::nullopt;  // ambiguous
    }

    return result;
}

} // namespace san
} // namespace chess
