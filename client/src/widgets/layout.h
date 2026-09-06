#pragma once

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace chess::client::layout {

// Shrink a rect by a uniform margin on all sides (clamped to positive size).
inline sf::FloatRect inset(const sf::FloatRect& rect, float margin)
{
    float w = std::max(0.f, rect.size.x - 2.f * margin);
    float h = std::max(0.f, rect.size.y - 2.f * margin);
    return sf::FloatRect(
        sf::Vector2f(rect.position.x + margin, rect.position.y + margin),
        sf::Vector2f(w, h));
}

// Return a rect of the given size centered inside `area`.
inline sf::FloatRect centerIn(const sf::FloatRect& area, sf::Vector2f size)
{
    float x = area.position.x + (area.size.x - size.x) / 2.f;
    float y = area.position.y + (area.size.y - size.y) / 2.f;
    return sf::FloatRect(sf::Vector2f(x, y), size);
}

// Lay out items vertically inside `area`. Each entry in `sizes` is one item;
// returns a rect per item in order. Items narrower than the area are centered
// horizontally.
inline std::vector<sf::FloatRect> vstack(const sf::FloatRect& area, float gap,
                                         const std::vector<sf::Vector2f>& sizes)
{
    std::vector<sf::FloatRect> out;
    out.reserve(sizes.size());

    float totalH = 0.f;
    for (const auto& s : sizes) {
        totalH += s.y;
        if (!out.empty()) totalH += gap;
    }

    float y = area.position.y + (area.size.y - totalH) / 2.f;
    for (const auto& s : sizes) {
        float w = std::min(s.x, area.size.x);
        float x = area.position.x + (area.size.x - w) / 2.f;
        out.emplace_back(sf::FloatRect(sf::Vector2f(x, y), sf::Vector2f(w, s.y)));
        y += s.y + gap;
    }
    return out;
}

// Lay out items horizontally inside `area`. Each entry in `sizes` is one item;
// returns a rect per item in order. Items shorter than the area are centered
// vertically.
inline std::vector<sf::FloatRect> hstack(const sf::FloatRect& area, float gap,
                                         const std::vector<sf::Vector2f>& sizes)
{
    std::vector<sf::FloatRect> out;
    out.reserve(sizes.size());

    float totalW = 0.f;
    for (const auto& s : sizes) {
        totalW += s.x;
        if (!out.empty()) totalW += gap;
    }

    float x = area.position.x + (area.size.x - totalW) / 2.f;
    for (const auto& s : sizes) {
        float h = std::min(s.y, area.size.y);
        float y = area.position.y + (area.size.y - h) / 2.f;
        out.emplace_back(sf::FloatRect(sf::Vector2f(x, y), sf::Vector2f(s.x, h)));
        x += s.x + gap;
    }
    return out;
}

} // namespace chess::client::layout