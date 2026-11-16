#include "analysis_screen.h"

#include "analysis_notes.h"
#include "move_quality.h"
#include "ui/theme.h"
#include "ui_helpers.h"
#include "widgets/layout.h"
#include "widgets/panel.h"

#include <chess/movegen.h>
#include <chess/san.h>
#include <chess/uci_move.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace chess::client {

namespace {
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
                 Color::White,
                 app.boardTheme(),
                 app.showCoordinates(),
                 app.coordinateStyle())
    , panelLayout_(layout::composeBoardLayout(
          static_cast<float>(App::WindowWidth),
          static_cast<float>(App::WindowHeight), boardView_,
          layout::BoardLayoutOptions{false, true}))
{
    // Identity + notes hash BEFORE the board is moved into the navigator.
    const std::string startFen = initialBoard.toFen();
    const std::vector<std::string> sans = sanMoves;
    notesHash_ = notes::gameHash(startFen, sans);
    key_ = evalKeyFor(0, startFen, sans);

    navigator_.setGame(std::move(initialBoard), std::move(moves), std::move(sanMoves));
    evals_.resize(navigator_.totalPlies() + 1);
    evalDone_.assign(navigator_.totalPlies() + 1, false);

    resultLabel_.setText(resultText_);
    resultLabel_.setFontSize(14);
    resultLabel_.setColor(ui::pal::textMuted);

    analysisLabel_.setText("Analysis");
    analysisLabel_.setFontSize(18);
    analysisLabel_.setColor(ui::pal::textSecondary);

    plyLabel_.setText("");
    plyLabel_.setFontSize(14);
    plyLabel_.setColor(ui::pal::textFaded);

    hint_.setText("Esc: back");
    hint_.setFontSize(12);
    hint_.setColor(ui::pal::stroke);

    evalTextLabel_.setFontSize(14);
    evalTextLabel_.setColor(ui::pal::textSecondary);
    turnLabel_.setFontSize(14);
    turnLabel_.setColor(ui::pal::textFaded);

    // 16.4 PV-arrows toggle: full principal-variation arrows on, the single
    // best-move arrow off. Persisted via the 9.5 config file.
    pvToggle_.setLabel("PV arrows");
    pvToggle_.setChecked(app.config().getBool("analysis.pv_arrows", false));
    pvToggle_.setOnToggle([this](bool on) {
        app_.config().set("analysis.pv_arrows", on ? "true" : "false");
        app_.config().save();
    });

    loadNotes();
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
            // One atomic read: the ply must not change between the bounds
            // check and the use.
            const int ply = analysisPly_.load(std::memory_order_relaxed);
            if (ply < 0 || ply >= static_cast<int>(evals_.size()))
                return;
            evals_[ply].score_cp = si.score_cp;
            evals_[ply].isMate = si.score_is_mate;
            evals_[ply].mateIn = si.mate_in;
            if (!si.pv.empty()) {
                evals_[ply].bestMove = si.pv[0];
                evals_[ply].pv = si.pv; // full line for the toggle
                evalDone_[ply] = true;  // eval-graph data point
            }
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
    resultLabel_.setPosition({panelLayout_.panelX, panelLayout_.analysisTitleY});
    analysisLabel_.setPosition({panelLayout_.panelX, panelLayout_.analysisInfoY});
    plyLabel_.setPosition({panelLayout_.panelX, panelLayout_.analysisPlyY});
    hint_.setPosition({panelLayout_.panelX, panelLayout_.hintY});

    navigator_.setLayout(panelLayout_.moveList, panelLayout_.navRowA);

    evalTextLabel_.setPosition({panelLayout_.panelX, panelLayout_.evalTextY});
    turnLabel_.setPosition({panelLayout_.panelX, panelLayout_.evalTurnY});

    pvToggle_.setRect(sf::FloatRect(
        sf::Vector2f(panelLayout_.panelX, panelLayout_.pvToggleY),
        sf::Vector2f(panelLayout_.panelWidth, 20.f)));
}

void AnalysisScreen::startEngineAnalysis()
{
    if (!engineReady_ || !engine_) return;

    engine_->stop();
    engine_->position(navigator_.board().toFen());
    analysisPly_ = navigator_.currentPly();
    lastAnalyzedPly_ = navigator_.currentPly();
    engine_->go(0, 0, true);
}

