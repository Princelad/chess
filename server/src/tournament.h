#pragma once

#include <chess/net/messages.h>

#include <optional>
#include <string>
#include <vector>

namespace chess::server {

// Arena tournaments (12.3). An arena pairs its queued, idle entrants every
// server-loop pass until each of them has played `roundsTarget` games:
// closest scores meet first, an immediate rematch is avoided whenever an
// alternative exists. Scoring runs W=2 / D=1 / L=0; the fixed quota makes
// the end condition exact — no timers, no sweeps.
//
// Identity model: a row's id is the *connection's* entrant slot, assigned
// when the connection joins. Disconnects drop the row (a fresh connection
// that re-joins becomes a fresh row at 0 points); finished games update
// rows by seat name, so a row that is already gone simply contributes
// nothing. Names are unique within one arena — the joiner is refused on a
// collision, which covers guests (accounts are unique by definition).

// One entrant row of an arena. Kept POD-ish so the pure pairing pass can
// be exercised without clients, sockets or a running server.
struct ArenaEntrant {
    std::int64_t id = 0;
    std::string name;
    int wins = 0;
    int losses = 0;
    int draws = 0;
    // The most recent opponent's row id (0 = none): the pairing pass
    // avoids pairing the same two entrants twice in a row.
    std::int64_t lastOpponentId = 0;
    // True while this entrant is seated in a live arena game — the next
    // pass skips them.
    bool inGame = false;
};

// Displayed score: W=2 / D=1 / L=0 (the draw point rewards both seats).
int arenaPoints(const ArenaEntrant& entrant);

// Rounds played so far; the arena ends when every member reached the
// target.
int arenaGames(const ArenaEntrant& entrant);

// Pure pairing pass over *idle* entrants (the caller must have filtered
// presence/quota/inGame already). Pairs the lowest-scored unpaired
// entrant with the unpaired entrant of closest score (ties break to the
// smaller id, then away from lastOpponentId); the previous opponent is
// only paired when no alternative exists. Odd counts leave one entrant
// unpaired. Deterministic: same input, same pairs.
std::vector<std::pair<std::size_t, std::size_t>>
arenaPairIdle(const std::vector<ArenaEntrant>& idle);

enum class ArenaPhase { Open, Ended };

class Tournament {
public:
    // roundsTarget < 1 collapses to 1 (everyone plays one game).
    explicit Tournament(std::string key, int roundsTarget);

    const std::string& key() const { return key_; }
    int roundsTarget() const { return roundsTarget_; }
    ArenaPhase phase() const { return phase_; }

    // Adds an entrant row and returns its id. Joining a full arena or a
    // name that is already present fails with the given error text; a
    // join into an Ended arena starts a fresh generation (rows cleared).
    std::optional<std::int64_t> join(const std::string& name,
                                     std::string& errorOut);

    // Drops the row. Invalid ids are a no-op that still returns false.
    bool leave(std::int64_t id);

    bool has(std::int64_t id) const;
    ArenaEntrant* entrant(std::int64_t id);
    const ArenaEntrant* entrant(std::int64_t id) const;
    // Row id of `name`, or 0 when absent (joining/validation use).
    std::int64_t findByName(const std::string& name) const;

    const std::vector<ArenaEntrant>& entrants() const { return entrants_; }

    // Records one finished arena game by seat name. A seat whose row is
    // already gone (mid-life disconnect reap) changes nothing but does
    // NOT void the game: the surviving member still gets their result —
    // a win is a win even with the opponent's row reaped. Abort results
    // change no scores. Returns true when any row changed, and clears the
    // inGame flags of every row it touched.
    bool applyResult(const std::string& whiteName, const std::string& blackName,
                     chess::net::GameResult result);

    // True when every member played its full quota — the trigger for the
    // final standings broadcast + phase flip.
    bool everyoneFinished() const;

    // Marks the final table: phase Ended, standings frozen until somebody
    // joins again (fresh generation).
    void endArena();

private:
    std::string key_;
    int roundsTarget_ = 1;
    ArenaPhase phase_ = ArenaPhase::Open;
    std::vector<ArenaEntrant> entrants_;
    std::int64_t nextId_ = 1;
};

} // namespace chess::server
