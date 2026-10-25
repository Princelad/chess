#pragma once

#include "app.h"
#include "board_interaction.h"
#include "boardview.h"
#include "chat_log.h"
#include "game_over_transition.h"
#include "hud.h"
#include "move_animator.h"
#include "premove.h"
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
               const std::string& resumeToken = {},
               const std::string& startFen = {},
               bool arena = false,
               bool spectator = false);
    // Spectator view (12.4): built from the live-game sync snapshot — no
    // seat, read-only board, both cards in seat names. Delegates to the
    // player ctor (White-oriented view) then replays the SAN chain.
    GameScreen(App& app, const chess::net::SpectateStateMsg& snapshot);
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
    // Rebuilds the board from a trimmed SAN list (11.3 takeback accepted):
    // the two retracted plies are gone, the requester is on the move. Same
    // reconstruction as a token resume, no animation or move sound.
    void applyTakeback(const std::vector<std::string>& sans);
    void syncViewHighlights();
    void clearBoardInput();
    bool ownPieceAt(int file, int rank);
    // Premove (11.4): the queued move auto-sends the instant the position
    // (the opponent's just-echoed move) makes it legal; flush clears it
    // either way — sent, or dropped as "now illegal".
    void queuePremove(int fromFile, int fromRank, int toFile, int toRank);
    void flushPremove();
    void setPremoveHighlight();
    sf::FloatRect autoQueenRect() const;
    std::optional<chess::PieceType> promoTypeForKey(sf::Keyboard::Key key) const;
    PromoCell promoCell(int index) const;
    void sendChat();
    void refreshRatingsHud();
    void drawButtons(sf::RenderWindow& window);
    void drawChat(sf::RenderWindow& window);
    void drawHelpOverlay(sf::RenderWindow& window);
    void finishGameOver();

    // Spectator mode (12.4): rebuild the empty board from the live-game
    // sync and leave the watch again. Input stays board-read-only.
    void applySpectateState(const chess::net::SpectateStateMsg& msg);
    void leaveWatch();

    // Reconnect & resume (11.2): automatic token-based rejoin after a
    // mid-game connection loss, with the game-over screen as the fallback.
    void updateReconnect(float dtSec);
    void giveUpReconnect();
    void applyResume(const chess::net::ResumeStateMsg& msg);

    // The board every SAN-chain reconstruction starts from (12.1): the
    // welcome's Start Position when the match is a Chess960 game, else the
    // standard start. Junk FENs fall back to the standard board rather
    // than crash the screen.
    Board startBoard() const;

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
    // Premove (11.4): at most one queued move while the opponent thinks.
    std::optional<Premove> premove_;
    GameOverTransition gameOverTransition_;

    std::string opponentName_;
    std::optional<std::pair<double, double>> opponentRating_;
    bool rated_ = false;
    // Spectator view (12.4): when true the screen owns no seat — input is
    // read-only over the board, the leave button swaps in for the action
    // buttons, and the game-over switch returns to the connect screen.
    bool spectator_ = false;
    std::string whiteName_;
    int spectatorCount_ = 0;

    std::vector<ChatEntry> chatLog_;
    TextField chatInput_;
    Panel chatLogBg_;
    Label chatLabel_;

    void addChatLine(const std::string& name, const std::string& text);
    void addSystemChat(const std::string& text);
    Label drawOfferLabel_;
    Label takebackLabel_;
    Button resignBtn_;
    Button offerDrawBtn_;
    Button takebackBtn_;
    // Spectator-mode replacement row (12.4): a single leave button.
    Button leaveBtn_;
    // 11.3: `takebackOfferPending_` is the opponent's incoming request
    // awaiting the accept/decline prompt; our own request is only a status
    // line (the server already errors on duplicate asks).
    bool takebackOfferPending_ = false;
    Button declineBtn_;
    Button acceptBtn_;
    std::string timeControl_;
    std::string resumeToken_;
    // 12.1: the game's own Start Position FEN (empty = standard start).
    // Every SAN-chain reconstruction — token resume, takeback replay —
    // must chain from HERE, never from fromStartPos().
    std::string startFen_;
    // Arena game (12.3): its game-over screen swaps the rematch row for
    // "return to arena" instead (the server refuses rematch offers on
    // arena games anyway).
    bool arena_ = false;

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