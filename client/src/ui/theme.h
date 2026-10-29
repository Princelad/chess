#pragma once

// 14.1 Design tokens — the single source every screen and widget paints from.
//
// Values mirror the pre-14.1 hardcoded palette verbatim (pure refactor);
// the 14.4/14.5 visual refresh changes values HERE, not at the ~300 former
// call sites. Pure helper functions live here so color derivations are
// unit-testable without a window.

#include <SFML/Graphics/Color.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace chess::client::ui {

// ---------------------------------------------------------------------------
// Color helper primitives (pure, testable)
// ---------------------------------------------------------------------------

constexpr std::uint8_t clampByte(int v)
{
    return static_cast<std::uint8_t>(std::clamp(v, 0, 255));
}

// Linear interpolation between two colors. Alpha is weighted by `t` too, so
// callers can fade translucency the same way as RGB.
constexpr sf::Color mix(sf::Color a, sf::Color b, float t)
{
    t = std::clamp(t, 0.f, 1.f);
    auto chan = [&](std::uint8_t x, std::uint8_t y) {
        return clampByte(static_cast<int>(x + (static_cast<float>(y) - static_cast<float>(x)) * t));
    };
    return { chan(a.r, b.r), chan(a.g, b.g), chan(a.b, b.b), chan(a.a, b.a) };
}

// Scale toward white (amount > 0) or black (amount < 0); amount is a 0..1
// fraction of the remaining headroom. Opaque colors stay opaque.
constexpr sf::Color shade(sf::Color c, float amount)
{
    const sf::Color white{ 255, 255, 255, c.a };
    const sf::Color black{ 0, 0, 0, c.a };
    return amount >= 0.f ? mix(c, white, amount) : mix(c, black, -amount);
}

// Perceptual-ish grayscale proxy [0..1] (same coefficients as the old
// labelColorFor brightness check in themes.h).
constexpr float relativeLuminance(sf::Color c)
{
    return (static_cast<float>(c.r) * 0.299f +
            static_cast<float>(c.g) * 0.587f +
            static_cast<float>(c.b) * 0.114f) /
           255.f;
}

// WCAG contrast ratio between two colors [1..21]. Uses the flat luminance
// proxy above (good enough for our light/dark pairing checks, far cheaper
// than the full sRGB pipeline).
constexpr float contrastRatio(sf::Color a, sf::Color b)
{
    const float la = relativeLuminance(a) + 0.05f;
    const float lb = relativeLuminance(b) + 0.05f;
    const float ratio = la > lb ? la / lb : lb / la;
    return ratio;
}

// Dark text on light backgrounds, light text on dark ones.
constexpr sf::Color readableTextOn(sf::Color background)
{
    return relativeLuminance(background) > 0.6f ? sf::Color{ 40, 40, 40 } : sf::Color{ 245, 245, 245 };
}

// ---------------------------------------------------------------------------
// Color palette
// ---------------------------------------------------------------------------

namespace pal {
    // Dark background family (app clear color = warm graphite)
    inline constexpr sf::Color bgApp{ 48, 46, 43 };
    inline constexpr sf::Color bgDeep{ 30, 30, 30 };      // chat log, overlays
    inline constexpr sf::Color bgSunken{ 25, 25, 25 };    // navigator well
    inline constexpr sf::Color bgPanel{ 28, 28, 30 };     // list screens panel
    inline constexpr sf::Color bgDetail{ 24, 24, 26 };    // history detail well
    inline constexpr sf::Color bgCard{ 42, 42, 44 };      // row/card fills
    inline constexpr sf::Color bgCardActive{ 52, 52, 56 };
    inline constexpr sf::Color bgCardHover{ 58, 58, 62 };
    inline constexpr sf::Color bgElevated{ 70, 70, 70 };  // disabled-row fills
    inline constexpr sf::Color bgWidget{ 58, 56, 54 };    // control idle fill

    // Text family
    inline constexpr sf::Color white{ 255, 255, 255 };
    inline constexpr sf::Color textHeader{ 235, 235, 235 };
    inline constexpr sf::Color textPrimary{ 240, 240, 240 };
    inline constexpr sf::Color textBright{ 230, 230, 230 };
    inline constexpr sf::Color textBody{ 220, 220, 220 };
    inline constexpr sf::Color textCell{ 210, 210, 210 }; // navigator/SAN cells
    inline constexpr sf::Color textSecondary{ 200, 200, 200 };
    inline constexpr sf::Color textSoft{ 170, 170, 170 };
    inline constexpr sf::Color textMuted{ 160, 160, 160 };
    inline constexpr sf::Color textDimmed{ 150, 150, 150 };
    inline constexpr sf::Color textFaint{ 130, 130, 130 };
    inline constexpr sf::Color textLow{ 140, 140, 140 };
    inline constexpr sf::Color textGhost{ 100, 100, 100 };
    inline constexpr sf::Color textFaded{ 120, 120, 120 };
    inline constexpr sf::Color textDisabled{ 90, 90, 90 };

