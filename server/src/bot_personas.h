#pragma once

#include <string>
#include <vector>

namespace chess::server {

// ── Configurable server bot personas (17.3) ─────────────────────────────────
// The server keeps its own persona table (`--bot-personas`): persona names
// mapped to depth/movetime/skill presets for spawned bots (the 8.3
// botSpawnQueue path, unchanged — zero wire changes; the persona name just
// becomes the bot's account-less display name).

struct BotPersona {
    std::string name;
    int depth = 1;
    int movetimeMs = 0;
    int skill = 20; // UCI Skill Level; -1 = leave the engine default
};

// Parses "Sable=4:500:6,Oracle=0:2000" style specs: entries separated by
// ',', each `Name=depth:movetime[:skill]`. Malformed entries are skipped
// best-effort (a bad operator flag must never kill the server startup).
std::vector<BotPersona> parseBotPersonas(const std::string& spec);

} // namespace chess::server
