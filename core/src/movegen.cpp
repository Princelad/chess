#include <chess/movegen.h>

namespace chess {

namespace {

constexpr int knightOffsets[8] = {33, 18, -14, -31, -33, -18, 14, 31};
constexpr int kingOffsets[8] = {1, 17, 16, 15, -1, -17, -16, -15};
constexpr int rookDirs[4] = {16, -16, 1, -1};
constexpr int bishopDirs[4] = {17, 15, -15, -17};

void genKnight(Square from, Color color, const Board& board, MoveFilter filter,
               std::vector<Move>& moves)
{
    for (int offset : knightOffsets) {
        const Square to = from + offset;
        if (offBoard(to)) continue;
        const Piece target = board.pieceAt(to);
        if (target.color == color) continue;
        if (filter == MoveFilter::CapturesOnly && target.isNone()) continue;
        if (target.isNone()) {
            moves.push_back(move(from, to));
        } else {
            moves.push_back(captureMove(from, to));
        }
    }
}

void genKing(Square from, Color color, const Board& board, MoveFilter filter,
             std::vector<Move>& moves)
{
    for (int offset : kingOffsets) {
        const Square to = from + offset;
        if (offBoard(to)) continue;
        const Piece target = board.pieceAt(to);
        if (target.color == color) continue;
        if (filter == MoveFilter::CapturesOnly && target.isNone()) continue;
        if (target.isNone()) {
            moves.push_back(move(from, to));
        } else {
            moves.push_back(captureMove(from, to));
        }
    }
}

constexpr bool isPromotionRank(Color color, Rank rank)
{
    return (color == Color::White && rank == Rank::R8)
        || (color == Color::Black && rank == Rank::R1);
}

constexpr Rank pawnStartRank(Color color)
{
    return color == Color::White ? Rank::R2 : Rank::R7;
}

constexpr Rank pawnPromotionRank(Color color)
{
    return color == Color::White ? Rank::R8 : Rank::R1;
}

constexpr int pawnPush(Color color)
{
    return color == Color::White ? 16 : -16;
}

constexpr int pawnDoublePush(Color color)
{
    return color == Color::White ? 32 : -32;
}

constexpr int pawnCaptureLeft(Color color)
{
    return color == Color::White ? 15 : -15;
}

constexpr int pawnCaptureRight(Color color)
{
    return color == Color::White ? 17 : -17;
}

void addPawnMove(Square from, Square to, Color color, std::vector<Move>& moves)
{
    if (isPromotionRank(color, rankOf(to))) {
        moves.push_back(promotionMove(from, to, PieceType::Knight));
        moves.push_back(promotionMove(from, to, PieceType::Bishop));
        moves.push_back(promotionMove(from, to, PieceType::Rook));
        moves.push_back(promotionMove(from, to, PieceType::Queen));
    } else {
        moves.push_back(move(from, to));
    }
}

void addPawnCapture(Square from, Square to, Color color, std::vector<Move>& moves)
{
    if (isPromotionRank(color, rankOf(to))) {
        Move m;
        m.from = from;
        m.to = to;
        m.flags = Capture | Promotion;

        m.promotion = PieceType::Knight;
        moves.push_back(m);
        m.promotion = PieceType::Bishop;
        moves.push_back(m);
        m.promotion = PieceType::Rook;
        moves.push_back(m);
        m.promotion = PieceType::Queen;
        moves.push_back(m);
    } else {
        moves.push_back(captureMove(from, to));
    }
}

void genPawn(Square from, Color color, const Board& board, MoveFilter filter,
             std::vector<Move>& moves)
{
    const int push = pawnPush(color);
    const Rank startRank = pawnStartRank(color);

    if (filter == MoveFilter::All) {
        const Square singleTo = from + push;
        if (!offBoard(singleTo) && board.isEmpty(singleTo)) {
            addPawnMove(from, singleTo, color, moves);

            if (rankOf(from) == startRank) {
                const Square doubleTo = from + pawnDoublePush(color);
                if (board.isEmpty(doubleTo)) {
                    moves.push_back(doublePushMove(from, doubleTo));
                }
            }
        }
    }

    const int captureOffsets[2] = {pawnCaptureLeft(color), pawnCaptureRight(color)};
    for (int offset : captureOffsets) {
        const Square to = from + offset;
        if (offBoard(to)) continue;
        if (board.enPassantSquare() == to) {
            moves.push_back(enPassantMove(from, to));
            continue;
        }
        const Piece target = board.pieceAt(to);
        if (target.isNone()) continue;
        if (target.color == color) continue;
        addPawnCapture(from, to, color, moves);
    }
}

void genSliding(Square from, Color color, const Board& board, MoveFilter filter,
                const int* dirs, int dirCount, std::vector<Move>& moves)
{
    for (int d = 0; d < dirCount; ++d) {
        const int dir = dirs[d];
        Square to = from + dir;
        while (!offBoard(to)) {
            const Piece target = board.pieceAt(to);
            if (target.color == color) break;
            if (target.isNone()) {
                if (filter != MoveFilter::CapturesOnly) {
                    moves.push_back(move(from, to));
                }
            } else {
                moves.push_back(captureMove(from, to));
                break;
            }
            to += dir;
        }
    }
}

constexpr Square kingHome(Color color) { return color == Color::White ? 4 : 116; }
constexpr Square rookHomeKs(Color color) { return color == Color::White ? 7 : 119; }
constexpr Square rookHomeQs(Color color) { return color == Color::White ? 0 : 112; }
constexpr Square kingDestKs(Color color) { return color == Color::White ? 6 : 118; }
constexpr Square kingDestQs(Color color) { return color == Color::White ? 2 : 114; }
constexpr CastlingRights ksRight(Color color) { return color == Color::White ? WhiteKingSide : BlackKingSide; }
constexpr CastlingRights qsRight(Color color) { return color == Color::White ? WhiteQueenSide : BlackQueenSide; }

bool squaresEmpty(const Board& board, Square from, Square to)
{
    const int step = (to > from) ? 1 : -1;
    for (Square sq = from + step; sq != to; sq += step) {
        if (!board.isEmpty(sq)) return false;
    }
    return true;
}

void genCastling(Square from, Color color, const Board& board, MoveFilter filter,
                 std::vector<Move>& moves)
{
    if (filter == MoveFilter::CapturesOnly) return;
    if (from != kingHome(color)) return;

    const int rights = board.castlingRights();

    if (canCastle(rights, ksRight(color))
        && !board.isEmpty(rookHomeKs(color))
        && squaresEmpty(board, from, rookHomeKs(color)))
    {
        moves.push_back(castleMove(from, kingDestKs(color)));
    }

    if (canCastle(rights, qsRight(color))
        && !board.isEmpty(rookHomeQs(color))
        && squaresEmpty(board, rookHomeQs(color), from))
    {
        moves.push_back(castleMove(from, kingDestQs(color)));
    }
}

} // namespace

bool isAttacked(const Board& board, Square sq, Color byColor)
{
    // Knight attacks
    for (int offset : knightOffsets) {
        const Square to = sq + offset;
        if (offBoard(to)) continue;
        const Piece p = board.pieceAt(to);
        if (p.color == byColor && p.type == PieceType::Knight) return true;
    }

    // King attacks
    for (int offset : kingOffsets) {
        const Square to = sq + offset;
        if (offBoard(to)) continue;
        const Piece p = board.pieceAt(to);
        if (p.color == byColor && p.type == PieceType::King) return true;
    }

    // Pawn attacks — check the two squares a pawn of byColor would attack from
    // White pawns capture at +15/+17, so attackers are at sq-15/sq-17
    // Black pawns capture at -15/-17, so attackers are at sq+15/sq+17
    const int pawnDir = (byColor == Color::White) ? -1 : 1;
    const int pawnAttackOffsets[2] = {15 * pawnDir, 17 * pawnDir};
    for (int offset : pawnAttackOffsets) {
        const Square to = sq + offset;
        if (offBoard(to)) continue;
        const Piece p = board.pieceAt(to);
        if (p.color == byColor && p.type == PieceType::Pawn) return true;
    }

    // Sliding attacks — rook rays
    for (int d = 0; d < 4; ++d) {
        Square to = sq + rookDirs[d];
        while (!offBoard(to)) {
            const Piece p = board.pieceAt(to);
            if (!p.isNone()) {
                if (p.color == byColor && (p.type == PieceType::Rook || p.type == PieceType::Queen))
                    return true;
                break;
            }
            to += rookDirs[d];
        }
    }

    // Sliding attacks — bishop rays
    for (int d = 0; d < 4; ++d) {
        Square to = sq + bishopDirs[d];
        while (!offBoard(to)) {
            const Piece p = board.pieceAt(to);
            if (!p.isNone()) {
                if (p.color == byColor && (p.type == PieceType::Bishop || p.type == PieceType::Queen))
                    return true;
                break;
            }
            to += bishopDirs[d];
        }
    }

    return false;
}

bool inCheck(const Board& board, Color color)
{
    for (int sq = 0; sq < BoardSize; ++sq) {
        if (offBoard(sq)) continue;
        const Piece p = board.pieceAt(sq);
        if (p.color == color && p.type == PieceType::King)
            return isAttacked(board, sq, opposite(color));
    }
    return false;
}

std::vector<Move> generateMoves(const Board& board, MoveFilter filter)
{
    std::vector<Move> moves;
    const Color side = board.sideToMove();

    for (int sq = 0; sq < BoardSize; ++sq) {
        if (offBoard(sq)) continue;
        const Piece piece = board.pieceAt(sq);
        if (piece.isNone() || piece.color != side) continue;

        switch (piece.type) {
            case PieceType::Knight:
                genKnight(sq, side, board, filter, moves);
                break;
            case PieceType::Bishop:
                genSliding(sq, side, board, filter, bishopDirs, 4, moves);
                break;
            case PieceType::Rook:
                genSliding(sq, side, board, filter, rookDirs, 4, moves);
                break;
            case PieceType::Queen:
                genSliding(sq, side, board, filter, rookDirs, 4, moves);
                genSliding(sq, side, board, filter, bishopDirs, 4, moves);
                break;
            case PieceType::King:
                genKing(sq, side, board, filter, moves);
                genCastling(sq, side, board, filter, moves);
                break;
            case PieceType::Pawn:
                genPawn(sq, side, board, filter, moves);
                break;
            default:
                break;
        }
    }

    return moves;
}

} // namespace chess
