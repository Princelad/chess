#include "move_quality.h"

#include "eco.h"

#include <algorithm>
#include <cmath>

namespace chess::client::quality {

float winPercent(int cp)
{
    // Logistic mapping; signed mate (±kMateCp) pins at 100/0 because
    // kWinSigmoid * 10000 is dozens of e-foldings past the knee.
    const double mapped = std::exp(-kWinSigmoid * static_cast<double>(cp));
    double win = 100.0 / (1.0 + mapped);
    // Snap the rails: far-tail floats land ~1e-14 off the clamp, never 0/100.
    if (mapped <= 1e-12) win = 100.0;
    if (mapped >= 1e12) win = 0.0;
    return static_cast<float>(std::clamp(win, 0.0, 100.0));
}

float moveWinGivenUp(const MoveEval& move)
{
    const float before = winPercent(move.cpBefore);
    const float after = winPercent(move.cpAfter);
    return std::clamp(before - after, 0.0f, 100.0f);
}

MoveClass classifyMove(const MoveEval& move)
{
    // Precedence (documented): book first (a listed opening move keeps its
    // label no matter its quality), then Brilliant (best + material given
    // away on purpose), then Best (the engine's own choice), then the
    // given-up thresholds, then plain Good.
    if (move.isBook) return MoveClass::Book;
    if (move.isBest) return move.isSacrifice ? MoveClass::Brilliant
                                             : MoveClass::Best;

    const float givenUp = moveWinGivenUp(move);
    if (givenUp >= kBlunderGive) return MoveClass::Blunder;
    if (givenUp >= kMistakeGive) return MoveClass::Mistake;
    if (givenUp >= kInaccuracyGive) return MoveClass::Inaccuracy;
    return MoveClass::Good;
}

float moveAccuracy(float winGivenUp)
{
    const double accuracy =
        kAccScale * std::exp(-kAccDecay * static_cast<double>(winGivenUp))
        - kAccLift;
    // Rail snaps: the authentic constant pair computes 99.9999 at zero
    // given-up (accuracy(0) displays as 100) and underflows far decay to 0.
    double snapped = accuracy;
    if (snapped >= 99.999) snapped = 100.0;
    if (snapped <= 0.001) snapped = 0.0;
    return static_cast<float>(std::clamp(snapped, 0.0, 100.0));
}

MoveQuality qualityOf(const MoveEval& move)
{
    MoveQuality out;
    out.winBefore = winPercent(move.cpBefore);
    out.winAfter = winPercent(move.cpAfter);
    out.givenUp = std::clamp(out.winBefore - out.winAfter, 0.0f, 100.0f);
    out.klass = classifyMove(move);
    out.accuracy = moveAccuracy(out.givenUp);
    return out;
}

GameQuality evaluateGame(const MoveEvalStream& moves)
{
    GameQuality game;
    game.moves.reserve(moves.size());

    struct SideSum
    {
        SideQuality* quality;
        int plies = 0;
    };
    SideSum sides[2] = {{&game.white, 0}, {&game.black, 0}};

    for (const MoveEval& move : moves) {
        MoveQuality quality = qualityOf(move);
        SideSum& side = sides[move.mover == Color::White ? 0 : 1];
        side.quality->accuracy += quality.accuracy;
        side.quality->counts[static_cast<std::size_t>(quality.klass)]++;
        ++side.plies;
        game.moves.push_back(quality);
    }
    for (SideSum& side : sides) {
        side.quality->accuracy = side.plies == 0
            ? 0.f
            : side.quality->accuracy / static_cast<float>(side.plies);
    }
    return game;
}

std::vector<bool> bookPlies(const std::vector<std::string>& sans)
{
    std::vector<bool> book(sans.size(), false);
    for (std::size_t ply = 0; ply < sans.size(); ++ply) {
        // The move must itself sit inside a listed line: a bare umbrella
        // ("e4") labels the OPENING for the game's whole life (11.5), but
        // book-ness ends the moment the history outruns the table match.
        const std::vector<std::string> prefix(
            sans.begin(), sans.begin() + static_cast<long>(ply) + 1);
        book[ply] = ecoLongestMatch(prefix).len >= ply + 1;
    }
    return book;
}

} // namespace chess::client::quality
