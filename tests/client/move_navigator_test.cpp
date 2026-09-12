#include "widgets/move_navigator.h"

#include <chess/fen.h>
#include <chess/san.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace chess::client {
namespace {

struct Game {
    Board initial;
    std::vector<chess::Move> moves;
    std::vector<std::string> sans;
    std::vector<std::string> fenPerPly;
};

// Plays the given SAN sequence from a start board, collecting the resolved
// moves and the FEN of every position reached (ply 0 = the start position).
Game playSans(const Board& start, const std::vector<std::string>& sans)
{
    Game g;
    g.initial = start;
    Board b = start;
    g.fenPerPly.push_back(b.toFen());
    for (const auto& san : sans) {
        auto m = chess::san::fromSan(b, san);
        if (!m) break;
        g.moves.push_back(*m);
        g.sans.push_back(san);
        b.makeMove(*m);
        g.fenPerPly.push_back(b.toFen());
    }
    return g;
}

TEST(MoveNavigator, ReplayMatchesManualReplay)
{
    auto g = playSans(Board::fromStartPos(),
                      { "e4", "e5", "Nf3", "Nc6", "Bb5", "a6" });

    MoveNavigator nav;
    nav.setGame(g.initial, g.moves, g.sans);

    EXPECT_EQ(nav.totalPlies(), static_cast<int>(g.moves.size()));
    EXPECT_EQ(nav.currentPly(), 0);
    EXPECT_EQ(nav.board().toFen(), g.fenPerPly[0]);

    for (std::size_t ply = 1; ply < g.fenPerPly.size(); ++ply) {
        nav.goToPly(static_cast<int>(ply));
        EXPECT_EQ(nav.board().toFen(), g.fenPerPly[ply]) << "at ply " << ply;
    }

    nav.goEnd();
    EXPECT_EQ(nav.board().toFen(), g.fenPerPly.back());
    EXPECT_EQ(nav.finalBoard().toFen(), g.fenPerPly.back());
}

TEST(MoveNavigator, GoToPlyClamps)
{
    auto g = playSans(Board::fromStartPos(), { "e4", "e5", "Nf3" });

    MoveNavigator nav;
    nav.setGame(g.initial, g.moves, g.sans);

    nav.goToPly(-7);
    EXPECT_EQ(nav.currentPly(), 0);
    EXPECT_TRUE(nav.atStart());

    nav.goToPly(1000);
    EXPECT_EQ(nav.currentPly(), g.moves.size());
    EXPECT_TRUE(nav.atEnd());
}

TEST(MoveNavigator, AppendAdvancesOnlyWhenAtEnd)
{
    auto g = playSans(Board::fromStartPos(), { "e4", "e5", "Nf3" });
    MoveNavigator nav;
    nav.setGame(g.initial, g.moves, g.sans);
    nav.goEnd();

    // Browsing back and appending keeps the current ply (read-only replay).
    nav.goToPly(1);
    auto m2 = chess::san::fromSan(nav.finalBoard(), "Nf6");
    ASSERT_TRUE(m2.has_value());
    nav.appendMove(*m2, "Nf6");
    EXPECT_EQ(nav.currentPly(), 1);
    EXPECT_EQ(nav.totalPlies(), 4);
    EXPECT_EQ(nav.board().toFen(), g.fenPerPly[1]);

    // Appending while at the end auto-advances to the new final position.
    nav.goEnd();
    auto m3 = chess::san::fromSan(nav.finalBoard(), "Bb5");
    ASSERT_TRUE(m3.has_value());
    nav.appendMove(*m3, "Bb5");
    EXPECT_EQ(nav.currentPly(), 5);
    EXPECT_EQ(nav.board().toFen(), nav.finalBoard().toFen());
}

TEST(MoveNavigator, CapturedEnPassant)
{
    auto start = Board::fromFen("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
    ASSERT_TRUE(start.has_value());
    auto g = playSans(*start, { "exd6" });
    ASSERT_EQ(g.moves.size(), 1u);
    ASSERT_TRUE(g.moves[0].isEnPassant());

    MoveNavigator nav;
    nav.setGame(g.initial, g.moves, g.sans);

    const auto& white = nav.capturedBy(Color::White);
    EXPECT_EQ(white.value, 1);
    EXPECT_EQ(white.counts[static_cast<int>(PieceType::Pawn)], 1);
    ASSERT_EQ(white.pieces.size(), 1u);
    EXPECT_EQ(white.pieces[0].color, Color::Black);
    EXPECT_EQ(white.pieces[0].type, PieceType::Pawn);
    EXPECT_EQ(nav.materialAdvantage(Color::White), 1);
    EXPECT_EQ(nav.materialAdvantage(Color::Black), -1);
}

TEST(MoveNavigator, CapturedPromotionCapture)
{
    auto start = Board::fromFen("5rk1/4P3/8/8/8/8/8/4K3 w - - 0 1");
    ASSERT_TRUE(start.has_value());
    auto g = playSans(*start, { "exf8=Q" });
    ASSERT_EQ(g.moves.size(), 1u);
    ASSERT_TRUE(g.moves[0].isPromotion());

    MoveNavigator nav;
    nav.setGame(g.initial, g.moves, g.sans);

    const auto& white = nav.capturedBy(Color::White);
    EXPECT_EQ(white.value, 5);
    EXPECT_EQ(white.counts[static_cast<int>(PieceType::Rook)], 1);
    ASSERT_EQ(white.pieces.size(), 1u);
    EXPECT_EQ(white.pieces[0].color, Color::Black);
    EXPECT_EQ(white.pieces[0].type, PieceType::Rook);
    EXPECT_EQ(nav.materialAdvantage(Color::White), 5);
}

TEST(MoveNavigator, CapturedByBlackAndTwoSidedDiff)
{
    auto start = Board::fromFen("4k3/8/8/8/8/8/4n3/4K1R1 b - - 0 1");
    ASSERT_TRUE(start.has_value());
    auto g = playSans(*start, { "Nxg1" });
    ASSERT_EQ(g.moves.size(), 1u);

    MoveNavigator nav;
    nav.setGame(g.initial, g.moves, g.sans);

    const auto& black = nav.capturedBy(Color::Black);
    EXPECT_EQ(black.value, 5);
    ASSERT_EQ(black.pieces.size(), 1u);
    EXPECT_EQ(black.pieces[0].color, Color::White);
    EXPECT_EQ(black.pieces[0].type, PieceType::Rook);
    EXPECT_EQ(nav.materialAdvantage(Color::Black), 5);
    EXPECT_EQ(nav.materialAdvantage(Color::White), -5);
}

TEST(MoveNavigator, LastMoveHighlights)
{
    auto g = playSans(Board::fromStartPos(), { "e4", "e5", "Nf3" });

    MoveNavigator nav;
    nav.setGame(g.initial, g.moves, g.sans);

    nav.goToPly(0);
    EXPECT_FALSE(nav.lastMoveFrom().has_value());
    EXPECT_FALSE(nav.lastMoveTo().has_value());

    nav.goToPly(3);
    ASSERT_TRUE(nav.lastMoveFrom().has_value());
    ASSERT_TRUE(nav.lastMoveTo().has_value());
    EXPECT_EQ(*nav.lastMoveFrom(), std::make_pair(6, 0)); // g1 (Nf3)
    EXPECT_EQ(*nav.lastMoveTo(), std::make_pair(5, 2));   // f3
}

} // namespace
} // namespace chess::client