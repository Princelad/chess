// End-to-end tests for 12.1 Chess960: a real server over TCP. The variant
// tag travels in JoinMsg, the rolled Start Position in both WelcomeMsgs,
// variant mismatches never pair, a bot plays a 960 game, the archive row
// carries the SP startFen, and a rematch keeps the variant but re-rolls.
#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <SFML/Network/TcpSocket.hpp>

#include <chess/board.h>
#include <chess/fen.h>
#include <chess/movegen.h>
#include <chess/net/messages.h>
#include <chess/net/protocol.h>
#include <chess/san.h>

#include "harness.h"
#include "server.h"

using namespace std::chrono_literals;

namespace {

// First legal SAN for the side to move in `fen`, verified round-trip
// through fromSan so the test uses the same spelling the server accepts.
std::string firstLegalSan(const std::string& fen)
{
    auto board = chess::fromFen(fen);
    if (!board)
        return {};
    for (const auto& move : chess::generateLegalMoves(*board)) {
        std::string san = chess::san::toSan(*board, move);
        if (chess::san::fromSan(*board, san).has_value())
            return san;
    }
    return {};
}

// True when the FEN's first (back) rank is a legal Chess960 arrangement:
// both bishops on opposite square colors, two rooks with the single king
// between them. Pawns/other ranks are not re-checked — the generator owns
// the full layout, this guards the invariants the rules rely on.
bool isValidChess960BackRank(const std::string& fen)
{
    // The FEN's first rank ends at the first '/' separator.
    const auto slash = fen.find('/');
    if (slash == std::string::npos)
        return false;
    const std::string rank = fen.substr(0, slash);

    std::vector<int> bishops, rooks, kings;  // file indices of a..h
    int file = 0;
    for (char c : rank) {
        // Rank 1 (FEN's first) is the black back rank — lowercase letters;
        // accept both cases.
        if (c == 'B' || c == 'b')
            bishops.push_back(file);
        else if (c == 'R' || c == 'r')
            rooks.push_back(file);
        else if (c == 'K' || c == 'k')
            kings.push_back(file);
        if (c != '/')
            ++file;
    }
    if (file != 8 || bishops.size() != 2 || rooks.size() != 2
        || kings.size() != 1)
        return false;

    // Rank 8 squares: (file + 7) % 2 — opposite parities is the constraint.
    if ((bishops[0] + bishops[1]) % 2 == 0)
        return false;

    return rooks[0] < kings[0] && kings[0] < rooks[1];
}

} // namespace

// ── Welcome carries the variant and a rolled Start Position ─────────────────

class Chess960E2eTest : public chess::e2e::E2eHarness {};

TEST_F(Chess960E2eTest, WelcomeCarriesVariantAndIdenticalStartFen)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();

    // Both joins are sent before awaiting: pairing waits for a variant twin.
    sendMsg(white, chess::net::JoinMsg{"Agata", "", "chess960"});
    sendMsg(black, chess::net::JoinMsg{"Bruno", "", "chess960"});
    auto wWelcome = await<chess::net::WelcomeMsg>(white);
    auto bWelcome = await<chess::net::WelcomeMsg>(black);
    ASSERT_TRUE(wWelcome.has_value());
    ASSERT_TRUE(bWelcome.has_value());

    EXPECT_EQ(wWelcome->variant, "chess960");
    EXPECT_EQ(bWelcome->variant, "chess960");
    EXPECT_EQ(wWelcome->startFen, bWelcome->startFen)
        << "both seats must see the same rolled position";

    EXPECT_FALSE(wWelcome->startFen.empty());
    EXPECT_TRUE(isValidChess960BackRank(wWelcome->startFen))
        << wWelcome->startFen
        << " must satisfy the Chess960 back-rank constraints";
    // The client only ever rebuilds the board from this FEN; it must parse.
    EXPECT_TRUE(chess::fromFen(wWelcome->startFen).has_value());
}

TEST_F(Chess960E2eTest, VariantMismatchDoesNotPair)
{
    sf::TcpSocket standard = connectClient();
    sf::TcpSocket nineSixty = connectClient();

    sendMsg(standard, chess::net::JoinMsg{"StandardStan"});
    sendMsg(nineSixty, chess::net::JoinMsg{"Nine", "", "chess960"});

    // The pairing predicate is (timeControl, variant) equality: neither side
    // must see a welcome while the only candidates disagree on variant.
    auto none = await<chess::net::WelcomeMsg>(standard, 400ms);
    EXPECT_FALSE(none.has_value()) << "standard must not pair a 960 player";
    auto noneAgain = await<chess::net::WelcomeMsg>(nineSixty, 100ms);
    EXPECT_FALSE(noneAgain.has_value());

    // A matching variant still pairs later in the queue.
    auto partner = connectClient();
    auto joined = joinVariant(partner, "Nine2", "chess960");
    ASSERT_TRUE(joined.has_value()) << "960 + 960 pairs";
    EXPECT_EQ(joined->opponent, "Nine");

    standard.disconnect();
    nineSixty.disconnect();
    partner.disconnect();
}

