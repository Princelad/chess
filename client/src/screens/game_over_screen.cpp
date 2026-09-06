#include "game_over_screen.h"
#include "analysis_screen.h"
#include "menu_screen.h"
#include "widgets/layout.h"

namespace chess::client {

namespace {
constexpr float BtnW = 200.f;
constexpr float BtnH = 44.f;
constexpr float BtnY = 300.f;
constexpr float AnalyzeBtnY = 360.f;
}

GameOverScreen::GameOverScreen(App& app,
                               net::GameResult result,
                               net::GameOverReason reason,
                               Board initialBoard,
                               std::vector<chess::Move> moves,
                               std::vector<std::string> sanMoves)
    : app_(app)
    , initialBoard_(std::move(initialBoard))
    , moves_(std::move(moves))
    , sanMoves_(std::move(sanMoves))
{
    std::string reasonStr;
    switch (reason) {
        case net::GameOverReason::Checkmate:          reasonStr = "Checkmate"; break;
        case net::GameOverReason::Stalemate:          reasonStr = "Stalemate"; break;
        case net::GameOverReason::FiftyMove:          reasonStr = "Fifty-move rule"; break;
        case net::GameOverReason::Repetition:         reasonStr = "Threefold repetition"; break;
        case net::GameOverReason::InsufficientMaterial: reasonStr = "Insufficient material"; break;
        case net::GameOverReason::Resignation:        reasonStr = "Resignation"; break;
        case net::GameOverReason::Disconnection:      reasonStr = "Disconnection"; break;
        case net::GameOverReason::Abort:              reasonStr = "Aborted"; break;
        case net::GameOverReason::AgreedDraw:         reasonStr = "Draw by agreement"; break;
        default:                                      reasonStr = "Unknown"; break;
    }

    std::string resultStr;
    switch (result) {
        case net::GameResult::WhiteWins:   resultStr = "White wins"; break;
        case net::GameResult::BlackWins:   resultStr = "Black wins"; break;
        case net::GameResult::Draw:        resultStr = "Draw"; break;
        case net::GameResult::Resignation: resultStr = "Resignation"; break;
        case net::GameResult::Abort:       resultStr = "Game aborted"; break;
        default:                           resultStr = "Unknown"; break;
    }

    switch (result) {
        case net::GameResult::Draw:
            resultText_ = resultStr;
            reasonText_ = reasonStr;
            break;
        case net::GameResult::Abort:
            resultText_ = resultStr;
            break;
        default:
            resultText_ = reasonStr + " — " + resultStr;
            break;
    }

    resultLabel_.setText(resultText_);
    resultLabel_.setFontSize(30);
    resultLabel_.setColor(sf::Color(255, 255, 255));

    reasonLabel_.setText("(" + reasonText_ + ")");
    reasonLabel_.setFontSize(20);
    reasonLabel_.setColor(sf::Color(160, 160, 160));

    hint_.setText("Tab to switch, Enter to select");
    hint_.setFontSize(14);
    hint_.setColor(sf::Color(100, 100, 100));

    rematchBtn_.setLabel("Rematch");
    rematchBtn_.setColors(sf::Color(60, 130, 60), sf::Color(80, 160, 80),
                          sf::Color(50, 110, 50), sf::Color(40, 90, 40),
                          sf::Color(120, 150, 120), sf::Color(240, 240, 240),
                          sf::Color(100, 100, 100), sf::Color(200, 200, 200),
                          sf::Color(160, 160, 160));
    rematchBtn_.setOnClick([this] { activateRematch(); });

    analyzeBtn_.setLabel("Analyze");
    analyzeBtn_.setColors(sf::Color(60, 80, 120), sf::Color(80, 100, 140),
                          sf::Color(50, 70, 100), sf::Color(40, 55, 80),
                          sf::Color(120, 130, 150), sf::Color(240, 240, 240),
                          sf::Color(100, 100, 100), sf::Color(200, 200, 200),
                          sf::Color(160, 160, 160));
    analyzeBtn_.setOnClick([this] { activateAnalyze(); });

    layoutWidgets();

    rematchBtn_.setFocused(true);
}

void GameOverScreen::layoutWidgets()
{
    const sf::FloatRect area(
        sf::Vector2f(0.f, 0.f),
        sf::Vector2f(App::WindowWidth, App::WindowHeight));
    const sf::Vector2f btnSize(BtnW, BtnH);

    auto rb = resultLabel_.bounds(app_.font());
    resultLabel_.setPosition({
        (App::WindowWidth - rb.size.x) / 2.f - rb.position.x, 200.f
    });

    auto rr = reasonLabel_.bounds(app_.font());
    reasonLabel_.setPosition({
        (App::WindowWidth - rr.size.x) / 2.f - rr.position.x, 250.f
    });

    rematchBtn_.setRect(layout::centerIn(
        sf::FloatRect(sf::Vector2f(0.f, BtnY),
                      sf::Vector2f(App::WindowWidth, BtnH)), btnSize));

    if (!moves_.empty()) {
        analyzeBtn_.setRect(layout::centerIn(
            sf::FloatRect(sf::Vector2f(0.f, AnalyzeBtnY),
                          sf::Vector2f(App::WindowWidth, BtnH)), btnSize));
    } else {
        analyzeBtn_.setEnabled(false);
    }

    const float hintY = !moves_.empty() ? AnalyzeBtnY + BtnH + 16.f
                                        : BtnY + BtnH + 16.f;
    auto hb = hint_.bounds(app_.font());
    hint_.setPosition({
        (App::WindowWidth - hb.size.x) / 2.f - hb.position.x, hintY
    });
}

void GameOverScreen::activateRematch()
{
    app_.connection().disconnect();
    app_.switchScreen(std::make_unique<MenuScreen>(app_));
}

void GameOverScreen::activateAnalyze()
{
    if (moves_.empty()) return;
    app_.connection().disconnect();
    app_.pushScreen(std::make_unique<AnalysisScreen>(
        app_, initialBoard_, moves_, sanMoves_, resultText_));
}

void GameOverScreen::cycleFocus()
{
    if (moves_.empty()) return;
    focus_ = 1 - focus_;
    rematchBtn_.setFocused(focus_ == 0);
    analyzeBtn_.setFocused(focus_ == 1);
}

void GameOverScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            activateRematch();
            return;
        }
        if (kp->code == sf::Keyboard::Key::Tab) {
            cycleFocus();
            return;
        }
    }

    sf::Vector2f local(0.f, 0.f);
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
        local = app_.toLocal(mm->position);
    else if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
        local = app_.toLocal(mb->position);
    else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
        local = app_.toLocal(rb->position);

    if (rematchBtn_.handleEvent(event, local)) return;
    if (moves_.empty()) return;
    if (analyzeBtn_.handleEvent(event, local)) return;
}

void GameOverScreen::update(float /*dtSec*/) {}

void GameOverScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    resultLabel_.draw(window, font);
    if (!reasonText_.empty())
        reasonLabel_.draw(window, font);

    if (rematchBtn_.isEnabled())
        rematchBtn_.draw(window, font);
    if (!moves_.empty())
        analyzeBtn_.draw(window, font);

    hint_.draw(window, font);
}

} // namespace chess::client