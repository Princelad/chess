// 16.2 unit tests — the canned MoveEvalStream fixtures drive the whole
// classifier; no engine, no I/O, single-threaded.

#include "move_quality.h"

#include <gtest/gtest.h>

#include <cmath>

#include <chess/board.h>
#include <chess/move.h>
#include <chess/san.h>

using namespace chess;
using namespace chess::client;
using namespace chess::client::quality;

namespace {

// Mate-collapsed convenience entry: mover's seat before/after.
MoveEval entry(Color mover, int cpBefore, int cpAfter)
{
    MoveEval e;
    e.mover = mover;
    e.cpBefore = cpBefore;
    e.cpAfter = cpAfter;
    return e;
}


// Test-local inverse of winPercent: finds the cp that maps to a given win%
// (the curve is strictly monotonic). Used to probe the classification
// boundaries without floating-point luck.
int cpForWin(float win, int nudge = 0)
{
    const double x = std::log(100.0 / static_cast<double>(win) - 1.0);
    const int cp = static_cast<int>(std::lround(-x / kWinSigmoid)) + nudge;
    return cp;
}

} // namespace

// --- stable constants --------------------------------------------------------

TEST(MoveQuality, WinCurveIsFiftyAtEqualityAndPinsMate)
{
    EXPECT_FLOAT_EQ(winPercent(0), 50.f);
    // Signed mate collapses hard (kMateCp rides past the sigmoid knee).
    EXPECT_FLOAT_EQ(winPercent(kMateCp), 100.f);
    EXPECT_FLOAT_EQ(winPercent(-kMateCp), 0.f);
    // The Lichess-style continuous mapping (pinned for tuning discipline).
    EXPECT_FLOAT_EQ(winPercent(100), 59.10259f);
}

TEST(MoveQuality, AccuracyCurveBoundaries)
{
    // accuracy(0) == kAccScale − kAccLift — the authentic chess.com constant
    // pair lands 99.9999, one click under 100.
    EXPECT_NEAR(moveAccuracy(0.f), 100.f, 0.01f);
    // Hard floor.
    EXPECT_FLOAT_EQ(moveAccuracy(1000.f), 0.f);
    // Decay shape (pinned as-is: classifier tuning requires test updates).
    EXPECT_FLOAT_EQ(moveAccuracy(20.f), 40.020428f);
}

// --- classification bands ------------------------------------------------------

TEST(MoveQuality, GivenUpBandsClassifyInOrder)
{
    // Equal position thrown away (clearly inside each band).
    EXPECT_EQ(classifyMove(entry(Color::White, 0, 0)), MoveClass::Good);
    EXPECT_EQ(classifyMove(entry(Color::White, 0, cpForWin(45.f))),
              MoveClass::Inaccuracy);  // ~2 % given up
    EXPECT_EQ(classifyMove(entry(Color::White, 0, cpForWin(35.f))),
              MoveClass::Mistake);    // ~15 % given up
    EXPECT_EQ(classifyMove(entry(Color::White, 0, cpForWin(20.f))),
              MoveClass::Blunder);    // ~30 % given up
    EXPECT_EQ(classifyMove(entry(Color::White, 0, cpForWin(1.f))),
              MoveClass::Blunder);    // total collapse
}