TEST_F(Chess960E2eTest, CasedVariantTagDegradesToStandard)
{
    // The server sanitizes the Join payload: only the exact "chess960" tag
    // selects the SP game — a casing variation is the standard game, so its
    // joiner pairs a plain guest.
    sf::TcpSocket cased = connectClient();
    sf::TcpSocket plain = connectClient();

    sendMsg(cased, chess::net::JoinMsg{"Casey", "", "Chess960"});
    // Unpaired so far in the queue — no welcome at all.
    auto casedAlone = await<chess::net::WelcomeMsg>(cased, 400ms);
    EXPECT_FALSE(casedAlone.has_value()) << "still queued, nothing to pair";

    sendMsg(plain, chess::net::JoinMsg{"Plain"});
    auto casedWelcome = await<chess::net::WelcomeMsg>(cased);
    auto plainWelcome = await<chess::net::WelcomeMsg>(plain);
    ASSERT_TRUE(casedWelcome.has_value());
    ASSERT_TRUE(plainWelcome.has_value());
    EXPECT_EQ(plainWelcome->opponent, "Casey");
    // The degraded join plays the standard game: no variant, no SP.
    EXPECT_EQ(casedWelcome->variant, "");
    EXPECT_EQ(casedWelcome->startFen, "");
    EXPECT_EQ(plainWelcome->variant, "");
    EXPECT_EQ(plainWelcome->startFen, "");

    cased.disconnect();
    plain.disconnect();
}

// ── Live 960 game: legal moves on the rolled position ───────────────────────

TEST_F(Chess960E2eTest, LegalMovesPlayOnTheRolledPosition)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();

    sendMsg(white, chess::net::JoinMsg{"Agata", "", "chess960"});
    sendMsg(black, chess::net::JoinMsg{"Bruno", "", "chess960"});
    auto wWelcome = await<chess::net::WelcomeMsg>(white);
    auto bWelcome = await<chess::net::WelcomeMsg>(black);
    ASSERT_TRUE(wWelcome.has_value());
    ASSERT_TRUE(bWelcome.has_value());

    // Mirror the played chain locally: the SANs the server echoes must be
    // legal when replayed from the welcome's start FEN.
    chess::Board board = *chess::fromFen(wWelcome->startFen);

    for (int i = 0; i < 2; ++i) {
        auto& sender = i == 0 ? white : black;
        std::string san = firstLegalSan(board.toFen());
        ASSERT_FALSE(san.empty());

        sendMsg(sender, chess::net::MoveMsg{san});
        auto wEcho = await<chess::net::ServerMoveMsg>(white);
        auto bEcho = await<chess::net::ServerMoveMsg>(black);
        ASSERT_TRUE(wEcho.has_value());
        ASSERT_TRUE(bEcho.has_value());
        ASSERT_EQ(wEcho->san, san);
        ASSERT_EQ(bEcho->san, san);

        auto applied = chess::san::fromSan(board, san);
        ASSERT_TRUE(applied.has_value());
        board.makeMove(*applied);
    }
}

// ── Archive: the row remembers the Start Position ───────────────────────────

class Chess960ArchiveE2eTest : public chess::e2e::E2eHarness {
protected:
    void beforeStart() override
    {
        dbPath_ = std::filesystem::temp_directory_path()
            / ("chess_chess960_e2e_" + std::to_string(++dbCounter) + ".db");
        std::filesystem::remove(dbPath_);
        std::filesystem::remove(dbPath_.string() + "-wal");
        std::filesystem::remove(dbPath_.string() + "-shm");
    }

    void configure(chess::server::ServerConfig& config) override
    {
        config.dbPath = dbPath_.string();
    }

    void afterStop() override
    {
        std::filesystem::remove(dbPath_);
        std::filesystem::remove(dbPath_.string() + "-wal");
        std::filesystem::remove(dbPath_.string() + "-shm");
    }

    std::filesystem::path dbPath_;
    static std::atomic<int> dbCounter;
};

std::atomic<int> Chess960ArchiveE2eTest::dbCounter{0};

