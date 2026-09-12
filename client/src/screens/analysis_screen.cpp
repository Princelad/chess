#include "analysis_screen.h"
#include "ui_helpers.h"
#include "widgets/layout.h"
#include "widgets/panel.h"

#include <chess/movegen.h>
#include <chess/san.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace chess::client {

namespace {
constexpr float MoveListTop = 130.f;
constexpr float MoveListBottom = 400.f;
constexpr float NavBtnY = 410.f;
constexpr float NavBtnH = 30.f;
constexpr float EvalTextY = 450.f;

float evalToBarFraction(int cp)
{
    float score = static_cast<float>(cp) / 100.f;
    float clamped = std::clamp(score, -6.f, 6.f);
    return 0.5f + clamped / 12.f;
}

std::string formatEval(const PlyEval& e)
{
    if (e.isMate) {
        if (e.mateIn == 0) return "M0";
        if (e.mateIn > 0) return "M+" + std::to_string(e.mateIn);
        return "M" + std::to_string(e.mateIn);
    }
    float pawns = static_cast<float>(e.score_cp) / 100.f;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", pawns);
    return buf;
}
}

AnalysisScreen::AnalysisScreen(App& app,
                               Board initialBoard,
                               std::vector<chess::Move> moves,
                               std::vector<std::string> sanMoves,
                               std::string result)
    : app_(app)
    , resultText_(std::move(result))
    , boardView_(static_cast<float>(App::WindowWidth),
                 static_cast<float>(App::WindowHeight),
                 Color::White)
{
    navigator_.setGame(std::move(initialBoard), std::move(moves), std::move(sanMoves));
    evals_.resize(navigator_.totalPlies() + 1);

    resultLabel_.setText(resultText_);
    resultLabel_.setFontSize(14);
    resultLabel_.setColor(sf::Color(160, 160, 160));

    analysisLabel_.setText("Analysis");
    analysisLabel_.setFontSize(18);
    analysisLabel_.setColor(sf::Color(200, 200, 200));

    plyLabel_.setText("");
    plyLabel_.setFontSize(14);
    plyLabel_.setColor(sf::Color(120, 120, 120));

    hint_.setText("Esc: back");
    hint_.setFontSize(12);
    hint_.setColor(sf::Color(80, 80, 80));

    evalTextLabel_.setFontSize(14);
    evalTextLabel_.setColor(sf::Color(200, 200, 200));
    turnLabel_.setFontSize(14);
    turnLabel_.setColor(sf::Color(120, 120, 120));

    layoutPanel();

    const char* envPath = std::getenv("CHESS_ENGINE_PATH");
    std::string enginePath = envPath ? envPath : "stockfish";
    engine_ = std::make_unique<uci::UciEngine>(enginePath);
    auto info = engine_->init();
    if (!info.name.empty() || engine_->isRunning()) {
        engine_->setOption("Threads", "1");
        engine_->setOption("Hash", "64");
        engine_->newGame();
        engineReady_ = true;
        engine_->onInfo([this](const uci::SearchInfo& si) {
            std::lock_guard<std::mutex> lock(evalMutex_);
            if (analysisPly_ < 0 || analysisPly_ >= static_cast<int>(evals_.size()))
                return;
            evals_[analysisPly_].score_cp = si.score_cp;
            evals_[analysisPly_].isMate = si.score_is_mate;
            evals_[analysisPly_].mateIn = si.mate_in;
            if (!si.pv.empty())
                evals_[analysisPly_].bestMove = si.pv[0];
        });
        startEngineAnalysis();
    }
}

AnalysisScreen::~AnalysisScreen()
{
    if (engine_) engine_->quit();
}

