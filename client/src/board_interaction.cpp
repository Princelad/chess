#include "board_interaction.h"

#include <algorithm>

namespace chess::client {

void BoardAnnotations::addArrow(int fromFile, int fromRank, int toFile, int toRank)
{
    for (auto& a : arrows_) {
        if (a.from == std::make_pair(fromFile, fromRank) &&
            a.to == std::make_pair(toFile, toRank))
            return;
    }
    arrows_.push_back({ { fromFile, fromRank }, { toFile, toRank } });
}

void BoardAnnotations::toggleCircle(int file, int rank)
{
    auto it = std::find(circles_.begin(), circles_.end(), std::make_pair(file, rank));
    if (it != circles_.end())
        circles_.erase(it);
    else
        circles_.push_back({ file, rank });
}

void BoardAnnotations::clear()
{
    arrows_.clear();
    circles_.clear();
}

DragTracker::DragTracker(float threshold)
    : threshold_(threshold)
{
}

void DragTracker::press(float x, float y)
{
    pressX_ = x;
    pressY_ = y;
    x_ = x;
    y_ = y;
    phase_ = DragPhase::Tracking;
}

void DragTracker::move(float x, float y)
{
    x_ = x;
    y_ = y;
    if (phase_ != DragPhase::Tracking) return;

    float dx = x_ - pressX_;
    float dy = y_ - pressY_;
    if (dx * dx + dy * dy >= threshold_ * threshold_)
        phase_ = DragPhase::Dragging;
}

bool DragTracker::release()
{
    bool wasDragging = phase_ == DragPhase::Dragging;
    phase_ = DragPhase::Idle;
    return wasDragging;
}

void DragTracker::cancel()
{
    phase_ = DragPhase::Idle;
}

} // namespace chess::client