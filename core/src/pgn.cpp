#include <chess/pgn.h>

#include <algorithm>
#include <sstream>

#include <chess/fen.h>
#include <chess/movegen.h>
#include <chess/san.h>

namespace chess {

namespace {

constexpr std::size_t LineWidth = 80;

bool isResultToken(const std::string& token)
{
    return token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*";
}

std::string stripResult(std::string_view movetext)
{
    std::string result = std::string(movetext);

    const std::vector<std::string_view> results = {"1/2-1/2", "1-0", "0-1", "*"};
    for (auto r : results) {
        auto pos = result.rfind(std::string(r));
        if (pos != std::string::npos) {
            result.erase(pos, r.size());
            break;
        }
    }

    return result;
}

std::string cleanMovetext(std::string_view movetext)
{
    std::string result;
    result.reserve(movetext.size());

    int commentDepth = 0;
    int variationDepth = 0;
    std::size_t i = 0;

    while (i < movetext.size()) {
        char c = movetext[i];

        if (c == '{') {
            ++commentDepth;
            ++i;
            continue;
        }
        if (c == '}') {
            if (commentDepth > 0) --commentDepth;
            ++i;
            continue;
        }
        if (commentDepth > 0) {
            ++i;
            continue;
        }

        if (c == '(') {
            ++variationDepth;
            ++i;
            continue;
        }
        if (c == ')') {
            if (variationDepth > 0) --variationDepth;
            ++i;
            continue;
        }
        if (variationDepth > 0) {
            ++i;
            continue;
        }

        if (c == '$') {
            ++i;
            while (i < movetext.size() && std::isdigit(static_cast<unsigned char>(movetext[i])))
                ++i;
            continue;
        }

        result += c;
        ++i;
    }

    return result;
}

std::vector<std::string> tokenize(const std::string& text)
{
    std::vector<std::string> tokens;
    std::istringstream iss(text);
    std::string token;
    while (iss >> token) {
        if (isResultToken(token))
            continue;

        bool isMoveNumber = true;
        std::size_t i = 0;
        while (i < token.size() && std::isdigit(static_cast<unsigned char>(token[i])))
            ++i;
        if (i == 0 || i >= token.size()) isMoveNumber = false;
        while (i < token.size() && token[i] == '.')
            ++i;
        if (i < token.size()) isMoveNumber = false;

        if (!isMoveNumber)
            tokens.push_back(token);
    }
    return tokens;
}

std::string emitHeaders(const std::vector<std::pair<std::string, std::string>>& headers)
{
    std::ostringstream out;

    static const char* order[] = {
        "Event", "Site", "Date", "Round", "White", "Black", "Result"
    };

    for (const char* key : order) {
        for (const auto& [k, v] : headers) {
            if (k == key) {
                out << "[" << k << " \"" << v << "\"]\n";
                break;
            }
        }
    }

    for (const auto& [k, v] : headers) {
        bool isStandard = false;
        for (const char* s : order) {
            if (k == s) { isStandard = true; break; }
        }
        if (!isStandard)
            out << "[" << k << " \"" << v << "\"]\n";
    }

    return out.str();
}

} // namespace

std::optional<PgnGame> parsePgn(std::string_view pgn)
{
    PgnGame game;
    std::string_view remaining = pgn;

    while (!remaining.empty()) {
        auto lineEnd = remaining.find('\n');
        std::string_view line = (lineEnd == std::string_view::npos)
            ? remaining
            : remaining.substr(0, lineEnd);

        auto trimStart = line.find_first_not_of(" \t\r");
        if (trimStart != std::string_view::npos)
            line = line.substr(trimStart);

        if (!line.empty() && line.front() == '[') {
            auto closeBracket = line.find(']');
            if (closeBracket == std::string_view::npos)
                return std::nullopt;

            std::string_view tagContent = line.substr(1, closeBracket - 1);
            auto spacePos = tagContent.find(' ');
            if (spacePos == std::string_view::npos)
                return std::nullopt;

            std::string key = std::string(tagContent.substr(0, spacePos));
            std::string_view valuePart = tagContent.substr(spacePos + 1);

            if (valuePart.size() < 2 || valuePart.front() != '"' || valuePart.back() != '"')
                return std::nullopt;

            std::string value = std::string(valuePart.substr(1, valuePart.size() - 2));
            game.headers.emplace_back(std::move(key), std::move(value));
        } else {
            break;
        }

        if (lineEnd != std::string_view::npos)
            remaining = remaining.substr(lineEnd + 1);
        else
            break;
    }

    std::string movetext(remaining);

    auto resultPos = movetext.rfind("1/2-1/2");
    if (resultPos == std::string_view::npos) {
        resultPos = movetext.rfind("1-0");
        if (resultPos == std::string_view::npos) {
            resultPos = movetext.rfind("0-1");
            if (resultPos == std::string_view::npos)
                resultPos = movetext.rfind('*');
        }
    }

    if (resultPos != std::string_view::npos) {
        if (movetext[resultPos] == '1' && resultPos + 3 <= movetext.size()
            && movetext.substr(resultPos, 3) == "1/2") {
            game.result = "1/2-1/2";
            movetext.erase(resultPos, 7);
        } else if (movetext[resultPos] == '1' && resultPos + 3 <= movetext.size()
                   && movetext.substr(resultPos, 3) == "1-0") {
            game.result = "1-0";
            movetext.erase(resultPos, 3);
        } else if (movetext[resultPos] == '0' && resultPos + 3 <= movetext.size()
                   && movetext.substr(resultPos, 3) == "0-1") {
            game.result = "0-1";
            movetext.erase(resultPos, 3);
        } else if (movetext[resultPos] == '*') {
            game.result = "*";
            movetext.erase(resultPos, 1);
        }
    }

    std::string cleaned = cleanMovetext(movetext);
    auto sanTokens = tokenize(cleaned);

    for (const auto& [k, v] : game.headers) {
        if (k == "FEN" && v != "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {
            game.startPosition = v;
            break;
        }
    }

    Board board = game.startPosition.empty()
        ? Board::fromStartPos()
        : *Board::fromFen(game.startPosition);

    for (const auto& san : sanTokens) {
        auto move = san::fromSan(board, san);
        if (!move.has_value())
            return std::nullopt;

        game.moves.push_back(*move);
        board.makeMove(*move);
    }

    return game;
}

std::string toPgn(const std::vector<std::pair<std::string, std::string>>& headers,
                  const std::vector<std::string>& SANmoves,
                  std::string_view result,
                  std::string_view startPosition)
{
    std::string output;

    std::vector<std::pair<std::string, std::string>> hdrs = headers;

    if (!startPosition.empty() && startPosition != "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {
        bool hasFen = false;
        for (auto& [k, v] : hdrs) {
            if (k == "FEN") { v = std::string(startPosition); hasFen = true; break; }
        }
        if (!hasFen)
            hdrs.emplace(hdrs.begin(), "FEN", std::string(startPosition));

        bool hasSetup = false;
        for (auto& [k, v] : hdrs) {
            if (k == "SetUp") { v = "1"; hasSetup = true; break; }
        }
        if (!hasSetup)
            hdrs.emplace(hdrs.begin() + (hasFen ? 1 : 0), "SetUp", "1");
    }

    output += emitHeaders(hdrs);
    output += "\n";

    std::ostringstream moveText;
    int moveNum = 1;

    for (std::size_t i = 0; i < SANmoves.size(); ++i) {
        if (i % 2 == 0) {
            moveText << moveNum << ". ";
            ++moveNum;
        }
        moveText << SANmoves[i];

        if (i + 1 < SANmoves.size())
            moveText << " ";
    }

    if (!result.empty()) {
        if (!SANmoves.empty())
            moveText << " ";
        moveText << result;
    }

    std::string raw = moveText.str();
    std::string wrapped;
    std::size_t col = 0;
    std::size_t i = 0;

    while (i < raw.size()) {
        if (raw[i] == '\n') {
            wrapped += '\n';
            col = 0;
            ++i;
            continue;
        }

        if (raw[i] == ' ') {
            std::size_t wordEnd = raw.find_first_not_of(' ', i);
            if (wordEnd == std::string::npos) wordEnd = raw.size();
            std::size_t wordLen = wordEnd - i;
            if (col + wordLen > LineWidth && col > 0) {
                wrapped += '\n';
                col = 0;
            }
            while (i < wordEnd) {
                wrapped += raw[i];
                ++col;
                ++i;
            }
            continue;
        }

        std::size_t wordStart = i;
        while (i < raw.size() && raw[i] != ' ' && raw[i] != '\n')
            ++i;
        std::size_t wordLen = i - wordStart;
        if (col + wordLen > LineWidth && col > 0) {
            wrapped += '\n';
            col = 0;
        }
        for (std::size_t j = wordStart; j < i; ++j) {
            wrapped += raw[j];
            ++col;
        }
    }

    output += wrapped;
    if (!wrapped.empty() && wrapped.back() != '\n')
        output += "\n";

    return output;
}

std::optional<Board> replayMoves(const std::vector<Move>& moves,
                                 std::string_view startPosition)
{
    Board board = startPosition.empty()
        ? Board::fromStartPos()
        : *Board::fromFen(startPosition);

    for (const auto& move : moves) {
        auto legal = generateLegalMoves(board);
        bool found = false;
        for (const auto& m : legal) {
            if (m.from == move.from && m.to == move.to
                && m.promotion == move.promotion) {
                found = true;
                break;
            }
        }
        if (!found)
            return std::nullopt;
        board.makeMove(move);
    }

    return board;
}

} // namespace chess
