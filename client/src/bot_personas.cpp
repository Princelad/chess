#include "bot_personas.h"

#include <cctype>

namespace chess::client::personas {

const std::vector<Persona>& all()
{
    static const std::vector<Persona> cast = {
        {"Pip", "Careful beginner", 1, 2, 250, 800},
        {"Sable", "Steady club player", 6, 4, 500, 1200},
        {"Meridian", "Solid tactician", 11, 8, 800, 1600},
        {"Vantage", "Sharp attacking expert", 16, 13, 1200, 2000},
        {"Oracle", "Full strength", 20, 0, 2000, 2500},
    };
    return cast;
}

const Persona* find(const std::string& name)
{
    auto lower = [](const std::string& s) {
        std::string out;
        for (char c : s)
            out.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(c))));
        return out;
    };
    const std::string needle = lower(name);
    for (const auto& p : all())
        if (needle == lower(p.name))
            return &p;
    return nullptr;
}

} // namespace chess::client::personas
