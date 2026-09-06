#include "analysis_screen.h"
#include "ui_helpers.h"
#include "widgets/layout.h"

#include <chess/movegen.h>
#include <chess/san.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace chess::client {

namespace {
constexpr float MoveLineH = 16.f;
constexpr float MoveFontSize = 13;
constexpr float MoveListTop = 130.f;
constexpr float MoveListBottom = 400.f;
constexpr float NavBtnY = 410.f;
constexpr float NavBtnH = 30.f;
constexpr float EvalTextY = 450.f;
const char* NavLabels[4] = { "|<", "<", ">", ">|" };

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
    , initialBoard_(std::move(initialBoard))
    , moves_(std::move(moves))
    , sanMoves_(std::move(sanMoves))
    , resultText_(std::move(result))
    , board_(Board::fromStartPos())
    , boardView_(static_cast<float>(App::WindowWidth),
                 static_cast<float>(App::WindowHeight),
                 Color::White)
{
    evals_.resize(moves_.size() + 1);

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

    for (int i = 0; i < 4; ++i) {
        navButtons_[i].setLabel(NavLabels[i]);
        navButtons_[i].setColors(sf::Color(60, 60, 70), sf::Color(70, 70, 80),
                                 sf::Color(50, 50, 58), sf::Color(40, 40, 45),
                                 sf::Color(80, 80, 85), sf::Color(220, 220, 220),
                                 sf::Color(80, 80, 90), sf::Color(140, 140, 160),
                                 sf::Color(160, 160, 180));
        navButtons_[i].setOnClick([this, i] {
            switch (i) {
                case 0: goToPly(0); break;
                case 1: goToPly(currentPly_ - 1); break;
                case 2: goToPly(currentPly_ + 1); break;
                case 3: goToPly(static_cast<int>(moves_.size())); break;
            }
        });
    }

    layoutPanel();
    goToPly(0);

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

    moveListBg_.setRect(sf::FloatRect(
        sf::Vector2f(px, MoveListTop),
        sf::Vector2f(panelW, MoveListBottom - MoveListTop)));
    moveListBg_.setFill(sf::Color(25, 25, 25));
    moveListBg_.setOutline(sf::Color(80, 80, 80));

    const auto rects = layout::hstack(
        sf::FloatRect(sf::Vector2f(px, NavBtnY),
                      sf::Vector2f(panelW, NavBtnH)),
        6.f,
        std::vector<sf::Vector2f>(4, {(panelW - 18.f) / 4.f, NavBtnH}));
    for (int i = 0; i < 4; ++i)
        navButtons_[i].setRect(rects[i]);

    evalTextLabel_.setPosition({px, EvalTextY});
    turnLabel_.setPosition({px, EvalTextY + 22.f});
}

void AnalysisScreen::goToPly(int ply)
{
    int clamped = std::clamp(ply, 0, static_cast<int>(moves_.size()));
    if (clamped == currentPly_) return;

    board_ = initialBoard_;
    hl_ = HighlightState{};

    for (int i = 0; i < clamped; ++i) {
        board_.makeMove(moves_[i]);
    }

    currentPly_ = clamped;

    if (clamped > 0) {
        const auto& last = moves_[clamped - 1];
        hl_.lastMoveFrom = { static_cast<int>(chess::fileOf(last.from)),
                             static_cast<int>(chess::rankOf(last.from)) };
        hl_.lastMoveTo = { static_cast<int>(chess::fileOf(last.to)),
                           static_cast<int>(chess::rankOf(last.to)) };
    }

    Color sideToMove = board_.sideToMove();
    hl_.checkSquare = chess::inCheck(board_, sideToMove)
        ? findKingSquare(board_, sideToMove)
        : std::optional<std::pair<int,int>>{};

    const bool atStart = (currentPly_ == 0);
    const bool atEnd = (currentPly_ == static_cast<int>(moves_.size()));
    navButtons_[0].setEnabled(!atStart);
    navButtons_[1].setEnabled(!atStart);
    navButtons_[2].setEnabled(!atEnd);
    navButtons_[3].setEnabled(!atEnd);

    startEngineAnalysis();

    float listH = MoveListBottom - MoveListTop;
    int vis = std::max(1, static_cast<int>(listH / MoveLineH));
    int pairIdx = clamped / 2;
    if (pairIdx > moveScroll_ + vis - 1)
        moveScroll_ = pairIdx - vis + 1;
    if (pairIdx < moveScroll_)
        moveScroll_ = pairIdx;
    moveScroll_ = std::clamp(moveScroll_, 0, std::max(0, static_cast<int>(moves_.size() + 1) / 2 - vis));
}

void AnalysisScreen::startEngineAnalysis()
{
    if (!engineReady_ || !engine_) return;

    engine_->stop();
    engine_->position(board_.toFen());
    analysisPly_ = currentPly_;
    engine_->go(0, 0, true);
}

void AnalysisScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        switch (kp->code) {
            case sf::Keyboard::Key::Right:
                goToPly(currentPly_ + 1);
                break;
            case sf::Keyboard::Key::Left:
                goToPly(currentPly_ - 1);
                break;
            case sf::Keyboard::Key::Home:
                goToPly(0);
                break;
            case sf::Keyboard::Key::End:
                goToPly(static_cast<int>(moves_.size()));
                break;
            case sf::Keyboard::Key::Escape:
                app_.goBack();
                break;
            default:
                break;
        }
        return;
    }

    sf::Vector2f local(0.f, 0.f);
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
        local = app_.toLocal(mm->position);
    else if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
        local = app_.toLocal(mb->position);
    else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
        local = app_.toLocal(rb->position);

    for (auto& btn : navButtons_) {
        if (btn.handleEvent(event, local)) return;
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return;
        float px = boardView_.panelX();
        float panelW = static_cast<float>(App::WindowWidth) - px - 8.f;
        if (local.x >= px && local.x < px + panelW &&
            local.y >= MoveListTop && local.y < MoveListBottom) {
            int lineIdx = static_cast<int>((local.y - MoveListTop + 2.f) / MoveLineH)
                          + moveScroll_;
            int ply = lineIdx * 2;
            if (lineIdx >= 0 && ply <= static_cast<int>(moves_.size())) {
                int clickedPly = std::min(ply, static_cast<int>(moves_.size()));
                goToPly(clickedPly);
            }
        }
        return;
    }

    if (const auto* ws = event.getIf<sf::Event::MouseWheelScrolled>()) {
        float px = boardView_.panelX();
        if (app_.toLocal(ws->position).x >= px) {
            float listH = MoveListBottom - MoveListTop;
            int vis = std::max(1, static_cast<int>(listH / MoveLineH));
            int maxScroll = std::max(0, static_cast<int>((moves_.size() + 1) / 2) - vis);
            moveScroll_ -= static_cast<int>(ws->delta);
            moveScroll_ = std::clamp(moveScroll_, 0, maxScroll);
        }
    }
}

