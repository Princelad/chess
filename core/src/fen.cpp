#include <chess/fen.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace chess {

namespace {

std::optional<Piece> pieceFromChar(char ch)
{
    switch (ch) {
        case 'P': return Piece::of(Color::White, PieceType::Pawn);
        case 'N': return Piece::of(Color::White, PieceType::Knight);
        case 'B': return Piece::of(Color::White, PieceType::Bishop);
        case 'R': return Piece::of(Color::White, PieceType::Rook);
        case 'Q': return Piece::of(Color::White, PieceType::Queen);
        case 'K': return Piece::of(Color::White, PieceType::King);
        case 'p': return Piece::of(Color::Black, PieceType::Pawn);
        case 'n': return Piece::of(Color::Black, PieceType::Knight);
        case 'b': return Piece::of(Color::Black, PieceType::Bishop);
        case 'r': return Piece::of(Color::Black, PieceType::Rook);
        case 'q': return Piece::of(Color::Black, PieceType::Queen);
        case 'k': return Piece::of(Color::Black, PieceType::King);
        default: return std::nullopt;
    }
}

std::optional<char> pieceToChar(const Piece& piece)
{
    char base;
    switch (piece.type) {
        case PieceType::Pawn:   base = 'P'; break;
        case PieceType::Knight: base = 'N'; break;
        case PieceType::Bishop: base = 'B'; break;
        case PieceType::Rook:   base = 'R'; break;
        case PieceType::Queen:  base = 'Q'; break;
        case PieceType::King:   base = 'K'; break;
        default: return std::nullopt;
    }
    if (piece.color == Color::Black) {
        base = static_cast<char>(base - 'A' + 'a');
    }
    return base;
}

std::string buildPlacement(const Board& board)
{
    std::string placement;
    for (int rank = 7; rank >= 0; --rank) {
        int emptyRun = 0;
        for (int file = 0; file < 8; ++file) {
            const Piece piece = board.pieceAt(squareOf(file, rank));
            if (piece.isNone()) {
                ++emptyRun;
            } else {
                if (emptyRun > 0) {
                    placement += static_cast<char>('0' + emptyRun);
                    emptyRun = 0;
                }
                placement += *pieceToChar(piece);
            }
        }
        if (emptyRun > 0) {
            placement += static_cast<char>('0' + emptyRun);
        }
        if (rank > 0) {
            placement += '/';
        }
    }
    return placement;
}

std::optional<int> parseNumber(std::string_view s)
{
    if (s.empty()) {
        return std::nullopt;
    }
    int value = 0;
    for (char ch : s) {
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        value = value * 10 + (ch - '0');
    }
    return value;
}

std::optional<std::vector<std::string_view>> splitFields(std::string_view fen)
{
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= fen.size(); ++i) {
        if (i == fen.size() || fen[i] == ' ') {
            if (i == start) {
                return std::nullopt;
            }
            fields.push_back(fen.substr(start, i - start));
            start = i + 1;
        }
    }
    return fields;
}

// File of `color`'s king on back rank `rank`, or -1 when absent.
int kingFileOnRank(const Board& board, Color color, int rank)
{
    for (int file = 0; file < 8; ++file) {
        const Piece p = board.pieceAt(squareOf(file, rank));
        if (p.color == color && p.type == PieceType::King) return file;
    }
    return -1;
}

// Outermost rook of `color` between the king and the board corner on back rank
// `rank`. Scans from the corner inward; returns -1 if the king is not on rank or
// no rook is found on the king's side of the corner.
int outermostRookFile(const Board& board, Color color, int rank, bool kingSide)
{
    const int kingFile = kingFileOnRank(board, color, rank);
    if (kingFile < 0) return -1;
    if (kingSide) {
        for (int file = 7; file > kingFile; --file) {
            const Piece p = board.pieceAt(squareOf(file, rank));
            if (p.color == color && p.type == PieceType::Rook) return file;
        }
    } else {
        for (int file = 0; file < kingFile; ++file) {
            const Piece p = board.pieceAt(squareOf(file, rank));
            if (p.color == color && p.type == PieceType::Rook) return file;
        }
    }
    return -1;
}

} // namespace

