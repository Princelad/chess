#pragma once

#include <cstdint>
#include <string_view>

namespace chess::db {

// Rating classes by time control (10.2). The class taxonomy is ready now so
// 11.1 clocks can key ratings per class the moment games carry a time
// control; with no clocks yet every game classifies as "unlimited", the
// single active class.
//
// Parsing accepts the "base+increment" form used by the server (minutes,
// e.g. "5+3"); pacing is estimated as base*60 + 40*increment seconds, the
// usual convention. Base-only forms ("90+0" = classical) work naturally.
//
// Boundaries (rounded game time): bullet < 3 min, blitz < 8 min,
// rapid < 25 min, classical otherwise; anything unparsable is "unlimited".
inline const char* classifyTimeControl(std::string_view tc)
{
    std::int64_t base = 0, increment = 0;
    std::size_t i = 0;
    while (i < tc.size() && tc[i] >= '0' && tc[i] <= '9')
        base = base * 10 + (tc[i++] - '0');
    // At least one base digit must precede the '+', so "+3" is garbage.
    if (i == 0 || i >= tc.size() || tc[i] != '+')
        return "unlimited";
    ++i;
    for (; i < tc.size(); ++i) {
        const char c = tc[i];
        if (c >= '0' && c <= '9')
            increment = increment * 10 + (c - '0');
        else
            return "unlimited";
    }
    if (base <= 0 && increment == 0)
        return "unlimited";

    const std::int64_t total = base * 60 + 40 * increment;
    if (total < 180)  return "bullet";
    if (total < 480)  return "blitz";
    if (total < 1500) return "rapid";
    return "classical";
}

} // namespace chess::db
