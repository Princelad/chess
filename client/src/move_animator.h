#pragma once

#include <chess/board.h>
#include <chess/move.h>
#include <chess/types.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace chess::client {

// Time-based, frame-independent piece-slide animation (phase 9.7). The
// board is drawn from its post-move state; this captures the moving piece
// beforehand and produces an eased 0..1 progress driven by the fixed
// timestep. A zero/negative duration (or disabled animation) makes it
// complete instantly (no slide).
class MoveAnimator {
public:
    struct Step {
        chess::Square from;
        chess::Square to;
        chess::Piece piece;
    };

    MoveAnimator() = default;

    // Capture a slide for `move` (with the piece sitting on the board just
    // before it is applied). `duration` is in seconds; <= 0 means instant.
    void start(const chess::Move& move, const Board& board, float duration)
    {
        steps_.clear();
        elapsed_ = 0.f;
        duration_ = duration <= 0.f ? 0.f : duration;

        steps_.push_back({move.from, move.to, board.pieceAt(move.from)});
        if (move.isCastle()) {
            const int rank = static_cast<int>(chess::rankOf(move.from));
            const int fromFile = static_cast<int>(chess::fileOf(move.from));
            if (static_cast<int>(chess::fileOf(move.to)) > fromFile) {
                // King castled kingside: rook from h-file to the king's old file.
                const chess::Square rookFrom = chess::squareOf(7, rank);
                steps_.push_back({rookFrom, chess::squareOf(fromFile + 1, rank),
                                  board.pieceAt(rookFrom)});
            } else {
                // Queenside: rook from a-file to the king's old file - 1.
                const chess::Square rookFrom = chess::squareOf(0, rank);
                steps_.push_back({rookFrom, chess::squareOf(fromFile - 1, rank),
                                  board.pieceAt(rookFrom)});
            }
        }
    }

    void update(float dt)
    {
        if (!active()) return;
        elapsed_ += dt;
        if (elapsed_ >= duration_) elapsed_ = duration_;
    }

    bool active() const { return duration_ > 0.f && elapsed_ < duration_; }

    // Eased progress in [0, 1]. 0 when idle (no move started).
    float progress() const
    {
        if (steps_.empty()) return 0.f;
        if (duration_ <= 0.f) return 1.f;
        const float t = std::clamp(elapsed_ / duration_, 0.f, 1.f);
        return smoothstep(t);
    }

    const std::vector<Step>& steps() const { return steps_; }
    chess::Piece piece() const
    {
        return steps_.empty() ? chess::Piece::None() : steps_.front().piece;
    }

    bool empty() const { return steps_.empty(); }

    static float smoothstep(float t)
    {
        const float x = std::clamp(t, 0.f, 1.f);
        return x * x * (3.f - 2.f * x);
    }

private:
    std::vector<Step> steps_;
    float elapsed_ = 0.f;
    float duration_ = 0.f;
};

} // namespace chess::client