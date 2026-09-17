#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <chess/board.h>
#include <chess/types.h>
#include "config.h"
#include "connection.h"
#include "sfx.h"
#include "themes.h"

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
    void pushScreen(std::unique_ptr<Screen> screen);
    void goBack();

    sf::Font& font() { return *font_; }
    Connection& connection() { return connection_; }
    SoundManager& sounds() { return sounds_; }

    // Map a window-space mouse pixel to logical (view-space) coordinates.
    sf::Vector2f toLocal(sf::Vector2i pixel) const;

    sf::Vector2u viewportSize() const { return viewport_; }

    void setLastConnection(const std::string& host, const std::string& port, const std::string& name) {
        lastHost_ = host;
        lastPort_ = port;
        lastName_ = name;
    }
    const std::string& lastHost() const { return lastHost_; }
    const std::string& lastPort() const { return lastPort_; }
    const std::string& lastName() const { return lastName_; }

    bool autoQueen() const { return config_.getBool("general.auto_queen", true); }
    void setAutoQueen(bool on)
    {
        config_.setBool("general.auto_queen", on);
        config_.save();
    }

    BoardTheme boardTheme() const
    {
        return boardThemeFromName(config_.get("board.colors", "classic"));
    }
    void setBoardTheme(BoardTheme theme)
    {
        config_.set("board.colors", boardThemeName(theme));
        config_.save();
    }

    bool showCoordinates() const
    {
        return config_.getBool("board.show_coordinates", true);
    }
    void setShowCoordinates(bool on)
    {
        config_.setBool("board.show_coordinates", on);
        config_.save();
    }

    const Config& config() const { return config_; }
    Config& config() { return config_; }

    static constexpr int PieceIndex(Color c, PieceType t) {
        return static_cast<int>(c) * 6 + static_cast<int>(t);
    }
    const sf::Texture& pieceTexture(Color color, PieceType type) const {
        return pieceTextures_[PieceIndex(color, type)];
    }
    bool piecesLoaded() const { return piecesLoaded_; }

    // Reload piece textures from the configured piece-set path; falls back to
    // the previous textures if loading fails. Returns true on full success.
    bool reloadPieces();

    static constexpr unsigned int WindowWidth = 960;
    static constexpr unsigned int WindowHeight = 640;

private:
    void loadAssets();
    bool loadPieceTextures();
    void buildView(unsigned int width, unsigned int height);

    sf::RenderWindow window_;
    Connection connection_;
    Config config_;
    SoundManager sounds_;
    std::unique_ptr<Screen> screen_;
    std::vector<std::unique_ptr<Screen>> stack_;
    std::optional<sf::Font> font_;
    std::array<sf::Texture, 12> pieceTextures_;
    bool piecesLoaded_ = false;
    std::string lastHost_ = "localhost";
    std::string lastPort_ = "5555";
    std::string lastName_;
    sf::View view_;
    sf::Vector2u viewport_;
};

} // namespace chess::client