std::optional<Board> fromFen(std::string_view fen)
{
    auto fields = splitFields(fen);
    if (!fields || fields->size() != 6) {
        return std::nullopt;
    }

    Board board;

    int rankIndex = 0;
    int fileIndex = 0;
    for (char ch : (*fields)[0]) {
        if (ch == '/') {
            if (fileIndex != 8) {
                return std::nullopt;
            }
            ++rankIndex;
            fileIndex = 0;
            if (rankIndex > 7) {
                return std::nullopt;
            }
        } else if (ch >= '1' && ch <= '8') {
            fileIndex += ch - '0';
            if (fileIndex > 8) {
                return std::nullopt;
            }
        } else {
            auto piece = pieceFromChar(ch);
            if (!piece || fileIndex >= 8) {
                return std::nullopt;
            }
            board.setPiece(squareOf(fileIndex, 7 - rankIndex), *piece);
            ++fileIndex;
        }
    }
    if (rankIndex != 7 || fileIndex != 8) {
        return std::nullopt;
    }

    if ((*fields)[1] == "w") {
        board.setSideToMove(Color::White);
    } else if ((*fields)[1] == "b") {
        board.setSideToMove(Color::Black);
    } else {
        return std::nullopt;
    }

    // Castling field: traditional KQkq plus X-FEN/Chess960 file letters. Letters
    // 'A'..'H' / 'a'..'h' name the rook's back-rank file (case = color). Rook-less
    // or king-less positions remain parseable: when inference is impossible the
    // right still grants, keyed to its standard home square (castling simply
    // never becomes playable, exactly like before Chess960 support).
    if ((*fields)[2] == "-") {
        board.setCastlingRights(NoCastling);
    } else {
        int rights = NoCastling;
        bool seen[4] = {false, false, false, false};
        auto grant = [&](CastlingRights right, Square rookFrom) -> bool {
            const int idx = castlingRightIndex(right);
            if (seen[idx]) return false;
            seen[idx] = true;
            rights |= static_cast<int>(right);
            board.setCastlingRookSquare(right, rookFrom);
            return true;
        };
        for (char ch : (*fields)[2]) {
            switch (ch) {
                case 'K': case 'Q':
                case 'k': case 'q': {
                    const Color color = (ch == 'K' || ch == 'Q')
                        ? Color::White : Color::Black;
                    const int rank = (color == Color::White) ? 0 : 7;
                    const bool kingSide = (ch == 'K' || ch == 'k');
                    const CastlingRights right = kingSide
                        ? (color == Color::White ? WhiteKingSide : BlackKingSide)
                        : (color == Color::White ? WhiteQueenSide : BlackQueenSide);
                    int file = outermostRookFile(board, color, rank, kingSide);
                    if (file < 0) {
                        // Fall back to the traditional corner when the position
                        // carries no decisive king/rook layout.
                        file = kingSide ? 7 : 0;
                    }
                    if (!grant(right, squareOf(file, rank))) return std::nullopt;
                    break;
                }
                default:
                    if ((ch >= 'A' && ch <= 'H')
                        || (ch >= 'a' && ch <= 'h')) {
                        const Color color = (ch <= 'H')
                            ? Color::White : Color::Black;
                        const int rank = (color == Color::White) ? 0 : 7;
                        const int file = (color == Color::White)
                            ? ch - 'A' : ch - 'a';
                        const int kingFile = kingFileOnRank(board, color, rank);
                        const int split = (kingFile >= 0) ? kingFile : 3;
                        const CastlingRights right = (file > split)
                            ? (color == Color::White ? WhiteKingSide : BlackKingSide)
                            : (color == Color::White ? WhiteQueenSide : BlackQueenSide);
                        if (!grant(right, squareOf(file, rank))) return std::nullopt;
                    } else {
                        return std::nullopt;
                    }
            }
        }
        board.setCastlingRights(rights);
    }

    if ((*fields)[3] == "-") {
        board.setEnPassantSquare(SquareNone);
    } else {
        const Square ep = stringToSquare((*fields)[3]);
        const Rank r = rankOf(ep);
        if (ep == SquareNone || (r != Rank::R3 && r != Rank::R6)) {
            return std::nullopt;
        }
        board.setEnPassantSquare(ep);
    }

    auto halfmove = parseNumber((*fields)[4]);
    auto fullmove = parseNumber((*fields)[5]);
    if (!halfmove || !fullmove || *fullmove < 1) {
        return std::nullopt;
    }
    board.setHalfmoveClock(*halfmove);
    board.setFullmoveNumber(*fullmove);

    return board;
}

std::optional<Board> Board::fromFen(std::string_view fen)
{
    return chess::fromFen(fen);
}

std::string toFen(const Board& board)
{
    std::string fen = buildPlacement(board);
    fen += ' ';
    fen += board.sideToMove() == Color::White ? 'w' : 'b';

    fen += ' ';
    const int rights = board.castlingRights();
    if (rights == NoCastling) {
        fen += '-';
    } else {
        // Traditional corner file → legacy letter; otherwise X-FEN file letter
        // naming the rook's back-rank square (Chess960).
        auto writeSide = [&](Color color) {
            const CastlingRights kSide = color == Color::White ? WhiteKingSide : BlackKingSide;
            const CastlingRights qSide = color == Color::White ? WhiteQueenSide : BlackQueenSide;
            const char kBase = color == Color::White ? 'K' : 'k';
            const char qBase = color == Color::White ? 'Q' : 'q';
            if (canCastle(rights, kSide)) {
                const int file = board.castlingRookSquare(kSide) & 7;
                if (file == 7) {
                    fen += kBase;
                } else if (color == Color::White) {
                    fen += static_cast<char>('A' + file);
                } else {
                    fen += static_cast<char>('a' + file);
                }
            }
            if (canCastle(rights, qSide)) {
                const int file = board.castlingRookSquare(qSide) & 7;
                if (file == 0) {
                    fen += qBase;
                } else if (color == Color::White) {
                    fen += static_cast<char>('A' + file);
                } else {
                    fen += static_cast<char>('a' + file);
                }
            }
        };
        writeSide(Color::White);
        writeSide(Color::Black);
    }

    fen += ' ';
    if (board.enPassantSquare() == SquareNone) {
        fen += '-';
    } else {
        fen += squareToString(board.enPassantSquare());
    }

    fen += ' ';
    fen += std::to_string(board.halfmoveClock());

    fen += ' ';
    fen += std::to_string(board.fullmoveNumber());

    return fen;
}

std::string Board::toFen() const
{
    return chess::toFen(*this);
}

} // namespace chess
