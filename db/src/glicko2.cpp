#include <chess/db/glicko2.h>

#include <cmath>

namespace chess::db {
namespace {

// Glicko-2 <-> Glicko conversion constants, hard-coded per the paper.
constexpr double Scale = 173.7178;
constexpr double Tau = 0.8;
constexpr double Epsilon = 0.000001;
// Deviation capped at the default value even with long inactivity.
constexpr double MaxDeviation = 350.0;
constexpr double DefaultRating = 1500.0;

double toMu(double rating) { return (rating - DefaultRating) / Scale; }
double toPhi(double deviation) { return deviation / Scale; }
double toRating(double mu) { return mu * Scale + DefaultRating; }
double toDeviation(double phi) { return phi * Scale; }

// g(φ): decreases with the opponent's uncertainty.
double g(double phi)
{
    return 1.0 / std::sqrt(1.0 + 3.0 * phi * phi / (M_PI * M_PI));
}

// E(μ, μⱼ, φⱼ): expected score against opponent j.
double expected(double mu, double opponentMu, double opponentPhi)
{
    return 1.0 / (1.0 + std::exp(-g(opponentPhi) * (mu - opponentMu)));
}

} // namespace

Glicko2 applyGames(const Glicko2& player, const std::vector<RatedOpponent>& games)
{
    const double mu = toMu(player.rating);
    const double phi = toPhi(player.deviation);

    if (games.empty()) {
        // Paper step 3: no games in the period — only the deviation grows.
        const double grown = std::sqrt(phi * phi + player.volatility * player.volatility);
        const double capped = std::min(grown, toPhi(MaxDeviation));
        return {player.rating, toDeviation(capped), player.volatility};
    }

    // Steps 3–4: variance v and estimated rating improvement Δ.
    double varianceInverse = 0.0;
    double outcomeSum = 0.0; // Σ g(φⱼ) (sⱼ − Eⱼ) — equals Δ/v
    for (const auto& game : games) {
        const double oppPhi = toPhi(game.deviation);
        const double gj = g(oppPhi);
        const double ej = expected(mu, toMu(game.rating), oppPhi);
        varianceInverse += gj * gj * ej * (1.0 - ej);
        outcomeSum += gj * (game.score - ej);
    }
    const double v = 1.0 / varianceInverse;
    const double delta = v * outcomeSum;

    // Step 5: new volatility, following the paper's iterative procedure.
    // The paper's variables A and B slide toward the root of f(x).
    double paperA = std::log(player.volatility * player.volatility);
    auto fOfX = [&](double x) {
        const double ex = std::exp(x);
        const double base = phi * phi + v + ex;
        return ex * (delta * delta - phi * phi - v - ex) / (2.0 * base * base)
            - (x - paperA) / (Tau * Tau);
    };

    // Step 5.2: initial second point B; moved below A in τ steps until
    // f(B) is non-negative when Δ² is small.
    double paperB = paperA;
    if (delta * delta > phi * phi + v) {
        paperB = std::log(delta * delta - phi * phi - v);
    } else {
        int k = 1;
        while (fOfX(paperA - k * Tau) < 0)
            ++k;
        paperB = paperA - k * Tau;
    }

    // Steps 5.3–5.4: secant-style refinement of the volatility root.
    double fA = fOfX(paperA);
    double fB = fOfX(paperB);
    while (std::abs(paperB - paperA) > Epsilon) {
        double c = paperA + (paperA - paperB) * fA / (fB - fA);
        double fC = fOfX(c);
        if (fC * fB <= 0) {   // paper, 2022 update: '<' replaced by '<='
            paperA = paperB;
            fA = fB;
        } else {
            fA = fA / 2.0;
        }
        paperB = c;
        fB = fC;
    }
    // Paper's A now holds the converged root; recover σ from it.
    const double sigmaPrime = std::exp(paperA / 2.0);

    // Step 6: pre-rating-period deviation growth (capped at the default).
    double phiPre = std::sqrt(phi * phi + sigmaPrime * sigmaPrime);
    if (phiPre > toPhi(MaxDeviation))
        phiPre = toPhi(MaxDeviation);

    // Step 7: the actual rating/deviation update.
    const double phiNew = 1.0 / std::sqrt(1.0 / (phiPre * phiPre) + 1.0 / v);
    const double muNew = mu + phiNew * phiNew * outcomeSum;

    return {toRating(muNew), toDeviation(phiNew), sigmaPrime};
}

} // namespace chess::db
