#pragma once

#include <chess/move.h>

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

} // namespace chess::client
