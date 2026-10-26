#pragma once

#include <cstdlib>
#include <string>
#include <vector>

// Runtime roots for locating assets/ (fonts, piece sets, puzzles.pgn).
// The CHESS_ASSETS env override comes first — packaged installs point it
// at the installed datadir — followed by the three relative roots a
// checkout build relies on (repo root, build/, tests/).
inline std::vector<std::string> assetRoots()
{
    std::vector<std::string> roots;
    if (const char* env = std::getenv("CHESS_ASSETS"))
        if (*env)
            roots.push_back(std::string(env) + "/");
    roots.push_back("assets/");
    roots.push_back("../assets/");
    roots.push_back("../../assets/");
    return roots;
}
