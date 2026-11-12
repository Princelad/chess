#include "daily.h"

namespace chess::client::daily {

// Howard Hinnant's days_from_civil: days since 1970-01-01.
int dayNumber(int year, int month, int day)
{
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy =
        (153u * static_cast<unsigned>(month + (month > 2 ? -3 : 9)) + 2u) / 5u
        + static_cast<unsigned>(day) - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + static_cast<int>(doe) - 719468;
}

int dailyIndex(int dayNumber, int poolSize)
{
    if (poolSize <= 0)
        return 0;
    std::uint64_t z =
        static_cast<std::uint64_t>(dayNumber) * 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z = z ^ (z >> 31);
    return static_cast<int>(z % static_cast<std::uint64_t>(poolSize));
}

int solveStreak(int lastDay, int currentStreak, int today)
{
    if (lastDay == today)
        return currentStreak;
    if (lastDay == today - 1)
        return currentStreak + 1;
    return 1;
}

int displayStreak(int lastDay, int storedStreak, int today)
{
    return lastDay >= today - 1 ? storedStreak : 0;
}

bool solvedToday(int lastDay, int today)
{
    return lastDay == today;
}

} // namespace chess::client::daily
