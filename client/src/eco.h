#pragma once

#include <optional>
#include <string>
#include <vector>

namespace chess::client {

// Opening names by SAN move list (11.5). A small ECO table is compiled in
// (headers, no assets and no I/O) and prefix-matched against the game's SAN
// history: the longest entry whose tokens begin the list wins, so the label
// refines as the game grows (e4 -> King's Pawn, e4 c6 -> Caro-Kann, e4 c6 d4
// d5 e5 -> Caro-Kann Advance). Move orders that transpose are covered by
// listing each spelling separately. An unknown or too-short position yields
// no label and the HUD simply shows none.
//
// Match normalization: check '+' and mate '#' suffixes are stripped from
// both sides, so a diverted line still names its family.
struct Opening {
    std::string code; // "B01"
    std::string name; // "Scandinavian Defence"
};

struct EcoEntry {
    const char* sans; // space-separated SAN prefix, e.g. "e4 c6 d4 d5"
    const char* code; // ECO letter+number
    const char* name;
};

// Strip '+'/'#' (and stray '!'/'?') decorations so table tokens and real
// SANs compare equal across sources ("e4" and "e4+" name the same line).
inline std::string ecoToken(const std::string& san)
{
    std::string clean = san;
    while (!clean.empty()
        && (clean.back() == '+' || clean.back() == '#' || clean.back() == '!'
            || clean.back() == '?'))
        clean.pop_back();
    return clean;
}

inline std::optional<Opening> ecoLookup(const std::vector<std::string>& sans)
{
    if (sans.empty())
        return std::nullopt;

    std::optional<Opening> best;
    std::size_t bestLen = 0; // matched token count

    // Compiled-in table (11.5): common first-book openings and mainlines.
    // ECO codes follow the standard letter ranges; depth is deliberately
    // conservative — entries only where the code is unambiguous.
    static const EcoEntry table[] = {
        // 1.a3/b3/b4/c3/e3/f3/f4/g3/g4/Nc3 — unnamed irregular first moves
        {"a3", "A00", "Anderssen's Opening"},
        {"b3", "A01", "Nimzo-Larsen Attack"},
        {"b4", "A00", "Polish Opening"},
        {"c3", "A00", "Saragossa Opening"},
        {"e3", "A00", "Van 't Kruijs Opening"},
        {"f3", "A00", "Barnes Opening"},
        {"f4", "A02", "Bird's Opening"},
        {"g3", "A00", "Benko's Opening"},
        {"g4", "A00", "Grob Opening"},
        {"Nc3", "A00", "Dunst (Sodium) Opening"},

        // Flank / 1.Nf3 family
        {"Nf3", "A04", "Zukertort (Réti) Opening"},
        {"Nf3 d5 g3", "A07", "King's Indian Attack"},
        {"Nf3 d5", "A06", "Réti Opening"},

        // 1.c4 English
        {"c4", "A10", "English Opening"},
        {"c4 e5", "A20", "King's English (Reversed Sicilian)"},
        {"c4 Nf6", "A15", "Anglo-Indian Defence"},
        {"c4 c5", "A30", "Symmetrical English"},

        // 1.d4 non-mainline
        {"d4", "A40", "Queen's Pawn Opening"},
        {"d4 c5", "A43", "Old Benoni"},
        {"d4 Nf6 c4 e5", "A51", "Budapest Gambit"},
        {"d4 Nf6 c4 c5 d5 b5", "A57", "Benko (Volga) Gambit"},
        {"d4 Nf6 Bg5", "A45", "Trompowsky Attack"},
        {"d4 d5 Nc3", "D01", "Veresov Attack"},
        {"d4 Nf6 c4 e6 g3", "E00", "Catalan Opening"},
        {"d4 Nf6 c4 e6 Nc3 Bb4", "E20", "Nimzo-Indian Defence"},
        {"d4 Nf6 c4 e6 Nf3 b6", "E12", "Queen's Indian Defence"},
        {"d4 Nf6 c4 e6 Nf3 Bb4+", "E11", "Bogo-Indian Defence"},
        {"d4 Nf6 c4 g6 Nc3 d5", "D80", "Grünfeld Defence"},
        {"d4 Nf6 c4 g6 Nc3 Bg7 e4 d6", "E70", "King's Indian Defence"},
        {"d4 f5", "A80", "Dutch Defence"},

        // 1.e4
        {"e4", "B00", "King's Pawn Opening"},

        // 1.e4 c5 Sicilian
        {"e4 c5", "B20", "Sicilian Defence"},
        {"e4 c5 c3", "B22", "Alapin Variation"},
        {"e4 c5 d4", "B21", "Smith-Morra Gambit"},
        {"e4 c5 b4", "B20", "Wing Gambit"},
        {"e4 c5 Nc3", "B23", "Closed Sicilian"},
        {"e4 c5 Nf3 d6", "B50", "Sicilian Defence"},
        {"e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 a6",
            "B90", "Najdorf Defence"},
        {"e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 g6",
            "B70", "Dragon Variation"},
        {"e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 Nc6",
            "B56", "Classical Sicilian"},
        {"e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 e6",
            "B80", "Scheveningen Variation"},
        {"e4 c5 Nf3 Nc6 d4 cxd4 Nxd4 g6", "B34", "Accelerated Dragon"},
        {"e4 c5 Nf3 Nc6 d4 cxd4 Nxd4 Nf6 Nc3 e5",
            "B33", "Sveshnikov Variation"},
        {"e4 c5 Nf3 Nc6 d4 cxd4 Nxd4 e5", "B32", "Kalashnikov Variation"},
        {"e4 c5 Nf3 Nc6 Bb5", "B30", "Rossolimo Variation"},
        {"e4 c5 Nf3 e6", "B40", "Sicilian Defence"},

        // 1.e4 c6 Caro-Kann
        {"e4 c6", "B10", "Caro-Kann Defence"},
        {"e4 c6 d4 d5 e5", "B12", "Caro-Kann: Advance Variation"},
        {"e4 c6 d4 d5 exd5 cxd5", "B13", "Caro-Kann: Exchange Variation"},
        {"e4 c6 d4 d5 Nc3", "B15", "Caro-Kann: Classical Main Line"},
        {"e4 c6 d4 d5 Nc3 Bf5", "B18", "Caro-Kann: Classical Variation"},

        // 1.e4 d5 / d6 / g6 / Nf6 / Nc6
        {"e4 d5", "B01", "Scandinavian Defence"},
        {"e4 d6", "B07", "Pirc Defence"},
        {"e4 g6", "B06", "Modern Defence"},
        {"e4 Nf6", "B02", "Alekhine's Defence"},
        {"e4 Nf6 e5", "B03", "Alekhine's Defence"},
        {"e4 Nf6 e5 Nd5 d4 d6", "B04", "Alekhine: Four Pawns Attack"},
        {"e4 Nc6", "B00", "Nimzowitsch Defence"},
        {"e4 b6", "B00", "Owen Defence"},

        // 1.e4 e6 French
        {"e4 e6", "C00", "French Defence"},
        {"e4 e6 d4 d5 exd5", "C01", "French: Exchange Variation"},
        {"e4 e6 d4 d5 e5", "C02", "French: Advance Variation"},
        {"e4 e6 d4 d5 Nd2", "C03", "French: Tarrasch Variation"},
        {"e4 e6 d4 d5 Nc3 dxe4", "C10", "French: Rubinstein Variation"},
        {"e4 e6 d4 d5 Nc3 Nf6", "C11", "French: Classical Variation"},
        {"e4 e6 d4 d5 Nc3 Nf6 Bg5", "C12", "French: McCutcheon Variation"},
        {"e4 e6 d4 d5 Nc3 Nf6 Bg5 Bb4", "C15", "French: Winawer Variation"},

        // Open games
        {"e4 e5", "C20", "King's Pawn Game"},
        {"e4 e5 Qf3", "C20", "Napoleon Opening"},
        {"e4 e5 Nf3", "C40", "King's Knight Opening"},
        {"e4 e5 Nf3 d6", "C41", "Philidor Defence"},
        {"e4 e5 Nf3 Nf6", "C42", "Petrov's Defence"},
        {"e4 e5 Nf3 Nf6 Nxe5", "C42", "Petrov: Modern Attack"},
        {"e4 e5 Nf3 Nc6", "C44", "King's Knight Opening"},
        {"e4 e5 Nf3 Nc6 Nc3", "C46", "Three Knights Game"},
        {"e4 e5 Nf3 Nc6 Nc3 Nf6", "C47", "Four Knights Game"},
        {"e4 e5 Nf3 Nc6 Nc3 Nf6 Bb5", "C48", "Four Knights: Spanish"},
        {"e4 e5 Nf3 Nc6 d4", "C44", "King's Knight Opening"},
        {"e4 e5 Nf3 Nc6 d4 exd4", "C45", "Scotch Game"},
        {"e4 e5 Nf3 Nc6 d4 exd4 Bc4", "C44", "Ponziani Opening"},
        {"e4 e5 Nf3 Nc6 d4 exd4 Nxd4", "C45", "Scotch Game: Main Line"},
        {"e4 e5 Bc4", "C23", "Bishop's Opening"},
        {"e4 e5 Nc3", "C25", "Vienna Game"},

        // Ruy Lopez
        {"e4 e5 Nf3 Nc6 Bb5", "C60", "Ruy Lopez"},
        {"e4 e5 Nf3 Nc6 Bb5 Nf6", "C65", "Ruy Lopez: Berlin Defence"},
        {"e4 e5 Nf3 Nc6 Bb5 Nf6 O-O Nxe4",
            "C80", "Ruy Lopez: Open Variation"},
        {"e4 e5 Nf3 Nc6 Bb5 a6", "C70", "Ruy Lopez: Morphy Defence"},
        {"e4 e5 Nf3 Nc6 Bb5 a6 Bxc6", "C68",
            "Ruy Lopez: Exchange Variation"},
        {"e4 e5 Nf3 Nc6 Bb5 d6", "C62",
            "Ruy Lopez: Old Steinitz Defence"},
        {"e4 e5 Nf3 Nc6 Bb5 f5", "C63", "Ruy Lopez: Schliemann Defence"},

        // Italian complex
        {"e4 e5 Nf3 Nc6 Bc4", "C50", "Italian Game"},
        {"e4 e5 Nf3 Nc6 Bc4 Nf6", "C55", "Two Knights Defence"},
        {"e4 e5 Nf3 Nc6 Bc4 Bc5", "C53",
            "Italian Game: Classical Variation"},
        {"e4 e5 Nf3 Nc6 Bc4 Bc5 b4", "C51", "Evans Gambit"},

        // Gambits: centre / Danish / King's
        {"e4 e5 d4", "C21", "Centre Game"},
        {"e4 e5 d4 exd4", "C21", "Centre Game Accepted"},
        {"e4 e5 d4 exd4 c3", "C21", "Danish Gambit"},
        {"e4 e5 f4", "C30", "King's Gambit"},
        {"e4 e5 f4 d5", "C31", "Falkbeer Countergambit"},
        {"e4 e5 f4 exf4", "C33", "King's Gambit Accepted"},
        {"e4 e5 f4 exf4 Bc4", "C33", "King's Gambit: Bishop's Gambit"},
        {"e4 e5 f4 exf4 Nf3", "C34",
            "King's Gambit Accepted: Modern Variation"},

        // 1.d4 d5 Queen's-pawn mainlines
        {"d4 d5", "D00", "Closed Game (Queen's Pawn)"},
        {"d4 d5 e4", "D00", "Blackmar-Diemer Gambit"},
        {"d4 d5 c4", "D06", "Queen's Gambit"},
        {"d4 d5 c4 dxc4", "D20", "Queen's Gambit Accepted"},
        {"d4 d5 c4 e6", "D30", "Queen's Gambit Declined"},
        {"d4 d5 c4 e6 Nc3", "D37", "Queen's Gambit Declined"},
        {"d4 d5 c4 e6 Nc3 c5", "D32", "Tarrasch Defence"},
        {"d4 d5 c4 e6 Nc3 Nf6 Nf3", "D38",
            "Queen's Gambit Declined: Main Line"},
        {"d4 d5 c4 e6 Nc3 Nf6 Nf3 c6", "D43", "Semi-Slav Defence"},
        {"d4 d5 c4 c6", "D10", "Slav Defence"},
        {"d4 d5 c4 c6 Nf3", "D11", "Slav Defence"},
        {"d4 d5 c4 c6 Nf3 Nf6 Nc3", "D15", "Slav Defence: Main Line"},
        {"d4 d5 c4 c6 Nf3 Nf6 Nc3 dxc4", "D16",
            "Slav Defence: Main Line"},
        {"d4 d5 Bf4", "D02", "London System"},
        {"d4 d5 Nf3", "D02", "Queen's Pawn Game"},
        {"d4 d5 e3", "D00", "Queen's Pawn Game"},
    };

    for (const EcoEntry& entry : table) {
        const std::string entrySans(entry.sans);
        // Walk the entry's tokens; a miss disqualifies it immediately.
        std::size_t len = 0;
        std::size_t pos = 0;
        bool matched = true;
        while (true) {
            const std::size_t sp = entrySans.find(' ', pos);
            const std::string token = entrySans.substr(
                pos, sp == std::string::npos ? std::string::npos : sp - pos);
            if (token.empty()) { // between/after double spaces: stop cleanly
                break;
            }
            if (len == sans.size()
                || ecoToken(sans[len]) != ecoToken(token)) {
                matched = false;
                break;
            }
            ++len;
            if (sp == std::string::npos)
                break;
            pos = sp + 1;
        }
        if (matched && len > bestLen) {
            bestLen = len;
            best = Opening{entry.code, entry.name};
        }
    }
    return best;
}

} // namespace chess::client
