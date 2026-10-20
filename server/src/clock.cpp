#include "clock.h"

#include <algorithm>

namespace chess::server {

void MatchClock::start(const chess::net::TimeControl& tc, Color first, std::int64_t nowMs)
{
    tc_ = tc;
    onMove_ = first;
    incrementMs_ = tc.incrementSeconds * 1000;
    started_ = !tc.unlimited;
    // Unit boundary: base/increment are seconds, the clock only already
    // speaks milliseconds — *60000 per base side, *1000 per increment.
    remainingMs_[0] = tc.baseSeconds * 60000;
    remainingMs_[1] = tc.baseSeconds * 60000;
    turnStartMs_ = nowMs;
}

std::int64_t MatchClock::elapsedOfCurrentTurnMs(std::int64_t nowMs) const
{
    // Note: a stale nowMs (caller mixing time sources) can only ever
    // shorten the charge, never create time out of nothing.
    return started_ ? std::max<std::int64_t>(0, nowMs - turnStartMs_) : 0;
}

std::int64_t MatchClock::remainingMs(Color side, std::int64_t nowMs) const
{
    const std::int64_t base = remainingMs_[side == Color::White ? 0 : 1];
    // Only the side holding the move ticks; the waiting side stays frozen
    // at its last completed move's figure.
    if (!started_ || side != onMove_)
        return base;
    return std::max<std::int64_t>(0, base - elapsedOfCurrentTurnMs(nowMs));
}

void MatchClock::completedMove(Color mover, std::int64_t nowMs,
                               std::int64_t& moverRemainingOut)
{
    if (!started_)
        return;

    const std::size_t idx = mover == Color::White ? 0 : 1;
    const std::int64_t charged = elapsedOfCurrentTurnMs(nowMs);
    remainingMs_[idx] = std::max<std::int64_t>(0, remainingMs_[idx] - charged)
                        + incrementMs_;

    moverRemainingOut = remainingMs_[idx];

    onMove_ = mover == Color::White ? Color::Black : Color::White;
    turnStartMs_ = nowMs;
}

std::optional<Color> MatchClock::checkFlag(std::int64_t nowMs) const
{
    if (!started_)
        return std::nullopt;
    if (remainingMs(onMove_, nowMs) <= 0)
        return onMove_;
    return std::nullopt;
}

} // namespace chess::server
