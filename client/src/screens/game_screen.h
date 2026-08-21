#pragma once

#include "app.h"
#include "boardview.h"
#include <chess/board.h>
#include <chess/types.h>

#include <chess/move.h>

#include <string>
#include <vector>

namespace chess::client {

struct PromotionState {
    int fromFile, fromRank;
    int toFile, toRank;
    std::vector<chess::Move> candidates;
};

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
    void sendPromotionMove(chess::PieceType type);
    void cancelPromotion();

    App& app_;
    Board board_;
    Color myColor_;
    std::string opponentName_;
    BoardView boardView_;

    HighlightState hl_;
    std::optional<PromotionState> promo_;
    bool inCheck_ = false;
    bool myTurn_ = false;
    bool gameOver_ = false;
    std::string statusMsg_;
    float statusTimer_ = 0.f;
};

} // namespace chess::client
