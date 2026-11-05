#pragma once

// 15.2: the player card widget — avatar + name line (+ rating suffix),
// captured-material row, material diff, clock pill and the state
// affordances (thinking… dot, connection icon, premove badge), closing
// the 9.3 residual "player-card composition untested". The card is a
// pure display unit over one seat: its timing/reformat/reseat inputs
// arrive as values on each draw, the Hud remains the state owner.
//
// Everything decided before a render target exists — clock formatting
// (m:ss / tenths under a minute), the low-time heat thresholds (red
// < 10 s, amber < 30 s, carried verbatim from 11.1's HUD), the rating
// suffix, and the slot/row geometry inside the band rect — is free
// stdlib math here, so headless tests cover the composition slice
// (RenderTarget-free); only the glyph/sprite drawing needs a window.

#include <SFML/Graphics.hpp>

#include <chess/move.h>
#include <chess/types.h>

#include <optional>
#include <string>
#include <vector>

namespace chess::client {

class App;

// Clock display: "m:ss" normally, tenths under a minute ("42.3"), the
// usual online-chess shape (moved verbatim from hud.cpp).
std::string formatClockText(double ms);

// Low time heats up: red inside the last 10 s, amber until 30 s (11.1).
sf::Color clockHeatColor(double ms);

// "1650 ± 132" suffix for a player card name line; omitted when unknown
// (e.g. guests or unrated games) (moved verbatim from hud.cpp).
std::string cardRatingSuffix(
    const std::optional<std::pair<double, double>>& rating);

struct PlayerCardState {
    // Full rendered name line; the caller composes prefixes ("You -",
    // turn ownership) — the card adds the rating suffix itself.
    std::string nameLine;
    std::optional<std::pair<double, double>> rating;
    sf::Color nameColor{ 120, 120, 120 }; // pal::textFaded by default

    // 14.3 avatar: 0 seed (or empty name) skips the avatar slot.
    std::uint64_t avatarSeed = 0;
    std::string avatarName;

    // nullopt = unlimited game (no clock display): never shown.
    std::optional<double> remainingMs;

    // Captured-material row: pieces this side took (sprite glyphs), and
    // a "+N" diff when this side is ahead on material.
    const std::vector<Piece>* captured = nullptr;
    int materialDiff = 0;

    // State affordances (15.2).
    bool thinking = false;       // pulsing dot (opponent searching)
    bool myTurn = false;         // green pulsing dot on the own card
    int connection = 0;          // 0 none, 1 connected, 2 away
    bool premovePending = false; // ghost badge after the captured row
    int watcherCount = 0;        // spectator stands (12.4), white card
};

class PlayerCard {
public:
    // Band rect inside which the card paints (from the shared board
    // composer: opponentCard / localCard).
    void setRect(sf::FloatRect band);

    sf::FloatRect rect() const { return band_; }

    // Pure slice: where the name row / captured row sit inside the band.
    sf::FloatRect nameSlot() const;
    sf::FloatRect capturedSlot() const;

    // Advances the pulsing states (thinking/turn dots).
    void update(float dtSec);

    void draw(sf::RenderWindow& window, const sf::Font& font, const App& app,
              const PlayerCardState& state);

private:
    sf::FloatRect band_{};
    float pulseSec_ = 0.f;
};

} // namespace chess::client
