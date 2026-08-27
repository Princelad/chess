#include <gtest/gtest.h>

#include <chess/fen.h>
#include <chess/pgn.h>
#include <chess/san.h>

#include <string>

using chess::Board;
using chess::Move;
using chess::PgnGame;
using chess::squareOf;

TEST(PgnTest, ParseHeaders)
{
    std::string pgn = R"([Event "Test Match"]
[Site "Test City"]
[Date "2024.01.01"]
[Round "1"]
[White "Alice"]
[Black "Bob"]
[Result "1-0"]

1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 1-0
)";

    auto game = chess::parsePgn(pgn);
    ASSERT_TRUE(game.has_value());
    EXPECT_EQ(game->headers.size(), 7u);
    EXPECT_EQ(game->headers[0].first, "Event");
    EXPECT_EQ(game->headers[0].second, "Test Match");
    EXPECT_EQ(game->headers[4].first, "White");
    EXPECT_EQ(game->headers[4].second, "Alice");
}

TEST(PgnTest, ParseMovetext)
{
    std::string pgn = R"([Event "Test"]
[Result "1-0"]

1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 1-0
)";

    auto game = chess::parsePgn(pgn);
    ASSERT_TRUE(game.has_value());
    EXPECT_EQ(game->result, "1-0");
    EXPECT_EQ(game->moves.size(), 6u);
}

TEST(PgnTest, ParseSkipsComments)
{
    std::string pgn = R"([Event "Test"]
[Result "1-0"]

1. e4 {this is a comment} e5 2. Nf3 Nc6 1-0
)";

    auto game = chess::parsePgn(pgn);
    ASSERT_TRUE(game.has_value());
    EXPECT_EQ(game->moves.size(), 4u);
}

TEST(PgnTest, ParseSkipsVariations)
{
    std::string pgn = R"([Event "Test"]
[Result "1-0"]

1. e4 (1. d4 d5) e5 2. Nf3 Nc6 1-0
)";

    auto game = chess::parsePgn(pgn);
    ASSERT_TRUE(game.has_value());
    EXPECT_EQ(game->moves.size(), 4u);
}

TEST(PgnTest, ParseSkipsNAGs)
{
    std::string pgn = R"([Event "Test"]
[Result "1-0"]

1. e4 $1 e5 $2 2. Nf3 Nc6 1-0
)";

    auto game = chess::parsePgn(pgn);
    ASSERT_TRUE(game.has_value());
    EXPECT_EQ(game->moves.size(), 4u);
}

TEST(PgnTest, ParseStartingPosition)
{
    std::string pgn = R"([Event "Test"]
[FEN "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"]
[SetUp "1"]
[Result "1/2-1/2"]

1... e5 2. Nf3 Nc6 1/2-1/2
)";

    auto game = chess::parsePgn(pgn);
    ASSERT_TRUE(game.has_value());
    EXPECT_EQ(game->startPosition, "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    EXPECT_EQ(game->moves.size(), 3u);
    EXPECT_EQ(game->result, "1/2-1/2");
}

TEST(PgnTest, ParseInvalidPgn)
{
    std::string pgn = R"([Event "Test"]
[Result "1-0"]

1. e4 e5 2. Zzz 1-0
)";

    auto game = chess::parsePgn(pgn);
    EXPECT_FALSE(game.has_value());
}

TEST(PgnTest, ParseIncompleteTag)
{
    std::string pgn = "[Event \"Test\"]\n[Result\n";
    auto game = chess::parsePgn(pgn);
    EXPECT_FALSE(game.has_value());
}

TEST(PgnTest, ParseNoResult)
{
    std::string pgn = R"([Event "Test"]
[Result "*"]

1. e4 e5 2. Nf3 *
)";

    auto game = chess::parsePgn(pgn);
    ASSERT_TRUE(game.has_value());
    EXPECT_EQ(game->result, "*");
    EXPECT_EQ(game->moves.size(), 3u);
}

TEST(PgnTest, ToPgnBasic)
{
    std::vector<std::pair<std::string, std::string>> headers = {
        {"Event", "Test"},
        {"Site", "Online"},
        {"Date", "2024.01.01"},
        {"Round", "1"},
        {"White", "Alice"},
        {"Black", "Bob"},
        {"Result", "1-0"}
    };
    std::vector<std::string> moves = {"e4", "e5", "Nf3", "Nc6", "Bb5", "a6"};

    std::string pgn = chess::toPgn(headers, moves, "1-0");

    EXPECT_NE(pgn.find("[Event \"Test\"]"), std::string::npos);
    EXPECT_NE(pgn.find("[White \"Alice\"]"), std::string::npos);
    EXPECT_NE(pgn.find("1. e4 e5"), std::string::npos);
    EXPECT_NE(pgn.find("2. Nf3 Nc6"), std::string::npos);
    EXPECT_NE(pgn.find("3. Bb5 a6 1-0"), std::string::npos);
}