TEST(MoveQuality, ThresholdsPinAtTheDocumentedBoundaries)
{
    // The crossing itself (documented as >=): 1 cp just PAST each threshold
    // (toward more given-up) classifies up; 1 cp just INSIDE stays in the
    // milder band. Nudge sign: more-negative cp ⇒ more given up.
    EXPECT_EQ(classifyMove(entry(Color::White, 0,
                    cpForWin(50.f - kInaccuracyGive, -1))),
              MoveClass::Inaccuracy);
    EXPECT_EQ(classifyMove(entry(Color::White, 0,
                    cpForWin(50.f + 0.0001f - kInaccuracyGive, +1))),
              MoveClass::Good);

    EXPECT_EQ(classifyMove(entry(Color::White, 0,
                    cpForWin(50.f - kMistakeGive, -1))),
              MoveClass::Mistake);
    EXPECT_EQ(classifyMove(entry(Color::White, 0,
                    cpForWin(50.f + 0.0001f - kMistakeGive, +1))),
              MoveClass::Inaccuracy);

    EXPECT_EQ(classifyMove(entry(Color::White, 0,
                    cpForWin(50.f - kBlunderGive, -1))),
              MoveClass::Blunder);
    EXPECT_EQ(classifyMove(entry(Color::White, 0,
                    cpForWin(50.f + 0.0001f - kBlunderGive, +1))),
              MoveClass::Mistake);
}

TEST(MoveQuality, ImprovementsNeverGiveUpAndClampHolds)
{
    // Winning cushion grown (0 → +kMateCp discovery): given up stays 0.
    auto q = qualityOf(entry(Color::White, 0, kMateCp));
    EXPECT_FLOAT_EQ(q.givenUp, 0.f);
    EXPECT_EQ(q.klass, MoveClass::Good);

    // Blunder math from the mover's seat (0 → −500 cp = 50 − 13.69 given).
    q = qualityOf(entry(Color::White, 0, -500));
    EXPECT_NEAR(q.givenUp, 36.307166f, 0.001f);
    EXPECT_FLOAT_EQ(q.winBefore, 50.f);
    EXPECT_NEAR(q.winAfter, 13.692834f, 0.001f);
}

// --- labels -------------------------------------------------------------------

TEST(MoveQuality, BookShadowsEverything)
{
    MoveEval bookMove = entry(Color::White, 0, -kMateCp);
    bookMove.isBook = true;
    EXPECT_EQ(classifyMove(bookMove), MoveClass::Book);

    bookMove.isBest = true;
    bookMove.isSacrifice = true;
    EXPECT_EQ(classifyMove(bookMove), MoveClass::Book);
}

TEST(MoveQuality, BestEchoIsBestAndSacrificeMakesItBrilliant)
{
    MoveEval best = entry(Color::White, 300, 300);
    best.isBest = true;
    EXPECT_EQ(classifyMove(best), MoveClass::Best);

    best.isSacrifice = true;
    EXPECT_EQ(classifyMove(best), MoveClass::Brilliant);

    // A sacrifice that is NOT the engine's choice is judged purely on the
    // delta — throwing a winning position into mate-range is a blunder,
    // sacrifice flag or not.
    MoveEval sac = entry(Color::White, kMateCp, 0);
    sac.isSacrifice = true;
    EXPECT_EQ(classifyMove(sac), MoveClass::Blunder);

    // The engine's own mate-delivery without sacrifice stays plain Best.
    MoveEval mateFinder = entry(Color::White, kMateCp, kMateCp);
    mateFinder.isBest = true;
    EXPECT_EQ(classifyMove(mateFinder), MoveClass::Best);
}

// --- game aggregation ----------------------------------------------------------

TEST(MoveQuality, StreamFixtureDrivesFullClassification)
{
    // Italian Game mainline (the ECO table lists it through Bc5), then a
    // knight retreat into a self-hang: White throws ~28% away = Blunder.
    const std::vector<std::string> sans{
        "e4", "e5", "Nf3", "Nc6", "Bc4", "Bc5", "Na3"};
    const auto book = bookPlies(sans);
    ASSERT_EQ(book.size(), 7u);

    MoveEvalStream stream;
    for (std::size_t i = 0; i < sans.size(); ++i) {
        MoveEval move = entry(i % 2 == 0 ? Color::White : Color::Black, 0, 0);
        move.isBook = book[i];
        if (i == 6) move.cpAfter = -350; // the retreats' hang
        stream.push_back(move);
    }

    const GameQuality game = evaluateGame(stream);
    ASSERT_EQ(game.moves.size(), 7u);
    EXPECT_EQ(game.moves[0].klass, MoveClass::Book);
    EXPECT_EQ(game.moves[4].klass, MoveClass::Book);
    EXPECT_EQ(game.moves[5].klass, MoveClass::Book);
    EXPECT_EQ(game.moves[6].klass, MoveClass::Blunder);

    // Per-side legend counts (Book ×3 white, Book ×3 black, Blunder ×1).
    EXPECT_EQ(game.white.counts[size_t(MoveClass::Book)], 3);
    EXPECT_EQ(game.white.counts[size_t(MoveClass::Blunder)], 1);
    EXPECT_EQ(game.black.counts[size_t(MoveClass::Book)], 3);
    EXPECT_EQ(game.black.counts[size_t(MoveClass::Blunder)], 0);
}