TEST_F(Chess960ArchiveE2eTest, ArchiveRowCarriesStartFen)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();

    sendMsg(white, chess::net::JoinMsg{"Agata", "", "chess960"});
    sendMsg(black, chess::net::JoinMsg{"Bruno", "", "chess960"});
    auto wWelcome = await<chess::net::WelcomeMsg>(white);
    auto bWelcome = await<chess::net::WelcomeMsg>(black);
    ASSERT_TRUE(wWelcome.has_value());
    ASSERT_TRUE(bWelcome.has_value());

    // One ply so the game is archived (zero-move aborts are not).
    std::string san = firstLegalSan(wWelcome->startFen);
    ASSERT_FALSE(san.empty());
    sendMsg(white, chess::net::MoveMsg{san});
    ASSERT_TRUE(await<chess::net::ServerMoveMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::ServerMoveMsg>(black).has_value());

    sendMsg(white, chess::net::ResignMsg{});
    ASSERT_TRUE(await<chess::net::GameOverMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::GameOverMsg>(black).has_value());

    // A third connection browses the archive as a guest (global list).
    auto archivist = connectClient();
    sendMsg(archivist, chess::net::ArchiveRequestMsg{50, false});
    auto list = await<chess::net::ArchiveListMsg>(archivist);
    ASSERT_TRUE(list.has_value());
    ASSERT_EQ(list->games.size(), 1u);

    sendMsg(archivist, chess::net::ArchiveGetMsg{list->games[0].id});
    auto record = await<chess::net::ArchiveGameMsg>(archivist);
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->startFen, wWelcome->startFen)
        << "replay must rebuild from the played Start Position";
    EXPECT_EQ(record->sanMoves, san);
    // White resigned, so the win is Black's.
    EXPECT_EQ(record->result,
              static_cast<int>(chess::net::GameResult::BlackWins));
    EXPECT_EQ(record->reason,
              static_cast<int>(chess::net::GameOverReason::Resignation));
}

// ── Bot plays a 960 game ────────────────────────────────────────────────────

class BotChess960E2eTest : public chess::e2e::E2eHarness {
protected:
    void configure(chess::server::ServerConfig& config) override
    {
        config.botsEnabled = true;
        // Same procedural mock the bot match tests use (MOCK_ENGINE_PATH
        // is injected by the ctest registration).
        if (const char* mockPath = std::getenv("MOCK_ENGINE_PATH"))
            config.botEnginePath = mockPath;
    }
};

TEST_F(BotChess960E2eTest, BotPairsAndPlaysTheRolledPosition)
{
    sf::TcpSocket human = connectClient();
    auto welcome = joinVariant(human, "Solouser", "chess960");
    ASSERT_TRUE(welcome.has_value()) << "server pairs the solo human with a bot";
    EXPECT_EQ(welcome->variant, "chess960");
    EXPECT_TRUE(isValidChess960BackRank(welcome->startFen));

    // The solo human queued first and is therefore White; the bot (Black)
    // answers one human move from the rolled position.
    ASSERT_EQ(welcome->color, chess::Color::White);
    std::string san = firstLegalSan(welcome->startFen);
    ASSERT_FALSE(san.empty());
    sendMsg(human, chess::net::MoveMsg{san});
    auto echo = await<chess::net::ServerMoveMsg>(human);
    ASSERT_TRUE(echo.has_value());
    auto botMove = await<chess::net::ServerMoveMsg>(human);
    ASSERT_TRUE(botMove.has_value()) << "the bot must reply on the 960 board";
}

// ── Rematch: variant kept, Start Position re-rolled ─────────────────────────

class Chess960RematchE2eTest : public chess::e2e::E2eHarness {
protected:
    // The rematch conversation must fit the (retention = grace) window.
    void beforeStart() override { testGraceMs_ = 2500; }
};

TEST_F(Chess960RematchE2eTest, RematchKeepsVariantAndRerollsStartFen)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();

    sendMsg(white, chess::net::JoinMsg{"Agata", "", "chess960"});
    sendMsg(black, chess::net::JoinMsg{"Bruno", "", "chess960"});
    auto wOld = await<chess::net::WelcomeMsg>(white);
    auto bOld = await<chess::net::WelcomeMsg>(black);
    ASSERT_TRUE(wOld.has_value());
    ASSERT_TRUE(bOld.has_value());

    sendMsg(white, chess::net::ResignMsg{});
    ASSERT_TRUE(await<chess::net::GameOverMsg>(white).has_value());
    ASSERT_TRUE(await<chess::net::GameOverMsg>(black).has_value());

    sendMsg(white, chess::net::RematchRequestMsg{});
    ASSERT_TRUE(await<chess::net::ServerRematchRequestMsg>(black).has_value());
    sendMsg(black, chess::net::RematchAcceptMsg{});
    auto wNew = await<chess::net::WelcomeMsg>(white);
    auto bNew = await<chess::net::WelcomeMsg>(black);
    ASSERT_TRUE(wNew.has_value());
    ASSERT_TRUE(bNew.has_value());

    // Colors swap (11.7) while the variant survives (12.1) — and the fresh
    // game carries an SP again, replayable from its own carried FEN.
    EXPECT_EQ(wNew->color, chess::Color::Black);
    EXPECT_EQ(bNew->color, chess::Color::White);
    EXPECT_EQ(wNew->variant, "chess960");
    EXPECT_EQ(bNew->variant, "chess960");
    EXPECT_EQ(wNew->startFen, bNew->startFen);
    EXPECT_FALSE(wNew->startFen.empty());
    EXPECT_TRUE(isValidChess960BackRank(wNew->startFen));
    EXPECT_NE(wNew->resumeToken, wOld->resumeToken);
}
