#include <chess/chess960.h>

#include <chess/fen.h>

#include <array>
#include <random>

namespace chess {

namespace {

// Builds the white back rank (file index 0..7) for a given SP number using
// Scharnagl's direct derivation:
//   1. light-square bishop among b/d/f/h        (id % 4,      then id /= 4)
//   2. dark-square bishop among a/c/e/g         (id % 4,      then id /= 4)
//   3. queen on the k-th still-free file        (id % 6,      then id /= 6)
//   4. knight pair on the 5 still-free files — 10 unordered placements keyed
//      by the last digit, pair (i, j) listed as ((0,1),(0,2),(0,3),(0,4),
//      (1,2),(1,3),(1,4),(2,3),(2,4),(3,4))
//   5. remaining three files take rook, king, rook (left to right), which
//      guarantees the king sits between the rooks.
std::array<PieceType, 8> chess960BackRank(int id)
{
    std::array<PieceType, 8> rank{};
    rank.fill(PieceType::None);
    bool used[8] = {false, false, false, false, false, false, false, false};

    int n = id;
    used[1 + 2 * (n % 4)] = true;   // light bishop: b, d, f, h
    rank[1 + 2 * (n % 4)] = PieceType::Bishop;
    n /= 4;
    used[2 * (n % 4)] = true;       // dark bishop: a, c, e, g
    rank[2 * (n % 4)] = PieceType::Bishop;
    n /= 4;

    auto kthFree = [&](int k) {
        for (int file = 0; file < 8; ++file) {
            if (!used[file]) {
                if (k == 0) return file;
                --k;
            }
        }
        return -1;  // unreachable for ids in 0..959
    };

    const int queenFile = kthFree(n % 6);
    used[queenFile] = true;
    rank[queenFile] = PieceType::Queen;
    n /= 6;

    static constexpr int knightPairs[10][2] = {
        {0, 1}, {0, 2}, {0, 3}, {0, 4},
        {1, 2}, {1, 3}, {1, 4},
        {2, 3}, {2, 4},
        {3, 4},
    };
    const int freeFiles[5] = {
        kthFree(0), kthFree(1), kthFree(2), kthFree(3), kthFree(4),
    };
    const int knights = n % 10;
    n /= 10;
    used[freeFiles[knightPairs[knights][0]]] = true;
    rank[freeFiles[knightPairs[knights][0]]] = PieceType::Knight;
    used[freeFiles[knightPairs[knights][1]]] = true;
    rank[freeFiles[knightPairs[knights][1]]] = PieceType::Knight;

    // Remaining free files, ascending: queenside rook, king, kingside rook.
    for (int file = 0; file < 8; ++file) {
        if (used[file]) continue;
        rank[file] = PieceType::Rook;
        used[file] = true;
        file = 8;  // leave the loop; middle file is handled next
        for (int mid = 0; mid < 8; ++mid) {
            if (rank[mid] == PieceType::None) {
                rank[mid] = PieceType::King;
                used[mid] = true;
                break;
            }
        }
        break;
    }
    for (int file = 7; file >= 0; --file) {
        if (rank[file] == PieceType::None) {
            rank[file] = PieceType::Rook;
            break;
        }
    }

    return rank;
}

} // namespace

std::optional<std::string> chess960StartFen(int id)
{
    if (id < 0 || id >= Chess960PositionCount) {
        return std::nullopt;
    }

    const auto whiteRank = chess960BackRank(id);

    Board board;
    for (int file = 0; file < 8; ++file) {
        const PieceType type = whiteRank[file];
        board.setPiece(squareOf(file, 0), Piece::of(Color::White, type));
        board.setPiece(squareOf(file, 7), Piece::of(Color::Black, type));
        board.setPiece(squareOf(file, 1), Piece::of(Color::White, PieceType::Pawn));
        board.setPiece(squareOf(file, 6), Piece::of(Color::Black, PieceType::Pawn));
    }

    board.setSideToMove(Color::White);
    board.setEnPassantSquare(SquareNone);
    board.setHalfmoveClock(0);
    board.setFullmoveNumber(1);

    // Castling: both rooks own their right from their actual back-rank square
    // (kingside = on the king's h-side). toFen() turns corner-file rooks into
    // K/Q/k/q and inner ones into X-FEN file letters.
    int kingFile = -1;
    for (int file = 0; file < 8; ++file) {
        if (whiteRank[file] == PieceType::King) {
            kingFile = file;
            break;
        }
    }
    board.setCastlingRights(AllCastling);
    for (int file = 0; file < 8; ++file) {
        if (whiteRank[file] != PieceType::Rook) continue;
        const bool kingSide = file > kingFile;
        board.setCastlingRookSquare(
            kingSide ? WhiteKingSide : WhiteQueenSide, squareOf(file, 0));
        board.setCastlingRookSquare(
            kingSide ? BlackKingSide : BlackQueenSide, squareOf(file, 7));
    }

    return board.toFen();
}

int chess960RandomId()
{
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, Chess960PositionCount - 1);
    return dist(rng);
}

} // namespace chess