TEST(MoveQuality, SideAccuracyMergesOnlyOwnPlies)
{
    MoveEvalStream stream;
    // White: one catastrophe, then a quiet equal move. Black: quiet.
    stream.push_back(entry(Color::White, 0, -500));
    stream.push_back(entry(Color::Black, 0, 0));
    stream.push_back(entry(Color::White, 0, 0));

    const GameQuality game = evaluateGame(stream);
    const float collapseAccuracy = moveAccuracy(36.307166f);
    EXPECT_NEAR(game.white.accuracy,
                (collapseAccuracy + 100.f) / 2.f, 0.01f);
    EXPECT_FLOAT_EQ(game.black.accuracy, 100.f);
}

// --- book flags from the ECO table (11.5 dependency) --------------------------

TEST(MoveQuality, BookFlagsEndWhereTheListedLineEnds)
{
    // The Italian mainline entry runs exactly five plies, so book holds
    // through Bc4 and stays through Classical's own Bc5 (the 6-ply entry).
    const std::vector<std::string> classical{
        "e4", "e5", "Nf3", "Nc6", "Bc4", "Bc5"};
    const auto flags = bookPlies(classical);
    ASSERT_EQ(flags.size(), 6u);
    for (int i = 0; i < 6; ++i) EXPECT_TRUE(flags[i]);

    // A bare umbrella never carries later moves: "e4" is listed alone, but
    // once the history outruns every listing the flag dies.
    const auto caroJunk = bookPlies({"e4", "c6", "Qb6", "Ke7"});
    ASSERT_EQ(caroJunk.size(), 4u);
    EXPECT_TRUE(caroJunk[0]);  // "e4"
    EXPECT_TRUE(caroJunk[1]);  // "e4 c6" is a listed Caro-Kann prefix
    EXPECT_FALSE(caroJunk[2]); // past the table: Qb6 is out
    EXPECT_FALSE(caroJunk[3]);
}

// --- 16.3 report-stream helpers ------------------------------------------------

namespace {

// Replays a FEN + SAN chain into moves (library-verified legality).
std::vector<chess::Move> chainFrom(const std::string& fen,
                                   const std::vector<std::string>& sans)
{
    std::optional<Board> board = Board::fromFen(fen);
    std::vector<chess::Move> moves;
    for (const std::string& san : sans) {
        auto move = san::fromSan(*board, san);
        if (!move) return {};
        moves.push_back(*move);
        board->makeMove(*move);
    }
    return moves;
}

EvalNode node(const std::string& fen, const std::string& san)
{
    EvalNode n;
    n.fen = fen;
    n.san = san;
    return n;
}

NodeResult scored(int cp)
{
    NodeResult r;
    r.eval.scoreCp = cp;
    r.valid = true;
    return r;
}

} // namespace

TEST(MoveQuality, MaterialCountsStandardAndCraftedPositions)
{
    // The classic P1/N3/B3/R5/Q9 sum: the start position's 39.
    auto board = Board::fromStartPos();
    EXPECT_EQ(materialIn(board, Color::White), 39);
    EXPECT_EQ(materialIn(board, Color::Black), 39);

    // Mid-game crafted board: white kept a queen, black balanced down.
    auto mid = Board::fromFen("4r3/3k4/3N4/8/8/8/8/4K3 w - - 0 1");
    ASSERT_TRUE(mid);
    EXPECT_EQ(materialIn(*mid, Color::White), 3);
    EXPECT_EQ(materialIn(*mid, Color::Black), 5);
}

