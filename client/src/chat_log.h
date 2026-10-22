#pragma once

#include "config.h"

#include <SFML/Graphics/Color.hpp>

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace chess::client {

// One chat line as rendered (11.8). `name` empty = a system line ("Draw
// offer received", mute feedback, ...); the sender's own echo uses the
// fixed `YouName` below so it colors distinctly from opponents.
struct ChatEntry {
    std::string name;
    std::string text;
    std::int64_t epochSec = 0;
};

inline const char* YouName() { return "You"; }

// Local-time "HH:MM" from an epoch second (the wall clock at arrival).
inline std::string chatTimestamp(std::int64_t epochSec)
{
    const std::time_t t = static_cast<std::time_t>(epochSec);
    std::tm local{};
    localtime_r(&t, &local);
    char buf[8];
    std::strftime(buf, sizeof buf, "%H:%M", &local);
    return buf;
}

// Deterministic per-name color: FNV-1a over the bytes picks one of a small
// palette of light hues readable on the dark chat panel. Same name always
// lands on the same color across the session (and across OS runs).
inline sf::Color chatNameColor(const std::string& name)
{
    static const sf::Color palette[] = {
        sf::Color(255, 200, 110), sf::Color(140, 220, 140),
        sf::Color(140, 190, 255), sf::Color(255, 150, 150),
        sf::Color(220, 160, 255), sf::Color(130, 230, 220),
        sf::Color(255, 190, 220), sf::Color(200, 200, 140),
        sf::Color(170, 170, 255), sf::Color(230, 230, 230),
    };
    std::uint64_t hash = 1469598103934665603ull; // FNV-1a offset basis
    for (unsigned char c : name) {
        hash ^= c;
        hash *= 1099511628211ull; // FNV-1a prime
    }
    return palette[hash % (sizeof palette / sizeof palette[0])];
}

// Per-player mute list (11.8), persisted as config `chat.muted` (CSV of
// names, blank-ignored). Client-local only: the server never learns who is
// muted; a muted line simply never enters this client's log.
using ChatMuteList = std::vector<std::string>;

// A mute name survives the INI store only when load()'s trim and comment
// rules keep it byte-identical on reload: '#' or ';' start a comment,
// '=' splits key/value, '[' opens a section header, control characters
// and edge blanks are trimmed away. The mute command refuses the rest —
// a silently persisted-but-different name would mute nothing after a
// restart (audit hardening).
inline bool chatMuteNamePersistable(const std::string& name)
{
    if (name.empty())
        return false;
    if (name.front() == '[' || name.front() == '#' || name.front() == ';')
        return false;
    for (unsigned char c : name) {
        if (c == '#' || c == ';' || c == '=' || c < ' ')
            return false; // covers '\n', '\r', tabs-as-controls
    }
    const char front = name.front(), back = name.back();
    if (front == ' ' || front == '\t' || back == ' ' || back == '\t')
        return false;
    return true;
}

inline ChatMuteList chatMutedFromConfig(const Config& config)
{
    ChatMuteList names;
    std::string csv = config.get("chat.muted", "");
    std::string current;
    auto flush = [&]() {
        if (!current.empty()) names.push_back(current);
        current.clear();
    };
    for (char c : csv) {
        if (c == ',')
            flush();
        else
            current += c;
    }
    flush();
    return names;
}

inline std::string chatSaveMuted(const ChatMuteList& names)
{
    std::string csv;
    for (const auto& name : names) {
        if (!csv.empty()) csv += ',';
        csv += name;
    }
    return csv;
}

inline bool chatIsMuted(const ChatMuteList& names, const std::string& name)
{
    for (const auto& muted : names)
        if (muted == name)
            return true;
    return false;
}

// Toggles one name in the config-persisted list; returns the new muted
// state. Does not save — the caller persists (it may batch other keys).
inline bool chatToggleMute(Config& config, const std::string& name)
{
    ChatMuteList names = chatMutedFromConfig(config);
    bool nowMuted = !chatIsMuted(names, name);
    if (nowMuted)
        names.push_back(name);
    else
        names.erase(std::remove(names.begin(), names.end(), name), names.end());
    config.set("chat.muted", chatSaveMuted(names));
    return nowMuted;
}

} // namespace chess::client
