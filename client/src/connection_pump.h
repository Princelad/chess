#pragma once

#include "app.h"
#include "screens/game_screen.h"

#include <chess/net/messages.h>

#include <optional>
#include <utility>

namespace chess::client {

// Audit 9.2: screens reachable over a live connection that do NOT own the
// rematch conversation (Analysis and Report, pushed from GameOverScreen)
// must still pump the link — the heartbeat, the pong bookkeeping and every
// message read all ride poll(), and the arena's GameOver pass re-pairs
// idle seats ~1 s after a game ends, so a pairing WelcomeMsg can be in
// flight the moment these screens open. A screen that never pumps strands
// the socket: the pairing is never consumed and the opponent is left in a
// phantom game.
//
// Consumes only what acts on this handover: WelcomeMsg (the pairing —
// when this returns true the caller must switch into the fresh GameScreen
// and return immediately, nothing of `this` after) and ProfileMsg (the
// app-owned profile refresh). Everything else — notably a buffered
// ServerRematchRequestMsg — is UNREAD back onto the inbox front, so
// GameOverScreen still sees it with exact ordering when the user returns.
inline bool pumpPairing(App& app)
{
    app.connection().poll();

    while (app.connection().hasMessages()) {
        auto msg = app.connection().nextMessage();
        if (auto* welcome = std::get_if<chess::net::WelcomeMsg>(&msg)) {
            std::optional<std::pair<double, double>> opponentRating;
            if (welcome->rated)
                opponentRating = std::make_pair(welcome->rating,
                                                welcome->deviation);
            app.switchScreen(std::make_unique<GameScreen>(
                app, welcome->color, welcome->opponent, opponentRating,
                welcome->rated, welcome->timeControl, welcome->resumeToken,
                welcome->startFen, welcome->arena));
            return true;
        }
        if (auto* profile = std::get_if<chess::net::ProfileMsg>(&msg)) {
            app.setProfile(*profile);
            continue;
        }
        // Not ours: give it back for the conversation's owner.
        app.connection().unreadMessage(std::move(msg));
        return false;
    }
    return false;
}

} // namespace chess::client