void AnalysisScreen::layoutPanel()
{
    auto& font = app_.font();

    float px = boardView_.panelX();
    float panelW = static_cast<float>(App::WindowWidth) - px - 8.f;

    resultLabel_.setPosition({px, 30.f});
    analysisLabel_.setPosition({px, 55.f});
    plyLabel_.setPosition({px, 80.f});
    hint_.setPosition({px, 600.f});

    navigator_.setLayout(
        sf::FloatRect(sf::Vector2f(px, MoveListTop),
                      sf::Vector2f(panelW, MoveListBottom - MoveListTop)),
        sf::FloatRect(sf::Vector2f(px, NavBtnY),
                      sf::Vector2f(panelW, NavBtnH)));

    evalTextLabel_.setPosition({px, EvalTextY});
    turnLabel_.setPosition({px, EvalTextY + 22.f});
}

void AnalysisScreen::startEngineAnalysis()
{
    if (!engineReady_ || !engine_) return;

    engine_->stop();
    engine_->position(navigator_.board().toFen());
    analysisPly_ = navigator_.currentPly();
    engine_->go(0, 0, true);
}

void AnalysisScreen::syncHighlights()
{
    const Board& shown = navigator_.board();
    hl_.lastMoveFrom = navigator_.lastMoveFrom();
    hl_.lastMoveTo = navigator_.lastMoveTo();
    Color stm = shown.sideToMove();
    hl_.checkSquare = chess::inCheck(shown, stm)
        ? findKingSquare(shown, stm)
        : std::optional<std::pair<int, int>>{};
}

void AnalysisScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            app_.goBack();
            return;
        }
        if (navigator_.handleEvent(event, {0.f, 0.f})) return;
        return;
    }

    sf::Vector2f local(0.f, 0.f);
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
        local = app_.toLocal(mm->position);
    else if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
        local = app_.toLocal(mb->position);
    else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
        local = app_.toLocal(rb->position);

    if (const auto* we = event.getIf<sf::Event::MouseWheelScrolled>()) {
        float px = boardView_.panelX();
        if (app_.toLocal(we->position).x >= px) {
            navigator_.handleScroll(we->delta);
            return;
        }
    }

    if (navigator_.handleEvent(event, local)) return;
}

void AnalysisScreen::update(float /*dtSec*/)
{
    if (!engineReady_ || !engine_) return;

    if (!engine_->isRunning()) {
        engineReady_ = false;
        return;
    }

    if (analysisPly_ == navigator_.currentPly()) {
        auto move = engine_->tryGetBestMove(navigator_.board());
        if (move) {
            engine_->stop();
            analysisPly_ = -1;
        }
    }
}

void AnalysisScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();
    const Board& shown = navigator_.board();

    boardView_.drawSquares(window);
    syncHighlights();
    boardView_.drawHighlights(window, hl_, shown);
    boardView_.drawLabels(window, font);
    boardView_.drawPieces(window, font, shown, app_);

    drawEvalBar(window);
    drawBestMoveArrow(window);
    navigator_.draw(window, font, app_);
    drawEvalText(window);

    resultLabel_.draw(window, font);
    analysisLabel_.draw(window, font);

    plyLabel_.setText("Position " + std::to_string(navigator_.currentPly())
                      + " / " + std::to_string(navigator_.totalPlies()));
    plyLabel_.draw(window, font);

    hint_.draw(window, font);
}

void AnalysisScreen::drawEvalBar(sf::RenderWindow& window) const
{
    sf::Vector2f origin = boardView_.boardOrigin();
    float boardH = boardView_.squareSize() * 8.f;

    Panel bg(sf::FloatRect(
        sf::Vector2f(origin.x - EvalBarWidth - 4.f, origin.y),
        sf::Vector2f(EvalBarWidth, boardH)),
        sf::Color(40, 40, 40));
    bg.draw(window);

    int ply = navigator_.currentPly();
    if (ply < 0 || ply >= static_cast<int>(evals_.size()))
        return;

    int scoreCp;
    {
        std::lock_guard<std::mutex> lock(evalMutex_);
        scoreCp = evals_[ply].score_cp;
    }

    float frac = evalToBarFraction(scoreCp);
    float whiteH = frac * boardH;

    sf::RectangleShape whiteBar({EvalBarWidth, whiteH});
    whiteBar.setPosition({origin.x - EvalBarWidth - 4.f, origin.y + boardH - whiteH});
    whiteBar.setFillColor(sf::Color(240, 240, 240));
    window.draw(whiteBar);

    sf::RectangleShape blackBar({EvalBarWidth, boardH - whiteH});
    blackBar.setPosition({origin.x - EvalBarWidth - 4.f, origin.y});
    blackBar.setFillColor(sf::Color(50, 50, 50));
    window.draw(blackBar);
}

