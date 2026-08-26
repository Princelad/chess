#include <gtest/gtest.h>

#include <chess/uci_move.h>
#include <chess/movegen.h>

using namespace chess;

TEST(UciMove, StartPos_e2e4)
{
    Board b = Board::fromStartPos();
    auto m = fromUci(b, "e2e4");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->from, squareOf(File::E, Rank::R2));
    EXPECT_EQ(m->to, squareOf(File::E, Rank::R4));
    EXPECT_TRUE(m->isDoublePush());
}

TEST(UciMove, StartPos_g1f3)
{
    Board b = Board::fromStartPos();
    auto m = fromUci(b, "g1f3");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->from, squareOf(File::G, Rank::R1));
    EXPECT_EQ(m->to, squareOf(File::F, Rank::R3));
}

TEST(UciMove, Castling_e1g1)
{
    Board b = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    auto m = fromUci(b, "e1g1");
    ASSERT_TRUE(m.has_value());
    EXPECT_TRUE(m->isCastle());
}

TEST(UciMove, Castling_e1c1)
{
    Board b = *Board::fromFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    auto m = fromUci(b, "e1c1");
    ASSERT_TRUE(m.has_value());
    EXPECT_TRUE(m->isCastle());
}

TEST(UciMove, Promotion_b7b8q)
{
    Board b = *Board::fromFen("4k3/1P6/8/8/8/8/8/4K3 w - - 0 1");
    auto m = fromUci(b, "b7b8q");
    ASSERT_TRUE(m.has_value());
    EXPECT_TRUE(m->isPromotion());
    EXPECT_EQ(m->promotion, PieceType::Queen);
}

TEST(UciMove, Promotion_b7b8n)
{
    Board b = *Board::fromFen("4k3/1P6/8/8/8/8/8/4K3 w - - 0 1");
    auto m = fromUci(b, "b7b8n");
    ASSERT_TRUE(m.has_value());
    EXPECT_TRUE(m->isPromotion());
    EXPECT_EQ(m->promotion, PieceType::Knight);
}

TEST(UciMove, Capture)
{
    Board b = *Board::fromFen("rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq e6 0 2");
    auto m = fromUci(b, "f3e5");
    ASSERT_TRUE(m.has_value());
    EXPECT_TRUE(m->isCapture());
}

TEST(UciMove, EnPassant)
{
    Board b = *Board::fromFen("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
    auto m = fromUci(b, "e5d6");
    ASSERT_TRUE(m.has_value());
    EXPECT_TRUE(m->isEnPassant());
}

TEST(UciMove, InvalidLength)
{
    Board b = Board::fromStartPos();
    EXPECT_FALSE(fromUci(b, "e2").has_value());
    EXPECT_FALSE(fromUci(b, "e2e4e6").has_value());
}

TEST(UciMove, InvalidSquare)
{
    Board b = Board::fromStartPos();
    EXPECT_FALSE(fromUci(b, "i2e4").has_value());
    EXPECT_FALSE(fromUci(b, "e2i4").has_value());
}

TEST(UciMove, IllegalMove)
{
    Board b = Board::fromStartPos();
    EXPECT_FALSE(fromUci(b, "e2e5").has_value());
}

TEST(UciMove, InvalidPromoChar)
{
    Board b = *Board::fromFen("4k3/1P6/8/8/8/8/8/4K3 w - - 0 1");
    EXPECT_FALSE(fromUci(b, "b7a8x").has_value());
}

TEST(UciMove, RoundTrip)
{
    Board b = Board::fromStartPos();
    auto legal = generateLegalMoves(b);
    for (const auto& m : legal) {
        std::string uci = toUci(m);
        auto parsed = fromUci(b, uci);
        ASSERT_TRUE(parsed.has_value()) << "Failed to parse: " << uci;
        EXPECT_EQ(parsed->from, m.from);
        EXPECT_EQ(parsed->to, m.to);
        EXPECT_EQ(parsed->promotion, m.promotion);
    }
}
