#include "widgets/player_card.h"

#include "avatars.h"
#include "ui/icons.h"
#include "ui/rounded_rect.h"
#include "ui/spinner.h"
#include "ui/theme.h"
#include "app.h"

#include <algorithm>
#include <cmath>

namespace chess::client {

// Clock display: "m:ss" normally, tenths under a minute ("42.3"), the
// usual online-chess shape.
std::string formatClockText(double ms)
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
sf::Color clockHeatColor(double ms)
{
    if (ms < 10000.0) return ui::pal::dangerRed;
    if (ms < 30000.0) return ui::pal::dangerAmber;
    return ui::pal::textCell;
}

// "1650 ± 132" suffix for a player card name line; omitted when unknown.
std::string cardRatingSuffix(
    const std::optional<std::pair<double, double>>& rating)
{
    if (!rating.has_value()) return "";
    const auto value = static_cast<long long>(rating->first + 0.5);
    const auto deviation = static_cast<long long>(rating->second + 0.5);
    return "  (" + std::to_string(value) + " \u00b1 " + std::to_string(deviation) + ")";
}

namespace {
constexpr unsigned NameTextSize = 16;
constexpr unsigned BadgeTextSize = 12;
constexpr float AvatarSize = 18.f;
constexpr float CardInset = 7.f;   // content inset from the band rect
constexpr float NameGap = 6.f;     // avatar -> name
constexpr float BandRadius = 8.f;

// A progressively stronger pulse so a thinking dot reads alive without
// any timer wiring on the caller side.
float thinkingAlpha(float pulseSec)
{
    const float phase = std::fmod(pulseSec * 2.5f, 2.f);
    return 0.55f + 0.45f * (0.5f - 0.5f * std::cos(phase * 3.14159265f));
}
} // namespace

void PlayerCard::setRect(sf::FloatRect band)
{
    band_ = band;
}

sf::FloatRect PlayerCard::nameSlot() const
{
    return sf::FloatRect(
        sf::Vector2f(band_.position.x, band_.position.y + 6.f),
        sf::Vector2f(band_.size.x, 15.f));
}

sf::FloatRect PlayerCard::capturedSlot() const
{
    return sf::FloatRect(
        sf::Vector2f(band_.position.x + CardInset, band_.position.y + 28.f),
        sf::Vector2f(band_.size.x - 2.f * CardInset, 15.f));
}

void PlayerCard::update(float dtSec)
{
    pulseSec_ += dtSec;
}

void PlayerCard::draw(sf::RenderWindow& window, const sf::Font& font,
                      const App& app, const PlayerCardState& state)
{
    // Card backdrop (15.2): the band got a real panel behind it.
    ui::RoundedRectShape bg(band_.size, BandRadius);
    bg.setPosition(band_.position);
    bg.setFillColor(ui::pal::bgCard);
    bg.setOutlineColor(ui::pal::strokeFaint);
    bg.setOutlineThickness(1.f);
    window.draw(bg);

    const float contentLeft = band_.position.x + CardInset;
    const float contentRight = band_.position.x + band_.size.x - CardInset;

    // Name row: avatar slot, then the name line (with the card's own
    // rating suffix appended here so the measurement stays exact).
    const std::string nameText = state.nameLine + cardRatingSuffix(state.rating);
    sf::Text name(font, nameText, NameTextSize);
    name.setFillColor(state.nameColor);
    const float nameY = nameSlot().position.y;
    const auto nb = name.getLocalBounds();
    const float nameCy = nameY + nb.position.y + nb.size.y * 0.5f;

    float nameX = contentLeft;
    if (state.avatarSeed != 0 && !state.avatarName.empty()) {
        const float avatarY = nameY + (nb.size.y - AvatarSize) * 0.5f
            + nb.position.y;
        avatars::drawAvatar(window, app.avatarPack(), state.avatarSeed,
                            {contentLeft, avatarY}, AvatarSize, &font,
                            state.avatarName);
        nameX = contentLeft + AvatarSize + NameGap;
    }
    name.setPosition({nameX, nameY});
    window.draw(name);

    // Right-side row, built from the outer edge inward like the HUD's
    // original composition: clock pill, watcher count, material diff.
    float right = contentRight;

    if (state.remainingMs.has_value() && *state.remainingMs > 0.0) {
        const std::string text = formatClockText(*state.remainingMs);
        const sf::Color heat = clockHeatColor(*state.remainingMs);

        sf::Text figure(font, text, 14);
        figure.setFillColor(heat);
        const float figW = figure.getLocalBounds().size.x;

        const float iconS = ui::icons::iconBox(14);
        // Pill wraps [icon .. figure] with breathing room.
        constexpr float PillPadX = 6.f;
        constexpr float PillIconGap = 4.f;
        const float pillW = PillPadX + iconS + PillIconGap + figW + PillPadX;
        constexpr float PillH = 22.f;
        sf::FloatRect pill({right - pillW, nameCy - PillH * 0.5f},
                           {pillW, PillH});
        ui::RoundedRectShape pillBg(pill.size, PillH * 0.5f);
        pillBg.setPosition(pill.position);
        pillBg.setFillColor(ui::pal::bgDeep);
        pillBg.setOutlineColor(ui::pal::strokeFaint);
        pillBg.setOutlineThickness(1.f);
        window.draw(pillBg);

        const float figY = nameY + nb.position.y + (nb.size.y - 17.f) * 0.5f;
        figure.setPosition({pill.position.x + pill.size.x - PillPadX - figW, figY});
        window.draw(figure);

        ui::icons::drawIcon(window, ui::icons::IconKind::Clock,
                            {pill.position.x + PillPadX,
                             nameCy - iconS * 0.5f},
                            iconS, heat);
        right = pill.position.x - 10.f;
    }

    if (state.watcherCount > 0) {
        // Spectator stands (12.4): eye + count on the white card's name
        // row, between the name text and the clock figure.
        sf::Text count(font, std::to_string(state.watcherCount), 14);
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

    if (state.materialDiff > 0) {
        sf::Text diff(font, "+" + std::to_string(state.materialDiff), 14);
        diff.setFillColor(ui::pal::okDim);
        diff.setPosition({right - diff.getLocalBounds().size.x, nameY});
        window.draw(diff);
        right -= diff.getLocalBounds().size.x + 8.f;
    }

    // State dots / link icon ride the name row between the name text and
    // the right-side stack.
    float dotX = nameX + nb.size.x + 8.f;

    if (state.thinking) {
        // 15.5: the waiting state spins — 8 ticks sweep the wheel while
        // the engine/search or the server's reply is in flight.
        const float radius = 6.f;
        ui::drawSpinner(window, { dotX + radius, nameCy },
                        radius, pulseSec_ / ui::spinner::SweepSec,
                        ui::pal::okTurn);
        dotX += 2.f * radius + 5.f;
    } else if (state.myTurn) {
        const float r = 3.5f;
        sf::Color dotColor = ui::pal::okTurn;
        dotColor.a = static_cast<std::uint8_t>(
            255.f * thinkingAlpha(pulseSec_));
        sf::CircleShape dot(r);
        dot.setPosition({dotX, nameCy - r});
        dot.setFillColor(dotColor);
        window.draw(dot);
        dotX += 2.f * r + 5.f;
    }

    if (state.connection != 0) {
        const float iconS = ui::icons::iconBox(BadgeTextSize);
        ui::icons::drawIcon(window, ui::icons::IconKind::Link,
                            {dotX, nameCy - iconS * 0.5f}, iconS,
                            state.connection == 1 ? ui::pal::okDim
                                                  : ui::pal::warnSoft);
        dotX += iconS + 5.f;
    }

    // Captured row: sprite glyphs pawns-first, then a ghosted premove
    // badge when a move is queued (11.4).
    float capX = capturedSlot().position.x;
    const float capY = capturedSlot().position.y;
    if (state.captured != nullptr) {
        constexpr float CapturedPieceSize = 15.f;
        constexpr float CapturedStep = 17.f;
        for (const Piece& piece : *state.captured) {
            if (app.piecesLoaded()) {
                const auto& tex = app.pieceTexture(piece.color, piece.type);
                sf::Sprite sprite(tex);
                float scale = CapturedPieceSize / static_cast<float>(tex.getSize().x);
                sprite.setScale({scale, scale});
                sprite.setPosition({capX, capY});
                window.draw(sprite);
            } else {
                const char letters[] = { 'P', 'N', 'B', 'R', 'Q', 'K' };
                sf::Text letter(font, std::string(1, letters[static_cast<int>(piece.type)]), 13);
                letter.setFillColor(piece.color == Color::White
                    ? ui::pal::textBody : ui::pal::textFaded);
                letter.setPosition({capX, capY});
                window.draw(letter);
            }
            capX += CapturedStep;
        }
    }

    if (state.premovePending) {
        sf::Text badge(font, "premove", BadgeTextSize);
        badge.setFillColor(ui::pal::warnSoft);
        badge.setPosition({capX + (state.captured ? 4.f : 0.f), capY - 1.f});
        window.draw(badge);
    }
}

} // namespace chess::client
