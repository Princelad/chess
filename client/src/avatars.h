#pragma once

// 14.3 Avatar system — a deterministic person emblem, plus an optional
// drop-in art pack under assets/avatars/.
//
// Pure base (header-only, headless-testable):
//   * seedFromName  — FNV-1a over the bytes (same discipline as the chat
//     name color), so the same person always lands on the same avatar;
//   * seedFor       — the self seed with the config `profile.avatar` knob:
//     any non-empty string there replaces the displayed name as the seed
//     input (a re-roll anyone can type into the INI);
//   * cellsFor      — a mirrored 5x5 cell grid off the seed (the mirror
//     symmetry is what turns hash noise into a deliberate-looking emblem);
//   * colorsFor     — one of a small curated palette family (deep tile +
//     light pattern), always readable on the dark panel.
//
// Drawing mirrors the 14.2 icons pattern: the draw helpers need a render
// target, so unit tests cover only the pure math. A pack tile (loaded
// sprite, e.g. art pack emotes) wins over the procedural identicon; the
// procedural path falls back to the person's initial on a flat tile below
// `letterThreshold`, where a 5x5 grid would collapse to mush.

#include "config.h"
#include "asset_paths.h"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace chess::client::avatars {

// FNV-1a 64-bit (same basis/prime constants as chat_log.h's name color).
inline std::uint64_t seedFromName(const std::string& name)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (unsigned char c : name) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

// The seed for a person: config `profile.avatar` (any string, 14.3) ranks
// first; empty config means "derive from the displayed name".
inline std::uint64_t seedFor(const Config& config,
                             const std::string& displayName)
{
    const std::string custom = config.get("profile.avatar", "");
    return seedFromName(custom.empty() ? displayName : custom);
}

// Pack index for a seed: uniform spread over the loaded tiles; zero tiles
// keeps 0 (callers check emptiness first).
inline std::size_t packIndexFor(std::uint64_t seed, std::size_t tiles)
{
    return tiles == 0 ? 0 : static_cast<std::size_t>(seed % tiles);
}

// The initial shown when a procedural avatar is too small for its
// pattern: first alnum byte, uppercased; '?' when nothing usable.
inline char initialOf(const std::string& name)
{
    for (unsigned char c : name)
        if (std::isalnum(c))
            return static_cast<char>(std::toupper(c));
    return '?';
}

// Below this side length a 5x5 pattern has cells under ~3 px and turns to
// noise — the flat initial rides instead.
constexpr float letterThreshold = 17.f;

struct AvatarColors {
    sf::Color background;
    sf::Color tile;

    // C++17: no defaulted comparison operators — explicit constexpr.
    constexpr bool operator==(const AvatarColors& other) const
    {
        return background == other.background && tile == other.tile;
    }
    constexpr bool operator!=(const AvatarColors& other) const
    {
        return !(*this == other);
    }
};

// Curated deep/light pairs; both stay far from the panel grays so every
// emblem pops on the dark chrome. Indexed by the seed spread over the
// table (never a raw hue computation — contrast stays guaranteed).
inline AvatarColors colorsFor(std::uint64_t seed)
{
    static const AvatarColors palette[] = {
        {sf::Color(58, 44, 84), sf::Color(196, 180, 255)},  // violet
        {sf::Color(24, 66, 60), sf::Color(150, 230, 205)},  // teal
        {sf::Color(72, 40, 42), sf::Color(255, 170, 160)},  // salmon
        {sf::Color(30, 52, 74), sf::Color(150, 200, 250)},  // blue
        {sf::Color(75, 58, 22), sf::Color(250, 210, 120)},  // amber
        {sf::Color(38, 62, 38), sf::Color(170, 230, 150)},  // green
        {sf::Color(60, 32, 60), sf::Color(240, 170, 240)},  // magenta
        {sf::Color(76, 28, 36), sf::Color(250, 160, 190)},  // rose
        {sf::Color(90, 66, 24), sf::Color(255, 200, 140)},  // orange
        {sf::Color(70, 72, 28), sf::Color(240, 240, 150)},  // lime
    };
    return palette[seedFromName(std::to_string(seed))
                   % (sizeof palette / sizeof palette[0])];
}

