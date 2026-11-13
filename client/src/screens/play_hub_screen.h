#pragma once

#include "app.h"
#include "connect_screen.h"
#include "bot_personas.h"
#include "widgets/button.h"

#include <SFML/Graphics.hpp>

#include <array>
#include <memory>
#include <vector>

namespace chess::client {

// 17.4: the play hub — the app's home screen in the chess.com style. Left
// zone: quick-pair time-control cards (11.1 presets), the variant cycler
// (12.1), the arena lobby card (12.3) and the connect drawer toggle; the
// ConnectScreen itself survives embedded as the collapsible right-side
// drawer (17.4 contract — no connect logic is duplicated, the drawer's
// queue keeps running while collapsed). Right zone: the vs-computer
// persona rows (17.3 cast as 15.2 player-card bands) and the tool cards
// (Daily, Puzzle Rush, Puzzles, Archive, Leaderboard, Settings) — every
// 9.1 menu entry lives here, none lost. Config `general.entry` (default
// "hub") opts back to the old menu for one version.
class PlayHubScreen : public Screen {
public:
    explicit PlayHubScreen(App& app);

    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

    // 17.4 quick-queue entry (the probe drives it directly too).
    void quickQueue(const std::string& tc);

private:
    void toggleDrawer();
    void quickBrowse(BrowseTarget target);
    void startPersona(std::size_t index);
    void layoutPersonaRows();

private:

    App& app_;
    std::unique_ptr<ConnectScreen> drawer_; // embedded connect drawer
    bool drawerOpen_ = false;

    // Quick-pair cards: Bullet / Blitz / Rapid / Classical (11.1 presets).
    static constexpr int QuickCount = 4;
    std::array<Button, QuickCount> quickBtns_;
    Button variantBtn_;
    Button arenaBtn_;
    Button drawerBtn_; // connect drawer toggle
    Button sideBtn_;   // vs-computer side cycler (Play as White/Black)
    bool playBlack_ = false;
    std::vector<Button> personaRows_; // one per persona (all() order)
    int pickedPersona_ = -1;

    // Tool cards: Daily / Puzzle Rush / Puzzles / Archive / Leaderboard /
    // Settings / Watch (indices into the fixed row below).
    static constexpr int ToolCount = 7;
    std::array<Button, ToolCount> toolBtns_;
};

} // namespace chess::client
