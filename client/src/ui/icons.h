#pragma once

// 14.2 Icon system — a small procedural registry, drawn with SFML
// primitives. No sprite/svg asset dependencies: the whole set is ours
// outright (the piece sets remain the project's external-art pattern,
// with README credits; icons intentionally stay self-contained since
// SFML 3 cannot rasterize SVG and PNG atlases fight the type-scale
// sizing rule).
//
// Sizing always derives from the UI text it accompanies — never
// hand-pick icon pixel sizes. `iconBox()` maps a type-scale font size
// to the icon square, `iconSlotRect()`/`iconTextRect()` are the pure
// layout math a control uses to reserve the leading slot (unit-tested
// headlessly; the draw path itself needs a render target).
//
// Icons are decorative companions to text: a Button with an icon still
// renders its label (the 6.2.2 letter-fallback discipline), and a label
// too wide for its remaining space drops the icon rather than overlap.

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/ConvexShape.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace chess::client::ui::icons {

enum class IconKind
{
    Clock,     // chess clock figure (HUD player cards)
    Flag,      // resignation
    HandDraw,  // draw offer
    Flip,      // board orientation (affordance lands with 15.x cards)
    Bulb,      // puzzle hint
    Medal,     // tournament/leaderboard achievement
    Eye,       // spectator watcher count (12.4)
    ArrowBack, // leave / back to menu
};

constexpr int iconKindCount() { return 8; }

// Icon square for text of `textSizePx` (e.g. ui::type::Heading -> ~23px).
inline float iconBox(unsigned textSizePx)
{
    return std::round(static_cast<float>(textSizePx) * 1.15f);
}

// Leading icon slot inside a control rect: a vertically centered square
// inset from the left by the same margin. Always inside the rect — for
// very short controls the box shrinks to fit.
inline sf::FloatRect iconSlotRect(sf::FloatRect rect, unsigned textSizePx)
{
    float box = iconBox(textSizePx);
    box = std::min(box, rect.size.y);
    const float insetY = (rect.size.y - box) * 0.5f;
    const float insetX = std::min(insetY, rect.size.x * 0.25f);
    return sf::FloatRect({rect.position.x + insetX, rect.position.y + insetY},
                         {box, box});
}

// What remains of the control for the label once the slot is reserved.
inline sf::FloatRect iconTextRect(sf::FloatRect rect, unsigned textSizePx)
{
    const sf::FloatRect slot = iconSlotRect(rect, textSizePx);
    const float left = slot.position.x + slot.size.x;
    return sf::FloatRect({left, rect.position.y},
                         {rect.position.x + rect.size.x - left, rect.size.y});
}

// Draws one icon filling the square [topLeft, topLeft + (size, size)].
inline void drawIcon(sf::RenderTarget& target, IconKind kind,
                     sf::Vector2f topLeft, float size, sf::Color color)
{
    const float u = size;

    const auto rect = [&](float x, float y, float w, float h) {
        sf::RectangleShape shape({w, h});
        shape.setPosition({topLeft.x + x, topLeft.y + y});
        shape.setFillColor(color);
        target.draw(shape);
    };
    const auto tri = [&](float ax, float ay, float bx, float by,
                         float cx, float cy) {
        sf::ConvexShape shape(3);
        shape.setPoint(0, {topLeft.x + ax, topLeft.y + ay});
        shape.setPoint(1, {topLeft.x + bx, topLeft.y + by});
        shape.setPoint(2, {topLeft.x + cx, topLeft.y + cy});
        shape.setFillColor(color);
        target.draw(shape);
    };
    const auto circle = [&](float cx, float cy, float r) {
        sf::CircleShape shape(r);
        shape.setPosition({topLeft.x + cx - r, topLeft.y + cy - r});
        shape.setFillColor(color);
        target.draw(shape);
    };
    const auto ring = [&](float cx, float cy, float r, float thickness) {
        sf::CircleShape shape(r);
        shape.setPosition({topLeft.x + cx - r, topLeft.y + cy - r});
        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineColor(color);
        shape.setOutlineThickness(thickness);
        target.draw(shape);
    };
    // Coordinates below are fractions of `u`.

    switch (kind) {
    case IconKind::Clock: {
        ring(0.50f * u, 0.50f * u, 0.40f * u, 0.09f * u);
        rect(0.455f * u, 0.15f * u, 0.09f * u, 0.35f * u); // minute hand (up)
        rect(0.455f * u, 0.455f * u, 0.33f * u, 0.09f * u); // hour hand (right)
        break;
    }
    case IconKind::Flag: {
        rect(0.22f * u, 0.06f * u, 0.07f * u, 0.88f * u); // pole
        tri(0.29f * u, 0.08f * u, 0.80f * u, 0.24f * u, 0.29f * u, 0.40f * u);
        break;
    }
    case IconKind::HandDraw: {
        rect(0.26f * u, 0.14f * u, 0.10f * u, 0.36f * u); // finger 1
        rect(0.40f * u, 0.08f * u, 0.10f * u, 0.42f * u); // finger 2
        rect(0.54f * u, 0.16f * u, 0.10f * u, 0.34f * u); // finger 3
        rect(0.23f * u, 0.46f * u, 0.54f * u, 0.28f * u); // palm
        tri(0.77f * u, 0.50f * u, 0.90f * u, 0.70f * u, 0.70f * u, 0.74f * u);
        break;
    }
    case IconKind::Flip: {
        rect(0.27f * u, 0.32f * u, 0.12f * u, 0.38f * u); // shaft up
        tri(0.21f * u, 0.36f * u, 0.45f * u, 0.36f * u, 0.33f * u, 0.12f * u);
        rect(0.61f * u, 0.30f * u, 0.12f * u, 0.38f * u); // shaft down
        tri(0.55f * u, 0.64f * u, 0.79f * u, 0.64f * u, 0.67f * u, 0.88f * u);
        break;
    }
    case IconKind::Bulb: {
        circle(0.50f * u, 0.36f * u, 0.26f * u); // filled glass silhouette
        rect(0.38f * u, 0.64f * u, 0.24f * u, 0.12f * u); // base
        rect(0.43f * u, 0.78f * u, 0.14f * u, 0.08f * u); // cap
        break;
    }
    case IconKind::Medal: {
        tri(0.26f * u, 0.04f * u, 0.47f * u, 0.04f * u, 0.33f * u, 0.38f * u);
        tri(0.53f * u, 0.04f * u, 0.74f * u, 0.04f * u, 0.67f * u, 0.38f * u);
        ring(0.50f * u, 0.63f * u, 0.21f * u, 0.075f * u);
        circle(0.50f * u, 0.63f * u, 0.085f * u);
        break;
    }
    case IconKind::Eye: {
        const float r = 0.235f * u;
        sf::CircleShape lens(r);
        // A horizontally stretched circle reads as the almond lens; the
        // position compensates so the stretched shape stays centered.
        lens.setScale({1.55f, 1.f});
        lens.setPosition({topLeft.x + 0.5f * u - r * 1.55f, topLeft.y + 0.5f * u - r});
        lens.setFillColor(sf::Color::Transparent);
        lens.setOutlineColor(color);
        lens.setOutlineThickness(0.07f * u);
        target.draw(lens);
        circle(0.50f * u, 0.50f * u, 0.11f * u); // pupil
        break;
    }
    case IconKind::ArrowBack: {
        tri(0.04f * u, 0.50f * u, 0.36f * u, 0.26f * u, 0.36f * u, 0.74f * u);
        rect(0.32f * u, 0.43f * u, 0.62f * u, 0.14f * u);
        break;
    }
    }
}

} // namespace chess::client::ui::icons
