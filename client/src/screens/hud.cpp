#include "hud.h"

#include "avatars.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "app.h"

#include <algorithm>
#include <string>

namespace chess::client {

namespace {
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

// 14.3 avatar seeds for the HUD cards. The self card honors the
// profile.avatar re-roll knob; everything else derives from the seat
// name, spectators reading the same both-cards form as the players.
std::string selfAvatarName(const App& app)
{
    if (const auto& profile = app.profile();
        profile.has_value() && !profile->isGuest)
        return profile->name;
    return app.lastName();
}

std::uint64_t selfAvatarSeed(const App& app)
{
    return avatars::seedFor(app.config(), selfAvatarName(app));
}

std::uint64_t nameAvatarSeed(const std::string& name)
{
    return avatars::seedFromName(name);
}
}

Hud::Hud(App& app, const layout::BoardLayout& panelLayout)
    : app_(app)
    , panel_(panelLayout)
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

    navigator_.setLayout(panel_.moveList, panel_.navRow);
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
    return panel_.actionRowY;
}

float Hud::moveListBottom() const
{
    return panel_.moveList.position.y + panel_.moveList.size.y;
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
                       panel_.opponentNameY, panel_.opponentCapturedY, false,
                       opponentClock, 0, nameAvatarSeed(spectatorBlack_),
                       spectatorBlack_);

        std::string whiteText = spectatorWhite_ + ratingSuffix(localRating_)
            + (!gameOver_ && whiteToMove ? " - to move" : "");
        // 14.2: the count moved out of the name string into an eye-icon
        // unit drawn by drawPlayerCard.
        drawPlayerCard(window, font, whiteText, Color::White,
                       panel_.localNameY, panel_.localCapturedY, true,
                       localClock, watcherCount_, nameAvatarSeed(spectatorWhite_),
                       spectatorWhite_);
    } else {
        std::string opponentText = "vs " + opponentName_
            + ratingSuffix(opponentRating_);
        drawPlayerCard(window, font, opponentText, opposite(myColor_),
                       panel_.opponentNameY, panel_.opponentCapturedY, false,
                       opponentClock, 0, nameAvatarSeed(opponentName_),
                       opponentName_);

        std::string colorStr = myColor_ == Color::White ? "White" : "Black";
        std::string localName = "You - " + colorStr + ratingSuffix(localRating_);
        if (!gameOver_)
            localName += myTurn_ ? "  -  Your turn" : "  -  Waiting...";
        localNameLabel_.setColor(myTurn_ ? ui::pal::okTurn
                                         : ui::pal::textFaded);
        drawPlayerCard(window, font, localName, myColor_,
                       panel_.localNameY, panel_.localCapturedY, true,
                       localClock, 0, selfAvatarSeed(app_), selfAvatarName(app_));
    }

    if (!statusMsg_.empty()) {
        statusLabel_.setText(statusMsg_);
        statusLabel_.setPosition({panel_.panelX, panel_.statusY});
        statusLabel_.draw(window, font);
    }

    sf::RectangleShape sep({panel_.panelWidth, 1.f});
    sep.setPosition({panel_.panelX, panel_.separatorY});
    sep.setFillColor(ui::pal::stroke);
    window.draw(sep);

    headerLabel_.setPosition({panel_.panelX, panel_.moveHeaderY});
    headerLabel_.draw(window, font);

    // Opening name rides the "Moves" row, right-aligned (11.5).
    if (!openingLabel_.text().empty()) {
        const float openingW = openingLabel_.size(font).x;
        openingLabel_.setPosition({panel_.panelX + panel_.panelWidth - openingW,
                                   panel_.moveHeaderY});
        openingLabel_.draw(window, font);
    }

    navigator_.draw(window, font, app_);
}

void Hud::drawPlayerCard(sf::RenderWindow& window, const sf::Font& font,
                         const std::string& name, Color cardColor,
                         float nameY, float capturedY, bool isLocal,
                         const std::string& clockText, int watcherCount,
                         std::uint64_t avatarSeed, const std::string& avatarName)
{
    Label& nameLabel = isLocal ? localNameLabel_ : opponentNameLabel_;
    nameLabel.setText(name);

    // 14.3: avatar slot left of the name (pack tile when the pack loads,
    // else the procedural seed emblem); the name shifts to make room.
    const float panelX_ = panel_.panelX;
    const float panelW_ = panel_.panelWidth;
    float nameX = panelX_;
    if (avatarSeed != 0) {
        constexpr float AvatarSize = 18.f;
        const auto nb = nameLabel.bounds(font);
        const float avatarY = nameY + (nb.size.y - AvatarSize) * 0.5f;
        avatars::drawAvatar(window, app_.avatarPack(), avatarSeed,
                            {panelX_, avatarY}, AvatarSize, &font, avatarName);
        nameX = panelX_ + AvatarSize + 6.f;
    }
    nameLabel.setPosition({nameX, nameY});
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