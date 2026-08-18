#pragma once

#include <chess/types.h>

namespace chess {
namespace net {

enum class GameResult {
    WhiteWins,
    BlackWins,
    Draw,
    Resignation,
    Abort,
};

enum class GameOverReason {
    Checkmate,
    Stalemate,
    FiftyMove,
    Repetition,
    InsufficientMaterial,
    Resignation,
    Disconnection,
    Abort,
};

} // namespace net
} // namespace chess
