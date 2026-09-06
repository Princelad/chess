#include "hud.h"
#include "ui_helpers.h"

#include <algorithm>

namespace chess::client {

namespace {
constexpr float InfoY = 30.f;
constexpr float InfoLineH = 25.f;
constexpr float SeparatorY = 100.f;
constexpr float MoveHeaderY = 110.f;
constexpr float MoveListTop = 130.f;
constexpr float MoveListBottom = 400.f;
constexpr float MoveLineH = 16.f;
constexpr float MoveFontSize = 13;
constexpr float StatusFontSize = 14;
constexpr float InfoFontSize = 18;
constexpr float HeaderFontSize = 12;

int visibleLines(float listHeight)
{
    return std::max(1, static_cast<int>(listHeight / MoveLineH));
}
}

Hud::Hud(float panelX, float panelWidth)
    : panelX_(panelX)
    , panelW_(panelWidth)
{
    opponentLabel_.setFontSize(InfoFontSize);
    opponentLabel_.setColor(sf::Color(180, 180, 180));

    infoLabel_.setFontSize(StatusFontSize);
    infoLabel_.setColor(sf::Color(160, 160, 160));

    statusLabel_.setFontSize(StatusFontSize);
    statusLabel_.setColor(sf::Color(255, 200, 60));

    headerLabel_.setText("Moves");
    headerLabel_.setFontSize(HeaderFontSize);
    headerLabel_.setColor(sf::Color(120, 120, 120));

    listBg_.setRect(sf::FloatRect(
        sf::Vector2f(panelX_, MoveListTop),
        sf::Vector2f(panelW_, moveListBottom() - MoveListTop)));
    listBg_.setFill(sf::Color(25, 25, 25));
    listBg_.setOutline(sf::Color(80, 80, 80));
}

void Hud::setInfo(const std::string& opponentName, Color myColor,
                   bool myTurn, bool gameOver)
{
    opponentName_ = opponentName;
    myColor_ = myColor;
    myTurn_ = myTurn;
    gameOver_ = gameOver;
}

void Hud::addMove(const std::string& san)
{
    int moveCount = static_cast<int>(movePairs_.size());
    bool isWhite = (moveCount == 0) || !movePairs_.back().second.empty();

    if (isWhite) {
        std::string numStr = std::to_string(moveCount + 1) + ".";
        movePairs_.emplace_back(numStr, san);
    } else {
        movePairs_.back().second = san;
    }

    float listH = moveListBottom() - MoveListTop;
    int vis = visibleLines(listH);
    moveScroll_ = std::max(0, static_cast<int>(movePairs_.size()) - vis);
}

void Hud::setStatus(const std::string& msg, float duration)
{
    statusMsg_ = msg;
    statusTimer_ = duration;
}

void Hud::setGameOver(bool gameOver)
{
    gameOver_ = gameOver;
}

void Hud::handleScroll(float delta)
{
    float listH = moveListBottom() - MoveListTop;
    int vis = visibleLines(listH);
    int maxScroll = std::max(0, static_cast<int>(movePairs_.size()) - vis);
    moveScroll_ += static_cast<int>(delta);
    moveScroll_ = std::clamp(moveScroll_, 0, maxScroll);
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
    return MoveListBottom + 20.f;
}

float Hud::moveListBottom() const
{
    return MoveListBottom;
}

void Hud::draw(sf::RenderWindow& window, const sf::Font& font)
{
    opponentLabel_.setText("vs " + opponentName_);
    opponentLabel_.setPosition({panelX_, InfoY});
    opponentLabel_.draw(window, font);

    const char* colorName = myColor_ == Color::White ? "White" : "Black";
    const char* turnStr = gameOver_ ? "Game over"
        : (myTurn_ ? "Your turn" : "Waiting...");
    infoLabel_.setText(std::string(colorName) + "  •  " + turnStr);
    infoLabel_.setColor(myTurn_ ? sf::Color(76, 175, 80) : sf::Color(160, 160, 160));
    infoLabel_.setPosition({panelX_, InfoY + InfoLineH});
    infoLabel_.draw(window, font);

    if (!statusMsg_.empty()) {
        statusLabel_.setText(statusMsg_);
        statusLabel_.setPosition({panelX_, InfoY + InfoLineH * 2.f});
        statusLabel_.draw(window, font);
    }

    sf::RectangleShape sep({panelW_, 1.f});
    sep.setPosition({panelX_, SeparatorY});
    sep.setFillColor(sf::Color(80, 80, 80));
    window.draw(sep);

    headerLabel_.setPosition({panelX_, MoveHeaderY});
    headerLabel_.draw(window, font);

    float listH = moveListBottom() - MoveListTop;
    if (listH <= 0.f) return;

    listBg_.draw(window);

    int vis = visibleLines(listH);
    int totalPairs = static_cast<int>(movePairs_.size());
    int startPair = moveScroll_;
    int endPair = std::min(startPair + vis, totalPairs);

    float y = MoveListTop + 2.f;
    for (int i = startPair; i < endPair; ++i) {
        if (y + MoveLineH > MoveListTop + listH) break;

        const auto& [num, san] = movePairs_[i];
        std::string line = num + " " + san;

        sf::Text moveText(font, line, MoveFontSize);
        moveText.setFillColor(sf::Color(200, 200, 200));
        moveText.setPosition({panelX_ + 6.f, y});

        auto lb = moveText.getLocalBounds();
        if (panelW_ > 24.f && lb.size.x > panelW_ - 12.f) {
            line = safeTruncate(line, static_cast<std::size_t>((panelW_ - 24.f) / 7.f));
            moveText.setString(line);
        }
        window.draw(moveText);
        y += MoveLineH;
    }

    if (totalPairs == 0) {
        Label empty("No moves yet", MoveFontSize, sf::Color(100, 100, 100));
        empty.setPosition({panelX_ + 6.f, MoveListTop + 4.f});
        empty.draw(window, font);
    }
}

} // namespace chess::client