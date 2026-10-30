#include "hud.h"

#include "ui/icons.h"
#include "ui/theme.h"
#include "app.h"

#include <algorithm>
#include <string>

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

// "1650 ± 132" suffix for a player card name line; omitted when unknown
// (e.g. guests or unrated games).
std::string ratingSuffix(const std::optional<std::pair<double, double>>& rating)
{
    if (!rating.has_value()) return "";
    const auto value = static_cast<long long>(rating->first + 0.5);
    const auto deviation = static_cast<long long>(rating->second + 0.5);
    return "  (" + std::to_string(value) + " \u00b1 " + std::to_string(deviation) + ")";
}

// Clock display: "m:ss" normally, tenths under a minute ("42.3"), the
// usual online-chess shape.
std::string clockText(double ms)
{
    const double s = std::max(0.0, ms) / 1000.0;
    if (s < 60.0) {
        const int tenths = static_cast<int>(s * 10.0 + 0.5);
        if (tenths >= 600) // rounding pushed us back to the minute
            return "1:00";
        return std::to_string(tenths / 10) + "."
               + std::to_string(tenths % 10);
    }
    const int total = static_cast<int>(s + 0.5);
    return std::to_string(total / 60) + ":"
           + (total % 60 < 10 ? "0" : "") + std::to_string(total % 60);
}

// Low time heats up: red inside the last 10 s, amber until 30 s.
sf::Color clockColor(double ms)
{
    if (ms < 10000.0) return ui::pal::dangerRed;
    if (ms < 30000.0) return ui::pal::dangerAmber;
    return ui::pal::textCell;
}
}

Hud::Hud(App& app, float panelX, float panelWidth)
    : app_(app)
    , panelX_(panelX)
    , panelW_(panelWidth)
{
    opponentNameLabel_.setFontSize(NameFontSize);
    opponentNameLabel_.setColor(ui::pal::textFaded);

    localNameLabel_.setFontSize(NameFontSize);
    localNameLabel_.setColor(ui::pal::textFaded);

    statusLabel_.setFontSize(StatusFontSize);
    statusLabel_.setColor(ui::pal::warn);

    headerLabel_.setText("Moves");
    headerLabel_.setFontSize(HeaderFontSize);
    headerLabel_.setColor(ui::pal::textFaded);

    openingLabel_.setFontSize(HeaderFontSize);
    openingLabel_.setColor(sf::Color(150, 140, 110)); // ECO bronze (HUD-only)

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
    refreshOpening();
}

void Hud::appendMove(const chess::Move& move, const std::string& san)
{
    navigator_.appendMove(move, san);
    refreshOpening();
}

void Hud::setInfo(const std::string& opponentName, Color myColor,
                  bool myTurn, bool gameOver)
{
    opponentName_ = opponentName;
    myColor_ = myColor;
    myTurn_ = myTurn;
    gameOver_ = gameOver;
}

void Hud::setRatings(std::optional<std::pair<double, double>> local,
                     std::optional<std::pair<double, double>> opponent)
{
    localRating_ = std::move(local);
    opponentRating_ = std::move(opponent);
}

void Hud::syncClocks(Color sideToMove, std::int64_t sideRemainingMs)
{
    clocksOn_ = sideRemainingMs > 0;
    if (!clocksOn_)
        return;
    remainingMs_[sideToMove == Color::White ? 0 : 1] =
        static_cast<double>(sideRemainingMs);
    clockTurn_ = sideToMove;
    sinceSyncSec_ = 0.f;
}

void Hud::syncClocks(std::int64_t whiteRemainingMs, std::int64_t blackRemainingMs,
                     Color sideToMove)
{
    clocksOn_ = whiteRemainingMs > 0 || blackRemainingMs > 0;
    if (!clocksOn_)
        return;
    remainingMs_[0] = static_cast<double>(whiteRemainingMs);
    remainingMs_[1] = static_cast<double>(blackRemainingMs);
    clockTurn_ = sideToMove;
    sinceSyncSec_ = 0.f;
}

void Hud::update(float dtSec)
{
    if (statusTimer_ > 0.f) {
        statusTimer_ -= dtSec;
        if (statusTimer_ <= 0.f) statusMsg_.clear();
    }
    if (clocksOn_ && !gameOver_)
        sinceSyncSec_ += dtSec;
}

double Hud::shownRemainingMs(Color side) const
{
    double ms = remainingMs_[side == Color::White ? 0 : 1];
    if (!clocksOn_)
        return ms;
    if (side == clockTurn_)
        ms -= sinceSyncSec_ * 1000.0;
    return std::max(0.0, ms);
}

void Hud::setStatus(const std::string& msg, float duration)
{
    statusMsg_ = msg;
    statusTimer_ = duration;
}

