#pragma once

#include "app.h"
#include "board_interaction.h"
#include "boardview.h"
#include "game_over_transition.h"
#include "hud.h"
#include "move_animator.h"
#include "promo_state.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"
#include "widgets/panel.h"
#include "widgets/text_field.h"
#include <chess/board.h>
#include <chess/move.h>
#include <chess/net/messages.h>
#include <chess/types.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace chess::client {

class GameScreen : public Screen {
public:
    GameScreen(App& app, Color myColor, const std::string& opponentName,
               std::optional<std::pair<double, double>> opponentRating = {},
               bool rated = false,
               const std::string& timeControl = {},
               const std::string& resumeToken = {});
    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    void selectPiece(int file, int rank);
    void trySendMove(int targetFile, int targetRank);    void deselect();
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
    void refreshRatingsHud();
    void drawButtons(sf::RenderWindow& window);
    void drawChat(sf::RenderWindow& window);
    void drawHelpOverlay(sf::RenderWindow& window);
    void finishGameOver();

    // Reconnect & resume (11.2): automatic token-based rejoin after a
    // mid-game connection loss, with the game-over screen as the fallback.
    void updateReconnect(float dtSec);
    void giveUpReconnect();
    void applyResume(const chess::net::ResumeStateMsg& msg);

    App& app_;
    Color myColor_;
    BoardView boardView_;
    HighlightState hl_;
    BoardAnnotations annotations_;
    DragTracker drag_;
    std::optional<std::pair<int, int>> dragFrom_;
    std::optional<std::pair<int, int>> rightPress_;
    sf::Vector2f cursor_{0.f, 0.f};
    std::pair<int, int> keyCursor_{4, 4};
    bool showingHelp_ = false;
    MoveAnimator anim_;
    int promoHover_ = -1;
    std::optional<PromotionState> promo_;
    Checkbox autoQueenCheck_;
    Hud hud_;
    bool myTurn_ = false;
    bool gameOver_ = false;
    bool drawOfferPending_ = false;
    GameOverTransition gameOverTransition_;

    std::string opponentName_;
    std::optional<std::pair<double, double>> opponentRating_;
    bool rated_ = false;

    std::vector<std::string> chatLog_;
    TextField chatInput_;
    Panel chatLogBg_;
    Label chatLabel_;
    Label drawOfferLabel_;
    Button resignBtn_;
    Button offerDrawBtn_;
    Button declineBtn_;
    Button acceptBtn_;
    std::string timeControl_;
    std::string resumeToken_;

    // Reconnect state machine (11.2). Idle until a real connection loss;
    // each step re-arms on new drops. RebindResumeFailed = token rejected
    // by the server short-circuits the give-up timer.
    enum class ReconnectPhase { None, Backoff, Connecting, AwaitingResume };
    ReconnectPhase reconnectPhase_ = ReconnectPhase::None;
    float reconnectWait_ = 0.f;
    float reconnectElapsed_ = 0.f;
    bool checkSelfResumeError_ = false;

    void layoutPanel();
};

} // namespace chess::client