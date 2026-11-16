#pragma once

#include "app.h"
#include "boardview.h"
#include "eval_harness.h"
#include "move_quality.h"
#include "screens/board_layout.h"
#include "widgets/label.h"
#include "widgets/move_navigator.h"

#include <chess/board.h>
#include <chess/move.h>
#include <chess/types.h>

#include <optional>
#include <string>
#include <vector>

namespace chess::client {

// 16.3: the post-game report — accuracy bars per side, the class legend,
// the opening name and a worst-moves pile that jumps the navigator. Reads
// from the App-owned eval harness (16.1) and the pure classifier (16.2);
// the engine spawns only through the harness's ensureEngine() and its job
// starts on ENTRY (on demand — never in the background during play).
class ReportScreen : public Screen {
public:
    ReportScreen(App& app,
                 Board initialBoard,
                 std::vector<chess::Move> moves,
                 std::vector<std::string> sanMoves,
                 std::string result,
                 std::int64_t archiveId = 0);
    ~ReportScreen() override;

    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    // Harness snapshot → classifier input → aggregates. Computes once per
    // report completion (a Done job is a quiet no-op afterward).
    void buildQuality();

    void refreshWorstList();
    void layoutPanel();

    App& app_;
    std::string resultText_;
    EvalKey key_;
    std::int64_t archiveId_ = 0;

    BoardView boardView_;
    MoveNavigator navigator_;
    layout::BoardLayout panelLayout_;

    Label reportTitleLabel_;
    Label resultLabel_;
    Label openingLabel_;
    Label plyLabel_;
    Label statusLabel_;   // analysis progress / engine-absent notice
    Label hintLabel_;

    int worstCount_ = 0; // visible entries (bounded by the panel height)
    // Click/jump rects for the visible worst entries (ply AFTER the move).
    std::vector<std::pair<sf::FloatRect, int>> worstRects_;
    // Rebuilt whenever the quality is rebuilt; header text cached.
    std::vector<std::string> worstLines_;

    // Computed once when the harness reports Done.
    bool computed_ = false;
    std::optional<quality::GameQuality> quality_;
};

} // namespace chess::client
