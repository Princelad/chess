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
constexpr float HeaderFontSize = 12;

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
    statusLabel_.setFontSize(StatusFontSize);
    statusLabel_.setColor(ui::pal::warn);

    headerLabel_.setText("Moves");
    headerLabel_.setFontSize(HeaderFontSize);
    headerLabel_.setColor(ui::pal::textFaded);

    openingLabel_.setFontSize(HeaderFontSize);
    openingLabel_.setColor(sf::Color(150, 140, 110)); // ECO bronze (HUD-only)

    navigator_.setLayout(panel_.moveList, panel_.navRow);

    // 15.2: the card widget owns each seat band composed by the composer.
    opponentCard_.setRect(panel_.opponentCard);
    localCard_.setRect(panel_.localCard);
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
    // The dots pulse from the same clock while they show.
    opponentCard_.update(dtSec);
    localCard_.update(dtSec);
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
    if (spectatorMode_) {
        // Both cards speak in seat names, there is no "You". The view is
        // White-oriented (the screen forces myColor_ to White), so the
        // top band is the black seat, the bottom the white one (12.4's
        // rating mapping unchanged). The to-move annotation is the
        // pulsing dot now instead of a name suffix.
        const bool whiteToMove =
            navigator_.finalBoard().sideToMove() == Color::White;
        const bool live = !gameOver_;

        PlayerCardState black;
        black.nameLine = spectatorBlack_;
        black.rating = opponentRating_;
        black.nameColor = ui::pal::textFaded;
        black.avatarSeed = nameAvatarSeed(spectatorBlack_);
        black.avatarName = spectatorBlack_;
        black.remainingMs = clocksOn_
            ? std::optional<double>(shownRemainingMs(Color::Black))
            : std::nullopt;
        black.captured = &navigator_.capturedBy(Color::Black).pieces;
        black.materialDiff = navigator_.materialAdvantage(Color::Black);
        black.myTurn = live && !whiteToMove;
        opponentCard_.draw(window, font, app_, black);

        PlayerCardState white;
        white.nameLine = spectatorWhite_;
        white.rating = localRating_;
        white.nameColor = ui::pal::textFaded;
        white.avatarSeed = nameAvatarSeed(spectatorWhite_);
        white.avatarName = spectatorWhite_;
        white.remainingMs = clocksOn_
            ? std::optional<double>(shownRemainingMs(Color::White))
            : std::nullopt;
        white.captured = &navigator_.capturedBy(Color::White).pieces;
        white.materialDiff = navigator_.materialAdvantage(Color::White);
        white.myTurn = live && whiteToMove;
        white.watcherCount = watcherCount_;
        localCard_.draw(window, font, app_, white);
    } else {
        PlayerCardState opponent;
        opponent.nameLine = "vs " + opponentName_;
        opponent.rating = opponentRating_;
        opponent.nameColor = ui::pal::textFaded;
        opponent.avatarSeed = nameAvatarSeed(opponentName_);
        opponent.avatarName = opponentName_;
        opponent.remainingMs = clocksOn_
            ? std::optional<double>(shownRemainingMs(opposite(myColor_)))
            : std::nullopt;
        opponent.captured = &navigator_.capturedBy(opposite(myColor_)).pieces;
        opponent.materialDiff = navigator_.materialAdvantage(opposite(myColor_));
        opponent.thinking = opponentThinking_ && !gameOver_;
        opponent.connection = opponentConnection_;
        opponentCard_.draw(window, font, app_, opponent);

        PlayerCardState local;
        local.nameLine = "You - "
            + std::string(myColor_ == Color::White ? "White" : "Black");
        local.rating = localRating_;
        local.nameColor = myTurn_ && !gameOver_ ? ui::pal::okTurn
                                                : ui::pal::textFaded;
        local.avatarSeed = selfAvatarSeed(app_);
        local.avatarName = selfAvatarName(app_);
        local.remainingMs = clocksOn_
            ? std::optional<double>(shownRemainingMs(myColor_))
            : std::nullopt;
        local.captured = &navigator_.capturedBy(myColor_).pieces;
        local.materialDiff = navigator_.materialAdvantage(myColor_);
        local.myTurn = myTurn_ && !gameOver_;
        local.premovePending = premovePending_;
        localCard_.draw(window, font, app_, local);
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

} // namespace chess::client