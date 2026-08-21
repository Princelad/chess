#pragma once

#include "app.h"
#include <chess/board.h>
#include <chess/types.h>

#include <string>

namespace chess::client {

class GameScreen : public Screen {
public:
    GameScreen(App& app, Color myColor, const std::string& opponentName);
    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    App& app_;
    Board board_;
    Color myColor_;
    std::string opponentName_;
};

} // namespace chess::client
