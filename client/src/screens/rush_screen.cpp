#include "rush_screen.h"

#include "ui/rounded_rect.h"
#include "ui/theme.h"
#include "ui_helpers.h"
#include "widgets/player_card.h"

#include <chess/san.h>

#include <chrono>

namespace chess::client {

RushScreen::RushScreen(App& app)
    : PuzzleScreen(app)
{
    // The base ctor loads puzzles, wires the 12.2 buttons and loads puzzle
    // 0 as the backdrop; the mode-select overlay rides on top of it.
    const int width = App::WindowWidth;
    const float cardX = static_cast<float>((width - 280) / 2);
    const char* labels[3];
    labels[0] = "3 min";
    labels[1] = "5 min";
    labels[2] = "Survival";
    const ui::ButtonStyle styles[3] = {
        ui::buttonConnect(), ui::buttonBlue(), ui::buttonPlum()};
    for (int i = 0; i < 3; ++i) {
        modeBtns_[i].setLabel(labels[i]);
        modeBtns_[i].setStyle(styles[i]);
        modeBtns_[i].setRect(sf::FloatRect(
            {cardX, 210.f + static_cast<float>(i) * 64.f}, {280.f, 44.f}));
        modeBtns_[i].setOnClick([this, i] {
            startRun(static_cast<rush::Mode>(i));
        });
    }

    // Repurpose the base's action column: Skip replaces Hint, Show solution
    // stays, Next is hidden while a run is live ("Play again" at the end).
    hintBtn_.setLabel("Skip");
    hintBtn_.setIcon(std::nullopt);
    hintBtn_.setStyle(ui::buttonAmber());
    hintBtn_.setOnClick([this] {
        if (selectState_ || run_.over || !solverTurn())
            return;
        if (run_.step(rush::Step::Skipped)) {
            finishRun();
            return;
        }
        setStatus("Skipped - streak reset.", 3.f);
        armAdvance(0.5f);
    });
}

void RushScreen::startRun(rush::Mode mode)
{
    if (puzzles_.empty())
        return;

    // Per-session feed: a fresh seed each run, recorded in config so the
    // shuffle stays reproducible within the session (17.1).
    seed_ = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    app_.config().setInt("puzzle.rush_seed",
                         static_cast<int>(seed_ & 0x7FFFFFFF));
    app_.config().save();

    order_ = rush::shuffledOrder(seed_, static_cast<int>(puzzles_.size()));
    orderPos_ = 0;
    selectState_ = false;
    advanceTimer_ = -1.f;
    run_ = rush::Run::start(mode, app_.config().getInt(rush::bestKey(mode), 0));

    nextBtn_.setLabel("Next puzzle");
    loadPuzzle(order_[0]);
}

void RushScreen::finishRun()
{
    if (run_.beatBest) {
        app_.config().setInt(rush::bestKey(run_.mode), run_.best);
        app_.config().save();
    }

    std::string msg;
    if (rush::timed(run_.mode))
        msg = "Time! Solved " + std::to_string(run_.score);
    else
        msg = "Run over. Solved " + std::to_string(run_.score);
    if (run_.beatBest)
        msg += " - new best!";
    else
        msg += " (best " + std::to_string(run_.best) + ")";
    setStatus(msg, 999.f);

    nextBtn_.setLabel("Play again");
    nextBtn_.setEnabled(true);
}

void RushScreen::advancePuzzle()
{
    if (puzzles_.empty())
        return;
    ++orderPos_;
    if (orderPos_ >= static_cast<int>(order_.size())) {
        // Pool exhausted: reshuffle from the seed stream and loop (only
        // Survival outlasts the bundled set).
        seed_ = seed_ * 6364136223846793005ull + 1442695040888963407ull;
        order_ = rush::shuffledOrder(seed_, static_cast<int>(puzzles_.size()));
        orderPos_ = 0;
    }
    loadPuzzle(order_[orderPos_]);
}

void RushScreen::armAdvance(float seconds)
{
    advanceTimer_ = seconds;
}

void RushScreen::backToSelect()
{
    selectState_ = true;
    advanceTimer_ = -1.f;
}

void RushScreen::onSolved()
{
    // Base: streak raise, puzzle.best_streak persist, "Solved!" status.
    PuzzleScreen::onSolved();
    if (run_.step(rush::Step::Solved)) {
        finishRun(); // defensive: neither mode ends on a solve today
        return;
    }
    armAdvance(0.8f);
}

void RushScreen::onWrongMove()
{
    // NOT the base restart: a rush run advances to the next puzzle.
    streak_.onFail();
    deselect();
    clearBoardInput();

    if (run_.step(rush::Step::Failed)) {
        finishRun();
        return;
    }
    if (rush::timed(run_.mode))
        setStatus("Wrong - next puzzle. Streak reset to 0.", 4.f);
    else
        setStatus("Wrong - strike " + std::to_string(run_.strikes)
                      + " of 3. Next puzzle.", 4.f);
    armAdvance(0.8f);
}

void RushScreen::revealSolution()
{
    PuzzleScreen::revealSolution(); // streak fail + full-line reveal
    if (run_.step(rush::Step::Skipped)) {
        finishRun();
        return;
    }
    setStatus("Solution shown - streak reset.", 3.f);
    armAdvance(1.2f);
}

void RushScreen::nextPuzzle()
{
    if (selectState_ || run_.over) {
        backToSelect();
        return;
    }
    PuzzleScreen::nextPuzzle();
}

bool RushScreen::solverTurn() const
{
    if (selectState_ || run_.over || advanceTimer_ >= 0.f)
        return false;
    return PuzzleScreen::solverTurn();
}

void RushScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape
            && (selectState_ || run_.over)) {
            backToMenu();
            return;
        }
        if (selectState_) {
            if (kp->code == sf::Keyboard::Key::Num1)
                startRun(rush::Mode::ThreeMin);
            else if (kp->code == sf::Keyboard::Key::Num2)
                startRun(rush::Mode::FiveMin);
            else if (kp->code == sf::Keyboard::Key::Num3)
                startRun(rush::Mode::Survival);
            return;
        }
    }

    if (selectState_) {
        sf::Vector2f local(0.f, 0.f);
        if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
            local = app_.toLocal(mm->position);
        else if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
            local = app_.toLocal(mb->position);
        else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
            local = app_.toLocal(rb->position);
        for (auto& b : modeBtns_)
            if (b.handleEvent(event, local)) return;
        backBtn_.handleEvent(event, local);
        return;
    }

    PuzzleScreen::handleEvent(event);
}

