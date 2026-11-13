#pragma once

#include <string>
#include <vector>

namespace chess::client::personas {

// ── Persona bots (17.3) ─────────────────────────────────────────────────────
// Named difficulty presets for LOCAL play: an engine-option set (UCI Skill
// Level + depth cap + move-time budget) under a fresh, neutral name of our
// own — NOT chess.com's bot cast. Zero protocol surface; the server keeps
// its own configurable table (server/src/bot_personas.{h,cpp}).

struct Persona {
    const char* name;     // display name, also the avatar seed
    const char* blurb;    // one-line flavor description
    int skill;            // UCI "Skill Level" 0..20
    int depth;            // search depth cap (0 = none)
    int movetimeMs;       // move-time budget (go depth, movetime)
    int approxElo;        // display-only strength hint
};

// The fixed cast, ordered weakest -> strongest.
const std::vector<Persona>& all();

// Case-insensitive lookup by name; nullptr when unknown.
const Persona* find(const std::string& name);

} // namespace chess::client::personas