void AnalysisScreen::loadNotes()
{
    notes_.assign(navigator_.totalPlies() + 1, BoardAnnotations{});
    for (int ply = 0; ply <= navigator_.totalPlies(); ++ply) {
        const std::string key =
            "analysis." + notesHash_ + "." + std::to_string(ply);
        const std::string value = app_.config().get(key);
        if (!value.empty()) notes::decodeAnnotations(value, notes_[ply]);
    }
}

void AnalysisScreen::saveCurrentNotes()
{
    const int ply = navigator_.currentPly();
    if (ply < 0 || ply >= static_cast<int>(notes_.size())) return;
    const std::string key =
        "analysis." + notesHash_ + "." + std::to_string(ply);
    const std::string value = notes::encodeAnnotations(notes_[ply]);
    if (value.empty())
        app_.config().remove(key);
    else
        app_.config().set(key, value);
    app_.config().save();
}

void AnalysisScreen::refreshClassLayer()
{
    if (classLayerFetched_) return;
    EvalHarness& harness = app_.evalHarness();
    if (harness.progress(key_).state != EvalState::Done) return;
    classLayerFetched_ = true;
    // Cached-only: a report computed elsewhere (post-game report screen)
    // colors this replay for free; nothing is spawned from here.
    if (auto report = harness.report(key_)) {
        classLayer_ = quality::classLayerFor(
            *report, navigator_.initialBoard(), navigator_.moves(),
            navigator_.sans(), quality::bookPlies(navigator_.sans()));
        navigator_.setClassLayer(classLayer_);

        // Seed the eval graph + arrow data from the cached report (per-node
        // evals ride the mover's seat exactly like the live engine's info).
        {
            std::lock_guard<std::mutex> lock(evalMutex_);
            const int count = std::min<int>(
                static_cast<int>(report->nodes.size()),
                static_cast<int>(evals_.size()));
            for (int i = 0; i < count; ++i) {
                const auto& ne = report->evals[i].eval;
                evals_[i].score_cp = ne.scoreCp;
                evals_[i].isMate = ne.isMate;
                evals_[i].mateIn = ne.mateIn;
                // SAN -> UCI for the arrow renderer (full PV is not cached).
                if (auto board = Board::fromFen(report->nodes[i].fen)) {
                    if (auto move = san::fromSan(*board, ne.bestSan))
                        evals_[i].bestMove = toUci(*move);
                }
                evalDone_[i] = true;
            }
        }
    }
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
        if (app_.toLocal(we->position).x >= panelLayout_.panelX) {
            navigator_.handleScroll(we->delta);
            return;
        }
    }

    // 16.4: right-mouse annotation editing over the shown position — a drag
    // draws an arrow, a tap toggles a circle. Left mouse stays navigator-only.
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Right
            && boardView_.pixelToSquare(local)) {
            rightDrag_.press(local.x, local.y);
            return;
        }
    }
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>()) {
        if (rightDrag_.isActive()) {
            rightDrag_.move(local.x, local.y);
            return;
        }
    }
    if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (rb->button == sf::Mouse::Button::Right && rightDrag_.isActive()) {
            const bool dragged = rightDrag_.release();
            const int ply = navigator_.currentPly();
            const auto from = boardView_.pixelToSquare(
                {rightDrag_.pressX(), rightDrag_.pressY()});
            const auto to = boardView_.pixelToSquare(local);
            if (ply >= 0 && ply < static_cast<int>(notes_.size()) && to) {
                if (dragged && from && *from != *to)
                    notes_[ply].addArrow(from->first, from->second,
                                         to->first, to->second);
                else if (!dragged)
                    notes_[ply].toggleCircle(to->first, to->second);
                saveCurrentNotes();
            }
            return;
        }
    }

    if (navigator_.handleEvent(event, local)) return;
}

void AnalysisScreen::update(float /*dtSec*/)
{
    refreshClassLayer();

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
    } else if (analysisPly_ == -1
               && lastAnalyzedPly_ != navigator_.currentPly()) {
        // Audit: after a search completes, analysisPly_ parks at -1 and
        // nothing ever re-armed it — navigating to a fresh ply showed the
        // stale arrow/eval of whichever ply was analyzed last, forever.
        startEngineAnalysis();
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

    const int ply = navigator_.currentPly();
    if (ply >= 0 && ply < static_cast<int>(notes_.size())
        && !notes_[ply].empty())
        boardView_.drawAnnotations(window, notes_[ply].arrows(),
                                   notes_[ply].circles());

    drawEvalBar(window);
    drawBestMoveArrows(window);
    navigator_.draw(window, font, app_);
    drawEvalText(window);
    drawEvalGraph(window);
    pvToggle_.draw(window, font);

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
        ui::pal::evalTrack); // eval bar track (analysis-only)
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
    whiteBar.setFillColor(ui::pal::textPrimary); // eval bar white side
    window.draw(whiteBar);

    sf::RectangleShape blackBar({EvalBarWidth, boardH - whiteH});
    blackBar.setPosition({origin.x - EvalBarWidth - 4.f, origin.y});
    blackBar.setFillColor(ui::pal::evalBlack); // eval bar black side
    window.draw(blackBar);
}

