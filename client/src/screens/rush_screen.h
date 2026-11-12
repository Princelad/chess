#pragma once

#include "puzzle_screen.h"
#include "rush.h"
#include "widgets/button.h"

#include <cstdint>
#include <vector>

namespace chess::client {

// Puzzle Rush & Survival (17.1): the 12.2 puzzle flow under a clock (3 min /
// 5 min) or a 3-strike Survival budget. A fail or a skip advances to the next
// puzzle in a per-session shuffled feed (seed recorded in config); the run's
// solved count persists per mode via config (rush::bestKey). Solution-reveal
// still zeroes the streak — the 12.2 discipline carries over verbatim.
class RushScreen : public PuzzleScreen {
public:
    explicit RushScreen(App& app);

    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

protected:
    // Flow hooks swapped for the run semantics.
    void onSolved() override;
    void onWrongMove() override;
    void revealSolution() override;
    void nextPuzzle() override;
    bool solverTurn() const override;

    void startRun(rush::Mode mode);
    void finishRun();
    void advancePuzzle();
    void armAdvance(float seconds);
    void backToSelect();
    void drawRunPanel(sf::RenderWindow& window);
    void drawSelectPanel(sf::RenderWindow& window);

private:
    // Mode-select state (before a run, and again after one ends).
    bool selectState_ = true;
    Button modeBtns_[3];

    rush::Run run_;
    std::vector<int> order_;
    int orderPos_ = -1;
    std::uint64_t seed_ = 0;

    // Armed wait before an auto-advance (-1 = off). Blocks solver input via
    // solverTurn() so a stalled animation can never double-apply.
    float advanceTimer_ = -1.f;
};

} // namespace chess::client
