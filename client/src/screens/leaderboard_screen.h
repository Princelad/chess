#pragma once

#include "app.h"
#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/panel.h"

#include <string>
#include <vector>

namespace chess::client {

// Server leaderboard browser (10.4): requests LeaderboardListMsg on open and
// shows the top-rated accounts ("1. Ada   1732 ±82   12W 3L 2D"). Assumes an
// established connection; the server answers the read-only request in any
// connection state. The viewer's own account row is highlighted when logged
// in; guests just see the global board.
class LeaderboardScreen : public Screen {
public:
    explicit LeaderboardScreen(App& app);
    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    enum class Phase { Busy, List };

    void request();
    void layoutRows();

    App& app_;
    Phase phase_ = Phase::Busy;
    std::vector<chess::net::LeaderboardEntryMsg> entries_;
    std::size_t topRow_ = 0;             // first visible entry (window scroll)
    std::string ownName_;                // lowercased account name; "" = guest

    static constexpr int PageRows = 15;

    Panel bg_;
    Label title_;
    Label status_;
    Button backBtn_;
    std::vector<Button> rowButtons_;
};

} // namespace chess::client
