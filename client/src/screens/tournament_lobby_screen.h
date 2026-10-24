#pragma once

#include "app.h"
#include "widgets/button.h"
#include "widgets/label.h"
#include <chess/net/messages.h>

#include <string>
#include <vector>

namespace chess::client {

// Arena tournament lobby (12.3): joins the arena for one time-control/
// variant pair on entry (idempotent for a returning member), shows the
// live standings broadcast, and hands this seat over to GameScreen when
// the server's pairing WelcomeMsg arrives. Pure-per-screen logic — the
// scoring/pairing rules live server-side (Tournament), so nothing here is
// worth unit testing.
class TournamentLobbyScreen : public Screen {
public:
    // joinName: guest join name (accounts ignore it server-side; empty is
    // fine for them, as app_.lastName() covers guests).
    TournamentLobbyScreen(App& app, std::string timeControl,
                          std::string variant, std::string joinName = {});

    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    void sendJoin();
    void leave();
    void enterGame(const chess::net::WelcomeMsg& welcome);
    void rebuildRows();
    void layoutRows();

    App& app_;
    std::string timeControl_;
    std::string variant_;
    std::string joinName_;
    bool joinSent_ = false;

    // Latest snapshot from TournamentStateMsg.
    std::vector<chess::net::TournamentRowMsg> rows_;
    int phase_ = 0; // 0 = running, 1 = ended
    int roundsTarget_ = 0;

    // One label per rendered cell, rebuilt per snapshot: name, points and
    // the W-L-D column of each row plus the table header.
    struct RowWidgets {
        Label name;
        Label points;
        Label record;
    };
    std::vector<RowWidgets> rowWidgets_;
    Label title_;
    Label subtitle_;
    Label headerName_;
    Label headerPoints_;
    Label headerRecord_;
    Label statusLabel_;
    Label resultLabel_;
    Button leaveBtn_;
};

} // namespace chess::client