TEST(MoveQuality, SacrificeFlagsPairNetLossNotPlainTrades)
{
    // A quiet development chain: every pair nets zero — no flags.
    {
        const auto moves =
            chainFrom(Board::fromStartPos().toFen(),
                      {"e4", "e5", "Nf3", "Nc6", "Bc4", "Bc5"});
        ASSERT_EQ(moves.size(), 6u);
        const auto flags = sacrificePlies(Board::fromStartPos(), moves);
        for (bool flag : flags) EXPECT_FALSE(flag);
    }

    // A plain knight trade: Nxe... Nxc6 bxc6 — net zero, not a sacrifice.
    {
        const auto moves = chainFrom(
            "8/1p6/2n5/4N3/8/8/8/K2k4 w - - 0 1", {"Nxc6", "bxc6"});
        ASSERT_EQ(moves.size(), 2u);
        const auto flags = sacrificePlies(*Board::fromFen(
            "8/1p6/2n5/4N3/8/8/8/K2k4 w - - 0 1"), moves);
        ASSERT_EQ(flags.size(), 2u);
        EXPECT_FALSE(flags[0]); // white won a knight, lost a knight: net 0
        EXPECT_FALSE(flags[1]); // black traded pawn for knight: net 2... no —
    }

    // A true give-away: 1. Nf6?? 2... gxf6 — the knight hangs to a pawn and
    // nothing is captured in return. The pair net clears kSacrificeNet.
    {
        Board start = *Board::fromFen("6k1/6p1/8/7N/8/8/8/K6R w - - 0 1");
        const auto moves = chainFrom(start.toFen(), {"Nf6", "gxf6"});
        ASSERT_EQ(moves.size(), 2u);
        const auto flags = sacrificePlies(start, moves);
        ASSERT_EQ(flags.size(), 2u);
        EXPECT_TRUE(flags[0]);  // the knight drop
        EXPECT_FALSE(flags[1]); // the pawn recapture itself wins material
    }

    // The final ply can have no reply pair — it is never flagged.
    {
        Board start = *Board::fromFen("6k1/6p1/8/7N/8/8/8/K6R w - - 0 1");
        const auto moves = chainFrom(start.toFen(), {"Nf6"});
        const auto flags = sacrificePlies(start, moves);
        ASSERT_EQ(flags.size(), 1u);
        EXPECT_FALSE(flags[0]);
    }
}

TEST(MoveQuality, MoverViewNegatesTheOpponentSeat)
{
    // Plain cp: the opponent sits on +100 → the mover sees -100.
    EXPECT_EQ(moverViewOf(scored(100)), -100);

    // Signed mate collapse: the opponent mates in 2 → the mover is mated;
    // the opponent is mated in 3 → the mover delivers.
    NodeResult oppMating;
    oppMating.eval.isMate = true;
    oppMating.eval.mateIn = 2;
    oppMating.valid = true;
    EXPECT_EQ(moverViewOf(oppMating), -kMateCp);

    NodeResult oppMated = oppMating;
    oppMated.eval.mateIn = -3;
    EXPECT_EQ(moverViewOf(oppMated), kMateCp);

    // An unscored node contributes nothing.
    NodeResult invalid;
    invalid.valid = false;
    EXPECT_EQ(moverViewOf(invalid), 0);
}

