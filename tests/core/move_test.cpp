#include <chess/move.h>

#include <sstream>

#include <gtest/gtest.h>

namespace chess {

TEST(Move, DefaultIsEmpty)
{
    const Move m;
    EXPECT_TRUE(m.isNone());
    EXPECT_EQ(m.from, SquareNone);
    EXPECT_EQ(m.to, SquareNone);
    EXPECT_EQ(m.flags, Quiet);
}

TEST(Move, QuietConstruction)
{
    const Square e2 = squareOf(File::E, Rank::R2);
    const Square e4 = squareOf(File::E, Rank::R4);
    const Move m = move(e2, e4);
    EXPECT_EQ(m.from, e2);
    EXPECT_EQ(m.to, e4);
    EXPECT_FALSE(m.isCapture());
    EXPECT_FALSE(m.isDoublePush());
    EXPECT_FALSE(m.isCastle());
    EXPECT_FALSE(m.isEnPassant());
    EXPECT_FALSE(m.isPromotion());
}

TEST(Move, FlagFactories)
{
    const Square a1 = squareOf(File::A, Rank::R1);
    const Square a8 = squareOf(File::A, Rank::R8);
    const Square e1 = squareOf(File::E, Rank::R1);

    EXPECT_TRUE(captureMove(a1, a8).isCapture());

    const Move dp = doublePushMove(squareOf(File::D, Rank::R2), squareOf(File::D, Rank::R4));
    EXPECT_TRUE(dp.isDoublePush());
    EXPECT_FALSE(dp.isCapture());

    EXPECT_TRUE(castleMove(e1, squareOf(File::G, Rank::R1)).isCastle());
    EXPECT_TRUE(castleMove(e1, squareOf(File::C, Rank::R1)).isCastle());

    const Move ep = enPassantMove(squareOf(File::E, Rank::R5), squareOf(File::D, Rank::R6));
    EXPECT_TRUE(ep.isEnPassant());
    EXPECT_TRUE(ep.isCapture());
}

TEST(Move, PromotionEncodesPieceType)
{
    const Square e7 = squareOf(File::E, Rank::R7);
    const Square e8 = squareOf(File::E, Rank::R8);
    const Move q = promotionMove(e7, e8, PieceType::Queen);
    EXPECT_TRUE(q.isPromotion());
    EXPECT_EQ(q.promotion, PieceType::Queen);

    const Move n = promotionMove(e7, e8, PieceType::Knight);
    EXPECT_EQ(n.promotion, PieceType::Knight);
}

TEST(Move, Equality)
{
    const Square e2 = squareOf(File::E, Rank::R2);
    const Square e4 = squareOf(File::E, Rank::R4);
    const Square e5 = squareOf(File::E, Rank::R5);
    EXPECT_EQ(move(e2, e4), move(e2, e4));
    EXPECT_NE(move(e2, e4), doublePushMove(e2, e4));
    EXPECT_NE(move(e2, e4), move(e2, e5));
    const Move q1 = promotionMove(e2, e4, PieceType::Queen);
    const Move q2 = promotionMove(e2, e4, PieceType::Queen);
    const Move n = promotionMove(e2, e4, PieceType::Knight);
    EXPECT_EQ(q1, q2);
    EXPECT_NE(q1, n);
}

TEST(Move, ToUci)
{
    EXPECT_EQ(toUci(move(squareOf(File::E, Rank::R2), squareOf(File::E, Rank::R4))), "e2e4");
    EXPECT_EQ(toUci(promotionMove(squareOf(File::E, Rank::R7), squareOf(File::E, Rank::R8), PieceType::Queen)),
              "e7e8q");
    EXPECT_EQ(toUci(promotionMove(squareOf(File::A, Rank::R7), squareOf(File::B, Rank::R8), PieceType::Knight)),
              "a7b8n");
    EXPECT_EQ(toUci(castleMove(squareOf(File::E, Rank::R1), squareOf(File::G, Rank::R1))), "e1g1");
}

TEST(Move, StreamPrinting)
{
    const Move m = enPassantMove(squareOf(File::E, Rank::R5), squareOf(File::D, Rank::R6));
    std::ostringstream out;
    out << m;
    EXPECT_EQ(out.str(), "e5d6+cap+ep");
}

} // namespace chess