void AnalysisScreen::drawBestMoveArrows(sf::RenderWindow& window) const
{
    int ply = navigator_.currentPly();
    if (ply < 0 || ply >= static_cast<int>(evals_.size()))
        return;
    const Board& shown = navigator_.board();

    // Arrow chain to draw: the full PV when the toggle is on, the single
    // best move otherwise. Tokens replay from the SHOWN board via fromUci
    // (full legality), so junk or stale lines degrade to nothing.
    std::vector<std::pair<int, int>> squares;
    {
        std::lock_guard<std::mutex> lock(evalMutex_);
        const PlyEval& pe = evals_[ply];
        if (pvToggle_.isChecked() && !pe.pv.empty()) {
            Board scratch = shown;
            for (const std::string& uci : pe.pv) {
                auto m = chess::fromUci(scratch, uci);
                if (!m) break;
                auto sq = [](Square s) {
                    return std::make_pair(static_cast<int>(fileOf(s)),
                                          static_cast<int>(rankOf(s)));
                };
                squares.push_back(sq(m->from));
                squares.push_back(sq(m->to));
                scratch.makeMove(*m);
            }
        } else if (pe.bestMove.size() >= 4) {
            squares.emplace_back(pe.bestMove[0] - 'a', pe.bestMove[1] - '1');
            squares.emplace_back(pe.bestMove[2] - 'a', pe.bestMove[3] - '1');
        }
    }
    if (squares.size() < 2) return;

    sf::Vector2f origin = boardView_.boardOrigin();
    float sq = boardView_.squareSize();
    bool flipped = boardView_.isFlipped();

    auto fileRankToPixel = [&](int file, int rank) -> sf::Vector2f {
        int col = flipped ? (7 - file) : file;
        int row = flipped ? rank : (7 - rank);
        return { origin.x + static_cast<float>(col) * sq + sq / 2.f,
                 origin.y + static_cast<float>(row) * sq + sq / 2.f };
    };

    auto drawArrow = [&](sf::Vector2f from, sf::Vector2f to) {
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
        shaft.setFillColor(sf::Color(ui::pal::bestMoveGreen.r,
                                     ui::pal::bestMoveGreen.g,
                                     ui::pal::bestMoveGreen.b, 140));
        window.draw(shaft);

        sf::Vector2f headBase = shaftEnd;

        sf::ConvexShape head(3);
        head.setPoint(0, to);
        head.setPoint(1, headBase + perp * (sq * 0.22f));
        head.setPoint(2, headBase - perp * (sq * 0.22f));
        head.setFillColor(sf::Color(ui::pal::bestMoveGreen.r,
                                    ui::pal::bestMoveGreen.g,
                                    ui::pal::bestMoveGreen.b, 140));
        window.draw(head);
    };

    for (std::size_t i = 0; i + 1 < squares.size(); i += 2) {
        const auto& fromSq = squares[i];
        const auto& toSq = squares[i + 1];
        if (fromSq.first < 0 || fromSq.first > 7 || fromSq.second < 0
            || fromSq.second > 7 || toSq.first < 0 || toSq.first > 7
            || toSq.second < 0 || toSq.second > 7)
            break;
        if (fromSq == toSq) continue; // castle-style from==to tokens
        drawArrow(fileRankToPixel(fromSq.first, fromSq.second),
                  fileRankToPixel(toSq.first, toSq.second));
    }
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

// 16.4 eval graph: the 8.5 eval-graph promise, upgraded — a win% polyline
// (White's seat, White advantage up) across every analyzed ply, with the
// chip layer's classified plies marked in their class color. Plies the
// engine never scored are gaps, not fake midlines.
void AnalysisScreen::drawEvalGraph(sf::RenderWindow& window)
{
    const float x = panelLayout_.panelX;
    const float w = panelLayout_.panelWidth;
    const float y = panelLayout_.evalGraphY;
    const float h = panelLayout_.evalGraphH;
    if (w <= 0.f || h <= 0.f) return;

    sf::RectangleShape bg({w, h});
    bg.setPosition({x, y});
    bg.setFillColor(ui::pal::bgSunken);
    bg.setOutlineColor(ui::pal::stroke);
    bg.setOutlineThickness(1.f);
    window.draw(bg);

    // 50% midline.
    sf::Vertex mid[2] = {
        sf::Vertex{sf::Vector2f(x + 1.f, y + h / 2.f), ui::pal::strokeFaint},
        sf::Vertex{sf::Vector2f(x + w - 1.f, y + h / 2.f), ui::pal::strokeFaint},
    };
    window.draw(mid, 2, sf::PrimitiveType::Lines);

    const int total = navigator_.totalPlies();
    if (total <= 0) return;

    // Mover at ply p alternates from the START board's side to move, so
    // FEN-start replays (history archive) keep White's seat honest.
    const bool whiteStarts =
        navigator_.initialBoard().sideToMove() == Color::White;
    auto moverAt = [&](int ply) {
        const bool white = whiteStarts == (ply % 2 == 0);
        return white ? Color::White : Color::Black;
    };

    auto pointAt = [&](int ply) -> sf::Vector2f {
        float whiteWin;
        {
            std::lock_guard<std::mutex> lock(evalMutex_);
            const PlyEval& pe = evals_[ply];
            const float moverWin = pe.isMate
                ? (pe.mateIn > 0 ? 100.f : 0.f)
                : quality::winPercent(pe.score_cp);
            const Color mover = moverAt(ply);
            whiteWin = mover == Color::White ? moverWin : 100.f - moverWin;
        }
        return {
            x + 1.f + (static_cast<float>(ply) / static_cast<float>(total))
                * (w - 2.f),
            y + 1.f + (1.f - whiteWin / 100.f) * (h - 2.f),
        };
    };

    // doneCopy: the engine callback flips evalDone_ under the mutex, so
    // the draw path reads it under the mutex too (a plain read is a data
    // race). Snapshot once, then draw — pointAt re-locks per call.
    std::vector<char> doneCopy;
    {
        std::lock_guard<std::mutex> lock(evalMutex_);
        doneCopy.assign(evalDone_.begin(), evalDone_.end());
    }

    // Polyline segments only between consecutive analyzed plies.
    for (int ply = 0; ply < total; ++ply) {
        if (!doneCopy[ply] || !doneCopy[ply + 1]) continue;
        sf::Vertex seg[2] = {
            sf::Vertex{pointAt(ply), ui::pal::selectionBlue},
            sf::Vertex{pointAt(ply + 1), ui::pal::selectionBlue},
        };
        window.draw(seg, 2, sf::PrimitiveType::Lines);
    }

    // Classified markers ride the chip layer's colors.
    for (int ply = 0;
         ply < static_cast<int>(classLayer_.size()) && ply <= total; ++ply) {
        if (classLayer_[ply] < 0 || !doneCopy[ply]) continue;
        sf::CircleShape mark(3.f);
        mark.setPosition(pointAt(ply) - sf::Vector2f(3.f, 3.f));
        switch (static_cast<quality::MoveClass>(classLayer_[ply])) {
            case quality::MoveClass::Best:
                mark.setFillColor(ui::chips::best); break;
            case quality::MoveClass::Brilliant:
                mark.setFillColor(ui::chips::brilliant); break;
            case quality::MoveClass::Good:
                mark.setFillColor(ui::chips::good); break;
            case quality::MoveClass::Inaccuracy:
                mark.setFillColor(ui::chips::inaccuracy); break;
            case quality::MoveClass::Mistake:
                mark.setFillColor(ui::chips::mistake); break;
            case quality::MoveClass::Blunder:
                mark.setFillColor(ui::chips::blunder); break;
            case quality::MoveClass::Book:
                mark.setFillColor(ui::chips::book); break;
        }
        window.draw(mark);
    }

    // Current-ply cursor: a thin white tick for orientation.
    const int ply = navigator_.currentPly();
    if (ply >= 0 && ply <= total && doneCopy[ply]) {
        sf::Vector2f p = pointAt(ply);
        sf::Vertex tick[2] = {
            sf::Vertex{sf::Vector2f(p.x, y + 1.f), ui::pal::white},
            sf::Vertex{sf::Vector2f(p.x, y + h - 1.f), ui::pal::white},
        };
        window.draw(tick, 2, sf::PrimitiveType::Lines);
    }
}

} // namespace chess::client