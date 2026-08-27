#pragma once

#include "app.h"
#include <chess/board.h>
#include <chess/move.h>
#include <chess/net/messages.h>

#include <string>
#include <vector>

namespace chess::client {

class GameOverScreen : public Screen {
public:
    GameOverScreen(App& app,
                   net::GameResult result,
                   net::GameOverReason reason,
                   Board initialBoard,
                   std::vector<chess::Move> moves,
                   std::vector<std::string> sanMoves);
    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    App& app_;
    std::string resultText_;
    std::string reasonText_;
    bool rematchHovered_ = false;
    bool analyzeHovered_ = false;
    Board initialBoard_;
    std::vector<chess::Move> moves_;
    std::vector<std::string> sanMoves_;
};

} // namespace chess::client
