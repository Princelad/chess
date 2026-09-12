#pragma once

#include "app.h"
#include "board_interaction.h"
#include "boardview.h"
#include "hud.h"
#include "promo_state.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
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
    void commitMove(const chess::Move& m);
    void buildPromotion(int fromFile, int fromRank, int toFile, int toRank);
    void sendPromotionMove(chess::PieceType type);
    void cancelPromotion();
    void syncViewHighlights();
    void clearBoardInput();
    bool ownPieceAt(int file, int rank);
    sf::FloatRect autoQueenRect() const;
    std::optional<chess::PieceType> promoTypeForKey(sf::Keyboard::Key key) const;
    PromoCell promoCell(int index) const;
    void sendChat();
    void drawButtons(sf::RenderWindow& window);
    void drawChat(sf::RenderWindow& window);

    App& app_;
    Color myColor_;
    BoardView boardView_;
    HighlightState hl_;
    BoardAnnotations annotations_;
    DragTracker drag_;
    std::optional<std::pair<int, int>> dragFrom_;
    std::optional<std::pair<int, int>> rightPress_;
    sf::Vector2f cursor_{0.f, 0.f};
    int promoHover_ = -1;
    std::optional<PromotionState> promo_;
    Checkbox autoQueenCheck_;
    Hud hud_;
    bool myTurn_ = false;
    bool gameOver_ = false;
    bool drawOfferPending_ = false;

    std::string opponentName_;

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