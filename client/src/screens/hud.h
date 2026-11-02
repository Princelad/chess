#pragma once

#include "board_layout.h"
#include "widgets/label.h"
#include "widgets/move_navigator.h"
#include "widgets/panel.h"
#include "eco.h"
#include <chess/move.h>
#include <chess/types.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace chess::client {

class App;

class Hud {
public:
    // 15.1: the Hud is a pure consumer of the shared board composer — it
    // owns no rect math, it reads the card-stack rows the composer lays
    // out for the screen.
    Hud(App& app, const layout::BoardLayout& panelLayout);

    void setGame(const Board& initialBoard, std::vector<chess::Move> moves,
                 std::vector<std::string> sans);
    void appendMove(const chess::Move& move, const std::string& san);
    MoveNavigator& navigator() { return navigator_; }

    void setInfo(const std::string& opponentName, Color myColor,
                 bool myTurn, bool gameOver);
    void setRatings(std::optional<std::pair<double, double>> local,
                    std::optional<std::pair<double, double>> opponent);
    // Spectator view (12.4): both name cards speak in seat names (no
    // "You"/turn ownership) and the watcher count annotates the white
    // card. ratingsSuffix reuse: localRating_ renders on the WHITE card
    // and opponentRating_ on the BLACK one — the screen maps both seats
    // accordingly.
    void setSpectatorMode(bool on) { spectatorMode_ = on; }
    void setSpectatorNames(const std::string& whiteName,
                           const std::string& blackName)
    {
        spectatorWhite_ = whiteName;
        spectatorBlack_ = blackName;
    }
    void setWatcherCount(int count)
    {
        watcherCount_ = count > 0 ? count : 0;
    }
    // Re-sync point from the (authoritative) server: `sideToMove` now owns
    // `sideRemainingMs` (11.1). Unlimited games feed 0, which disables the
    // display until the first clocked sync arrives.
    void syncClocks(Color sideToMove, std::int64_t sideRemainingMs);
    // Full-resync shape for a token resume (11.2): both figures sampled at
    // one instant plus the side that is on the move.
    void syncClocks(std::int64_t whiteRemainingMs, std::int64_t blackRemainingMs,
                    Color sideToMove);
    void setStatus(const std::string& msg, float duration);
    void setGameOver(bool gameOver) { gameOver_ = gameOver; }
    void handleScroll(float delta) { navigator_.handleScroll(delta); }
    void update(float dtSec);
    void draw(sf::RenderWindow& window, const sf::Font& font);

    float contentBottom() const;
    float moveListBottom() const;

private:
    void drawPlayerCard(sf::RenderWindow& window, const sf::Font& font,
                        const std::string& name, Color cardColor,
                        float nameY, float capturedY, bool isLocal,
                        const std::string& clockText, int watcherCount = 0,
                        std::uint64_t avatarSeed = 0,
                        const std::string& avatarName = "");
    // Current shown remaining of one side, honoring local ticking since the
    // last server sync for the side on the move.
    double shownRemainingMs(Color side) const;
    // Re-runs the ECO prefix-match (11.5) over the SAN history; called
    // wherever the move list changes (setGame / appendMove).
    void refreshOpening();

    App& app_;
    layout::BoardLayout panel_;

    std::string opponentName_;
    std::optional<std::pair<double, double>> localRating_;
    std::optional<std::pair<double, double>> opponentRating_;
    Color myColor_ = Color::White;
    bool myTurn_ = false;
    bool gameOver_ = false;

    // Spectator view (12.4).
    bool spectatorMode_ = false;
    std::string spectatorWhite_;
    std::string spectatorBlack_;
    int watcherCount_ = 0;

    // Chess clocks (11.1). remainingMs_[0]=White, [1]=Black, values sampled
    // at the last server move; the side on the move ticks locally in
    // update() between syncs. Infinite clocks stay disabled.
    bool clocksOn_ = false;
    Color clockTurn_ = Color::White;
    std::array<double, 2> remainingMs_ = {0.0, 0.0};
    double sinceSyncSec_ = 0.f;

    MoveNavigator navigator_;

    std::string statusMsg_;
    float statusTimer_ = 0.f;

    // Opening name (11.5), recomputed from the SAN history; empty label
    // when the position does not name a known line.
    std::optional<Opening> opening_;
    Label openingLabel_;

    Label opponentNameLabel_;
    Label localNameLabel_;
    Label statusLabel_;
    Label headerLabel_;
};

} // namespace chess::client