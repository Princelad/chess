#pragma once

#include "app.h"
#include "board_layout.h"
#include "boardview.h"
#include "board_interaction.h"
#include "eval_harness.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"
#include "widgets/move_navigator.h"
#include <chess/board.h>
#include <chess/move.h>
#include <chess/uci/engine.h>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace chess::client {

struct PlyEval {
    int score_cp = 0;
    bool isMate = false;
    int mateIn = 0;
    std::string bestMove;
    std::vector<std::string> pv; // 16.4: full principal variation (UCI tokens)
};

class AnalysisScreen : public Screen {
public:
    AnalysisScreen(App& app,
                   Board initialBoard,
                   std::vector<chess::Move> moves,
                   std::vector<std::string> sanMoves,
                   std::string result);
    ~AnalysisScreen() override;

    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    void startEngineAnalysis();
    void syncHighlights();
    void drawEvalBar(sf::RenderWindow& window) const;
    void drawBestMoveArrows(sf::RenderWindow& window) const;
    void drawEvalText(sf::RenderWindow& window);
    void drawEvalGraph(sf::RenderWindow& window);
    void layoutPanel();

    // 16.4 annotation editing + persistence (client-local config).
    void loadNotes();
    void saveCurrentNotes();
    void refreshClassLayer();

    static constexpr float EvalBarWidth = 20.f;

    App& app_;
    std::string resultText_;
    std::vector<PlyEval> evals_;
    std::vector<bool> evalDone_;   // ply holds engine data (eval graph)
    MoveNavigator navigator_;

    BoardView boardView_;
    // 15.1: composed panel geometry (shared board composer consumer).
    layout::BoardLayout panelLayout_;
    HighlightState hl_;

    std::unique_ptr<uci::UciEngine> engine_;
    bool engineReady_ = false;
    // Atomic: the engine's onInfo callback reads it from the search
    // thread while the UI writes it in update()/startEngineAnalysis().
    std::atomic<int> analysisPly_{-1};
    int lastAnalyzedPly_ = -1; // re-arm trigger when navigation moves on
    mutable std::mutex evalMutex_;

    Label resultLabel_;
    Label analysisLabel_;
    Label plyLabel_;
    Label hint_;
    Label evalTextLabel_;
    Label turnLabel_;

    // 16.4: PV-arrows toggle + eval-graph chip source + annotations.
    Checkbox pvToggle_;
    std::vector<signed char> classLayer_;
    bool classLayerFetched_ = false;
    EvalKey key_{};
    std::string notesHash_;
    std::vector<BoardAnnotations> notes_;
    DragTracker rightDrag_{12.f};
};

} // namespace chess::client