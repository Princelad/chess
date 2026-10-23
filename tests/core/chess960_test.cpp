#include <chess/chess960.h>

#include <chess/board.h>
#include <chess/fen.h>
#include <chess/move.h>
#include <chess/movegen.h>
#include <chess/san.h>

#include <gtest/gtest.h>

#include <set>
#include <string>

namespace chess {

namespace {

// Back-rank invariants of every legal Chess960 SP: bishops on opposite colors,
// king strictly between the rooks.
bool validBackRank(const Board& board, Color color)
{
    int midRookFile = -1;   // first-encountered (a-side) rook
    int hRookFile = -1;
    int kingFile = -1;
    int lightBishop = -1;
    int darkBishop = -1;
    const int rank = (color == Color::White) ? 0 : 7;
    for (int file = 0; file < 8; ++file) {
        const Piece p = board.pieceAt(squareOf(file, rank));
        if (p.color != color) continue;
        switch (p.type) {
            case PieceType::Rook:
                if (midRookFile < 0) midRookFile = file;
                else hRookFile = file;
                break;
            case PieceType::King:
                kingFile = file;
                break;
            case PieceType::Bishop:
                if (((file + rank) & 1) == 0) darkBishop = file;
                else lightBishop = file;
                break;
            default:
                break;
        }
    }
    if (midRookFile < 0 || hRookFile < 0 || kingFile < 0) return false;
    if (lightBishop < 0 || darkBishop < 0) return false;
    return midRookFile < kingFile && kingFile < hRookFile;
}

bool hasCastleMove(const Board& board)
{
    for (const auto& m : generateLegalMoves(board)) {
        if (m.isCastle()) return true;
    }
    return false;
}

} // namespace

TEST(Chess960, StandardPositionIsId518)
{
    auto fen = chess960StartFen(Chess960StandardId);
    ASSERT_TRUE(fen.has_value());
    EXPECT_EQ(*fen, Board::fromStartPos().toFen());
    EXPECT_EQ(*fen, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

TEST(Chess960, AllPositionsAreDistinctAndValid)
{
    std::set<std::string> seen;
    for (int id = 0; id < Chess960PositionCount; ++id) {
        auto fen = chess960StartFen(id);
        ASSERT_TRUE(fen.has_value()) << "id " << id;
        auto board = fromFen(*fen);
        ASSERT_TRUE(board.has_value()) << "id " << id << " fen " << *fen;
        EXPECT_TRUE(validBackRank(*board, Color::White)) << "id " << id;
        EXPECT_TRUE(validBackRank(*board, Color::Black)) << "id " << id;
        seen.insert(*fen);
    }
    EXPECT_EQ(seen.size(), Chess960PositionCount);
}

TEST(Chess960, OutOfRangeIdIsRejected)
{
    EXPECT_FALSE(chess960StartFen(-1).has_value());
    EXPECT_FALSE(chess960StartFen(Chess960PositionCount).has_value());
}

TEST(Chess960, RandomIdInRange)
{
    for (int i = 0; i < 64; ++i) {
        const int id = chess960RandomId();
        EXPECT_GE(id, 0);
        EXPECT_LT(id, Chess960PositionCount);
    }
}

TEST(Chess960, EveryPositionParseRoundTrip)
{
    for (int id = 0; id < Chess960PositionCount; ++id) {
        auto fen = chess960StartFen(id);
        ASSERT_TRUE(fen.has_value()) << "id " << id;
        auto board = fromFen(*fen);
        ASSERT_TRUE(board.has_value()) << "id " << id;
        EXPECT_EQ(toFen(*board), *fen) << "id " << id;
        auto reparsed = fromFen(toFen(*board));
        ASSERT_TRUE(reparsed.has_value()) << "id " << id;
        EXPECT_EQ(board->castlingRights(), reparsed->castlingRights()) << "id " << id;
        for (int i = 0; i < 4; ++i) {
            const CastlingRights right = static_cast<CastlingRights>(1 << i);
            EXPECT_EQ(board->castlingRookSquare(right),
                      reparsed->castlingRookSquare(right))
                << "id " << id << " right bit " << i;
        }
    }
}

// Sweep: in every SP, each generated castle move puts king/rook on the FRC
// destination files, clears exactly that color's two rights, and is reversible.
TEST(Chess960, CastleSweep)
{
    for (int id = 0; id < Chess960PositionCount; ++id) {
        auto fen = chess960StartFen(id);
        ASSERT_TRUE(fen.has_value()) << "id " << id;
        auto board = fromFen(*fen);
        ASSERT_TRUE(board.has_value()) << "id " << id;

        auto sweepOne = [&](const Board& position) {
            const int rankIdx = (position.sideToMove() == Color::White) ? 0 : 7;
            const int rankBase = rankIdx * 16;
            const int ownRights = (rankIdx == 0)
                ? (WhiteKingSide | WhiteQueenSide)
                : (BlackKingSide | BlackQueenSide);
            const int otherRights = (rankIdx == 0)
                ? (BlackKingSide | BlackQueenSide)
                : (WhiteKingSide | WhiteQueenSide);
            const int startRights = position.castlingRights();

            for (const auto& m : generateLegalMoves(position)) {
                if (!m.isCastle()) continue;

                auto applied = position;
                applied.makeMove(m);

                const bool kingside = (m.to & 7) == 6;
                const Square kingTo = kingside ? rankBase + 6 : rankBase + 2;
                const Square rookTo = kingside ? rankBase + 5 : rankBase + 3;
                EXPECT_EQ(m.to, kingTo) << "wrong king destination; id " << id;
                EXPECT_EQ(applied.pieceAt(kingTo),
                          Piece::of(position.sideToMove(), PieceType::King))
                    << "king must land on the FRC king destination; id " << id;
                EXPECT_EQ(applied.pieceAt(rookTo),
                          Piece::of(position.sideToMove(), PieceType::Rook))
                    << "rook must land on the f/d file; id " << id;
                // King origin empties out — unless the rook lands exactly there
                // (kingside rook destination f1 may coincide with the king's
                // start file).
                const Piece kingOriginAfter = applied.pieceAt(m.from);
                const bool rookLandsOnKingOrigin = kingOriginAfter.type == PieceType::Rook
                    && kingOriginAfter.color == position.sideToMove();
                if (!rookLandsOnKingOrigin) {
                    EXPECT_EQ(applied.pieceAt(m.from), Piece::None())
                        << "king origin must be empty; id " << id;
                }

                EXPECT_EQ(applied.castlingRights() & ownRights, 0)
                    << "own rights must be cleared; id " << id;
                EXPECT_EQ(applied.castlingRights() & otherRights,
                          startRights & otherRights)
                    << "opponent rights must be untouched; id " << id;

                // Undo must restore the position exactly.
                auto undo = applied;
                undo.undoMove(m);
                EXPECT_EQ(undo.state().castlingRights, startRights) << "id " << id;
                for (int i = 0; i < 4; ++i) {
                    EXPECT_EQ(undo.castlingRookSquare(static_cast<CastlingRights>(1 << i)),
                              position.castlingRookSquare(static_cast<CastlingRights>(1 << i)))
                        << "rook square not restored; id " << id;
                }
                EXPECT_EQ(undo.toFen(), position.toFen()) << "id " << id;
            }
        };

        // White to move...
        sweepOne(*board);
        // ...and flipped: same position, black to move.
        auto black = *board;
        black.setSideToMove(Color::Black);
        sweepOne(black);
    }
}

TEST(Chess960, FenCastlingFileLetters)
{
    // White king on h1, queenside rook on b1: 'B' names the rook file.
    auto board = fromFen("8/8/8/8/8/8/8/1R5K w B - 0 1");
    ASSERT_TRUE(board.has_value());
    EXPECT_TRUE(canCastle(board->castlingRights(), WhiteQueenSide));
    EXPECT_FALSE(canCastle(board->castlingRights(), WhiteKingSide));
    EXPECT_EQ(board->castlingRookSquare(WhiteQueenSide), squareOf(1, 0));
    EXPECT_EQ(toFen(*board), "8/8/8/8/8/8/8/1R5K w B - 0 1");

    // Black kingside rook on g8 named by its file letter; corner-file rook may
    // still use 'q'. Field order in the input is free; output is canonical
    // (kingside before queenside per color pair).
    auto black = fromFen("6r1/8/8/8/8/8/8/R6K b qg - 0 1");
    ASSERT_TRUE(black.has_value());
    EXPECT_TRUE(canCastle(black->castlingRights(), BlackQueenSide));
    EXPECT_TRUE(canCastle(black->castlingRights(), BlackKingSide));
    EXPECT_EQ(black->castlingRookSquare(BlackQueenSide), squareOf(0, 7));
    EXPECT_EQ(black->castlingRookSquare(BlackKingSide), squareOf(6, 7));
    EXPECT_EQ(toFen(*black), "6r1/8/8/8/8/8/8/R6K b gq - 0 1");
}

TEST(Chess960, FenRejectsBadCastlingLetters)
{
    const char* invalid[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KK - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KkK - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w GH - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w X - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkqX - 0 1",
    };
    for (const char* fen : invalid) {
        EXPECT_FALSE(fromFen(fen).has_value()) << "expected rejection: " << fen;
    }
}

TEST(Chess960, ExoticCastleGeometry)
{
    // King on b1 with queenside rook on a1: king hop is one square but the
    // rook slides to d1.
    auto kingCorner = fromFen("8/8/8/8/8/8/8/RK6 w Q - 0 1");
    ASSERT_TRUE(kingCorner.has_value());
    EXPECT_TRUE(hasCastleMove(*kingCorner));
    for (const auto& m : generateLegalMoves(*kingCorner)) {
        if (m.isCastle()) EXPECT_EQ(m, castleMove(squareOf(1, 0), squareOf(2, 0)));
    }
    // Same layout with a kingside right but no kingside rook: nothing generated.
    auto noRook = fromFen("8/8/8/8/8/8/8/RK6 w K - 0 1");
    ASSERT_TRUE(noRook.has_value());
    EXPECT_FALSE(hasCastleMove(*noRook));

    // King on f1, rook on h1: the rook must pass over the king's origin square.
    auto fKing = fromFen("8/8/8/8/8/8/8/5K1R w K - 0 1");
    ASSERT_TRUE(fKing.has_value());
    EXPECT_TRUE(hasCastleMove(*fKing));

    // King already on g1: castling is still legal (rook h1→f1 over the king).
    auto gKing = fromFen("8/8/8/8/8/8/8/6KR w K - 0 1");
    ASSERT_TRUE(gKing.has_value());
    for (const auto& m : generateLegalMoves(*gKing)) {
        if (m.isCastle()) EXPECT_EQ(m, castleMove(squareOf(6, 0), squareOf(6, 0)));
    }

    // King on e1, kingside rook on g1: the king lands on the rook's square.
    auto rookOnDest = fromFen("8/8/8/8/8/8/8/4K1R1 w G - 0 1");
    ASSERT_TRUE(rookOnDest.has_value());
    EXPECT_TRUE(hasCastleMove(*rookOnDest));

    // ...but a blocker on f1 stops it.
    auto blocked = fromFen("8/8/8/8/8/8/8/4K1N1 w G - 0 1");
    ASSERT_TRUE(blocked.has_value());
    EXPECT_FALSE(hasCastleMove(*blocked));

    // Destination attacked by a pawn (g1 falls to a pawn on h2): no castling.
    auto attackedDest = fromFen("8/8/8/8/8/8/7p/4K2R w K - 0 1");
    ASSERT_TRUE(attackedDest.has_value());
    EXPECT_FALSE(hasCastleMove(*attackedDest));
}

TEST(Chess960, SanRoundTripAndKingWalkDistinction)
{
    for (int id = 0; id < Chess960PositionCount; ++id) {
        auto fen = chess960StartFen(id);
        auto board = fromFen(*fen);
        ASSERT_TRUE(board.has_value()) << "id " << id;
        for (Color side : {Color::White, Color::Black}) {
            auto position = *board;
            position.setSideToMove(side);
            for (const auto& m : generateLegalMoves(position)) {
                if (!m.isCastle() && !m.isPromotion()) continue;
                const std::string san = san::toSan(position, m);
                auto parsed = san::fromSan(position, san);
                ASSERT_TRUE(parsed.has_value()) << "id " << id << " san " << san;
                EXPECT_EQ(*parsed, m) << "id " << id << " san " << san;
            }
        }
    }
}

TEST(Chess960, PlainKingWalkIsNotCastling)
{
    // King on f1 with a kingside rook on h1: both a plain king walk to g1 and
    // the castling jump f1→g1 exist. The SAN spellings must be distinct.
    auto board = fromFen("8/8/8/8/8/8/8/5K1R w K - 0 1");
    ASSERT_TRUE(board.has_value());

    auto walk = san::fromSan(*board, "Kg1");
    ASSERT_TRUE(walk.has_value());
    EXPECT_FALSE(walk->isCastle());
    EXPECT_EQ(*walk, move(squareOf(5, 0), squareOf(6, 0)));

    auto castle = san::fromSan(*board, "O-O");
    ASSERT_TRUE(castle.has_value());
    EXPECT_TRUE(castle->isCastle());
    EXPECT_EQ(*castle, castleMove(squareOf(5, 0), squareOf(6, 0)));
}

// Publisher: Andrew Grant's Chess960 perft suite (Ethereal), positions checked
// against Stockfish — https://chessprogramming.org/Chess960_Perft_Results.
// Castling field is Shredder-FEN (pure file letters); our X-FEN parser must
// accept it.
TEST(Chess960, GrantPerftSpotChecks)
{
    struct Entry {
        const char* fen;
        std::uint64_t d1, d2, d3;
    };
    const Entry entries[] = {
        {"bqnb1rkr/pp3ppp/3ppn2/2p5/5P2/P2P4/NPP1P1PP/BQ1BNRKR w HFhf - 2 9",
         21, 528, 12189},
        {"b1q1rrkb/pppppppp/3nn3/8/P7/1PPP4/4PPPP/BQNNRKRB w GE - 1 9",
         20, 479, 10471},
        {"qnbnr1kr/ppp1b1pp/4p3/3p1p2/8/2NPP3/PPP1BPPP/QNB1R1KR w HEhe - 1 9",
         29, 899, 26578},
        {"bnqnrbkr/1pp2pp1/p7/3pP2p/4P1P1/8/PPPP3P/BNQNRBKR w HEhe d6 0 9",
         31, 984, 28677},
        {"nqn2krb/p1prpppp/1pbp4/7P/5P2/8/PPPPPKP1/NQNRB1RB w g - 3 9",
         21, 461, 10608},
        {"bnnqrbkr/pp1p2p1/2p1p2p/5p2/1P5P/1R6/P1PPPPP1/BNNQRBK1 w Ehe - 0 9",
         33, 1022, 32724},
        {"1nbbnrkr/p1p1ppp1/3p4/1p3P1p/3Pq2P/8/PPP1P1P1/QNBBNRKR w HFhf - 0 9",
         28, 1120, 31058},
    };
    for (const auto& e : entries) {
        auto board = fromFen(e.fen);
        ASSERT_TRUE(board.has_value()) << "bad FEN: " << e.fen;
        auto scratch = *board;
        EXPECT_EQ(perft(scratch, 1), e.d1) << e.fen;
        scratch = *board;
        EXPECT_EQ(perft(scratch, 2), e.d2) << e.fen;
        scratch = *board;
        EXPECT_EQ(perft(scratch, 3), e.d3) << e.fen;
    }
}

} // namespace chess
