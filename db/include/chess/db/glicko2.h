#pragma once

#include <vector>

namespace chess::db {

// Glicko-2 rating state per Glickman's paper
// (http://www.glicko.net/glicko/glicko2.pdf).
struct Glicko2 {
    double rating = 1500.0;     // default new-player rating
    double deviation = 350.0;   // rating deviation (uncertainty)
    double volatility = 0.06;   // expected rating fluctuation
};

// One game in a rating period: the pre-update opponent rating/deviation and
// this player's outcome (1 = win, 0 = loss, 0.5 = draw).
struct RatedOpponent {
    double rating = 1500.0;
    double deviation = 350.0;
    double score = 0.5;
};

// Applies one rating period to the player. Typically one opponent per call
// (our server updates immediately on GAME_OVER), but the math supports the
// multi-opponent periods of the original system.
Glicko2 applyGames(const Glicko2& player, const std::vector<RatedOpponent>& games);

} // namespace chess::db