// Mirrored 5x5 grid: only the left 3 columns carry bits; columns 0..2
// mirror into 4..3, so the emblem is horizontally symmetric no matter
// what the hash rolls. index = row * 5 + column.
inline std::array<bool, 25> cellsFor(std::uint64_t seed)
{
    // Spread first: consecutive name hashes differ in low bits that a
    // 15-bit draw would otherwise lose.
    const std::uint64_t bits = seedFromName(std::to_string(seed));
    std::array<bool, 25> cells{};
    for (int row = 0; row < 5; ++row)
        for (int col = 0; col < 3; ++col) {
            const bool on = ((bits >> (row * 3 + col)) & 1ull) != 0ull;
            cells[row * 5 + col] = on;
            cells[row * 5 + (4 - col)] = on;
        }
    return cells;
}

// Optional art pack: square PNG tiles under `assets/avatars/` sorted by
// filename. Empty pack = the procedural path everywhere; missing dir
// degrades silently (the bullets' contract).
struct AvatarPack {
    std::vector<sf::Texture> tiles;

    bool empty() const { return tiles.empty(); }
    const sf::Texture& tileFor(std::uint64_t seed) const
    {
        return tiles[packIndexFor(seed, tiles.size())];
    }
};

// Loads the pack from `assets/<dirName>` under the runtime roots; the
// FIRST root holding the directory wins (puzzles.pgn discipline — never
// merge across roots). Caller owns the returned pack for the session.
AvatarPack loadAvatarPack(const std::string& dirName = "avatars");

// Procedural emblem: flat rounded-tone tile, the mirrored pattern on top;
// below `letterThreshold` the person's initial replaces the pattern.
inline void drawProcedural(sf::RenderTarget& target, std::uint64_t seed,
                           sf::Vector2f topLeft, float size,
                           const sf::Font* font,
                           const std::string& personName)
{
    const AvatarColors colors = colorsFor(seed);

    sf::RectangleShape background({size, size});
    background.setPosition(topLeft);
    background.setFillColor(colors.background);
    target.draw(background);

    if (size < letterThreshold) {
        // Flat-tile initial: the 6.2.2 letter-fallback discipline, in
        // emblem form.
        if (font != nullptr) {
            sf::Text letter(*font, std::string(1, initialOf(personName)),
                            static_cast<unsigned>(size * 0.62f));
            letter.setFillColor(colors.tile);
            const auto b = letter.getLocalBounds();
            letter.setPosition({
                topLeft.x + (size - b.size.x) * 0.5f - b.position.x,
                topLeft.y + (size - b.size.y) * 0.5f - b.position.y,
            });
            target.draw(letter);
        }
        return;
    }

    // Pattern identicon: inner square inset from the tile, 5x5 cells.
    const std::array<bool, 25> cells = cellsFor(seed);
    const float pad = size * 0.14f;
    const float cell = (size - 2.f * pad) / 5.f;
    sf::RectangleShape square({cell - size * 0.012f, cell - size * 0.012f});
    square.setFillColor(colors.tile);
    for (int row = 0; row < 5; ++row)
        for (int col = 0; col < 5; ++col) {
            if (!cells[row * 5 + col]) continue;
            square.setPosition({
                topLeft.x + pad + col * cell,
                topLeft.y + pad + row * cell,
            });
            target.draw(square);
        }
}

// The one draw entry point: pack tile when available, procedural emblem
// otherwise. `font`/`personName` only feed the tiny-size letter fallback.
inline void drawAvatar(sf::RenderTarget& target, const AvatarPack& pack,
                       std::uint64_t seed, sf::Vector2f topLeft, float size,
                       const sf::Font* font,
                       const std::string& personName)
{
    if (!pack.empty()) {
        const sf::Texture& tile = pack.tileFor(seed);
        const auto texSize = tile.getSize();
        if (texSize.x > 0 && texSize.y > 0) {
            sf::Sprite sprite(tile);
            sprite.setScale({size / static_cast<float>(texSize.x),
                             size / static_cast<float>(texSize.y)});
            sprite.setPosition(topLeft);
            target.draw(sprite);
            return;
        }
    }
    drawProcedural(target, seed, topLeft, size, font, personName);
}

} // namespace chess::client::avatars
