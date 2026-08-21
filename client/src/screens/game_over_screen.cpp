#include "game_over_screen.h"
#include "connect_screen.h"

namespace chess::client {

namespace {
constexpr float BtnW = 200.f;
constexpr float BtnH = 44.f;
constexpr float BtnY = 340.f;
}

GameOverScreen::GameOverScreen(App& app,
                               net::GameResult result,
                               net::GameOverReason reason)
    : app_(app)
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
            resultText_ = reasonStr;
            break;
        case net::GameResult::Abort:
            resultText_ = resultStr;
            break;
        default:
            resultText_ = reasonStr + " — " + resultStr;
            break;
    }
}

void GameOverScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Enter ||
            kp->code == sf::Keyboard::Key::Escape) {
            app_.connection().disconnect();
            app_.switchScreen(std::make_unique<ConnectScreen>(app_));
        }
    }

    if (const auto* mm = event.getIf<sf::Event::MouseMoved>()) {
        float mx = static_cast<float>(mm->position.x);
        float my = static_cast<float>(mm->position.y);
        float btnX = (App::WindowWidth - BtnW) / 2.f;
        rematchHovered_ = (mx >= btnX && mx < btnX + BtnW &&
                           my >= BtnY && my < BtnY + BtnH);
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            float mx = static_cast<float>(mb->position.x);
            float my = static_cast<float>(mb->position.y);
            float btnX = (App::WindowWidth - BtnW) / 2.f;
            if (mx >= btnX && mx < btnX + BtnW &&
                my >= BtnY && my < BtnY + BtnH) {
                app_.connection().disconnect();
                app_.switchScreen(std::make_unique<ConnectScreen>(app_));
            }
        }
    }
}

void GameOverScreen::update(float /*dtSec*/) {}

void GameOverScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    sf::Text resultLabel(font, resultText_, 30);
    resultLabel.setFillColor(sf::Color(255, 255, 255));
    auto rb = resultLabel.getGlobalBounds();
    resultLabel.setPosition({(App::WindowWidth - rb.size.x) / 2.f - rb.position.x, 200.f});
    window.draw(resultLabel);

    if (!reasonText_.empty()) {
        sf::Text reasonLabel(font, "(" + reasonText_ + ")", 20);
        reasonLabel.setFillColor(sf::Color(160, 160, 160));
        auto rr = reasonLabel.getGlobalBounds();
        reasonLabel.setPosition({(App::WindowWidth - rr.size.x) / 2.f - rr.position.x, 250.f});
        window.draw(reasonLabel);
    }

    float btnX = (App::WindowWidth - BtnW) / 2.f;
    sf::RectangleShape btn({BtnW, BtnH});
    btn.setPosition({btnX, BtnY});
    btn.setFillColor(rematchHovered_
        ? sf::Color(80, 160, 80) : sf::Color(60, 130, 60));
    btn.setOutlineColor(sf::Color(100, 100, 100));
    btn.setOutlineThickness(1.f);
    window.draw(btn);

    sf::Text btnText(font, "Rematch", 18);
    btnText.setFillColor(sf::Color(240, 240, 240));
    auto bb = btnText.getGlobalBounds();
    btnText.setPosition({
        btnX + (BtnW - bb.size.x) / 2.f - bb.position.x,
        BtnY + (BtnH - bb.size.y) / 2.f - bb.position.y
    });
    window.draw(btnText);

    sf::Text hint(font, "or press Enter", 14);
    hint.setFillColor(sf::Color(100, 100, 100));
    auto hb = hint.getGlobalBounds();
    hint.setPosition({(App::WindowWidth - hb.size.x) / 2.f - hb.position.x, BtnY + BtnH + 16.f});
    window.draw(hint);
}

} // namespace chess::client
