#pragma once

// 14.5 runtime chrome — rounded rectangle shape for the component
// defaults (pill Buttons, rounded Panels, underlined TextFields).
// Header-only geometry over `sf::Shape`: the outline/fill rendering is
// SFML's, we only feed sampled corner arcs; straight edges are the
// implicit polygon chords between arc endpoints.
//
// Pure geometry only (no window needed to construct); the draw path is
// exercised visually like every other rendering surface in this suite.

#include <SFML/Graphics/Shape.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace chess::client::ui {

class RoundedRectShape : public sf::Shape {
public:
    explicit RoundedRectShape(sf::Vector2f size, float radius)
        : size_(size), radius_(radius)
    {
        clampRadius();
        update();
    }

    void setSize(sf::Vector2f size)
    {
        size_ = size;
        clampRadius();
        update();
    }

    void setRadius(float radius)
    {
        radius_ = radius;
        clampRadius();
        update();
    }

    std::size_t getPointCount() const override
    {
        return cornerCount * steps;
    }

    sf::Vector2f getPoint(std::size_t index) const override
    {
        // Traversed clockwise on screen (y down): TR, BR, BL, TL arcs at
        // base angles 270/0/90/180, with straight edges left implicit
        // between consecutive arc endpoints.
        const std::size_t corner = index / steps;
        const std::size_t step = index % steps;
        const float a = static_cast<float>(270.f + corner * 90.f
                                           + step * (90.f / static_cast<float>(steps)))
            * (3.14159265358979323846f / 180.f);

        const float r = radius_;
        const float cx = (corner == 0 || corner == 1) ? size_.x - r : r;
        const float cy = (corner == 0 || corner == 3) ? r : size_.y - r;

        return { cx + r * std::cos(a), cy + r * std::sin(a) };
    }

private:
    void clampRadius()
    {
        const float lim = std::min(size_.x, size_.y) * 0.5f;
        if (radius_ > lim) radius_ = lim;
        if (radius_ < 0.f) radius_ = 0.f;
    }

    static constexpr std::size_t cornerCount = 4;
    static constexpr std::size_t steps = 6; // arc samples per corner
    sf::Vector2f size_ = { 0.f, 0.f };
    float radius_ = 0.f;
};

} // namespace chess::client::ui
