#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include <chess/board.h>
#include <chess/types.h>
#include "connection.h"

#include <SFML/Graphics.hpp>

namespace chess::client {

class App;

class Screen {
public:
    virtual ~Screen() = default;
    virtual void handleEvent(const sf::Event& event) = 0;
    virtual void update(float dtSec) = 0;
    virtual void draw(sf::RenderWindow& window) = 0;
};

class App {
public:
    App();
    void run();
    void switchScreen(std::unique_ptr<Screen> screen);

    sf::Font& font() { return *font_; }
    Connection& connection() { return connection_; }

    static constexpr unsigned int WindowWidth = 960;
    static constexpr unsigned int WindowHeight = 640;
    static constexpr unsigned int BoardPixels = 640;
    static constexpr float SquareSize = BoardPixels / 8.f;

private:
    void loadAssets();

    sf::RenderWindow window_;
    Connection connection_;
    std::unique_ptr<Screen> screen_;
    std::optional<sf::Font> font_;
};

} // namespace chess::client
