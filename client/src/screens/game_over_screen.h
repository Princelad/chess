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
                   std::vector<std::string> sanMoves,
                   bool arena = false,
                   const std::string& arenaTimeControl = {},
                   const std::string& arenaVariant = {});
    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    // Rematch (11.7) lifecycle from this seat's side:
    // Idle     — nothing on the table; the Rematch button is live.
    // Sent     — our offer is out, waiting for the opponent to answer.
    // Offered  — the opponent's offer is on screen; accept/decline live.
    enum class RematchPhase { Idle, Sent, Offered };

    void activateRematch();
    void activateMenu();
    void activateArchive();
    void activateAnalyze();
    // Arena games (12.3) run their own post-game flow: the rematch button
    // becomes the return-to-the-lobby switch (the lobby re-affirms the
    // membership and shows the standings; the next pairing arrives there.
    void activateArenaLobby();
    void acceptRematch();
    void declineRematch();
    void cycleFocus();
    void layoutWidgets();
    void setStatusText(const std::string& text);
    void enterIdleRematch(const std::string& note);

    App& app_;
    std::string resultText_;
    std::string reasonText_;
    Board initialBoard_;
    std::vector<chess::Move> moves_;
    std::vector<std::string> sanMoves_;
    RematchPhase rematchPhase_ = RematchPhase::Idle;
    // Arena game (12.3): true when the finished game was a tournament
    // pairing; its tc/variant re-enter the lobby.
    bool arena_ = false;
    std::string arenaTimeControl_;
    std::string arenaVariant_;

    Button rematchBtn_;
    Button offerAcceptBtn_;
    Button offerDeclineBtn_;
    Button archiveBtn_;
    Button analyzeBtn_;
    Button menuBtn_;
    Label resultLabel_;
    Label reasonLabel_;
    Label statusLabel_;
    Label hint_;
    int focus_ = 0;
};

} // namespace chess::client
