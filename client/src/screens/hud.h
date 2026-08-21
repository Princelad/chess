#pragma once

#include <chess/types.h>

#include <SFML/Graphics.hpp>

#include <string>
#include <vector>

namespace chess::client {

class Hud {
public:
    Hud() = default;
    Hud(float panelX, float panelWidth, float windowHeight);

    void setInfo(const std::string& opponentName, Color myColor,
                 bool myTurn, bool gameOver);
    void addMove(const std::string& san);
    void setStatus(const std::string& msg, float duration);
    void setGameOver(bool gameOver);
    void handleScroll(float delta);
    void update(float dtSec);
    void draw(sf::RenderWindow& window, const sf::Font& font) const;

    float contentBottom() const;

private:
    float panelX_ = 0.f;
    float panelW_ = 0.f;
    float windowHeight_ = 0.f;

    std::string opponentName_;
    Color myColor_ = Color::White;
    bool myTurn_ = false;
    bool gameOver_ = false;

    std::vector<std::pair<std::string, std::string>> movePairs_;
    int moveScroll_ = 0;

    std::string statusMsg_;
    float statusTimer_ = 0.f;
};

} // namespace chess::client
