#include "analysis_notes.h"

#include "config.h"
#include "eval_harness.h"

#include <cctype>
#include <cstdio>

namespace chess::client::notes {

std::string gameHash(const std::string& startFen,
                     const std::vector<std::string>& sans)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(
                      evalGameHash(startFen, sans)));
    return buf;
}

std::string encodeAnnotations(const BoardAnnotations& notes)
{
    std::string value;
    for (const auto& circle : notes.circles()) {
        if (!value.empty()) value += ' ';
        value += 'c';
        value += static_cast<char>('0' + circle.first);
        value += static_cast<char>('0' + circle.second);
    }
    for (const auto& arrow : notes.arrows()) {
        if (!value.empty()) value += ' ';
        value += 'a';
        value += static_cast<char>('0' + arrow.from.first);
        value += static_cast<char>('0' + arrow.from.second);
        value += static_cast<char>('0' + arrow.to.first);
        value += static_cast<char>('0' + arrow.to.second);
    }
    return value;
}

void decodeAnnotations(const std::string& value, BoardAnnotations& notes)
{
    notes.clear();
    std::size_t i = 0;
    while (i < value.size()) {
        while (i < value.size() && value[i] == ' ') ++i;
        if (i >= value.size()) break;
        const char kind = value[i++];
        const auto digit = [&]() -> int {
            if (i >= value.size() || !std::isdigit(static_cast<unsigned char>(value[i])))
                return -1;
            return value[i++] - '0';
        };
        if (kind == 'c') {
            const int file = digit();
            const int rank = digit();
            if (file >= 0 && file < 8 && rank >= 0 && rank < 8)
                notes.toggleCircle(file, rank);
        } else if (kind == 'a') {
            const int fromFile = digit();
            const int fromRank = digit();
            const int toFile = digit();
            const int toRank = digit();
            if (fromFile >= 0 && fromFile < 8 && fromRank >= 0 && fromRank < 8
                && toFile >= 0 && toFile < 8 && toRank >= 0 && toRank < 8)
                notes.addArrow(fromFile, fromRank, toFile, toRank);
        }
        // Unknown token kinds: skip to the next space (best effort).
        while (i < value.size() && value[i] != ' ') ++i;
    }
}

} // namespace chess::client::notes
