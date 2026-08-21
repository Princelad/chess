#include "hud.h"

#include <algorithm>
#include <sstream>

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

std::string safeTruncate(const std::string& s, std::size_t maxBytes)
{
    if (s.size() <= maxBytes) return s;
    std::size_t n = maxBytes;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
    return s.substr(0, n > 0 ? n - 1 : 0) + "...";
}

int visibleLines(float listHeight)
{
    return std::max(1, static_cast<int>(listHeight / MoveLineH));
}
}

Hud::Hud(float panelX, float panelWidth, float windowHeight)
    : panelX_(panelX)
    , panelW_(panelWidth)
    , windowHeight_(windowHeight)
{
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

void Hud::draw(sf::RenderWindow& window, const sf::Font& font) const
{
    sf::Text opponentText(font, "vs " + opponentName_, InfoFontSize);
    opponentText.setFillColor(sf::Color(180, 180, 180));
    opponentText.setPosition({panelX_, InfoY});
    window.draw(opponentText);

    const char* colorName = myColor_ == Color::White ? "White" : "Black";
    const char* turnStr = gameOver_ ? "Game over"
        : (myTurn_ ? "Your turn" : "Waiting...");
    std::string infoLine = std::string(colorName) + "  •  " + turnStr;
    sf::Text infoText(font, infoLine, StatusFontSize);
    infoText.setFillColor(myTurn_ ? sf::Color(76, 175, 80) : sf::Color(160, 160, 160));
    infoText.setPosition({panelX_, InfoY + InfoLineH});
    window.draw(infoText);

    if (!statusMsg_.empty()) {
        sf::Text statusText(font, statusMsg_, StatusFontSize);
        statusText.setFillColor(sf::Color(255, 200, 60));
        statusText.setPosition({panelX_, InfoY + InfoLineH * 2.f});
        window.draw(statusText);
    }

    sf::RectangleShape sep({panelW_, 1.f});
    sep.setPosition({panelX_, SeparatorY});
    sep.setFillColor(sf::Color(80, 80, 80));
    window.draw(sep);

    sf::Text header(font, "Moves", HeaderFontSize);
    header.setFillColor(sf::Color(120, 120, 120));
    header.setPosition({panelX_, MoveHeaderY});
    window.draw(header);

    float listH = moveListBottom() - MoveListTop;
    if (listH <= 0.f) return;

    sf::RectangleShape listBg({panelW_, listH});
    listBg.setPosition({panelX_, MoveListTop});
    listBg.setFillColor(sf::Color(25, 25, 25));
    window.draw(listBg);

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

        auto lb = moveText.getGlobalBounds();
        if (panelW_ > 24.f && lb.size.x > panelW_ - 12.f) {
            line = safeTruncate(line, static_cast<std::size_t>((panelW_ - 24.f) / 7.f));
            moveText.setString(line);
        }
        window.draw(moveText);
        y += MoveLineH;
    }

    if (totalPairs == 0) {
        sf::Text empty(font, "No moves yet", MoveFontSize);
        empty.setFillColor(sf::Color(100, 100, 100));
        empty.setPosition({panelX_ + 6.f, MoveListTop + 4.f});
        window.draw(empty);
    }
}

} // namespace chess::client
