#pragma once

#include <string>

namespace chess::client {

// Named bundled piece sets (11.6). Each set maps to an `assets/pieces/<dir>/`
// folder holding the 12 sprites (wp.png ... bk.png), documented with a
// README.md stating author and license (cburnett GPLv2+/CC BY-SA, merida
// GPLv2+, chessnut Apache 2.0). Persisted as config `pieces.set`; the
// explicit custom directory (`pieces.path`) keeps priority — user-pointed
// intent wins over the named umbrella.
enum class PieceSet { Cburnett, Merida, Chessnut, Count };

inline const char* pieceSetDirName(PieceSet set)
{
    switch (set) {
        case PieceSet::Cburnett: return "cburnett";
        case PieceSet::Merida:   return "merida";
        case PieceSet::Chessnut: return "chessnut";
        default:                 return "cburnett";
    }
}

inline PieceSet nextPieceSet(PieceSet set)
{
    return static_cast<PieceSet>(
        (static_cast<int>(set) + 1) % static_cast<int>(PieceSet::Count));
}

inline PieceSet pieceSetFromName(const std::string& name)
{
    for (int i = 0; i < static_cast<int>(PieceSet::Count); ++i)
        if (name == pieceSetDirName(static_cast<PieceSet>(i)))
            return static_cast<PieceSet>(i);
    return PieceSet::Cburnett;
}

} // namespace chess::client
