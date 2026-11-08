#pragma once

#include <array>
#include <string>
#include <vector>

#include <chess/board.h>
#include <chess/move.h>
#include <chess/types.h> // Color

#include "eval_harness.h"

namespace chess::client::quality {

// --- 16.2 move classification + accuracy ------------------------------------
// Pure: eval inputs in, labels/accs out. No engine, no I/O — the canned
// fixture vectors in the tests drive everything, and threshold changes
// REQUIRE test updates (the 2.x perft-constants discipline).

// A move's before/after position scores, both from the MOVER's seat, both
// mate-collapsed (see kMateWinPercent below — signed mate never survives as
// a fraction). Flags ride from the caller: the engine's best-SAN echo marks
// isBest, a per-move material loss can mark isSacrifice, and 11.5's ECO
// prefix match marks isBook.
struct MoveEval
{
    Color mover = Color::White;
    int cpBefore = 0; // mover's seat before the move
    int cpAfter = 0;  // resulting position, still the mover's seat view

    bool isBest = false;      // played SAN == engine's best SAN
    bool isSacrifice = false; // the move gave away material on purpose
    bool isBook = false;      // still inside listed opening theory (11.5)
};

// A canned stream of these drives the whole classifier (TODO 16.2 namesake).
using MoveEvalStream = std::vector<MoveEval>;

enum class MoveClass
{
    Best,
    Brilliant,
    Good,
    Inaccuracy,
    Mistake,
    Blunder,
    Book,
};

inline constexpr int kClassCount = 7;

inline const char* moveClassName(MoveClass c)
{
    switch (c) {
        case MoveClass::Best: return "Best";
        case MoveClass::Brilliant: return "Brilliant";
        case MoveClass::Good: return "Good";
        case MoveClass::Inaccuracy: return "Inaccuracy";
        case MoveClass::Mistake: return "Mistake";
        case MoveClass::Blunder: return "Blunder";
        case MoveClass::Book: return "Book";
    }
    return "Good";
}

// --- tuning surface (test-pinned) --------------------------------------------

// Logistic win%-of-cp mapping (the Lichess/chess.com continuous equivalent).
inline constexpr double kWinSigmoid = 0.00368208;

// Signed mate collapses to this cp magnitude before the sigmoid — far past
// the curve's knee, so mates pin at 100/0 win% exactly.
inline constexpr int kMateCp = 10000;

// Classification thresholds, in win% GIVEN UP by the mover (winBefore −
// winAfter, clamped at 0). Deliberately ordered ~chess.com shape; any change
// requires updating tests/client/move_quality_test.cpp.
inline constexpr float kInaccuracyGive = 2.0f;
inline constexpr float kMistakeGive = 10.0f;
inline constexpr float kBlunderGive = 20.0f;

// Accuracy% for a single move: the classic exponential decay over the
// win% given up (accuracy(0) == 100 by construction, floor clamps to 0).
inline constexpr double kAccScale = 103.1668;
inline constexpr double kAccDecay = 0.04354;
inline constexpr double kAccLift = 3.1669;

float winPercent(int cp);      // mover's seat, clamped 0..100
float moveWinGivenUp(const MoveEval& move); // 0..100, clamped
MoveClass classifyMove(const MoveEval& move);      // one move
float moveAccuracy(float winGivenUp);              // 0..100

struct MoveQuality
{
    MoveClass klass = MoveClass::Good;
    float winBefore = 50.f;
    float winAfter = 50.f;
    float givenUp = 0.f;    // mover's seat
    float accuracy = 100.f; // this move's accuracy contribution
};

MoveQuality qualityOf(const MoveEval& move);

// Per-side aggregate: mean clamped accuracy over the side's plies and the
// class counts for the report legend.
struct SideQuality
{
    float accuracy = 0.f;
    std::array<int, kClassCount> counts{};
};

struct GameQuality
{
    std::vector<MoveQuality> moves; // parallel to the input stream
    SideQuality white;
    SideQuality black;
};

GameQuality evaluateGame(const MoveEvalStream& moves);

// --- book flags (11.5 ECO prefix dependency) ---------------------------------

// Per-ply book flags for a game's SAN list: a move is book when the longest
// ECO table match over its prefix reaches at least this move — so a listed
// mainline line keeps the flag while a bare umbrella ("e4") does not carry
// every later move of the game. Wrapper-free for canned-stream tests: the
// classifier itself takes isBook as input.
std::vector<bool> bookPlies(const std::vector<std::string>& sans);

// --- report stream (16.3): harness output → classifier input ------------------

// Piece values (P1/N3/B3/R5/Q9) summed for one color on a board.
int materialIn(const Board& board, Color color);

// Per-ply sacrifice flags from the board chain: the move costs the mover at
// least kSacrificeValue (3: a knight/bishop) of material versus the firm
// capture value of what it lands on — a receiver-side approximation, exact
// enough for the Brilliant label (the mate-in-two sacrificing bolt that the
// opponent captures on spot is the classic shape it detects).
std::vector<bool> sacrificePlies(const Board& initialBoard,
                                 const std::vector<chess::Move>& moves);

// Mover-seat view of a scored position. report.evals[i] is always judged in
// the seat that moves at node i; ply i's AFTER position is node i+1 where
// the OPPONENT sits, so the mover's view is the negation. Mate keeps the
// collapse: an opponent mating = the mover is mated (−kMateCp), the
// opponent already losing = the mover mating (+kMateCp).
int moverViewOf(const NodeResult& opponentSeatNode);

// Builds the classifier stream from a COMPLETE report (every ply valid;
// empty otherwise — evalNodes count must equal the SAN list length).
// The final move carries no after-node: a mate SAN pins its after at
// +kMateCp (the mover delivered), any other finish keeps before (the
// game result adds no information).
MoveEvalStream streamFromReport(const EvalReport& report,
                                const std::vector<std::string>& sans,
                                const std::vector<bool>& sacrifices);


} // namespace chess::client::quality