TEST(MoveQuality, StreamFromReportBuildsMoverSeatPairs)
{
    // Two-ply game 1. e4 e5. Node fens chain from the start; every node
    // scored dead-equal so only the pairing math is exercised.
    Board beforeE4 = Board::fromStartPos();
    Board afterE4 = beforeE4;
    {
        auto m = san::fromSan(afterE4, "e4");
        ASSERT_TRUE(m);
        afterE4.makeMove(*m);
    }
    Board afterE5 = afterE4;
    {
        auto m = san::fromSan(afterE5, "e5");
        ASSERT_TRUE(m);
        afterE5.makeMove(*m);
    }

    EvalReport report;
    report.nodes = {node(beforeE4.toFen(), "e4"),
                    node(afterE4.toFen(), "e5")};
    report.evals = {scored(30), scored(0)};

    // Node 0's seat: White (mover of e4) before 30; after-node 1's seat is
    // Black → the mover's view is -0 = 0. Improvement? 30 given 0 stays.
    auto stream = streamFromReport(report, {"e4", "e5"}, {false, false});
    ASSERT_EQ(stream.size(), 2u);
    EXPECT_EQ(stream[0].mover, Color::White);
    EXPECT_EQ(stream[0].cpBefore, 30);
    EXPECT_EQ(stream[0].cpAfter, 0);
    // Ply 1: black's seat before 0; after-node doesn't exist — a non-mate
    // SAN keeps before (delta 0).
    EXPECT_EQ(stream[1].mover, Color::Black);
    EXPECT_EQ(stream[1].cpBefore, 0);
    EXPECT_EQ(stream[1].cpAfter, 0);
}

TEST(MoveQuality, StreamFromReportFinishesMateAndRefusesGaps)
{
    Board beforeQf7 = *Board::fromFen(
        "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5Q2/PPPP1PPP/RNB1K1NR w KQkq - 0 1");
    Board afterQf7 = beforeQf7;
    {
        auto m = san::fromSan(afterQf7, "Qxf7#");
        ASSERT_TRUE(m);
        afterQf7.makeMove(*m);
    }

    EvalReport report;
    report.nodes = {node(beforeQf7.toFen(), "Qxf7#")};
    report.evals = {scored(900)};

    // A mate SAN's final ply pins the after-view at full mate for the
    // mover (who delivered it).
    auto stream = streamFromReport(report, {"Qxf7#"}, {false});
    ASSERT_EQ(stream.size(), 1u);
    EXPECT_EQ(stream[0].cpBefore, 900);
    EXPECT_EQ(stream[0].cpAfter, kMateCp);
    EXPECT_EQ(stream[0].mover, Color::White);

    // Any invalid eval rejects the whole report.
    EvalReport broken = report;
    broken.nodes.push_back(node(afterQf7.toFen(), "junk"));
    broken.evals.push_back(scored(0));
    broken.evals[1].valid = false;
    EXPECT_TRUE(streamFromReport(broken, {"Qxf7#", "junk"}, {false, false})
                    .empty());

    // Shape mismatch (evals vs nodes) is refused too.
    EvalReport wrongShape;
    wrongShape.nodes = report.nodes;
    wrongShape.evals = {scored(0), scored(0), scored(0)};
    wrongShape.evals[0].valid = true;
    wrongShape.evals[1].valid = true;
    wrongShape.evals[2].valid = true;
    EXPECT_TRUE(streamFromReport(wrongShape, {"Qxf7#"}, {false}).empty());
}

// --- 16.4 chip layer ------------------------------------------------------------

TEST(MoveQuality, StreamFromReportStampsTheBestMoveEcho)
{
    // The engine's own choice echoing the played SAN marks isBest; a
    // different (or absent) best SAN leaves it false.
    Board beforeE4 = Board::fromStartPos();
    Board afterE4 = beforeE4;
    {
        auto m = san::fromSan(afterE4, "e4");
        ASSERT_TRUE(m);
        afterE4.makeMove(*m);
    }

    EvalReport echo;
    echo.nodes = {node(beforeE4.toFen(), "e4")};
    NodeResult r = scored(10);
    r.eval.bestSan = "e4";
    echo.evals = {r};
    auto stream = streamFromReport(echo, {"e4"}, {false});
    ASSERT_EQ(stream.size(), 1u);
    EXPECT_TRUE(stream[0].isBest);

    r.eval.bestSan = "d4";
    echo.evals = {r};
    stream = streamFromReport(echo, {"e4"}, {false});
    ASSERT_EQ(stream.size(), 1u);
    EXPECT_FALSE(stream[0].isBest);

    r.eval.bestSan.clear();
    echo.evals = {r};
    stream = streamFromReport(echo, {"e4"}, {false});
    ASSERT_EQ(stream.size(), 1u);
    EXPECT_FALSE(stream[0].isBest);
}

