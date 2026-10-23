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
            // FRC-aware (12.1): the king always lands on the g-file
            // (king-side) or c-file (queen-side) and the rook on the
            // adjacent f-file / d-file — derivable from the king's
            // DESTINATION, not its origin, which sits anywhere in 960 (and
            // can be its own landing square: king-at-g1 castles are legal
            // Move{from == to, Castle}).
            const int rank = static_cast<int>(chess::rankOf(move.from));
            const int kingFromFile = static_cast<int>(chess::fileOf(move.from));
            const int kingDestFile = static_cast<int>(chess::fileOf(move.to));
            const bool kingSide = kingDestFile >= 6;
            const int rookToFile = kingSide ? 5 : 3; // f- or d-file
            // The participating rook flanks the king on the castling wing:
            // scan outward from the king for the first same-color rook.
            const chess::Piece king = board.pieceAt(move.from);
            const int dir = kingSide ? 1 : -1;
            for (int f = kingFromFile + dir; f >= 0 && f <= 7; f += dir) {
                const chess::Square rookFrom = chess::squareOf(f, rank);
                const chess::Piece piece = board.pieceAt(rookFrom);
                if (piece.type == PieceType::Rook && piece.color == king.color) {
                    steps_.push_back({rookFrom, chess::squareOf(rookToFile, rank), piece});
                    break;
                }
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

    // Drops any running slide: the position it animated no longer exists
    // (a takeback rebuilt the board underneath it).
    void cancel()
    {
        steps_.clear();
        elapsed_ = 0.f;
        duration_ = 0.f;
    }

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