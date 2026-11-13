#pragma once

#include "app.h"
#include "widgets/button.h"

#include <SFML/Graphics.hpp>

#include <vector>

namespace chess::client {

// 17.3: persona selection for local play. The fixed bot cast renders as
// player cards (14.3 avatar + name + strength hint); clicking a card
// starts LocalGameScreen against that persona's engine preset.
class PersonaSelectScreen : public Screen {
public:
    explicit PersonaSelectScreen(App& app);

    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    void startGame();

    App& app_;
    std::vector<Button> cards_; // one per persona (all() order)
    Button sideBtn_;            // Play as White / Black cycler
    Button backBtn_;
    bool playBlack_ = false;
    int picked_ = -1; // hovered/clicked row index
};

} // namespace chess::client
