#pragma once

#include "app.h"
#include "boardview.h"
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
    void selectPiece(int file, int rank);
    void trySendMove(int targetFile, int targetRank);
    void deselect();

    App& app_;
    Board board_;
    Color myColor_;
    std::string opponentName_;
    BoardView boardView_;

    HighlightState hl_;
    bool inCheck_ = false;
    bool myTurn_ = false;
    bool gameOver_ = false;
    std::string statusMsg_;
    float statusTimer_ = 0.f;
};

} // namespace chess::client