void AnalysisScreen::drawBestMoveArrow(sf::RenderWindow& window) const
{
    int ply = navigator_.currentPly();
    if (ply < 0 || ply >= static_cast<int>(evals_.size()))
        return;

    std::string bm;
    {
        std::lock_guard<std::mutex> lock(evalMutex_);
        bm = evals_[ply].bestMove;
    }
    if (bm.empty() || bm.size() < 4)
        return;

    int fromFile = bm[0] - 'a';
    int fromRank = bm[1] - '1';
    int toFile = bm[2] - 'a';
    int toRank = bm[3] - '1';

    if (fromFile < 0 || fromFile > 7 || fromRank < 0 || fromRank > 7 ||
        toFile < 0 || toFile > 7 || toRank < 0 || toRank > 7)
        return;

    sf::Vector2f origin = boardView_.boardOrigin();
    float sq = boardView_.squareSize();
    bool flipped = boardView_.isFlipped();

    auto fileRankToPixel = [&](int file, int rank) -> sf::Vector2f {
        int col = flipped ? (7 - file) : file;
        int row = flipped ? rank : (7 - rank);
        return { origin.x + static_cast<float>(col) * sq + sq / 2.f,
                 origin.y + static_cast<float>(row) * sq + sq / 2.f };
    };

    sf::Vector2f from = fileRankToPixel(fromFile, fromRank);
    sf::Vector2f to = fileRankToPixel(toFile, toRank);

    sf::Vector2f dir = to - from;
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len < 1.f) return;

    sf::Vector2f unit = dir / len;

    float shaftLen = len - sq * 0.4f;
    sf::Vector2f shaftEnd = from + unit * shaftLen;

    float shaftW = sq * 0.12f;
    sf::Vector2f perp(-unit.y, unit.x);

    sf::ConvexShape shaft(4);
    shaft.setPoint(0, from + perp * shaftW);
    shaft.setPoint(1, shaftEnd + perp * shaftW);
    shaft.setPoint(2, shaftEnd - perp * shaftW);
    shaft.setPoint(3, from - perp * shaftW);
    shaft.setFillColor(sf::Color(0, 180, 0, 140));
    window.draw(shaft);

    float headLen = sq * 0.45f;
    float headW = sq * 0.22f;
    sf::Vector2f headBase = shaftEnd;
    sf::Vector2f headTip = to;

    sf::ConvexShape head(3);
    head.setPoint(0, headTip);
    head.setPoint(1, headBase + perp * headW);
    head.setPoint(2, headBase - perp * headW);
    head.setFillColor(sf::Color(0, 180, 0, 140));
    window.draw(head);
}

void AnalysisScreen::drawEvalText(sf::RenderWindow& window)
{
    auto& font = app_.font();
    int ply = navigator_.currentPly();

    if (ply >= 0 && ply < static_cast<int>(evals_.size())) {
        std::string evalStr;
        {
            std::lock_guard<std::mutex> lock(evalMutex_);
            evalStr = formatEval(evals_[ply]);
        }
        evalTextLabel_.setText("Eval: " + evalStr);
    }
    evalTextLabel_.draw(window, font);

    Color sideToMove = navigator_.board().sideToMove();
    turnLabel_.setText(sideToMove == Color::White ? "White to move" : "Black to move");
    turnLabel_.draw(window, font);
}

} // namespace chess::client