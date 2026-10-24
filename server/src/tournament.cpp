#include "tournament.h"

#include <algorithm>
#include <numeric>

namespace chess::server {

int arenaPoints(const ArenaEntrant& entrant)
{
    return 2 * entrant.wins + entrant.draws;
}

int arenaGames(const ArenaEntrant& entrant)
{
    return entrant.wins + entrant.losses + entrant.draws;
}

// Deterministic arena pairing (12.3). Repeatedly seats the lowest-scored
// unpaired entrant and pairs them with the remainder's closest score.
// Seats are scanned in (points ASC, id ASC) order, so the first remaining
// candidate IS the closest-score/smallest-id pick — the only reason to
// look further is the immediate-rematch guard, and its previous opponent
// is only chosen when no alternative exists.
std::vector<std::pair<std::size_t, std::size_t>>
arenaPairIdle(const std::vector<ArenaEntrant>& idle)
{
    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    if (idle.size() < 2)
        return pairs;

    std::vector<std::size_t> unpaired(idle.size());
    std::iota(unpaired.begin(), unpaired.end(), std::size_t{0});
    std::sort(unpaired.begin(), unpaired.end(),
              [&idle](std::size_t a, std::size_t b) {
                  const int pa = arenaPoints(idle[a]);
                  const int pb = arenaPoints(idle[b]);
                  if (pa != pb) return pa < pb;
                  return idle[a].id < idle[b].id;
              });

    while (unpaired.size() >= 2) {
        const std::size_t first = unpaired.front();
        unpaired.erase(unpaired.begin());

        std::optional<std::size_t> fresh;
        std::optional<std::size_t> rematchOnly;
        for (const std::size_t candidate : unpaired) {
            const bool wasLastOpponent =
                idle[first].lastOpponentId != 0
                && idle[candidate].id == idle[first].lastOpponentId;
            if (wasLastOpponent) {
                // Keep scanning: a further candidate may still be fresh.
                if (!rematchOnly) rematchOnly = candidate;
                continue;
            }
            fresh = candidate;
            break; // closest score, smallest id — deeper scan can't beat it
        }
        // Both branches break on the first eligible candidate: anything
        // later in the scan order has a worse-or-equal score distance and
        // a bigger id. Only when the sole candidate is the previous
        // opponent does the rematch pairing happen.
        if (fresh) {
            unpaired.erase(std::find(unpaired.begin(), unpaired.end(), *fresh));
            pairs.emplace_back(first, *fresh);
        } else if (rematchOnly) {
            unpaired.erase(std::find(unpaired.begin(), unpaired.end(), *rematchOnly));
            pairs.emplace_back(first, *rematchOnly);
        } else {
            break; // unreachable: `unpaired` had >= 1 candidate
        }
    }
    return pairs;
}

Tournament::Tournament(std::string key, int roundsTarget)
    : key_(std::move(key))
    , roundsTarget_(std::max(roundsTarget, 1))
{
}

std::optional<std::int64_t> Tournament::join(const std::string& name,
                                             std::string& errorOut)
{
    // Joining an ended arena is a fresh generation: the frozen table is
    // replaced by an empty running one. The clear happens BEFORE the
    // name check — the joiner's own frozen row must not block their
    // re-join.
    if (phase_ == ArenaPhase::Ended) {
        entrants_.clear();
        phase_ = ArenaPhase::Open;
    }
    if (entrants_.size() >= 100) {
        errorOut = "The arena is full";
        return std::nullopt;
    }
    if (findByName(name) != 0) {
        errorOut = "That name is already in the arena";
        return std::nullopt;
    }
    ArenaEntrant entrant;
    entrant.id = nextId_++;
    entrant.name = name;
    entrants_.push_back(entrant);
    return entrant.id;
}

bool Tournament::leave(std::int64_t id)
{
    auto it = std::find_if(entrants_.begin(), entrants_.end(),
                           [id](const ArenaEntrant& e) { return e.id == id; });
    if (it == entrants_.end())
        return false;
    entrants_.erase(it);
    return true;
}

bool Tournament::has(std::int64_t id) const
{
    return entrant(id) != nullptr;
}

ArenaEntrant* Tournament::entrant(std::int64_t id)
{
    auto it = std::find_if(entrants_.begin(), entrants_.end(),
                           [id](const ArenaEntrant& e) { return e.id == id; });
    return it == entrants_.end() ? nullptr : &*it;
}

const ArenaEntrant* Tournament::entrant(std::int64_t id) const
{
    auto it = std::find_if(entrants_.begin(), entrants_.end(),
                           [id](const ArenaEntrant& e) { return e.id == id; });
    return it == entrants_.end() ? nullptr : &*it;
}

std::int64_t Tournament::findByName(const std::string& name) const
{
    auto it = std::find_if(entrants_.begin(), entrants_.end(),
                           [&name](const ArenaEntrant& e) { return e.name == name; });
    return it == entrants_.end() ? 0 : it->id;
}

bool Tournament::applyResult(const std::string& whiteName,
                             const std::string& blackName,
                             chess::net::GameResult result)
{
    ArenaEntrant* white = nullptr;
    ArenaEntrant* black = nullptr;
    for (auto& entrant : entrants_) {
        if (entrant.name == whiteName) white = &entrant;
        else if (entrant.name == blackName) black = &entrant;
    }
    // At least one row must exist; nothing to touch means no game of
    // ours ever finished.
    if (white == nullptr && black == nullptr)
        return false;

    switch (result) {
        case chess::net::GameResult::WhiteWins:
            if (white) ++white->wins;
            if (black) ++black->losses;
            break;
        case chess::net::GameResult::BlackWins:
            if (white) ++white->losses;
            if (black) ++black->wins;
            break;
        case chess::net::GameResult::Draw:
            if (white) ++white->draws;
            if (black) ++black->draws;
            break;
        default:
            // Abort (and anything future) changes no scores — an aborted
            // arena game neither counts a round nor awards points.
            break;
    }

    // Every game's finish ends an active pairing; the touched rows are
    // idle for the next pass either way.
    if (white) white->inGame = false;
    if (black) black->inGame = false;
    return true;
}

bool Tournament::everyoneFinished() const
{
    if (entrants_.empty())
        return false;
    return std::all_of(entrants_.begin(), entrants_.end(), [this](const auto& e) {
        return arenaGames(e) >= roundsTarget_;
    });
}

void Tournament::endArena()
{
    phase_ = ArenaPhase::Ended;
}

} // namespace chess::server
