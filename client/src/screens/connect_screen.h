#pragma once

#include "app.h"
#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/text_field.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace chess::client {

enum class ConnectPhase {
    Idle, Connecting, ConnectingBrowse, AwaitingProfile, WaitingForOpponent
};

// Read-only server browsing targets reachable without joining a queue
// (10.3 archive, 10.4 leaderboard); both connect and then switch screens.
enum class BrowseTarget { None, Archive, Leaderboard };

class ConnectScreen : public Screen {
public:
    explicit ConnectScreen(App& app);
    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    static constexpr std::size_t FieldCount = 4;
    // Time-control presets before pairing (11.1); first entry = unlimited.
    static constexpr int TimeControlCount = 7;

    void tryConnectOrRetry();
    void startBrowse(BrowseTarget target);
    void openBrowseTarget();
    void cycleField(bool backward);
    void selectTimeControl();
    void updateTimeControlLabel();
    void layoutWidgets();
    const std::string& name() const;
    const std::string& password() const;
    const std::string& timeControl() const { return timeControls_[tcIndex_]; }

    App& app_;
    ConnectPhase phase_ = ConnectPhase::Idle;

    std::array<TextField, FieldCount> fields_;
    std::array<Label, FieldCount> labels_;
    std::size_t activeField_ = 0;
    std::array<std::string, TimeControlCount> timeControls_;
    std::size_t tcIndex_ = 0;
    Button connectBtn_;
    Button archiveBtn_;
    Button leaderboardBtn_;
    Button tcBtn_;
    Label tcLabel_;
    Label title_;
    Label hint_;
    Label status_;
    Label error_;
    BrowseTarget browseTarget_ = BrowseTarget::None;
};

} // namespace chess::client