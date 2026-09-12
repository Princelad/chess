#pragma once

#include "app.h"
#include "board_interaction.h"
#include "boardview.h"
#include "hud.h"
#include "promo_state.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include <chess/board.h>
#include <chess/move.h>
#include <chess/types.h>
#include <chess/uci/engine.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace chess::client {

class LocalGameScreen : public Screen {
public:
    LocalGameScreen(App& app, Color myColor, std::string enginePath, int depth);
    ~LocalGameScreen() override;

    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    void selectPiece(int file, int rank);
    void tryMove(int targetFile, int targetRank);
    void deselect();
    void applyMove(const chess::Move& m);
    void buildPromotion(int fromFile, int fromRank, int toFile, int toRank);
    void applyPromotionMove(chess::PieceType type);
    void cancelPromotion();
    void syncViewHighlights();
    void clearBoardInput();
    bool ownPieceAt(int file, int rank);
    sf::FloatRect autoQueenRect() const;
    std::optional<chess::PieceType> promoTypeForKey(sf::Keyboard::Key key) const;
    bool applyEngineMove();
    void checkGameOver();
    void returnToMenu();

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
    bool engineThinking_ = false;
    bool engineFailed_ = false;

    std::unique_ptr<uci::UciEngine> engine_;
    int engineDepth_;

    PromoCell promoCell(int index) const;
    Button backBtn_;
};

} // namespace chess::client