    // Strokes / outlines
    inline constexpr sf::Color strokeFaint{ 68, 66, 64 };
    inline constexpr sf::Color stroke{ 80, 80, 80 };
    inline constexpr sf::Color strokeStrong{ 120, 120, 120 };
    inline constexpr sf::Color strokeFocus{ 180, 180, 180 };

    // Semantic status
    inline constexpr sf::Color ok{ 140, 220, 140 };       // wins / solved
    inline constexpr sf::Color okDim{ 140, 200, 140 };    // material diff
    inline constexpr sf::Color okTurn{ 76, 175, 80 };     // active-turn name
    inline constexpr sf::Color warn{ 255, 200, 60 };      // draw/takeback banners
    inline constexpr sf::Color warnSoft{ 190, 180, 120 }; // status line
    inline constexpr sf::Color warnBanner{ 220, 200, 130 }; // offer prompts
    inline constexpr sf::Color dangerRed{ 255, 90, 90 };  // clock < 10 s
    inline constexpr sf::Color dangerAmber{ 255, 190, 70 }; // clock < 30 s
    inline constexpr sf::Color goldSelf{ 255, 215, 120 }; // arena/leaderboard own row
    inline constexpr sf::Color goldChatSelf{ 255, 230, 130 }; // own chat line

    // Selection (navigator current row, analysis ply highlight)
    inline constexpr sf::Color selectionBlue{ 0, 120, 215 };
    inline constexpr sf::Color hoverWash{ 120, 120, 120 };
    inline constexpr sf::Color bestMoveGreen{ 0, 180, 0 }; // analysis best arrows
} // namespace pal

// Shared dialog/overlay chrome (promotion picker, help overlay, dim layerish)
namespace overlay {
    inline constexpr sf::Color dim{ 0, 0, 0, 120 };
    inline constexpr sf::Color panelFill{ 30, 30, 30, 235 };
    inline constexpr sf::Color panelStroke{ 90, 90, 90 };
    inline constexpr sf::Color cellFill{ 40, 38, 35, 220 };
    inline constexpr sf::Color cellStroke{ 180, 180, 180, 180 };
    inline constexpr sf::Color cellHover{ 255, 255, 255, 30 };
    inline constexpr sf::Color cellGlyph{ 240, 240, 240 };
} // namespace overlay

// ---------------------------------------------------------------------------
// Type scale
// ---------------------------------------------------------------------------

namespace type {
    constexpr unsigned Display = 36; // screen titles
    constexpr unsigned Result = 30;  // game-over result line
    constexpr unsigned Big = 28;     // settings title
    constexpr unsigned Status = 18;  // connection status line
    constexpr unsigned Heading = 20; // buttons, section headers
    constexpr unsigned Body = 14;    // labels, hints
    constexpr unsigned Small = 13;   // checkboxes, dense rows
    constexpr unsigned Caption = 12; // spectators/metadata lines
} // namespace type

// ---------------------------------------------------------------------------
// Spacing scale
// ---------------------------------------------------------------------------

namespace space {
    constexpr float Xs = 4.f;
    constexpr float Sm = 8.f;
    constexpr float Md = 12.f;
    constexpr float Lg = 16.f;
    constexpr float Xl = 24.f;
    constexpr float Xxl = 32.f;
} // namespace space

// ---------------------------------------------------------------------------
// Component style structs
// ---------------------------------------------------------------------------

struct ButtonStyle
{
    sf::Color idleFill{ 58, 56, 54 };
    sf::Color hoverFill{ 70, 68, 66 };
    sf::Color pressedFill{ 50, 48, 46 };
    sf::Color disabledFill{ 40, 38, 36 };
    sf::Color disabledLabel{ 90, 90, 90 };
    sf::Color label{ 220, 220, 220 };
    sf::Color outline{ 80, 78, 76 };
    sf::Color activeOutline{ 140, 140, 140 };
    sf::Color focusOutline{ 180, 180, 180 };
    unsigned textSize = type::Heading;

