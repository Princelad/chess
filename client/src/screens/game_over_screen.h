#pragma once

#include "app.h"
#include <chess/net/messages.h>

#include <string>

namespace chess::client {

class GameOverScreen : public Screen {
public:
    GameOverScreen(App& app,
                   net::GameResult result,
                   net::GameOverReason reason);
    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    App& app_;
    std::string resultText_;
    std::string reasonText_;
};

} // namespace chess::client
