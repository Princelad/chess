#include "widgets/move_navigator.h"

#include <chess/fen.h>
#include <chess/movegen.h>
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

TEST(MoveNavigator, MultiCaptureAccumulatesForBothSides)
{
    auto start = Board::fromFen("4k3/8/8/p7/1n6/3N4/8/Q3K3 w - - 0 1");
    ASSERT_TRUE(start.has_value());
    auto g = playSans(*start, { "Qxa5", "Nxd3" });
    ASSERT_EQ(g.moves.size(), 2u);
    ASSERT_TRUE(g.moves[0].isCapture());
    ASSERT_TRUE(g.moves[1].isCapture());

    MoveNavigator nav;
    nav.setGame(g.initial, g.moves, g.sans);

    const auto& white = nav.capturedBy(Color::White);
    EXPECT_EQ(white.value, 1);
    EXPECT_EQ(white.counts[static_cast<int>(PieceType::Pawn)], 1);
    ASSERT_EQ(white.pieces.size(), 1u);
    EXPECT_EQ(white.pieces[0].color, Color::Black);

    const auto& black = nav.capturedBy(Color::Black);
    EXPECT_EQ(black.value, 3);
    EXPECT_EQ(black.counts[static_cast<int>(PieceType::Knight)], 1);
    ASSERT_EQ(black.pieces.size(), 1u);
    EXPECT_EQ(black.pieces[0].color, Color::White);
    EXPECT_EQ(black.pieces[0].type, PieceType::Knight);

    EXPECT_EQ(nav.materialAdvantage(Color::White), -2);
    EXPECT_EQ(nav.materialAdvantage(Color::Black), 2);
}

TEST(MoveNavigator, AppendAccountingMatchesSetGameRebuild)
{
    auto g = playSans(Board::fromStartPos(),
                      { "e4", "d5", "exd5", "Nf6", "Nc3", "Qxd5" });
    ASSERT_EQ(g.moves.size(), 6u);

    MoveNavigator viaSet;
    viaSet.setGame(g.initial, g.moves, g.sans);
    viaSet.goEnd();

    MoveNavigator viaAppend;
    viaAppend.setGame(Board::fromStartPos(), {}, {});
    for (std::size_t i = 0; i < g.moves.size(); ++i)
        viaAppend.appendMove(g.moves[i], g.sans[i]);

    EXPECT_EQ(viaAppend.totalPlies(), 6);
    EXPECT_EQ(viaAppend.board().toFen(), viaSet.board().toFen());
    EXPECT_EQ(viaAppend.finalBoard().toFen(), viaSet.finalBoard().toFen());
    const auto& setWhite = viaSet.capturedBy(Color::White);
    const auto& appendWhite = viaAppend.capturedBy(Color::White);
    EXPECT_EQ(appendWhite.value, setWhite.value);
    EXPECT_EQ(appendWhite.counts, setWhite.counts);
    EXPECT_EQ(appendWhite.pieces.size(), setWhite.pieces.size());
    EXPECT_EQ(viaAppend.materialAdvantage(Color::White),
              viaSet.materialAdvantage(Color::White));
    EXPECT_EQ(viaAppend.materialAdvantage(Color::Black),
              viaSet.materialAdvantage(Color::Black));
}

// --- 15.3 move-list restyle -------------------------------------------------

TEST(MoveNavigator, ZebraShadesOddMoveNumberLinesOnly)
{
    EXPECT_TRUE(MoveNavigator::shadedLine(1));
    EXPECT_FALSE(MoveNavigator::shadedLine(2));
    EXPECT_TRUE(MoveNavigator::shadedLine(3));
    EXPECT_FALSE(MoveNavigator::shadedLine(24));
    EXPECT_TRUE(MoveNavigator::shadedLine(25));
    // The ply-0 slot and junk inputs never shade.
    EXPECT_FALSE(MoveNavigator::shadedLine(0));
    EXPECT_FALSE(MoveNavigator::shadedLine(-3));
}

TEST(MoveNavigator, VisibleLinesFollowsRowHeightRules)
{
    // Same formula that sized the grid since 9.3: (h - 2) / 16 lines.
    EXPECT_EQ(MoveNavigator::visibleLines(162.f), 10);
    EXPECT_EQ(MoveNavigator::visibleLines(16.f), 1);   // one row always fits
    EXPECT_EQ(MoveNavigator::visibleLines(0.f), 1);    // degenerate rect still 1
}

TEST(MoveNavigator, AppendAtTailScrollsFollowsTheLivePly)
{
    MoveNavigator nav;
    nav.setGame(Board::fromStartPos(), {}, {});
    nav.setLayout(sf::FloatRect({0.f, 0.f}, {100.f, 162.f}),
                  sf::FloatRect({0.f, 0.f}, {100.f, 20.f}));

    // Quiet start: the whole opening position fits, no scroll.
    Board b = Board::fromStartPos();
    const int vis = MoveNavigator::visibleLines(162.f);
    for (int i = 0; i < 40; ++i) {
        const auto legal = generateMoves(b);
        ASSERT_FALSE(legal.empty());
        const chess::Move& m = legal[0];
        const std::string san = chess::san::toSan(b, m);
        nav.appendMove(m, san);
        b.makeMove(m);
    }

    // 40 plies = 20 pairs on a 10-line window; sitting at the tail the
    // gutter's last line stays visible.
    ASSERT_EQ(nav.totalPlies(), 40);
    EXPECT_EQ(nav.scrollLine(), 20 - vis);

    // A tail append keeps the tail visible (same rule).
    const auto legal = generateMoves(b);
    ASSERT_FALSE(legal.empty());
    nav.appendMove(legal[0], chess::san::toSan(b, legal[0]));
    EXPECT_EQ(nav.scrollLine(), 21 - vis);
}

TEST(MoveNavigator, AppendWhileBrowsingNeverYanksTheScroll)
{
    auto g = playSans(Board::fromStartPos(),
                      {"e4", "e5", "Nf3", "Nc6", "Bb5", "a6"});
    MoveNavigator nav;
    nav.setGame(g.initial, g.moves, g.sans);
    nav.setLayout(sf::FloatRect({0.f, 0.f}, {100.f, 60.f}),
                  sf::FloatRect({0.f, 0.f}, {100.f, 20.f}));

    // Park mid-game on an early ply (scrolled toward the top), then
    // let a fresh move arrive: the panel must not yank to the tail.
    nav.goToPly(2);
    const int scrollWhileBrowsing = nav.scrollLine();

    // A real 7th move from the live position (the SAN history replays it).
    Board live = g.initial;
    for (const auto& m : g.moves) live.makeMove(m);
    const auto legal = generateMoves(live);
    ASSERT_FALSE(legal.empty());
    nav.appendMove(legal[0], chess::san::toSan(live, legal[0]));

    EXPECT_EQ(nav.currentPly(), 2);                    // still parked on ply 2
    EXPECT_EQ(nav.scrollLine(), scrollWhileBrowsing);  // scroll untouched
}

} // namespace
} // namespace chess::client