#include "game_screen.h"
#include "game_over_screen.h"

#include <chess/movegen.h>
#include <chess/san.h>
#include <chess/net/messages.h>

namespace chess::client {

GameScreen::GameScreen(App& app, Color myColor, const std::string& opponentName)
    : app_(app)
    , board_(Board::fromStartPos())
    , myColor_(myColor)
    , opponentName_(opponentName)
{
}

void GameScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            app_.connection().send(chess::net::ResignMsg{});
            app_.connection().disconnect();
            return;
        }
    }
}

void GameScreen::update(float /*dtSec*/)
{
    app_.connection().poll();

    while (app_.connection().hasMessages()) {
        auto msg = app_.connection().nextMessage();

        if (auto* move = std::get_if<chess::net::ServerMoveMsg>(&msg)) {
            auto parsed = chess::san::fromSan(board_, move->san);
            if (parsed) board_.makeMove(*parsed);
        }
        else if (auto* gameOver = std::get_if<chess::net::GameOverMsg>(&msg)) {
            app_.switchScreen(std::make_unique<GameOverScreen>(
                app_, gameOver->result, gameOver->reason));
            return;
        }
        else if (auto* err = std::get_if<chess::net::ErrorMsg>(&msg)) {
            (void)err;
        }
    }

    if (app_.connection().state() == ConnectionState::Disconnected) {
        app_.switchScreen(std::make_unique<GameOverScreen>(
            app_, net::GameResult::Abort, net::GameOverReason::Disconnection));
    }
}

void GameScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    sf::Text title(font, "Game in progress", 24);
    title.setFillColor(sf::Color(200, 200, 200));
    title.setPosition({50.f, 30.f});
    window.draw(title);

    sf::Text opponent(font, "vs " + opponentName_, 18);
    opponent.setFillColor(sf::Color(160, 160, 160));
    opponent.setPosition({50.f, 70.f});
    window.draw(opponent);

    const char* colorName = myColor_ == Color::White ? "White" : "Black";
    sf::Text colorText(font, std::string("Playing as ") + colorName, 18);
    colorText.setFillColor(myColor_ == Color::White
        ? sf::Color(240, 240, 240) : sf::Color(100, 100, 100));
    colorText.setPosition({50.f, 100.f});
    window.draw(colorText);

    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            bool light = (file + rank) % 2 == 0;
            sf::RectangleShape sq({App::SquareSize, App::SquareSize});
            sq.setPosition({static_cast<float>(file) * App::SquareSize,
                           static_cast<float>(rank) * App::SquareSize});
            sq.setFillColor(light
                ? sf::Color(240, 217, 181)
                : sf::Color(181, 136, 99));
            window.draw(sq);
        }
    }
}

} // namespace chess::client
