#include "game_over_screen.h"
#include "connect_screen.h"

namespace chess::client {

GameOverScreen::GameOverScreen(App& app,
                               net::GameResult result,
                               net::GameOverReason reason)
    : app_(app)
{
    switch (result) {
        case net::GameResult::WhiteWins:   resultText_ = "White wins"; break;
        case net::GameResult::BlackWins:   resultText_ = "Black wins"; break;
        case net::GameResult::Draw:        resultText_ = "Draw"; break;
        case net::GameResult::Resignation: resultText_ = "Resignation"; break;
        case net::GameResult::Abort:       resultText_ = "Game aborted"; break;
    }

    switch (reason) {
        case net::GameOverReason::Checkmate:         reasonText_ = "Checkmate"; break;
        case net::GameOverReason::Stalemate:         reasonText_ = "Stalemate"; break;
        case net::GameOverReason::FiftyMove:         reasonText_ = "Fifty-move rule"; break;
        case net::GameOverReason::Repetition:        reasonText_ = "Threefold repetition"; break;
        case net::GameOverReason::InsufficientMaterial: reasonText_ = "Insufficient material"; break;
        case net::GameOverReason::Resignation:       reasonText_ = "Resignation"; break;
        case net::GameOverReason::Disconnection:     reasonText_ = "Disconnection"; break;
        case net::GameOverReason::Abort:             reasonText_ = "Aborted"; break;
    }
}

void GameOverScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Enter ||
            kp->code == sf::Keyboard::Key::Escape) {
            app_.switchScreen(std::make_unique<ConnectScreen>(app_));
        }
    }
}

void GameOverScreen::update(float /*dtSec*/) {}

void GameOverScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    sf::Text resultLabel(font, resultText_, 36);
    resultLabel.setFillColor(sf::Color(255, 255, 255));
    auto rb = resultLabel.getGlobalBounds();
    resultLabel.setPosition({(App::WindowWidth - rb.size.x) / 2.f, 220.f});
    window.draw(resultLabel);

    sf::Text reasonLabel(font, "(" + reasonText_ + ")", 22);
    reasonLabel.setFillColor(sf::Color(180, 180, 180));
    auto rr = reasonLabel.getGlobalBounds();
    reasonLabel.setPosition({(App::WindowWidth - rr.size.x) / 2.f, 280.f});
    window.draw(reasonLabel);

    sf::Text hint(font, "Press Enter to return", 18);
    hint.setFillColor(sf::Color(120, 120, 120));
    auto hb = hint.getGlobalBounds();
    hint.setPosition({(App::WindowWidth - hb.size.x) / 2.f, 360.f});
    window.draw(hint);
}

} // namespace chess::client
