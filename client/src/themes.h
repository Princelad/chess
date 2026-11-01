#pragma once

#include <SFML/Graphics.hpp>

#include <string>

namespace chess::client {

// Board color themes. Selected in the settings screen and persisted to the
// config file (`board.colors`); the foundation for phase 11 themes.
//
// 14.4 refresh: Green/Blue landed with 11.6 and stay IN PLACE (persistence
// is by name — appending preserves every previously stored value, never
// reorder); Green's values were retuned to the chess.com-inspired family
// look (ours — not lifted RGBs) and Brown/Purple appended after Moss.
enum class BoardTheme { Classic, Green, Blue, Dark, Wood, Moss, Brown, Purple, Count };

struct BoardColors {
    sf::Color light;
    sf::Color dark;
};

inline BoardColors boardColorsFor(BoardTheme theme)
{
    switch (theme) {
        case BoardTheme::Classic: return { sf::Color(240, 217, 181), sf::Color(181, 136, 99) };
        case BoardTheme::Green:   return { sf::Color(233, 235, 210), sf::Color(117, 149, 86) };
        case BoardTheme::Blue:    return { sf::Color(133, 168, 199), sf::Color(76, 112, 148) };
        case BoardTheme::Dark:    return { sf::Color(86, 92, 98),    sf::Color(56, 60, 66) };
        case BoardTheme::Wood:    return { sf::Color(233, 193, 143), sf::Color(158, 99, 62) };
        case BoardTheme::Moss:    return { sf::Color(176, 182, 166), sf::Color(64, 94, 78) };
        case BoardTheme::Brown:   return { sf::Color(228, 196, 154), sf::Color(156, 93, 74) };
        case BoardTheme::Purple:  return { sf::Color(220, 208, 231), sf::Color(124, 95, 158) };
    }
    return { sf::Color(240, 217, 181), sf::Color(181, 136, 99) };
}

inline const char* boardThemeName(BoardTheme theme)
{
    switch (theme) {
        case BoardTheme::Classic: return "Classic";
        case BoardTheme::Green:   return "Green";
        case BoardTheme::Blue:    return "Blue";
        case BoardTheme::Dark:    return "Dark";
        case BoardTheme::Wood:    return "Wood";
        case BoardTheme::Moss:    return "Moss";
        case BoardTheme::Brown:   return "Brown";
        case BoardTheme::Purple:  return "Purple";
        default:                  return "Classic";
    }
}

inline BoardTheme nextBoardTheme(BoardTheme theme)
{
    return static_cast<BoardTheme>((static_cast<int>(theme) + 1) % static_cast<int>(BoardTheme::Count));
}

inline BoardTheme boardThemeFromName(const std::string& name)
{
    for (int i = 0; i < static_cast<int>(BoardTheme::Count); ++i)
        if (name == boardThemeName(static_cast<BoardTheme>(i)))
            return static_cast<BoardTheme>(i);
    return BoardTheme::Classic;
}

// 14.4 coordinates style, persisted as config `board.coordinates`
// ("corner" | "inside"; the `board.show_coordinates` bool stays the
// on/off master across both styles). `Corner` is the pre-14.4 rendering
// — small letters/numbers tucked into the square corners; `Inside` is
// the chess.com-like prominence: bigger glyphs sitting INSIDE the squares
// (file letters bottom-center of the bottom rank, rank numbers
// center-left of the a-file column).
enum class CoordinateStyle { Corner, Inside, Count };

inline const char* coordinateStyleName(CoordinateStyle style)
{
    switch (style) {
        case CoordinateStyle::Corner: return "corner";
        case CoordinateStyle::Inside: return "inside";
        default:                      return "corner";
    }
}

inline CoordinateStyle coordinateStyleFromName(const std::string& name)
{
    if (name == coordinateStyleName(CoordinateStyle::Inside))
        return CoordinateStyle::Inside;
    return CoordinateStyle::Corner; // junk tags degrade like the variant strings
}

// A readable label color for a given square color.
inline sf::Color labelColorFor(const sf::Color& square)
{
    float lum = 0.2126f * (square.r / 255.f)
              + 0.7152f * (square.g / 255.f)
              + 0.0722f * (square.b / 255.f);
    return lum > 0.6f ? sf::Color(40, 40, 40) : sf::Color(245, 245, 245);
}

} // namespace chess::client