// 16.2 unit tests — the canned MoveEvalStream fixtures drive the whole
// classifier; no engine, no I/O, single-threaded.

#include "move_quality.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace chess;
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
