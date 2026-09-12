#pragma once

#include <chess/move.h>
#include <chess/types.h>

#include <optional>
#include <utility>
#include <vector>

namespace chess::client {

struct AnnotatedArrow {
    std::pair<int, int> from;
    std::pair<int, int> to;
};

class BoardAnnotations {
public:
    void addArrow(int fromFile, int fromRank, int toFile, int toRank);
    void toggleCircle(int file, int rank);
    void clear();

    bool empty() const { return arrows_.empty() && circles_.empty(); }
    const std::vector<AnnotatedArrow>& arrows() const { return arrows_; }
    const std::vector<std::pair<int, int>>& circles() const { return circles_; }

private:
    std::vector<AnnotatedArrow> arrows_;
    std::vector<std::pair<int, int>> circles_;
};

enum class DragPhase {
    Idle,
    Tracking, // press seen, below activation threshold
    Dragging, // pointer moved beyond the activation threshold
};

class DragTracker {
public:
    DragTracker() = default;
    explicit DragTracker(float threshold);

    void press(float x, float y);
    void move(float x, float y);
    // Returns true if a full drag (Dragging) was in progress at release.
    bool release();
    void cancel();

    DragPhase phase() const { return phase_; }
    bool isActive() const { return phase_ != DragPhase::Idle; }
    bool isDragging() const { return phase_ == DragPhase::Dragging; }
    float pressX() const { return pressX_; }
    float pressY() const { return pressY_; }
    float x() const { return x_; }
    float y() const { return y_; }

private:
    float threshold_;
    float pressX_ = 0.f;
    float pressY_ = 0.f;
    float x_ = 0.f;
    float y_ = 0.f;
    DragPhase phase_ = DragPhase::Idle;
};

// Returns the Queen promotion move if present among the candidates.
inline std::optional<chess::Move> autoQueenMove(const std::vector<chess::Move>& candidates)
{
    for (const auto& m : candidates)
        if (m.promotion == PieceType::Queen)
            return m;
    return std::nullopt;
}

} // namespace chess::client