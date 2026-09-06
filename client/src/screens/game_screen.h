#pragma once

#include "app.h"
#include "boardview.h"
#include "hud.h"
#include "promo_state.h"
#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/panel.h"
#include "widgets/text_field.h"
#include <chess/board.h>
#include <chess/move.h>
#include <chess/types.h>

#include <optional>
#include <string>
#include <vector>

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
    void sendPromotionMove(chess::PieceType type);
    void cancelPromotion();
    PromoCell promoCell(int index) const;
    void sendChat();
    void drawButtons(sf::RenderWindow& window);
    void drawChat(sf::RenderWindow& window);

    App& app_;
    Board board_;
    Color myColor_;
    BoardView boardView_;
    HighlightState hl_;
    std::optional<PromotionState> promo_;
    bool inCheck_ = false;
    Hud hud_;
    bool myTurn_ = false;
    bool gameOver_ = false;
    bool drawOfferPending_ = false;

    std::string opponentName_;
    Board initialBoard_;
    std::vector<chess::Move> moves_;
    std::vector<std::string> sanMoves_;

    std::vector<std::string> chatLog_;
    TextField chatInput_;
    Panel chatLogBg_;
    Label chatLabel_;
    Label drawOfferLabel_;
    Button resignBtn_;
    Button offerDrawBtn_;
    Button declineBtn_;
    Button acceptBtn_;

    void layoutPanel();
};

} // namespace chess::client