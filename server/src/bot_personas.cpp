#include "bot_personas.h"

#include <cstdlib>
#include <sstream>

namespace chess::server {

namespace {

bool parseInt(const std::string& text, int& out)
{
    if (text.empty())
        return false;
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end != text.c_str() + text.size())
        return false;
    out = static_cast<int>(value);
    return true;
}

} // namespace

std::vector<BotPersona> parseBotPersonas(const std::string& spec)
{
    std::vector<BotPersona> personas;
    std::istringstream entries(spec);
    std::string entry;
    while (std::getline(entries, entry, ',')) {
        if (entry.empty())
            continue;

        const std::size_t eq = entry.find('=');
        if (eq == std::string::npos || eq == 0)
            continue; // no name, or empty name — skip
        BotPersona persona;
        persona.name = entry.substr(0, eq);
        const std::string values = entry.substr(eq + 1);

        std::istringstream nums(values);
        std::string depthText, movetimeText, skillText;
        if (!std::getline(nums, depthText, ':')
            || !std::getline(nums, movetimeText, ':'))
            continue; // need at least depth:movetime — skip
        if (!parseInt(depthText, persona.depth)
            || !parseInt(movetimeText, persona.movetimeMs)
            || persona.depth < 0 || persona.movetimeMs < 0)
            continue;

        if (std::getline(nums, skillText, ':')) {
            if (!parseInt(skillText, persona.skill) || persona.skill < 0)
                continue; // negative skill is nonsense (0..20 is the UCI range)
        }
        personas.push_back(std::move(persona));
    }
    return personas;
}

} // namespace chess::server
