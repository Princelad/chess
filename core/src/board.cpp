#include <chess/board.h>

namespace chess {

std::string squareToString(Square sq)
{
    const char file = "abcdefgh"[sq & 7];
    const char rank = '1' + ((sq >> 4) & 7);
    return {file, rank};
}

Square stringToSquare(std::string_view name)
{
    if (name.size() != 2) {
        return SquareNone;
    }
    const char file = name[0];
    const char rank = name[1];
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
        return SquareNone;
    }
    return squareOf(file - 'a', rank - '1');
}

} // namespace chess