    constexpr bool operator==(const ButtonStyle& other) const
    {
        return idleFill == other.idleFill && hoverFill == other.hoverFill &&
               pressedFill == other.pressedFill && disabledFill == other.disabledFill &&
               disabledLabel == other.disabledLabel && label == other.label &&
               outline == other.outline && activeOutline == other.activeOutline &&
               focusOutline == other.focusOutline && textSize == other.textSize;
    }
};

struct TextFieldStyle
{
    sf::Color fillIdle{ 58, 56, 54 };      // unfocused + enabled
    sf::Color strokeIdle{ 100, 100, 100 };
    sf::Color fillFocused{ 70, 68, 66 };   // focused
    sf::Color strokeFocused{ 180, 180, 180 };
    sf::Color fillDimmed{ 48, 46, 43 };    // disabled
    sf::Color strokeDimmed{ 68, 66, 64 };
    sf::Color textDisabled{ 100, 100, 100 };
    sf::Color placeholder{ 120, 120, 120 };
    sf::Color text{ 255, 255, 255 };
    sf::Color caret{ 255, 255, 255 };
};

struct PanelStyle
{
    sf::Color fill{ 30, 30, 30 };
    sf::Color outline{ 80, 80, 80 };
    float outlineThickness = 1.f;
};

struct LabelStyle
{
    unsigned textSize = type::Body;
    sf::Color color = pal::textBody;
};

struct CheckboxStyle
{
    sf::Color fillChecked{ 60, 140, 60 };
    sf::Color fillUnchecked{ 50, 50, 50 };
    sf::Color strokeFocused{ 180, 180, 180 };
    sf::Color strokeHovered{ 120, 120, 120 };
    sf::Color strokeIdle{ 80, 80, 80 };
    sf::Color mark{ 240, 240, 240 };
    sf::Color textFocused{ 240, 240, 240 };
    sf::Color textEnabled{ 200, 200, 200 };
    sf::Color textDisabled{ 90, 90, 90 };
    unsigned textSize = type::Small;
};

// ---------------------------------------------------------------------------
// Named button variants — each copies a trio literally present across the
// screens today (pure refactor; do NOT "improve" the derivations here, the
// 14.5 restyle retunes values intentionally).
// ---------------------------------------------------------------------------

// Button/widget defaults (Button/TextField mnemonics, LocalGame takeback row)
constexpr ButtonStyle buttonNeutral()
{
    return {};
}

// "Resign"/"Back"/"Decline (game screen)" red — shared by 5 screens
constexpr ButtonStyle buttonDanger()
{
    return { sf::Color{ 160, 55, 55 },  sf::Color{ 185, 65, 65 },
             sf::Color{ 135, 45, 45 },  sf::Color{ 70, 45, 45 },
             sf::Color{ 140, 90, 90 },  sf::Color{ 240, 240, 240 },
             sf::Color{ 90, 60, 60 },   sf::Color{ 210, 120, 120 },
             sf::Color{ 180, 140, 140 }, type::Heading };
}

// "Decline"/"Leave" (game over, arena lobby) — muted red
constexpr ButtonStyle buttonDangerMuted()
{
    return { sf::Color{ 140, 70, 60 },  sf::Color{ 165, 90, 80 },
             sf::Color{ 115, 60, 50 },  sf::Color{ 95, 48, 40 },
             sf::Color{ 150, 120, 120 }, sf::Color{ 240, 240, 240 },
             sf::Color{ 100, 100, 100 }, sf::Color{ 200, 200, 200 },
             sf::Color{ 160, 160, 160 }, type::Heading };
}

// "Rematch"/"Accept" (game over screen)
constexpr ButtonStyle buttonSuccess()
{
    return { sf::Color{ 60, 130, 60 },  sf::Color{ 80, 160, 80 },
             sf::Color{ 50, 110, 50 },  sf::Color{ 40, 90, 40 },
             sf::Color{ 120, 150, 120 }, sf::Color{ 240, 240, 240 },
             sf::Color{ 100, 100, 100 }, sf::Color{ 200, 200, 200 },
             sf::Color{ 160, 160, 160 }, type::Heading };
}

// "Accept" (draw/takeback offer, game screen)
constexpr ButtonStyle buttonSuccessBright()
{
    return { sf::Color{ 60, 140, 60 },  sf::Color{ 75, 165, 75 },
             sf::Color{ 48, 115, 48 },  sf::Color{ 45, 80, 45 },
             sf::Color{ 110, 140, 110 }, sf::Color{ 240, 240, 240 },
             sf::Color{ 70, 100, 70 },  sf::Color{ 140, 200, 140 },
             sf::Color{ 180, 220, 180 }, type::Heading };
}

