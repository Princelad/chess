#pragma once

#include <chess/move.h>
#include <ui/theme.h>

#include <SFML/System/Vector2.hpp>

#include <vector>

namespace chess::client {

struct PromotionState {
    int fromFile, fromRank;
    int toFile, toRank;
    std::vector<chess::Move> candidates;
};

struct PromoCell {
    sf::Vector2f pos;
    float size;
};

// 15.6 shared picker geometry (was three identical promoCell copies in
// GameScreen/LocalGameScreen/PuzzleScreen): one candidate cell per slot,
// stacked contiguously down the promotion column — anchored so the first
// cell centers on the destination square and the stack grows toward the
// board center: downward when the destination sits in the visual top half,
// UPWARD when it sits in the bottom half (unflipped-black and flipped-white
// promotions anchor on the bottom row, and an unconditional downward stack
// hung cells 1..3 off the board — audit 8.1). Cells read bigger than the
// squares beneath (ui::overlay::promoCellScale) and spill 7.5% of a square
// into the neighbor files — inside the board margin, never off-window.
inline PromoCell promoStackCell(float squareSize, sf::Vector2f origin,
                                bool flipped, int toFile, int toRank,
                                int index)
{
    const float cell = squareSize * ui::overlay::promoCellScale;
    const float col = static_cast<float>(flipped ? (7 - toFile) : toFile);
    const float row = static_cast<float>(flipped ? toRank : (7 - toRank));
    const float inset = (squareSize - cell) * 0.5f;
    // Promotion destinations always land on the outer rank, so the anchor
    // row is exactly 0 (top) or 7 (bottom); the half-board split keeps the
    // two already-pinned top-anchored combos pixel-identical.
    const float dir = row < 4.f ? 1.f : -1.f;
    return {
        { origin.x + col * squareSize + inset,
          origin.y + row * squareSize + inset
              + static_cast<float>(index) * cell * dir },
        cell
    };
}

} // namespace chess::client
