#include <chess/uci_move.h>
#include <chess/movegen.h>

namespace chess {

std::optional<Move> fromUci(const Board& board, std::string_view uci)
{
    if (uci.size() < 4 || uci.size() > 5)
        return std::nullopt;

    Square from = stringToSquare(uci.substr(0, 2));
    Square to   = stringToSquare(uci.substr(2, 2));
    if (from == SquareNone || to == SquareNone)
        return std::nullopt;

    PieceType promoType = PieceType::None;
    if (uci.size() == 5) {
        switch (uci[4]) {
            case 'n': promoType = PieceType::Knight; break;
            case 'b': promoType = PieceType::Bishop; break;
            case 'r': promoType = PieceType::Rook;   break;
            case 'q': promoType = PieceType::Queen;  break;
            default:  return std::nullopt;
        }
    }

    auto legal = generateLegalMoves(board);
    for (const auto& m : legal) {
        if (m.from != from || m.to != to) continue;
        if (promoType == PieceType::None) {
            if (!m.isPromotion()) return m;
        } else {
            if (m.isPromotion() && m.promotion == promoType) return m;
        }
    }

    return std::nullopt;
}

} // namespace chess
