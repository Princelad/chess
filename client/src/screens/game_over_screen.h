#pragma once

#include "app.h"
#include "widgets/button.h"
#include "widgets/label.h"
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
    void activateRematch();
    void activateAnalyze();
    void cycleFocus();
    void layoutWidgets();

    App& app_;
    std::string resultText_;
    std::string reasonText_;
    Board initialBoard_;
    std::vector<chess::Move> moves_;
    std::vector<std::string> sanMoves_;

    Button rematchBtn_;
    Button analyzeBtn_;
    Label resultLabel_;
    Label reasonLabel_;
    Label hint_;
    int focus_ = 0;
};

} // namespace chess::client