void Hud::refreshOpening()
{
    opening_ = ecoLookup(navigator_.sans());
    openingLabel_.setText(opening_.has_value()
        ? opening_->code + " - " + opening_->name
        : std::string());
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
    std::string opponentClock, localClock;
    if (clocksOn_) {
        opponentClock = clockText(shownRemainingMs(opposite(myColor_)));
        localClock = clockText(shownRemainingMs(myColor_));
    }

    if (spectatorMode_) {
        // Both cards speak in seat names, there is no "You". The view is
        // White-oriented (the screen forces myColor_ to White), so the
        // to-move annotation rides the card the navigator shows.
        const bool whiteToMove =
            navigator_.finalBoard().sideToMove() == Color::White;
        std::string blackText = spectatorBlack_ + ratingSuffix(opponentRating_)
            + (!gameOver_ && !whiteToMove ? " - to move" : "");
        drawPlayerCard(window, font, blackText, Color::Black,
                       OpponentNameY, OpponentCapturedY, false, opponentClock);

        std::string whiteText = spectatorWhite_ + ratingSuffix(localRating_)
            + (!gameOver_ && whiteToMove ? " - to move" : "");
        // 14.2: the count moved out of the name string into an eye-icon
        // unit drawn by drawPlayerCard.
        drawPlayerCard(window, font, whiteText, Color::White,
                       LocalNameY, LocalCapturedY, true, localClock,
                       watcherCount_);
    } else {
        std::string opponentText = "vs " + opponentName_
            + ratingSuffix(opponentRating_);
        drawPlayerCard(window, font, opponentText, opposite(myColor_),
                       OpponentNameY, OpponentCapturedY, false, opponentClock);

        std::string colorStr = myColor_ == Color::White ? "White" : "Black";
        std::string localName = "You - " + colorStr + ratingSuffix(localRating_);
        if (!gameOver_)
            localName += myTurn_ ? "  -  Your turn" : "  -  Waiting...";
        localNameLabel_.setColor(myTurn_ ? ui::pal::okTurn
                                         : ui::pal::textFaded);
        drawPlayerCard(window, font, localName, myColor_,
                       LocalNameY, LocalCapturedY, true, localClock);
    }

    if (!statusMsg_.empty()) {
        statusLabel_.setText(statusMsg_);
        statusLabel_.setPosition({panelX_, StatusY});
        statusLabel_.draw(window, font);
    }

    sf::RectangleShape sep({panelW_, 1.f});
    sep.setPosition({panelX_, SeparatorY});
    sep.setFillColor(ui::pal::stroke);
    window.draw(sep);

    headerLabel_.setPosition({panelX_, MoveHeaderY});
    headerLabel_.draw(window, font);

    // Opening name rides the "Moves" row, right-aligned (11.5).
    if (!openingLabel_.text().empty()) {
        const float openingW = openingLabel_.size(font).x;
        openingLabel_.setPosition({panelX_ + panelW_ - openingW, MoveHeaderY});
        openingLabel_.draw(window, font);
    }

    navigator_.draw(window, font, app_);
}

void Hud::drawPlayerCard(sf::RenderWindow& window, const sf::Font& font,
                         const std::string& name, Color cardColor,
                         float nameY, float capturedY, bool isLocal,
                         const std::string& clockText, int watcherCount)
{
    Label& nameLabel = isLocal ? localNameLabel_ : opponentNameLabel_;
    nameLabel.setText(name);
    nameLabel.setPosition({panelX_, nameY});
    nameLabel.draw(window, font);

    float right = panelX_ + panelW_;

    if (!clockText.empty()) {
        sf::Text clock(font, clockText, 14);
        clock.setFillColor(clockColor(shownRemainingMs(cardColor)));
        const float clockW = clock.getLocalBounds().size.x;
        clock.setPosition({right - clockW, nameY});
        window.draw(clock);

        // 14.2: small clock glyph beside the figure, vertically centered
        // on the text's glyph box.
        const float iconS = ui::icons::iconBox(14);
        const auto cb = clock.getLocalBounds();
        const float clockCy = nameY + cb.position.y + cb.size.y * 0.5f;
        ui::icons::drawIcon(window, ui::icons::IconKind::Clock,
                            {right - clockW - iconS - 4.f, clockCy - iconS * 0.5f},
                            iconS, clock.getFillColor());
        right -= clockW + iconS + 14.f;
    }

    if (watcherCount > 0) {
        // Spectator stands (12.4): eye + count on the white card's name
        // row, between the name text and the clock figure.
        sf::Text count(font, std::to_string(watcherCount), 14);
        count.setFillColor(ui::pal::textFaded);
        const float countW = count.getLocalBounds().size.x;
        count.setPosition({right - countW, nameY});
        window.draw(count);

        const float iconS = ui::icons::iconBox(14);
        const auto cb = count.getLocalBounds();
        const float countCy = nameY + cb.position.y + cb.size.y * 0.5f;
        ui::icons::drawIcon(window, ui::icons::IconKind::Eye,
                            {right - countW - iconS - 4.f, countCy - iconS * 0.5f},
                            iconS, ui::pal::textFaded);
        right -= countW + iconS + 14.f;
    }

    int advantage = navigator_.materialAdvantage(cardColor);
    if (advantage > 0) {
        sf::Text diff(font, "+" + std::to_string(advantage), 14);
        diff.setFillColor(ui::pal::okDim);
        float x = right - diff.getLocalBounds().size.x;
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
                ? ui::pal::textBody : ui::pal::textFaded);
            letter.setPosition({x, capturedY});
            window.draw(letter);
        }

        x += CapturedStep;
    }
}

} // namespace chess::client