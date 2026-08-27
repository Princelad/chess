#pragma once

#include "app.h"
#include "boardview.h"
#include <chess/board.h>
#include <chess/move.h>
#include <chess/uci/engine.h>

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
    void goToPly(int ply);
    void startEngineAnalysis();
    void drawEvalBar(sf::RenderWindow& window) const;
    void drawBestMoveArrow(sf::RenderWindow& window) const;
    void drawMoveList(sf::RenderWindow& window) const;
    void drawNavButtons(sf::RenderWindow& window);
    void drawEvalText(sf::RenderWindow& window) const;

    static constexpr float EvalBarWidth = 20.f;

    App& app_;
    Board initialBoard_;
    std::vector<chess::Move> moves_;
    std::vector<std::string> sanMoves_;
    std::string resultText_;
    Board board_;
    int currentPly_ = 0;

    BoardView boardView_;
    HighlightState hl_;
    std::vector<PlyEval> evals_;

    std::unique_ptr<uci::UciEngine> engine_;
    bool engineReady_ = false;
    int analysisPly_ = -1;
    mutable std::mutex evalMutex_;

    int moveScroll_ = 0;

    struct NavBtn {
        sf::FloatRect rect;
        std::string label;
        NavBtn(float x, float y, float w, float h, std::string l)
            : rect({x, y}, {w, h}), label(std::move(l)) {}
    };
    std::vector<NavBtn> navBtns_;
};

} // namespace chess::client
