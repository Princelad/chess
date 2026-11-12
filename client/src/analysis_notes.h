#pragma once

#include "board_interaction.h"

#include <string>
#include <vector>

namespace chess::client::notes {

// --- 16.4 per-ply annotations, persisted client-local ------------------------
// Config keys are "analysis.<hash>.<ply>"; Config splits on the FIRST dot so
// the two-dot key round-trips (section "analysis", key "<hash>.<ply>").
// The hash is the game's content identity (16 hex digits of the FNV-1a over
// start FEN + SAN chain — the eval cache's hash family), so archive replays
// and re-opened local games land on the same notes.

// Stable identity string for a game's chain.
std::string gameHash(const std::string& startFen,
                     const std::vector<std::string>& sans);

// One ply's annotations as a config value: space-separated tokens,
//   c<file><rank>                          circle on that square
//   a<fromFile><fromRank><toFile><toRank>  arrow
// Empty string = no annotations. decodeAnnotations skips malformed tokens
// (best effort — a hand-edited config line must never throw).
std::string encodeAnnotations(const BoardAnnotations& notes);
void decodeAnnotations(const std::string& value, BoardAnnotations& notes);

} // namespace chess::client::notes
