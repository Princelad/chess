#include <chess/fen.h>

#include <optional>
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

    if ((*fields)[2] == "-") {
        board.setCastlingRights(NoCastling);
    } else {
        int rights = NoCastling;
        bool seenWhiteK = false;
        bool seenWhiteQ = false;
        bool seenBlackK = false;
        bool seenBlackQ = false;
        for (char ch : (*fields)[2]) {
            switch (ch) {
                case 'K':
                    if (seenWhiteK) return std::nullopt;
                    seenWhiteK = true;
                    rights |= WhiteKingSide;
                    break;
                case 'Q':
                    if (seenWhiteQ) return std::nullopt;
                    seenWhiteQ = true;
                    rights |= WhiteQueenSide;
                    break;
                case 'k':
                    if (seenBlackK) return std::nullopt;
                    seenBlackK = true;
                    rights |= BlackKingSide;
                    break;
                case 'q':
                    if (seenBlackQ) return std::nullopt;
                    seenBlackQ = true;
                    rights |= BlackQueenSide;
                    break;
                default:
                    return std::nullopt;
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
    if (!halfmove || !fullmove) {
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

} // namespace chess
