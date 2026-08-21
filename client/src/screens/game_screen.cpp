#include "game_screen.h"
#include "game_over_screen.h"

#include <chess/movegen.h>
#include <chess/san.h>
#include <chess/net/messages.h>

namespace chess::client {

namespace {
std::pair<int, int> findKingSquare(const Board& board, Color color)
{
    Piece king = Piece::of(color, PieceType::King);
    for (int file = 0; file < 8; ++file)
        for (int rank = 0; rank < 8; ++rank)
            if (board.pieceAt(squareOf(file, rank)) == king)
                return { file, rank };
    return { 4, color == Color::White ? 0 : 7 };
}
}

GameScreen::GameScreen(App& app, Color myColor, const std::string& opponentName)
    : app_(app)
    , board_(Board::fromStartPos())
    , myColor_(myColor)
    , opponentName_(opponentName)
    , boardView_(static_cast<float>(App::WindowWidth),
                 static_cast<float>(App::WindowHeight),
                 myColor)
{
    inCheck_ = chess::inCheck(board_, myColor_);
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
            if (parsed) {
                hl_.lastMoveFrom = std::make_pair(
                    static_cast<int>(chess::fileOf(parsed->from)),
                    static_cast<int>(chess::rankOf(parsed->from)));
                hl_.lastMoveTo = std::make_pair(
                    static_cast<int>(chess::fileOf(parsed->to)),
                    static_cast<int>(chess::rankOf(parsed->to)));
                hl_.selectedSquare.reset();
                hl_.legalMoveTargets.clear();

                board_.makeMove(*parsed);
                inCheck_ = chess::inCheck(board_, myColor_);
                hl_.checkSquare = inCheck_
                    ? findKingSquare(board_, myColor_)
                    : std::optional<std::pair<int,int>>{};
            }
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

    boardView_.drawSquares(window);
    boardView_.drawHighlights(window, hl_, board_);
    boardView_.drawLabels(window, font);
    boardView_.drawPieces(window, font, board_, app_);

    float px = boardView_.panelX();

    sf::Text title(font, "Game in progress", 24);
    title.setFillColor(sf::Color(200, 200, 200));
    title.setPosition({px, 30.f});
    window.draw(title);

    sf::Text opponent(font, "vs " + opponentName_, 18);
    opponent.setFillColor(sf::Color(160, 160, 160));
    opponent.setPosition({px, 70.f});
    window.draw(opponent);

    const char* colorName = myColor_ == Color::White ? "White" : "Black";
    sf::Text colorText(font, std::string("Playing as ") + colorName, 18);
    colorText.setFillColor(myColor_ == Color::White
        ? sf::Color(240, 240, 240) : sf::Color(100, 100, 100));
    colorText.setPosition({px, 100.f});
    window.draw(colorText);
}

} // namespace chess::client