TEST(PgnTest, ToPgnLineWrap)
{
    std::vector<std::pair<std::string, std::string>> headers = {
        {"Event", "Test"},
        {"Result", "1-0"}
    };
    std::vector<std::string> moves;
    for (int i = 0; i < 40; ++i) {
        moves.push_back("e4");
        moves.push_back("e5");
    }

    std::string pgn = chess::toPgn(headers, moves, "1-0");
    bool hasNewline = false;
    for (char c : pgn) {
        if (c == '\n') { hasNewline = true; break; }
    }
    EXPECT_TRUE(hasNewline);

    std::istringstream iss(pgn);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.front() != '[')
            EXPECT_LE(line.size(), 81u);
    }
}

TEST(PgnTest, RoundTrip)
{
    std::string original = R"([Event "Test"]
[Site "Online"]
[Date "2024.01.01"]
[Round "1"]
[White "Alice"]
[Black "Bob"]
[Result "1-0"]

1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 1-0
)";

    auto game1 = chess::parsePgn(original);
    ASSERT_TRUE(game1.has_value());

    std::vector<std::string> sanMoves;
    for (const auto& move : game1->moves) {
        Board board = game1->startPosition.empty()
            ? Board::fromStartPos()
            : *Board::fromFen(game1->startPosition);
        for (std::size_t i = 0; i < game1->moves.size(); ++i) {
            if (i == (&move - &game1->moves[0])) {
                sanMoves.push_back(chess::san::toSan(board, move));
                break;
            }
            board.makeMove(game1->moves[i]);
        }
    }

    std::string exported = chess::toPgn(game1->headers, sanMoves, game1->result);
    auto game2 = chess::parsePgn(exported);
    ASSERT_TRUE(game2.has_value());
    EXPECT_EQ(game2->moves.size(), game1->moves.size());
    EXPECT_EQ(game2->result, game1->result);

    for (std::size_t i = 0; i < game1->moves.size(); ++i) {
        EXPECT_EQ(game1->moves[i].from, game2->moves[i].from);
        EXPECT_EQ(game1->moves[i].to, game2->moves[i].to);
    }
}

TEST(PgnTest, ReplayMoves)
{
    std::string pgn = R"([Event "Test"]
[Result "1-0"]

1. e4 e5 2. Nf3 1-0
)";

    auto game = chess::parsePgn(pgn);
    ASSERT_TRUE(game.has_value());

    auto board = chess::replayMoves(game->moves);
    ASSERT_TRUE(board.has_value());

    Board expected = Board::fromStartPos();
    for (const auto& move : game->moves)
        expected.makeMove(move);

    EXPECT_EQ(board->toFen(), expected.toFen());
}

TEST(PgnTest, ReplayMovesFromFen)
{
    std::string fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1";
    std::string pgn = R"([Event "Test"]
[FEN ")" + fen + R"("]
[SetUp "1"]
[Result "*"]

1... e5 2. Nf3 *
)";

    auto game = chess::parsePgn(pgn);
    ASSERT_TRUE(game.has_value());

    auto board = chess::replayMoves(game->moves, game->startPosition);
    ASSERT_TRUE(board.has_value());

    Board expected = *Board::fromFen(fen);
    for (const auto& move : game->moves)
        expected.makeMove(move);

    EXPECT_EQ(board->toFen(), expected.toFen());
}

TEST(PgnTest, ReplayMovesInvalid)
{
    std::string pgn = R"([Event "Test"]
[Result "1-0"]

1. e4 1-0
)";

    auto game = chess::parsePgn(pgn);
    ASSERT_TRUE(game.has_value());

    std::vector<chess::Move> badMoves = {game->moves[0]};
    Board board = Board::fromStartPos();
    board.makeMove(badMoves[0]);

    chess::Move fake;
    fake.from = chess::stringToSquare("d2");
    fake.to = chess::stringToSquare("d4");
    badMoves.push_back(fake);

    auto result = chess::replayMoves(badMoves);
    EXPECT_FALSE(result.has_value());
}

TEST(PgnTest, ParseEmpty)
{
    auto game = chess::parsePgn("");
    ASSERT_TRUE(game.has_value());
    EXPECT_TRUE(game->moves.empty());
    EXPECT_TRUE(game->result.empty());
}

TEST(PgnTest, ParseMoveNumbersOnly)
{
    std::string pgn = R"([Event "Test"]
[Result "1-0"]

1. e4 e5
2. Nf3 Nc6
3. Bb5 a6
1-0
)";

    auto game = chess::parsePgn(pgn);
    ASSERT_TRUE(game.has_value());
    EXPECT_EQ(game->moves.size(), 6u);
}

TEST(PgnTest, ToPgnExtraHeaders)
{
    std::vector<std::pair<std::string, std::string>> headers = {
        {"Event", "Test"},
        {"ECO", "C65"},
        {"Result", "1-0"}
    };
    std::vector<std::string> moves = {"e4", "e5"};

    std::string pgn = chess::toPgn(headers, moves, "1-0");
    EXPECT_NE(pgn.find("[ECO \"C65\"]"), std::string::npos);
    auto ecoPos = pgn.find("[ECO");
    auto resultPos = pgn.find("[Result");
    EXPECT_GT(ecoPos, resultPos);
}