void RushScreen::update(float dtSec)
{
    if (selectState_)
        return;

    PuzzleScreen::update(dtSec);

    // Clock countdown; expiry ends the run and locks solver input (the
    // solverTurn() guard covers a stall mid-animation).
    run_.tick(dtSec);
    if (run_.over) {
        if (nextBtn_.label() != "Play again")
            finishRun();
        return;
    }

    if (advanceTimer_ >= 0.f) {
        advanceTimer_ -= dtSec;
        if (advanceTimer_ <= 0.f) {
            advanceTimer_ = -1.f;
            advancePuzzle();
        }
    }
}

void RushScreen::draw(sf::RenderWindow& window)
{
    drawBoardLayer(window);

    auto& font = app_.font();
    const float px = panelLayout_.panelX;
    const float height = static_cast<float>(App::WindowHeight);
    sf::RectangleShape panel({panelLayout_.panelEdgeWidth, height});
    panel.setPosition({px, 0.f});
    panel.setFillColor(ui::pal::puzzlePanel);
    window.draw(panel);

    if (selectState_) {
        drawSelectPanel(window);
        return;
    }
    drawRunPanel(window);
    if (showingHelp_) drawHelpOverlay(window);
}

void RushScreen::drawSelectPanel(sf::RenderWindow& window)
{
    auto& font = app_.font();
    // Dimmed board with a centered mode column (mouse, or 1/2/3 keys).
    sf::RectangleShape dim({static_cast<float>(App::WindowWidth),
                            static_cast<float>(App::WindowHeight)});
    dim.setFillColor(ui::overlay::dim);
    window.draw(dim);

    const float px = panelLayout_.panelX + panelLayout_.buttonInsetX;

    sf::Text title(font, "Puzzle Rush", 16);
    title.setFillColor(ui::pal::textPrimary);
    title.setPosition({px, panelLayout_.puzzleTitleY});
    window.draw(title);

    sf::Text sub(font, "Solve as many as you can", 14);
    sub.setFillColor(ui::pal::textGhost);
    sub.setPosition({px, panelLayout_.puzzleStreakY});
    window.draw(sub);

    for (int i = 0; i < 3; ++i) {
        const auto mode = static_cast<rush::Mode>(i);
        const int best = app_.config().getInt(rush::bestKey(mode), 0);
        std::string text = rush::label(mode);
        if (best > 0)
            text += "  -  best " + std::to_string(best);
        modeBtns_[i].setLabel(text);
        modeBtns_[i].draw(window, font);
    }
    backBtn_.draw(window, font);
}

