#pragma once

#include "app.h"
#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/text_field.h"

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace chess::client {

enum class ConnectPhase {
    Idle, Connecting, ConnectingBrowse, AwaitingProfile, WaitingForOpponent
};

// Read-only server browsing targets reachable without joining a queue
// (10.3 archive, 10.4 leaderboard), plus the arena (12.3) and the
// spectator view (12.4): all connect first, then switch screens.
enum class BrowseTarget { None, Archive, Leaderboard, Arena, Watch };

class ConnectScreen : public Screen {
public:
    explicit ConnectScreen(App& app);
    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

    // ── 17.4 play-hub drawer API ────────────────────────────────────────
    // The hub embeds this screen as a collapsible right-side drawer. These
    // wrappers expose the pieces the hub needs; zero logic is duplicated.
    // Drops the screen's own title/hint chrome (the hub paints its own).
    void embedAsDrawer();
    // Abandon an in-flight attempt so the drawer can collapse without
    // leaving a stale phase (fields re-enabled, statuses cleared).
    void cancelAttempt();
    // Quick-queue: pick the TC preset and immediately connect + join with
    // the persisted host/port/name. Returns false (state unchanged beyond
    // the TC pick) when the name is empty or a connect is already running
    // — the caller then just expands the drawer for manual entry.
    bool quickQueueTc(const std::string& tc);
    // Quick browse (archive/leaderboard/arena/watch): same gates as the
    // buttons, so the hub cards can drive them.
    void quickBrowse(BrowseTarget target) { startBrowse(target); }
    // The hub's variant cycler drives the drawer's persisted pick.
    void cycleVariant() { selectVariant(); }
    ConnectPhase phase() const { return phase_; }
    // 17.4: the hub collapses the drawer while a quick attempt runs; a
    // failed connect/login/queue would otherwise fail INVISIBLY. The hook
    // fires on every failure path that leaves the attempt dead (the hub
    // expands the drawer so the error is visible). No-op when unset —
    // the standalone screen never needs it.
    void setOnFailure(std::function<void()> hook) { onFailure_ = std::move(hook); }

private:
    static constexpr std::size_t FieldCount = 4;
    // Time-control presets before pairing (11.1); first entry = unlimited.
    static constexpr int TimeControlCount = 7;
    // Game variant presets before pairing (12.1); first entry = the standard
    // game, second the exact "chess960" tag the server sanitizes.
    static constexpr int VariantCount = 2;

    void tryConnectOrRetry();
    void startBrowse(BrowseTarget target);
    void openBrowseTarget();    void cycleField(bool backward);
    void selectTimeControl();
    void updateTimeControlLabel();
    void selectVariant();
    void updateVariantLabel();
    void layoutWidgets();
    void notifyFailure();
    const std::string& name() const;
    const std::string& password() const;
    const std::string& timeControl() const { return timeControls_[tcIndex_]; }
    const std::string& variant() const { return variants_[variantIndex_]; }

    App& app_;
    ConnectPhase phase_ = ConnectPhase::Idle;

    std::array<TextField, FieldCount> fields_;
    std::array<Label, FieldCount> labels_;
    std::size_t activeField_ = 0;
    std::array<std::string, TimeControlCount> timeControls_;
    std::size_t tcIndex_ = 0;
    std::array<std::string, VariantCount> variants_;
    std::size_t variantIndex_ = 0;
    Button connectBtn_;
    Button archiveBtn_;
    Button leaderboardBtn_;
    Button arenaBtn_;
    Button watchBtn_;
    Button tcBtn_;
    Label tcLabel_;
    Button variantBtn_;
    Label variantLabel_;
    Label title_;
    Label hint_;
    Label status_;
    Label error_;
    BrowseTarget browseTarget_ = BrowseTarget::None;
    std::function<void()> onFailure_;
};

} // namespace chess::client