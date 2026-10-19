#include "archive_pgn.h"

#include <gtest/gtest.h>

namespace chess::client {
namespace {

chess::net::ArchiveGameMsg sampleGame()
{
    chess::net::ArchiveGameMsg game;
    game.id = 5;
    game.whiteName = "Ada";
    game.blackName = "Bob";
    game.result = int(chess::net::GameResult::BlackWins);
    game.reason = int(chess::net::GameOverReason::Checkmate);
    game.timeControl = "5+3";
    game.startFen = ""; // standard start position
    game.sanMoves = "e4 e5 Nf3 Nc6 Bb5 a6";
    game.playedAt = 1729000000; // 2024-10-15 13:46 UTC
    game.rated = true;
    return game;
}

TEST(ArchivePgn, ResultTokensAndTextCoverAllOutcomes)
{
    EXPECT_STREQ(archiveResultToken(int(chess::net::GameResult::WhiteWins)), "1-0");
    EXPECT_STREQ(archiveResultToken(int(chess::net::GameResult::BlackWins)), "0-1");
    EXPECT_STREQ(archiveResultToken(int(chess::net::GameResult::Draw)), "1/2-1/2");
    EXPECT_STREQ(archiveResultToken(int(chess::net::GameResult::Abort)), "*");
    EXPECT_STREQ(archiveResultText(int(chess::net::GameResult::WhiteWins)), "White wins");
    EXPECT_STREQ(archiveResultText(int(chess::net::GameResult::Draw)), "Draw");
}

TEST(ArchivePgn, SplitsSanMoves)
{
    auto parts = archivePgnParts(sampleGame());
    EXPECT_EQ(parts.sans.size(), 6u);
    EXPECT_EQ(parts.sans[0], "e4");
    EXPECT_EQ(parts.sans[5], "a6");
}

TEST(ArchivePgn, HeadersCarryPlayersAndOutcome)
{
    auto parts = archivePgnParts(sampleGame());
    bool hasWhite = false, hasBlack = false, hasResult = false, hasEvent = false;
    for (const auto& [key, value] : parts.headers) {
        hasWhite |= key == "White" && value == "Ada";
        hasBlack |= key == "Black" && value == "Bob";
        hasResult |= key == "Result" && value == "0-1";
        hasEvent |= key == "Event" && value == "Rated game";
    }
    EXPECT_TRUE(hasWhite);
    EXPECT_TRUE(hasBlack);
    EXPECT_TRUE(hasResult);
    EXPECT_TRUE(hasEvent);
}

TEST(ArchivePgn, UnratedAndFenHeaders)
{
    auto game = sampleGame();
    game.rated = false;
    game.startFen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPP1PPPP/RNBQKBNR b KQkq - 0 1";
    game.timeControl.clear();
    auto parts = archivePgnParts(game);
    bool hasFen = false, hasSetUp = false, hasTimeControl = false, casual = false;
    for (const auto& [key, value] : parts.headers) {
        hasFen |= key == "FEN" && value.rfind("rnbqkbnr", 0) == 0;
        hasSetUp |= key == "SetUp" && value == "1";
        hasTimeControl |= key == "TimeControl";
        casual |= key == "Event" && value == "Casual game";
    }
    EXPECT_TRUE(hasFen);
    EXPECT_TRUE(hasSetUp);
    EXPECT_FALSE(hasTimeControl);
    EXPECT_TRUE(casual);
}

TEST(ArchivePgn, DateHeaderIsPgnFormat)
{
    EXPECT_EQ(archiveDateHeader(1729000000), "2024.10.15");
    EXPECT_EQ(archiveDateHeader(0), "1970.01.01");
}

TEST(ArchivePgn, GeneratedPgnIsWellFormed)
{
    auto game = sampleGame();
    const std::string pgn = gameToPgn(game);
    EXPECT_NE(pgn.find("[White \"Ada\"]"), std::string::npos);
    EXPECT_NE(pgn.find("[Black \"Bob\"]"), std::string::npos);
    EXPECT_NE(pgn.find("[Date \"2024.10.15\"]"), std::string::npos);
    EXPECT_NE(pgn.find("[Result \"0-1\"]"), std::string::npos);
    // Move text reuses the standard writer: numbered SAN pairs, result token.
    EXPECT_NE(pgn.find("1. e4 e5"), std::string::npos);
    EXPECT_NE(pgn.find("3. Bb5 a6"), std::string::npos);
    EXPECT_NE(pgn.find("0-1"), std::string::npos);
    // Parsed back: SAN list round-trips through the reader.
    auto parsed = chess::parsePgn(pgn);
    ASSERT_TRUE(parsed.has_value());
    ASSERT_EQ(parsed->moves.size(), 6u);
    EXPECT_EQ(parsed->result, "0-1");
}

TEST(ArchivePgn, EmptyMovesProduceResultOnlyGame)
{
    auto game = sampleGame();
    game.sanMoves = "";
    const std::string pgn = gameToPgn(game);
    auto parsed = chess::parsePgn(pgn);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(parsed->moves.empty());
    EXPECT_EQ(parsed->result, "0-1");
}

} // namespace
} // namespace chess::client
