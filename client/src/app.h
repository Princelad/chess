#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>

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

    static constexpr int PieceIndex(Color c, PieceType t) {
        return static_cast<int>(c) * 6 + static_cast<int>(t);
    }
    const sf::Texture& pieceTexture(Color color, PieceType type) const {
        return pieceTextures_[PieceIndex(color, type)];
    }
    bool piecesLoaded() const { return piecesLoaded_; }

    static constexpr unsigned int WindowWidth = 960;
    static constexpr unsigned int WindowHeight = 640;

private:
    void loadAssets();
    void loadPieceTextures();

    sf::RenderWindow window_;
    Connection connection_;
    std::unique_ptr<Screen> screen_;
    std::optional<sf::Font> font_;
    std::array<sf::Texture, 12> pieceTextures_;
    bool piecesLoaded_ = false;
};

} // namespace chess::client
