#include <chess/board.h>
#include <chess/fen.h>
#include <chess/move.h>
#include <chess/movegen.h>
#include <chess/san.h>

#include <gtest/gtest.h>

namespace chess {
namespace {

TEST(SAN, SimplePawnPush)
{
    auto board = Board::fromStartPos();
    Move m = doublePushMove(squareOf(File::E, Rank::R2), squareOf(File::E, Rank::R4));
    EXPECT_EQ(san::toSan(board, m), "e4");
}

TEST(SAN, SinglePawnPush)
{
    auto board = Board::fromStartPos();
    Move m = move(squareOf(File::D, Rank::R2), squareOf(File::D, Rank::R3));
    EXPECT_EQ(san::toSan(board, m), "d3");
}

TEST(SAN, PawnCapture)
{
    auto board = *Board::fromFen("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");
    Move m = captureMove(squareOf(File::E, Rank::R4), squareOf(File::D, Rank::R5));
    EXPECT_EQ(san::toSan(board, m), "exd5");
}

TEST(SAN, KnightMove)
{
    auto board = Board::fromStartPos();
    Move m = move(squareOf(File::G, Rank::R1), squareOf(File::F, Rank::R3));
    EXPECT_EQ(san::toSan(board, m), "Nf3");
}

TEST(SAN, KnightCapture)
{
    auto board = *Board::fromFen("rnbqkbnr/pppp1ppp/8/4p3/4N3/8/PPPPPPPP/RNBQKB1R w KQkq - 0 2");
    Move m = captureMove(squareOf(File::E, Rank::R4), squareOf(File::D, Rank::R6));
    EXPECT_EQ(san::toSan(board, m), "Nxd6+");
}

TEST(SAN, DisambiguationByFile)
{
    auto board = *Board::fromFen("rnbqkbnr/pppppppp/8/8/8/2N1N3/PPPPPPPP/R1BQKB1R w KQkq - 0 1");
    Move m = move(squareOf(File::C, Rank::R3), squareOf(File::D, Rank::R5));
    EXPECT_EQ(san::toSan(board, m), "Ncd5");
}

TEST(SAN, DisambiguationByRank)
{
    auto board = *Board::fromFen("4k3/8/8/R7/8/8/8/R3K3 w - - 0 1");
    Move m = move(squareOf(File::A, Rank::R1), squareOf(File::A, Rank::R3));
    EXPECT_EQ(san::toSan(board, m), "R1a3");
}

TEST(SAN, CastlingKingSide)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    Move m = castleMove(squareOf(File::E, Rank::R1), squareOf(File::G, Rank::R1));
    EXPECT_EQ(san::toSan(board, m), "O-O");
}

TEST(SAN, CastlingQueenSide)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    Move m = castleMove(squareOf(File::E, Rank::R1), squareOf(File::C, Rank::R1));
    EXPECT_EQ(san::toSan(board, m), "O-O-O");
}

TEST(SAN, Promotion)
{
    auto board = *Board::fromFen("8/4P3/8/8/8/8/8/4K2k w - - 0 1");
    Move m = promotionMove(squareOf(File::E, Rank::R7), squareOf(File::E, Rank::R8), PieceType::Queen);
    EXPECT_EQ(san::toSan(board, m), "e8=Q");
}

TEST(SAN, PromotionCapture)
{
    auto board = *Board::fromFen("3r3k/4P3/8/8/8/8/8/4K3 w - - 0 1");
    Move m{squareOf(File::E, Rank::R7), squareOf(File::D, Rank::R8), Promotion | Capture};
    m.promotion = PieceType::Knight;
    EXPECT_EQ(san::toSan(board, m), "exd8=N");
}

TEST(SAN, Check)
{
    // White bishop on c4, black king on f8. Bc4 attacks f7 — move Bf7+ is check
    auto board = *Board::fromFen("rnbqk2r/pppp1ppp/5n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4");
    Move m = captureMove(squareOf(File::C, Rank::R4), squareOf(File::F, Rank::R7));
    EXPECT_EQ(san::toSan(board, m), "Bxf7+");
}

TEST(SAN, Checkmate)
{
    // Ra8# — back-rank mate
    auto board = *Board::fromFen("6k1/5ppp/8/8/8/8/8/R3K2R w KQ - 0 1");
    Move m = move(squareOf(File::A, Rank::R1), squareOf(File::A, Rank::R8));
    EXPECT_EQ(san::toSan(board, m), "Ra8#");
}