void RushScreen::drawRunPanel(sf::RenderWindow& window)
{
    auto& font = app_.font();
    const float px = panelLayout_.panelX + panelLayout_.buttonInsetX;
    const auto* puzzle = current();

    sf::Text title(font,
                   std::string("Puzzle Rush - ") + rush::label(run_.mode),
                   15);
    title.setFillColor(ui::pal::textPrimary);
    title.setPosition({px, panelLayout_.puzzleTitleY});
    window.draw(title);

    // Time pill (11.1's clock styling): heat color as the clock drains.
    if (rush::timed(run_.mode)) {
        const double ms = static_cast<double>(run_.remainingSec) * 1000.0;
        const std::string text = formatClockText(ms);
        sf::Text clockText(font, text, 14);
        clockText.setFillColor(clockHeatColor(ms));
        const auto lb = clockText.getLocalBounds();
        constexpr float PillW = 64.f;
        constexpr float PillH = 24.f;
        ui::RoundedRectShape pill(
            {PillW, PillH}, PillH * 0.5f);
        pill.setPosition({px + panelLayout_.panelWidth - panelLayout_.buttonInsetX - PillW,
                          panelLayout_.puzzleTitleY - 3.f});
        pill.setFillColor(ui::pal::bgCard);
        pill.setOutlineColor(clockHeatColor(ms));
        pill.setOutlineThickness(1.f);
        window.draw(pill);
        clockText.setPosition({
            pill.getPosition().x + (PillW - lb.size.x) / 2.f - lb.position.x,
            pill.getPosition().y + (PillH - lb.size.y) / 2.f - lb.position.y});
        window.draw(clockText);
    }

    std::string runLine = "Solved " + std::to_string(run_.score)
        + "   streak " + std::to_string(run_.streak)
        + "   best " + std::to_string(run_.best);
    if (run_.beatBest)
        runLine += "   NEW BEST!";
    sf::Text runText(font, runLine, 14);
    runText.setFillColor(run_.beatBest ? ui::pal::warnBanner
                                       : ui::pal::streakSage);
    runText.setPosition({px, panelLayout_.puzzleStreakY});
    window.draw(runText);

    sf::Text statusText(font, status_, 14);
    statusText.setFillColor(run_.over ? ui::pal::ok
                                      : (solved_ ? ui::pal::ok
                                                 : ui::pal::warnBanner));
    statusText.setPosition({px, panelLayout_.puzzleStatusY});
    window.draw(statusText);

    if (!playedSans_.empty()) {
        std::string line;
        for (const auto& s : playedSans_) {
            if (!line.empty()) line += ' ';
            line += s;
        }
        sf::Text lineText(font, safeTruncate(line, 32), 14);
        lineText.setFillColor(ui::pal::solutionSlate);
        lineText.setPosition({px, panelLayout_.puzzleSolutionY});
        window.draw(lineText);
    }

    // Action column: Skip + Show solution while live, Play again when over.
    hintBtn_.setEnabled(puzzle && !run_.over && !solved_ && !revealed_);
    solutionBtn_.setEnabled(puzzle && !run_.over && !solved_ && !revealed_);
    backBtn_.setEnabled(true);

    if (run_.over) {
        nextBtn_.setEnabled(true);
        nextBtn_.draw(window, font);
    }
    hintBtn_.draw(window, font);
    solutionBtn_.draw(window, font);
    backBtn_.draw(window, font);
}

} // namespace chess::client
