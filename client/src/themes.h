#pragma once

#include <SFML/Graphics.hpp>

#include <string>

namespace chess::client {

// Board color themes. Selected in the settings screen and persisted to the
// config file (`board.colors`); the foundation for phase 11 themes.
enum class BoardTheme { Classic, Green, Blue, Dark, Count };

struct BoardColors {
    sf::Color light;
    sf::Color dark;
};

inline BoardColors boardColorsFor(BoardTheme theme)
{
    switch (theme) {
        case BoardTheme::Classic: return { sf::Color(240, 217, 181), sf::Color(181, 136, 99) };
        case BoardTheme::Green:   return { sf::Color(167, 203, 149), sf::Color(88, 140, 89) };
        case BoardTheme::Blue:    return { sf::Color(133, 168, 199), sf::Color(76, 112, 148) };
        case BoardTheme::Dark:    return { sf::Color(86, 92, 98),    sf::Color(56, 60, 66) };
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

// A readable label color for a given square color.
inline sf::Color labelColorFor(const sf::Color& square)
{
    float lum = 0.2126f * (square.r / 255.f)
              + 0.7152f * (square.g / 255.f)
              + 0.0722f * (square.b / 255.f);
    return lum > 0.6f ? sf::Color(40, 40, 40) : sf::Color(245, 245, 245);
}

} // namespace chess::client