TEST(MoveQuality, ClassLayerMarksClassesAndGoodStaysUnmarked)
{
    // A dead-quiet two-ply game classifies Good on both plies -> unmarked.
    {
        Board beforeE4 = Board::fromStartPos();
        Board afterE4 = beforeE4;
        {
            auto m = san::fromSan(afterE4, "e4");
            ASSERT_TRUE(m);
            afterE4.makeMove(*m);
        }
        EvalReport report;
        report.nodes = {node(beforeE4.toFen(), "e4"),
                        node(afterE4.toFen(), "e5")};
        report.evals = {scored(10), scored(0)};

        auto layer = classLayerFor(report, Board::fromStartPos(),
                                   chainFrom(Board::fromStartPos().toFen(),
                                             {"e4", "e5"}),
                                   {"e4", "e5"}, {});
        ASSERT_EQ(layer.size(), 2u);
        EXPECT_EQ(layer[0], -1);
        EXPECT_EQ(layer[1], -1);

        // Book stamps over the verdict: the bare "e4" umbrella labels ply 0.
        auto bookLayer = classLayerFor(report, Board::fromStartPos(),
                                       chainFrom(Board::fromStartPos().toFen(),
                                                 {"e4", "e5"}),
                                       {"e4", "e5"}, {true, false});
        ASSERT_EQ(bookLayer.size(), 2u);
        EXPECT_EQ(bookLayer[0], static_cast<signed char>(MoveClass::Book));
    }

    // A real blunder surfaces with its ordinal: 1. f3 e5 2. g4?? Qh4#.
    {
        const std::vector<std::string> sans = {"f3", "e5", "g4", "Qh4#"};
        const auto moves =
            chainFrom(Board::fromStartPos().toFen(), sans);
        ASSERT_EQ(moves.size(), 4u);

        // Node fens chain from the start; the g4 node's cp crash flags it.
        std::vector<Board> boards;
        Board b = Board::fromStartPos();
        for (const auto& san : sans) {
            auto m = san::fromSan(b, san);
            ASSERT_TRUE(m);
            b.makeMove(*m);
            boards.push_back(b);
        }
        EvalReport report;
        report.nodes = {node(Board::fromStartPos().toFen(), "f3"),
                        node(boards[0].toFen(), "e5"),
                        node(boards[1].toFen(), "g4"),
                        node(boards[2].toFen(), "Qh4#")};
        report.evals = {scored(0), scored(0), scored(-50), scored(900)};

        auto layer = classLayerFor(report, Board::fromStartPos(), moves,
                                   sans, {});
        ASSERT_EQ(layer.size(), 4u);
        EXPECT_EQ(layer[2], static_cast<signed char>(MoveClass::Blunder));
        // The mating reply itself is no class (mover gains, not gives).
        EXPECT_EQ(layer[3], -1);
    }

    // Refusals mirror streamFromReport: shape mismatch -> empty.
    {
        EvalReport report;
        report.nodes = {node(Board::fromStartPos().toFen(), "e4")};
        report.evals = {scored(10), scored(10)};
        report.evals[0].valid = true;
        report.evals[1].valid = true;
        EXPECT_TRUE(classLayerFor(report, Board::fromStartPos(),
                                  chainFrom(Board::fromStartPos().toFen(),
                                            {"e4"}),
                                  {"e4"}, {}).empty());
    }
}