// "Leave game" (game screen) — muted green-gray
constexpr ButtonStyle buttonLeave()
{
    return { sf::Color{ 70, 90, 70 },   sf::Color{ 85, 110, 85 },
             sf::Color{ 55, 72, 55 },   sf::Color{ 42, 55, 42 },
             sf::Color{ 105, 125, 105 }, sf::Color{ 235, 240, 235 },
             sf::Color{ 75, 95, 75 },   sf::Color{ 140, 165, 140 },
             sf::Color{ 175, 200, 175 }, type::Heading };
}

// "Analyze" (game over)
constexpr ButtonStyle buttonInfo()
{
    return { sf::Color{ 60, 80, 120 },  sf::Color{ 80, 100, 140 },
             sf::Color{ 50, 70, 100 },  sf::Color{ 40, 55, 80 },
             sf::Color{ 120, 130, 150 }, sf::Color{ 240, 240, 240 },
             sf::Color{ 100, 100, 100 },  sf::Color{ 200, 200, 200 },
             sf::Color{ 160, 160, 160 }, type::Heading };
}

// "Copy PGN"/"Archive" — deeper blue with bluey focus wash
constexpr ButtonStyle buttonBlue()
{
    return { sf::Color{ 60, 90, 160 },  sf::Color{ 75, 105, 180 },
             sf::Color{ 52, 80, 140 },  sf::Color{ 45, 60, 90 },
             sf::Color{ 120, 130, 150 }, sf::Color{ 240, 240, 240 },
             sf::Color{ 52, 78, 135 },  sf::Color{ 160, 190, 240 },
             sf::Color{ 210, 230, 255 }, type::Heading };
}

// Puzzle "Hint"
constexpr ButtonStyle buttonHint()
{
    return { sf::Color{ 60, 90, 140 },  sf::Color{ 75, 105, 160 },
             sf::Color{ 50, 75, 115 },  sf::Color{ 70, 70, 70 },
             sf::Color{ 140, 140, 140 }, sf::Color{ 240, 240, 240 },
             sf::Color{ 90, 90, 90 },   sf::Color{ 200, 200, 200 },
             sf::Color{ 180, 180, 180 }, type::Heading };
}

// Puzzle "Solution" / amber accents
constexpr ButtonStyle buttonAmber()
{
    return { sf::Color{ 140, 110, 40 }, sf::Color{ 160, 130, 50 },
             sf::Color{ 110, 88, 32 },  sf::Color{ 70, 70, 70 },
             sf::Color{ 140, 140, 140 }, sf::Color{ 240, 240, 240 },
             sf::Color{ 90, 90, 90 },   sf::Color{ 200, 200, 200 },
             sf::Color{ 180, 180, 180 }, type::Heading };
}

// Puzzle "Next"
constexpr ButtonStyle buttonGreen()
{
    return { sf::Color{ 60, 120, 60 },  sf::Color{ 75, 140, 75 },
             sf::Color{ 48, 95, 48 },   sf::Color{ 70, 70, 70 },
             sf::Color{ 140, 140, 140 }, sf::Color{ 240, 240, 240 },
             sf::Color{ 90, 90, 90 },   sf::Color{ 200, 200, 200 },
             sf::Color{ 180, 180, 180 }, type::Heading };
}

// Game-over "Menu" — flat gray
constexpr ButtonStyle buttonPlain()
{
    return { sf::Color{ 80, 80, 80 },   sf::Color{ 100, 100, 100 },
             sf::Color{ 70, 70, 70 },   sf::Color{ 60, 60, 60 },
             sf::Color{ 130, 130, 130 }, sf::Color{ 240, 240, 240 },
             sf::Color{ 100, 100, 100 }, sf::Color{ 200, 200, 200 },
             sf::Color{ 160, 160, 160 }, type::Heading };
}

// Connect screen cycle buttons (time control / variant)
constexpr ButtonStyle buttonCycle()
{
    return { sf::Color{ 70, 74, 96 },   sf::Color{ 84, 88, 112 },
             sf::Color{ 58, 62, 82 },   sf::Color{ 45, 48, 66 },
             sf::Color{ 150, 150, 150 }, sf::Color{ 255, 255, 255 },
             sf::Color{ 60, 64, 86 },   sf::Color{ 160, 168, 200 },
             sf::Color{ 200, 208, 240 }, type::Heading };
}

