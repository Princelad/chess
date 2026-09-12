#include "hud.h"
#include "app.h"

#include <algorithm>

namespace chess::client {

namespace {
constexpr float OpponentNameY = 20.f;
constexpr float OpponentCapturedY = 42.f;
constexpr float LocalNameY = 66.f;
constexpr float LocalCapturedY = 88.f;
constexpr float StatusY = 112.f;
constexpr float SeparatorY = 134.f;
constexpr float MoveHeaderY = 144.f;
constexpr float MoveListTop = 162.f;
constexpr float MoveListBottom = 398.f;
constexpr float NavBtnY = 402.f;
constexpr float NavBtnH = 26.f;
constexpr float StatusFontSize = 14;
constexpr float NameFontSize = 16;
constexpr float HeaderFontSize = 12;
constexpr float CapturedPieceSize = 15.f;
constexpr float CapturedStep = 17.f;
constexpr int MaxCapturedShown = 16;
}

Hud::Hud(App& app, float panelX, float panelWidth)
    : app_(app)
    , panelX_(panelX)
    , panelW_(panelWidth)
{
    opponentNameLabel_.setFontSize(NameFontSize);
    opponentNameLabel_.setColor(sf::Color(180, 180, 180));

    localNameLabel_.setFontSize(NameFontSize);
    localNameLabel_.setColor(sf::Color(180, 180, 180));

    statusLabel_.setFontSize(StatusFontSize);
    statusLabel_.setColor(sf::Color(255, 200, 60));

    headerLabel_.setText("Moves");
    headerLabel_.setFontSize(HeaderFontSize);
    headerLabel_.setColor(sf::Color(120, 120, 120));

    navigator_.setLayout(
        sf::FloatRect(sf::Vector2f(panelX_, MoveListTop),
                      sf::Vector2f(panelW_, MoveListBottom - MoveListTop)),
        sf::FloatRect(sf::Vector2f(panelX_, NavBtnY),
                      sf::Vector2f(panelW_, NavBtnH)));
}

void Hud::setGame(const Board& initialBoard, std::vector<chess::Move> moves,
                  std::vector<std::string> sans)
{
    navigator_.setGame(initialBoard, std::move(moves), std::move(sans));
}

void Hud::appendMove(const chess::Move& move, const std::string& san)
{
    navigator_.appendMove(move, san);
}

void Hud::setInfo(const std::string& opponentName, Color myColor,
                  bool myTurn, bool gameOver)
{
    opponentName_ = opponentName;
    myColor_ = myColor;
    myTurn_ = myTurn;
    gameOver_ = gameOver;
}

void Hud::setStatus(const std::string& msg, float duration)
{
    statusMsg_ = msg;
    statusTimer_ = duration;
}

void Hud::update(float dtSec)
{
    if (statusTimer_ > 0.f) {
        statusTimer_ -= dtSec;
        if (statusTimer_ <= 0.f) statusMsg_.clear();
    }
}

float Hud::contentBottom() const
{
    return NavBtnY + NavBtnH + 4.f;
}

float Hud::moveListBottom() const
{
    return MoveListBottom;
}

void Hud::draw(sf::RenderWindow& window, const sf::Font& font)
{
    drawPlayerCard(window, font, "vs " + opponentName_, opposite(myColor_),
                   OpponentNameY, OpponentCapturedY, false);

    std::string colorStr = myColor_ == Color::White ? "White" : "Black";
    std::string localName = "You - " + colorStr;
    if (!gameOver_)
        localName += myTurn_ ? "  -  Your turn" : "  -  Waiting...";
    localNameLabel_.setColor(myTurn_ ? sf::Color(76, 175, 80)
                                     : sf::Color(180, 180, 180));
    drawPlayerCard(window, font, localName, myColor_,
                   LocalNameY, LocalCapturedY, true);

    if (!statusMsg_.empty()) {
        statusLabel_.setText(statusMsg_);
        statusLabel_.setPosition({panelX_, StatusY});
        statusLabel_.draw(window, font);
    }

    sf::RectangleShape sep({panelW_, 1.f});
    sep.setPosition({panelX_, SeparatorY});
    sep.setFillColor(sf::Color(80, 80, 80));
    window.draw(sep);

    headerLabel_.setPosition({panelX_, MoveHeaderY});
    headerLabel_.draw(window, font);

    navigator_.draw(window, font, app_);
}

void Hud::drawPlayerCard(sf::RenderWindow& window, const sf::Font& font,
                         const std::string& name, Color cardColor,
                         float nameY, float capturedY, bool isLocal)
{
    Label& nameLabel = isLocal ? localNameLabel_ : opponentNameLabel_;
    nameLabel.setText(name);
    nameLabel.setPosition({panelX_, nameY});
    nameLabel.draw(window, font);

    int advantage = navigator_.materialAdvantage(cardColor);
    if (advantage > 0) {
        sf::Text diff(font, "+" + std::to_string(advantage), 14);
        diff.setFillColor(sf::Color(140, 200, 140));
        float x = panelX_ + panelW_ - diff.getLocalBounds().size.x;
        diff.setPosition({x, nameY});
        window.draw(diff);
    }

    const auto& captured = navigator_.capturedBy(cardColor).pieces;
    float x = panelX_;
    int shown = std::min(MaxCapturedShown, static_cast<int>(captured.size()));

    for (int i = 0; i < shown; ++i) {
        const Piece piece = captured[i];

        if (app_.piecesLoaded()) {
            const auto& tex = app_.pieceTexture(piece.color, piece.type);
            sf::Sprite sprite(tex);
            float scale = CapturedPieceSize / static_cast<float>(tex.getSize().x);
            sprite.setScale({scale, scale});
            sprite.setPosition({x, capturedY});
            window.draw(sprite);
        } else {
            const char letters[] = { 'P', 'N', 'B', 'R', 'Q', 'K' };
            sf::Text letter(font, std::string(1, letters[static_cast<int>(piece.type)]), 13);
            letter.setFillColor(piece.color == Color::White
                ? sf::Color(220, 220, 220) : sf::Color(120, 120, 120));
            letter.setPosition({x, capturedY});
            window.draw(letter);
        }

        x += CapturedStep;
    }
}

} // namespace chess::client