// --- fromSan tests ---

TEST(SAN, FromSanPawnPush)
{
    auto board = Board::fromStartPos();
    auto m = san::fromSan(board, "e4");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, doublePushMove(squareOf(File::E, Rank::R2), squareOf(File::E, Rank::R4)));
}

TEST(SAN, FromSanPawnShortPush)
{
    auto board = Board::fromStartPos();
    auto m = san::fromSan(board, "d3");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, move(squareOf(File::D, Rank::R2), squareOf(File::D, Rank::R3)));
}

TEST(SAN, FromSanPawnCapture)
{
    auto board = *Board::fromFen("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");
    auto m = san::fromSan(board, "exd5");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, captureMove(squareOf(File::E, Rank::R4), squareOf(File::D, Rank::R5)));
}

TEST(SAN, FromSanKnightMove)
{
    auto board = Board::fromStartPos();
    auto m = san::fromSan(board, "Nf3");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, move(squareOf(File::G, Rank::R1), squareOf(File::F, Rank::R3)));
}

TEST(SAN, FromSanDisambiguationByFile)
{
    auto board = *Board::fromFen("rnbqkbnr/pppppppp/8/8/8/2N1N3/PPPPPPPP/R1BQKB1R w KQkq - 0 1");
    auto m = san::fromSan(board, "Ncd5");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, move(squareOf(File::C, Rank::R3), squareOf(File::D, Rank::R5)));
}

TEST(SAN, FromSanDisambiguationByRank)
{
    auto board = *Board::fromFen("4k3/8/8/R7/8/8/8/R3K3 w - - 0 1");
    auto m = san::fromSan(board, "R1a3");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, move(squareOf(File::A, Rank::R1), squareOf(File::A, Rank::R3)));
}

TEST(SAN, FromSanCastlingKingSide)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    auto m = san::fromSan(board, "O-O");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, castleMove(squareOf(File::E, Rank::R1), squareOf(File::G, Rank::R1)));
}

TEST(SAN, FromSanCastlingQueenSide)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    auto m = san::fromSan(board, "O-O-O");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, castleMove(squareOf(File::E, Rank::R1), squareOf(File::C, Rank::R1)));
}

TEST(SAN, FromSanPromotion)
{
    auto board = *Board::fromFen("8/4P3/8/8/8/8/8/4K2k w - - 0 1");
    auto m = san::fromSan(board, "e8=Q");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, promotionMove(squareOf(File::E, Rank::R7), squareOf(File::E, Rank::R8), PieceType::Queen));
}

TEST(SAN, FromSanPromotionCapture)
{
    auto board = *Board::fromFen("3r3k/4P3/8/8/8/8/8/4K3 w - - 0 1");
    auto m = san::fromSan(board, "exd8=N");
    ASSERT_TRUE(m.has_value());
    Move expected{squareOf(File::E, Rank::R7), squareOf(File::D, Rank::R8), Promotion | Capture};
    expected.promotion = PieceType::Knight;
    EXPECT_EQ(*m, expected);
}

TEST(SAN, FromSanCheckStripped)
{
    auto board = *Board::fromFen("rnbqk2r/pppp1ppp/5n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4");
    auto m = san::fromSan(board, "Bxf7+");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, captureMove(squareOf(File::C, Rank::R4), squareOf(File::F, Rank::R7)));
}

TEST(SAN, FromSanCheckmateStripped)
{
    auto board = *Board::fromFen("6k1/5ppp/8/8/8/8/8/R3K2R w KQ - 0 1");
    auto m = san::fromSan(board, "Ra8#");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, move(squareOf(File::A, Rank::R1), squareOf(File::A, Rank::R8)));
}

TEST(SAN, FromSanInvalidReturnsNullopt)
{
    auto board = Board::fromStartPos();
    EXPECT_FALSE(san::fromSan(board, "Z9").has_value());
    EXPECT_FALSE(san::fromSan(board, "").has_value());
    EXPECT_FALSE(san::fromSan(board, "e5").has_value());  // not a legal move from start
}

// --- Round-trip tests ---

