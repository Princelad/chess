#pragma once

#include <cstdint>
#include <optional>

#include <chess/net/messages.h>
#include <chess/types.h>

namespace chess::server {

// One side's shared clock pair: the authoritative remaining time for
// both players plus which side is on the move. Pure math — the caller
// supplies monotonic timestamps, so it is directly unit-testable. The
// wire form and its parser live in chess::net (messages.h).
//
// White's clock starts running when start() is called (game start per
// custom); after a completed move the mover is charged the elapsed time,
// receives the Fischer increment, and the opponent's clock starts.
class MatchClock {
public:
    MatchClock() = default; // unlimited: passive until armed with start()

    bool active() const { return !tc_.unlimited; }
    const chess::net::TimeControl& timeControl() const { return tc_; }

    Color onMove() const { return onMove_; }

    // Arms the clock for a fresh game (base time each) with `first` on move.
    void start(const chess::net::TimeControl& tc, Color first, std::int64_t nowMs);

    // Live remaining time of a side, subtracting an ongoing turn. Between
    // two timestamps the figures drift, so callers pass one consistent now.
    std::int64_t remainingMs(Color side, std::int64_t nowMs) const;

    // Charges `mover` for the elapsed turn, applies the Fischer increment
    // (even on the move that leaves base time), hands the turn to the
    // opponent, and returns the mover's post-increment remaining time —
    // the figure to broadcast to both clients with the move.
    void completedMove(Color mover, std::int64_t nowMs,
                       std::int64_t& moverRemainingOut);

    // The side to move has run out of time, if any. Checked by the server
    // loop between moves; never flags an unlimited or idle clock.
    std::optional<Color> checkFlag(std::int64_t nowMs) const;

private:
    // Elapsed running time of the current turn, from nowMs (callers pick
    // one time source; the class never reads a clock itself).
    std::int64_t elapsedOfCurrentTurnMs(std::int64_t nowMs) const;

    chess::net::TimeControl tc_;
    std::int64_t remainingMs_[2] = {0, 0};
    Color onMove_ = Color::White;
    bool started_ = false;
    std::int64_t turnStartMs_ = 0;
    std::int64_t incrementMs_ = 0;
};

} // namespace chess::server