void AnalysisScreen::update(float /*dtSec*/)
{
    if (!engineReady_ || !engine_) return;

    if (!engine_->isRunning()) {
        engineReady_ = false;
        return;
    }

    if (analysisPly_ == currentPly_) {
        auto move = engine_->tryGetBestMove(board_);
        if (move) {
            engine_->stop();
            analysisPly_ = -1;
        }
    }
}

void AnalysisScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    boardView_.drawSquares(window);
    boardView_.drawHighlights(window, hl_, board_);
    boardView_.drawLabels(window, font);
    boardView_.drawPieces(window, font, board_, app_);

    drawEvalBar(window);
    drawBestMoveArrow(window);
    drawMoveList(window);
    for (auto& btn : navButtons_)
        btn.draw(window, font);
    drawEvalText(window);

    resultLabel_.draw(window, font);
    analysisLabel_.draw(window, font);

    plyLabel_.setText("Position " + std::to_string(currentPly_)
                      + " / " + std::to_string(static_cast<int>(moves_.size())));
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

    if (currentPly_ < 0 || currentPly_ >= static_cast<int>(evals_.size()))
        return;

    int scoreCp;
    {
        std::lock_guard<std::mutex> lock(evalMutex_);
        scoreCp = evals_[currentPly_].score_cp;
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
    if (currentPly_ < 0 || currentPly_ >= static_cast<int>(evals_.size()))
        return;

    std::string bm;
    {
        std::lock_guard<std::mutex> lock(evalMutex_);
        bm = evals_[currentPly_].bestMove;
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

void AnalysisScreen::drawMoveList(sf::RenderWindow& window)
{
    float px = boardView_.panelX();
    float panelW = static_cast<float>(App::WindowWidth) - px - 8.f;
    auto& font = app_.font();

    sf::RectangleShape sep({panelW, 1.f});
    sep.setPosition({px, 100.f});
    sep.setFillColor(sf::Color(80, 80, 80));
    window.draw(sep);

    sf::Text header(font, "Moves", 12);
    header.setFillColor(sf::Color(120, 120, 120));
    header.setPosition({px, 110.f});
    window.draw(header);

    float listH = MoveListBottom - MoveListTop;
    moveListBg_.draw(window);

    int vis = std::max(1, static_cast<int>(listH / MoveLineH));
    int totalPairs = (static_cast<int>(moves_.size()) + 1) / 2;

    int endPair = std::min(moveScroll_ + vis, totalPairs);

    float y = MoveListTop + 2.f;
    for (int i = moveScroll_; i < endPair; ++i) {
        if (y + MoveLineH > MoveListTop + listH) break;

        int whitePly = i * 2;
        int blackPly = i * 2 + 1;

        std::string line = std::to_string(i + 1) + ".";

        if (whitePly <= static_cast<int>(moves_.size())) {
            if (whitePly < static_cast<int>(sanMoves_.size()))
                line += " " + sanMoves_[whitePly];
            else
                line += " ...";
        }

        if (blackPly <= static_cast<int>(moves_.size())) {
            if (blackPly < static_cast<int>(sanMoves_.size()))
                line += "  " + sanMoves_[blackPly];
        }

        bool highlightWhite = (whitePly == currentPly_);
        bool highlightBlack = (blackPly == currentPly_);

        sf::Text moveText(font, line, MoveFontSize);
        moveText.setPosition({px + 6.f, y});

        if (highlightWhite || highlightBlack) {
            auto lb = moveText.getGlobalBounds();
            float hlX = px + 4.f;
            float hlW = panelW - 8.f;

            if (highlightWhite && !highlightBlack) {
                if (whitePly < static_cast<int>(sanMoves_.size())) {
                    auto before = sf::Text(font, std::to_string(i + 1) + ". ", MoveFontSize);
                    hlW = before.getLocalBounds().size.x + 6.f;
                }
            } else if (highlightBlack) {
                if (whitePly < static_cast<int>(sanMoves_.size())) {
                    auto before = sf::Text(font,
                        std::to_string(i + 1) + ". " + sanMoves_[whitePly] + "  ",
                        MoveFontSize);
                    hlX = px + 4.f + before.getLocalBounds().size.x;
                    if (blackPly < static_cast<int>(sanMoves_.size())) {
                        hlW = sf::Text(font, sanMoves_[blackPly], MoveFontSize)
                            .getLocalBounds().size.x + 6.f;
                    }
                }
            }

            sf::RectangleShape hl({hlW, MoveLineH});
            hl.setPosition({hlX, y});
            hl.setFillColor(sf::Color(0, 120, 215, 100));
            window.draw(hl);
        }

        moveText.setFillColor(sf::Color(200, 200, 200));
        window.draw(moveText);
        y += MoveLineH;
    }

    if (moves_.empty()) {
        sf::Text empty(font, "No moves", MoveFontSize);
        empty.setFillColor(sf::Color(100, 100, 100));
        empty.setPosition({px + 6.f, MoveListTop + 4.f});
        window.draw(empty);
    }
}

void AnalysisScreen::drawEvalText(sf::RenderWindow& window)
{
    auto& font = app_.font();

    if (currentPly_ >= 0 && currentPly_ < static_cast<int>(evals_.size())) {
        std::string evalStr;
        {
            std::lock_guard<std::mutex> lock(evalMutex_);
            evalStr = formatEval(evals_[currentPly_]);
        }
        evalTextLabel_.setText("Eval: " + evalStr);
    }
    evalTextLabel_.draw(window, font);

    Color sideToMove = board_.sideToMove();
    turnLabel_.setText(sideToMove == Color::White ? "White to move" : "Black to move");
    turnLabel_.draw(window, font);
}

} // namespace chess::client