TEST(SAN, RoundTripStartPos)
{
    auto board = Board::fromStartPos();
    auto legal = generateLegalMoves(board);
    for (const auto& m : legal) {
        std::string s = san::toSan(board, m);
        auto parsed = san::fromSan(board, s);
        ASSERT_TRUE(parsed.has_value()) << "Failed round-trip for: " << s;
        EXPECT_EQ(*parsed, m) << "Round-trip mismatch for: " << s;
    }
}

TEST(SAN, RoundTripCastlingPosition)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    auto legal = generateLegalMoves(board);
    for (const auto& m : legal) {
        std::string s = san::toSan(board, m);
        auto parsed = san::fromSan(board, s);
        ASSERT_TRUE(parsed.has_value()) << "Failed round-trip for: " << s;
        EXPECT_EQ(*parsed, m) << "Round-trip mismatch for: " << s;
    }
}

// --- En passant SAN ---

TEST(SAN, ToSanEnPassant)
{
    auto board = *Board::fromFen("8/8/8/3pP3/8/8/8/8 w - d6 0 1");
    Move m = enPassantMove(squareOf(File::E, Rank::R5), squareOf(File::D, Rank::R6));
    EXPECT_EQ(san::toSan(board, m), "exd6");
}

TEST(SAN, FromSanEnPassant)
{
    auto board = *Board::fromFen("8/8/8/3pP3/8/8/8/8 w - d6 0 1");
    auto m = san::fromSan(board, "exd6");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, enPassantMove(squareOf(File::E, Rank::R5), squareOf(File::D, Rank::R6)));
}

// --- Disambiguation by both file and rank ---

TEST(SAN, ToSanDisambiguationByBoth)
{
    // Rooks on a1, a5, c3. All can reach a3. Need both file and rank disambiguation for Ra1.
    auto board = *Board::fromFen("8/8/8/R7/8/2R5/8/R3k3 w - - 0 1");
    Move m = move(squareOf(File::A, Rank::R1), squareOf(File::A, Rank::R3));
    EXPECT_EQ(san::toSan(board, m), "Ra1a3");
}

// --- Black-side SAN ---

TEST(SAN, ToSanBlackPawnPush)
{
    auto board = *Board::fromFen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    Move m = doublePushMove(squareOf(File::D, Rank::R7), squareOf(File::D, Rank::R5));
    EXPECT_EQ(san::toSan(board, m), "d5");
}

TEST(SAN, ToSanBlackCastling)
{
    auto board = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R b KQkq - 0 1");
    Move m = castleMove(squareOf(File::E, Rank::R8), squareOf(File::G, Rank::R8));
    EXPECT_EQ(san::toSan(board, m), "O-O");
}

TEST(SAN, FromSanBlackMove)
{
    auto board = *Board::fromFen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    auto m = san::fromSan(board, "d5");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, doublePushMove(squareOf(File::D, Rank::R7), squareOf(File::D, Rank::R5)));
}

// --- Promotion+check ---

TEST(SAN, ToSanPromotionCheck)
{
    // White pawn e7, black king f8. e8=Q+ (queen checks along 8th rank)
    auto board = *Board::fromFen("5k2/4P3/8/8/8/8/8/4K3 w - - 0 1");
    Move m = promotionMove(squareOf(File::E, Rank::R7), squareOf(File::E, Rank::R8), PieceType::Queen);
    EXPECT_EQ(san::toSan(board, m), "e8=Q+");
}

// --- fromSan castling legality ---

TEST(SAN, FromSanCastlingIllegalReturnsNullopt)
{
    // Black can't castle KS here — king on e8, rook on h8, but squares f8/g8 are attacked by white queen on h7? No, let me use a position where castling is not legal.
    // King moved: position after 1.e3 e6 2.Bb5 — no, simpler: just use a position where rook is missing.
    auto board = *Board::fromFen("r3k3/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQ - 0 1");
    // Black can't castle QS because rights are only K (white KS). Wait, the castling field says "K" which means white KS only.
    // Let me make it clearer: black has no castling rights.
    auto board2 = *Board::fromFen("4k3/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQ - 0 1");
    // Now try to castle as black — black has no castling rights
    // But fromSan always constructs from the sideToMove's perspective. board2 has white to move. Let me use a position with black to move but no castling rights.
    auto board3 = *Board::fromFen("4k3/pppppppp/8/8/8/8/PPPPPPPP/R3K2R b - - 0 1");
    auto m = san::fromSan(board3, "O-O");
    EXPECT_FALSE(m.has_value());
}