// Draw-offer / takeback row (game screen) — plum-gray
constexpr ButtonStyle buttonPlum()
{
    return { sf::Color{ 70, 70, 95 },   sf::Color{ 85, 85, 115 },
             sf::Color{ 58, 58, 78 },   sf::Color{ 45, 45, 58 },
             sf::Color{ 105, 105, 120 }, sf::Color{ 230, 230, 235 },
             sf::Color{ 80, 80, 100 },  sf::Color{ 150, 150, 175 },
             sf::Color{ 180, 180, 205 }, type::Heading };
}

// Connect screen "Connect" — bright action green
constexpr ButtonStyle buttonConnect()
{
    return { sf::Color{ 76, 175, 80 },  sf::Color{ 90, 190, 95 },
             sf::Color{ 60, 150, 70 },  sf::Color{ 40, 80, 45 },
             sf::Color{ 150, 150, 150 }, sf::Color{ 255, 255, 255 },
             sf::Color{ 60, 140, 65 },  sf::Color{ 160, 220, 160 },
             sf::Color{ 220, 255, 220 }, type::Heading };
}

// Connect screen "Leaderboard" — mint green
constexpr ButtonStyle buttonMint()
{
    return { sf::Color{ 46, 62, 58 },   sf::Color{ 58, 78, 72 },
             sf::Color{ 52, 70, 64 },   sf::Color{ 40, 55, 52 },
             sf::Color{ 150, 150, 150 }, sf::Color{ 240, 250, 240 },
             sf::Color{ 70, 110, 90 },  sf::Color{ 140, 200, 170 },
             sf::Color{ 180, 235, 205 }, type::Heading };
}

// Connect screen "Arena" — bronze
constexpr ButtonStyle buttonBronze()
{
    return { sf::Color{ 96, 74, 50 },   sf::Color{ 118, 92, 60 },
             sf::Color{ 80, 62, 42 },   sf::Color{ 66, 52, 35 },
             sf::Color{ 150, 150, 150 }, sf::Color{ 255, 250, 240 },
             sf::Color{ 70, 110, 90 },  sf::Color{ 140, 200, 170 },
             sf::Color{ 180, 235, 205 }, type::Heading };
}

// Connect screen "Watch" — cool slate
constexpr ButtonStyle buttonSlate()
{
    return { sf::Color{ 60, 80, 130 },  sf::Color{ 74, 98, 160 },
             sf::Color{ 50, 68, 110 },  sf::Color{ 40, 55, 88 },
             sf::Color{ 150, 160, 180 }, sf::Color{ 240, 245, 255 },
             sf::Color{ 55, 85, 130 },  sf::Color{ 130, 170, 230 },
             sf::Color{ 180, 210, 250 }, type::Heading };
}

// Leaderboard non-own row
constexpr ButtonStyle buttonRow()
{
    return { sf::Color{ 42, 42, 44 },   sf::Color{ 58, 58, 62 },
             sf::Color{ 52, 52, 56 },   sf::Color{ 42, 42, 44 },
             sf::Color{ 120, 120, 120 }, sf::Color{ 220, 220, 220 },
             sf::Color{ 90, 90, 90 },   sf::Color{ 200, 200, 120 },
             sf::Color{ 220, 220, 140 }, type::Heading };
}

// Leaderboard own row (green-tinted)
constexpr ButtonStyle buttonRowSelf()
{
    return { sf::Color{ 46, 62, 58 },   sf::Color{ 58, 78, 72 },
             sf::Color{ 52, 70, 64 },   sf::Color{ 46, 62, 58 },
             sf::Color{ 150, 150, 150 }, sf::Color{ 230, 245, 230 },
             sf::Color{ 70, 110, 90 },  sf::Color{ 120, 190, 150 },
             sf::Color{ 150, 220, 180 }, type::Heading };
}

// History list row
constexpr ButtonStyle buttonRowHistory()
{
    return { sf::Color{ 42, 42, 44 },   sf::Color{ 58, 58, 62 },
             sf::Color{ 52, 52, 56 },   sf::Color{ 42, 42, 44 },
             sf::Color{ 120, 120, 120 }, sf::Color{ 220, 220, 220 },
             sf::Color{ 90, 90, 90 },   sf::Color{ 200, 200, 120 },
             sf::Color{ 220, 220, 140 }, type::Heading };
}

} // namespace chess::client::ui
