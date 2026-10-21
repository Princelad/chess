#pragma once

#include <chess/board.h>
#include <chess/move.h>
#include <chess/types.h>

#include <optional>
#include <utility>

namespace chess::client {

// One queued premove (11.4): raw from/to squares clicked while the
// opponent is thinking, sent the instant the position catches up. The
// square data is enough — capture/promotion ambiguity resolves against
// the board at flush time — and a promotion frame pre-move auto-queens
// (the do-it-now picker cannot exist for a position that is still hypothetical).
struct Premove {
    int fromFile = 0;
    int fromRank = 0;
    int toFile = 0;
    int toRank = 0;
};

// Resolves a queued premove against the live position: the unique legal
// move matching from/to. Semantics fall out of legality:
//   * premove "captures" a square that turns out empty (except via en
//     passant/castling frames) -> no match, dropped;
//   * premove lands on an own piece -> no match, dropped;
//   * the move became pinned/blocked -> no match, dropped;
//   * king-by-two pattern -> resolves to the actual castling move;
//   * target is the promotion last rank -> the matching promotion, always
//     the queen (see Premove above).
inline std::optional<chess::Move> resolvePremove(const chess::Board& board,
                                                 const Premove& premove)
{
    const chess::Square from =
        chess::squareOf(premove.fromFile, premove.fromRank);
    const chess::Square to = chess::squareOf(premove.toFile, premove.toRank);
    if (from == to)
        return std::nullopt;

    std::optional<chess::Move> promotionMatch;
    for (const auto& m : chess::generateLegalMoves(board)) {
        if (m.from != from || m.to != to)
            continue;
        if (!m.isPromotion())
            return m; // quiet/capture/castle/en-passant: unique frame
        if (!promotionMatch.has_value()
            || m.promotion == chess::PieceType::Queen)
            promotionMatch = m;
    }
    // Without a queen candidate (never happens for a legal promotion
    // frame) the first matching promotion stands in.
    return promotionMatch;
}

} // namespace chess::client
