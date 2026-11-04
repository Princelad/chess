#pragma once

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>

#include <cmath>
#include <cstdint>

namespace chess::client::ui {
namespace spinner {

// 15.5 "waiting" spinner: 8 ticks on a small circle whose brightness
// pattern sweeps around it, so the wheel reads as rotating while any
// engine/server think is in flight. Pure alpha math below, geometry is
// just ticks — no assets, no timers (the caller feeds seconds).
inline constexpr int TickCount = 8;

// One sweep of the head around the wheel, in seconds (used by callers
// to advance their phase; tickAlpha itself accepts any phase value).
inline constexpr float SweepSec = 0.9f;

// Pure: brightness of tick `tick` at rotation phase `phase` (seconds /
// SweepSec; unwrapped — fmod handles any value). The tick whose slot
// matches the phase holds the head at full alpha and brightness falls
// off smoothly with circular slot distance down to the 0.15 tail floor,
// giving the wheel a clear leading edge. Monotonic from head to tail.
inline float tickAlpha(float phase, int tick)
{
    phase -= std::floor(phase); // phase -> [0,1)
    const float slot = static_cast<float>((tick % TickCount + TickCount)
                       % TickCount) / static_cast<float>(TickCount);
    float d = phase - slot;
    d -= std::floor(d);             // circular distance in [0,1)
    // Smooth head->tail ramp: d=0 -> 1.0, d=0.5 -> 0.575, d->1 -> 0.15.
    return 0.15f + 0.85f * (0.5f + 0.5f * std::cos(d * 3.14159265f));
}

} // namespace spinner

// Draws the wheel: ticks sit at fixed angles (head starts at 12 o'clock
// and advances clockwise with the phase); alpha does the rotating.
inline void drawSpinner(sf::RenderWindow& window, sf::Vector2f center,
                        float radius, float phase, sf::Color color)
{
    for (int k = 0; k < spinner::TickCount; ++k) {
        const float angle = -1.57079632f
            + static_cast<float>(k) * 0.785398163f; // 2*pi/8
        const sf::Vector2f pos = {
            center.x + std::cos(angle) * radius,
            center.y + std::sin(angle) * radius,
        };
        const float a = spinner::tickAlpha(phase, k);
        sf::Color c = color;
        c.a = static_cast<std::uint8_t>(255.f * a);
        const float r = radius * 0.26f;
        sf::CircleShape tick(r);
        tick.setOrigin({ r, r });
        tick.setPosition(pos);
        tick.setFillColor(c);
        window.draw(tick);
    }
}

} // namespace chess::client::ui
