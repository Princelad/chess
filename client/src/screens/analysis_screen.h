#pragma once

#include "app.h"
#include "boardview.h"
#include "widgets/label.h"
#include "widgets/move_navigator.h"
#include <chess/board.h>
#include <chess/move.h>
#include <chess/uci/engine.h>

#include <array>
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
    void drawBestMoveArrow(sf::RenderWindow& window) const;
    void drawEvalText(sf::RenderWindow& window);
    void layoutPanel();

    static constexpr float EvalBarWidth = 20.f;

    App& app_;
    std::string resultText_;
    std::vector<PlyEval> evals_;
    MoveNavigator navigator_;

    BoardView boardView_;
    HighlightState hl_;

    std::unique_ptr<uci::UciEngine> engine_;
    bool engineReady_ = false;
    int analysisPly_ = -1;
    mutable std::mutex evalMutex_;

    Label resultLabel_;
    Label analysisLabel_;
    Label plyLabel_;
    Label hint_;
    Label evalTextLabel_;
    Label turnLabel_;
};

} // namespace chess::client