// --- Round-trip with EP, promotion, disambiguation ---

TEST(SAN, RoundTripEnPassantPosition)
{
    // Position with en passant available
    auto board = *Board::fromFen("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3");
    auto legal = generateLegalMoves(board);
    for (const auto& m : legal) {
        std::string s = san::toSan(board, m);
        auto parsed = san::fromSan(board, s);
        ASSERT_TRUE(parsed.has_value()) << "Failed round-trip for: " << s;
        EXPECT_EQ(*parsed, m) << "Round-trip mismatch for: " << s;
    }
}

TEST(SAN, RoundTripPromotionPosition)
{
    auto board = *Board::fromFen("r1bqkb1r/ppppP2p/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 0 5");
    auto legal = generateLegalMoves(board);
    for (const auto& m : legal) {
        std::string s = san::toSan(board, m);
        auto parsed = san::fromSan(board, s);
        ASSERT_TRUE(parsed.has_value()) << "Failed round-trip for: " << s;
        EXPECT_EQ(*parsed, m) << "Round-trip mismatch for: " << s;
    }
}

// --- Game playthrough round-trip ---

TEST(SAN, GamePlaythroughRoundTrip)
{
    // Italian Game: 10 full moves covering pawn pushes, captures, knights,
    // bishops, disambiguation, check, and castling.
    const char* moves[] = {
        "e4", "e5", "Nf3", "Nc6", "Bc4", "Bc5",
        "c3", "Nf6", "d4", "exd4", "cxd4", "Bb4+",
        "Bd2", "Bxd2+", "Nbxd2", "d5", "exd5", "Nxd5",
        "O-O", "O-O",
    };

    auto board = Board::fromStartPos();
    for (const char* san : moves) {
        // Verify fromSan resolves
        auto m = san::fromSan(board, san);
        ASSERT_TRUE(m.has_value()) << "fromSan failed for: " << san
                                   << " at FEN: " << board.toFen();

        // Verify toSan round-trips
        std::string formatted = san::toSan(board, *m);
        auto reparsed = san::fromSan(board, formatted);
        ASSERT_TRUE(reparsed.has_value()) << "Round-trip reparse failed for: " << formatted;
        EXPECT_EQ(*reparsed, *m) << "Round-trip mismatch for: " << formatted;

        board.makeMove(*m);
    }
}

// --- fromSan disambiguation by both file and rank ---

TEST(SAN, FromSanDisambiguationByBoth)
{
    // Three rooks can reach a3. "Ra1a3" specifies both file and rank.
    auto board = *Board::fromFen("8/8/8/R7/8/2R5/8/R3k3 w - - 0 1");
    auto m = san::fromSan(board, "Ra1a3");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, move(squareOf(File::A, Rank::R1), squareOf(File::A, Rank::R3)));
}

// --- Ambiguous SAN returns nullopt ---

TEST(SAN, FromSanAmbiguousReturnsNullopt)
{
    // Two white knights (c3 and e3) can both reach d5. Plain "Nd5" is ambiguous.
    auto board = *Board::fromFen("rnbqkbnr/pppppppp/8/8/8/2N1N3/PPPPPPPP/R1BQKB1R w KQkq - 0 1");
    auto m = san::fromSan(board, "Nd5");
    EXPECT_FALSE(m.has_value());
}

// --- fromSan promotion with check/checkmate suffix ---

TEST(SAN, FromSanPromotionCheck)
{
    auto board = *Board::fromFen("5k2/4P3/8/8/8/8/8/4K3 w - - 0 1");
    auto m = san::fromSan(board, "e8=Q+");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, promotionMove(squareOf(File::E, Rank::R7), squareOf(File::E, Rank::R8), PieceType::Queen));
}

TEST(SAN, FromSanPromotionCheckmate)
{
    // Black pawn on b2 promotes to knight on a1 delivering checkmate.
    auto board = *Board::fromFen("4k2K/8/8/8/8/8/1p6/R7 b - - 0 1");
    auto m = san::fromSan(board, "bxa1=N#");
    ASSERT_TRUE(m.has_value());
    Move expected{squareOf(File::B, Rank::R2), squareOf(File::A, Rank::R1), Promotion | Capture};
    expected.promotion = PieceType::Knight;
    EXPECT_EQ(*m, expected);
}

} // namespace
} // namespace chess
