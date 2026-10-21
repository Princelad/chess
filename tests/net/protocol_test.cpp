#include <chess/net/messages.h>
#include <chess/net/protocol.h>

#include <gtest/gtest.h>
#include <SFML/Network/Packet.hpp>

namespace chess {
namespace net {
namespace {

sf::Packet serializeClient(const ClientMessage& msg)
{
    sf::Packet p;
    serialize(p, msg);
    return p;
}

sf::Packet serializeServer(const ServerMessage& msg)
{
    sf::Packet p;
    serialize(p, msg);
    return p;
}

sf::Packet freshPacket(const sf::Packet& src)
{
    sf::Packet p;
    p.append(src.getData(), src.getDataSize());
    return p;
}

// ── Client round-trip ───────────────────────────────────────────────────────

TEST(ProtocolRoundTrip, ClientJoin)
{
    ClientMessage msg = JoinMsg{"Alice"};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<JoinMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->name, "Alice");
}

TEST(ProtocolRoundTrip, ClientMove)
{
    ClientMessage msg = MoveMsg{"e4"};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<MoveMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->san, "e4");
}

TEST(ProtocolRoundTrip, ClientDrawOffer)
{
    ClientMessage msg = DrawOfferMsg{};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<DrawOfferMsg>(*result));
}

TEST(ProtocolRoundTrip, ClientDrawAccept)
{
    ClientMessage msg = DrawAcceptMsg{};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<DrawAcceptMsg>(*result));
}

TEST(ProtocolRoundTrip, ClientDrawDecline)
{
    ClientMessage msg = DrawDeclineMsg{};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<DrawDeclineMsg>(*result));
}

TEST(ProtocolRoundTrip, ClientResign)
{
    ClientMessage msg = ResignMsg{};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<ResignMsg>(*result));
}

TEST(ProtocolRoundTrip, ClientChat)
{
    ClientMessage msg = ChatMsg{"hello world"};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ChatMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->text, "hello world");
}

TEST(ProtocolRoundTrip, ClientPing)
{
    ClientMessage msg = PingMsg{};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<PingMsg>(*result));
}

TEST(ProtocolRoundTrip, ClientJoinEmptyName)
{
    ClientMessage msg = JoinMsg{""};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<JoinMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->name, "");
}

TEST(ProtocolRoundTrip, ClientMoveEmptySan)
{
    ClientMessage msg = MoveMsg{""};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<MoveMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->san, "");
}

TEST(ProtocolRoundTrip, ClientChatEmptyText)
{
    ClientMessage msg = ChatMsg{""};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ChatMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->text, "");
}

TEST(ProtocolRoundTrip, ClientLogin)
{
    ClientMessage msg = LoginMsg{"Alice", "s3cret"};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<LoginMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->name, "Alice");
    EXPECT_EQ(m->password, "s3cret");
}

TEST(ProtocolRoundTrip, ClientArchiveRequest)
{
    ClientMessage msg = ArchiveRequestMsg{25, true};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ArchiveRequestMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->limit, 25);
    EXPECT_TRUE(m->mineOnly);
}

TEST(ProtocolRoundTrip, ClientArchiveGet)
{
    ClientMessage msg = ArchiveGetMsg{17};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ArchiveGetMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->id, 17);
}

TEST(ProtocolGarbage, ClientArchiveGetNegativeId)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(10); // ArchiveGet tag
    p << static_cast<std::int64_t>(-5);
    EXPECT_FALSE(deserializeClient(p).has_value());
}

TEST(ProtocolRoundTrip, ClientLeaderboardRequest)
{
    ClientMessage msg = LeaderboardRequestMsg{25};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<LeaderboardRequestMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->limit, 25);
}

TEST(ProtocolRoundTrip, ClientLeaderboardRequestDefault)
{
    ClientMessage msg = LeaderboardRequestMsg{};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<LeaderboardRequestMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->limit, 50);
}

TEST(ProtocolRoundTrip, ServerArchiveListEmpty)
{
    ServerMessage msg = ArchiveListMsg{};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ArchiveListMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_TRUE(m->games.empty());
}

TEST(ProtocolRoundTrip, ServerArchiveListEntries)
{
    ArchiveEntryMsg a{7, "Ada", "Bob", static_cast<int>(GameResult::WhiteWins),
                      1729000000, true};
    ArchiveEntryMsg b{9, "Eve", "Mallory", static_cast<int>(GameResult::Draw),
                      1729000500, false};
    ServerMessage msg = ArchiveListMsg{{a, b}};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ArchiveListMsg>(&*result);
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->games.size(), 2u);
    EXPECT_EQ(m->games[0].id, 7);
    EXPECT_EQ(m->games[0].whiteName, "Ada");
    EXPECT_EQ(m->games[0].blackName, "Bob");
    EXPECT_EQ(m->games[0].result, static_cast<int>(GameResult::WhiteWins));
    EXPECT_EQ(m->games[0].playedAt, 1729000000);
    EXPECT_TRUE(m->games[0].rated);
    EXPECT_EQ(m->games[1].id, 9);
    EXPECT_EQ(m->games[1].result, static_cast<int>(GameResult::Draw));
    EXPECT_FALSE(m->games[1].rated);
}

TEST(ProtocolRoundTrip, ServerArchiveGame)
{
    ServerMessage msg = ArchiveGameMsg{
        42, "Ada", "Bob",
        static_cast<int>(GameResult::BlackWins),
        static_cast<int>(GameOverReason::Checkmate),
        "3+2", "", "e4 e5 Nf3 Nc6 Bb5 a6", 1729000100, true};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ArchiveGameMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->id, 42);
    EXPECT_EQ(m->whiteName, "Ada");
    EXPECT_EQ(m->blackName, "Bob");
    EXPECT_EQ(m->result, static_cast<int>(GameResult::BlackWins));
    EXPECT_EQ(m->reason, static_cast<int>(GameOverReason::Checkmate));
    EXPECT_EQ(m->timeControl, "3+2");
    EXPECT_EQ(m->startFen, "");
    EXPECT_EQ(m->sanMoves, "e4 e5 Nf3 Nc6 Bb5 a6");
    EXPECT_EQ(m->playedAt, 1729000100);
    EXPECT_TRUE(m->rated);
}

TEST(ProtocolRoundTrip, ServerErrorEmptyMessage)
{
    ServerMessage msg = ErrorMsg{""};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ErrorMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->message, "");
}

// ── Server round-trip ───────────────────────────────────────────────────────

TEST(ProtocolRoundTrip, ServerWelcome)
{
    ServerMessage msg = WelcomeMsg{Color::White, "Bob", 1650.0, 132.5, true};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<WelcomeMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->color, Color::White);
    EXPECT_EQ(m->opponent, "Bob");
    EXPECT_DOUBLE_EQ(m->rating, 1650.0);
    EXPECT_DOUBLE_EQ(m->deviation, 132.5);
    EXPECT_TRUE(m->rated);
}

TEST(ProtocolRoundTrip, ServerWelcomeBlack)
{
    ServerMessage msg = WelcomeMsg{Color::Black, "Alice"};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<WelcomeMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->color, Color::Black);
    EXPECT_EQ(m->opponent, "Alice");
}

TEST(ProtocolRoundTrip, ServerOpponentJoined)
{
    ServerMessage msg = OpponentJoinedMsg{"Eve"};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<OpponentJoinedMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->name, "Eve");
}

TEST(ProtocolRoundTrip, ServerOpponentLeft)
{
    ServerMessage msg = OpponentLeftMsg{};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<OpponentLeftMsg>(*result));
}

TEST(ProtocolRoundTrip, ServerMove)
{
    ServerMessage msg = ServerMoveMsg{"Nf3"};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ServerMoveMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->san, "Nf3");
}

TEST(ProtocolRoundTrip, ServerDrawOffer)
{
    ServerMessage msg = ServerDrawOfferMsg{};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<ServerDrawOfferMsg>(*result));
}

TEST(ProtocolRoundTrip, ServerGameOver)
{
    ServerMessage msg = GameOverMsg{GameResult::WhiteWins, GameOverReason::Checkmate};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<GameOverMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->result, GameResult::WhiteWins);
    EXPECT_EQ(m->reason, GameOverReason::Checkmate);
}

TEST(ProtocolRoundTrip, ServerGameOverTimeout)
{
    ServerMessage msg = GameOverMsg{GameResult::BlackWins, GameOverReason::Timeout};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<GameOverMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->reason, GameOverReason::Timeout);
}

// ── 11.1 clock fields ───────────────────────────────────────────────────────

TEST(ProtocolRoundTrip, ClientJoinCarriesTimeControl)
{
    ClientMessage msg = JoinMsg{"Alice", "5+3"};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<JoinMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->name, "Alice");
    EXPECT_EQ(m->timeControl, "5+3");
}

TEST(ProtocolRoundTrip, ClientJoinUnlimitedRoundTripsEmptyTimeControl)
{
    ClientMessage msg = JoinMsg{"Alice", ""};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<JoinMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->name, "Alice");
    EXPECT_EQ(m->timeControl, "");
    EXPECT_TRUE(parseTimeControl(m->timeControl).unlimited);
}

TEST(ProtocolRoundTrip, ServerWelcomeCarriesTimeControl)
{
    ServerMessage msg = WelcomeMsg{Color::White, "Bob", 1500.0, 350.0, true, "10+0"};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<WelcomeMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timeControl, "10+0");
}

TEST(ProtocolRoundTrip, ServerWelcomeCarriesResumeToken)
{
    // 11.2: every seat gets its rejoin credential with the welcome.
    ServerMessage msg = WelcomeMsg{Color::White, "Bob", 1500.0, 350.0, true,
                                   "10+0", "deadbeefcafe"};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<WelcomeMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->resumeToken, "deadbeefcafe");
}

TEST(ProtocolRoundTrip, ClientResume)
{
    ClientMessage msg = ResumeMsg{"deadbeefcafe"};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ResumeMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->token, "deadbeefcafe");
}

TEST(ProtocolGarbage, ClientResumeEmptyToken)
{
    sf::Packet p;
    p << ProtocolVersion << static_cast<uint8_t>(12) << std::string();
    EXPECT_FALSE(deserializeClient(p).has_value());
}

TEST(ProtocolGarbage, ClientResumeOversizedToken)
{
    sf::Packet p;
    p << ProtocolVersion << static_cast<uint8_t>(12)
      << std::string(65, 'a'); // one past the wire guard
    EXPECT_FALSE(deserializeClient(p).has_value());
}

TEST(ProtocolRoundTrip, ServerResumeState)
{
    ResumeStateMsg msg;
    msg.color = Color::Black;
    msg.opponent = "Alice";
    msg.rating = 1727.5;
    msg.deviation = 88.0;
    msg.rated = true;
    msg.timeControl = "5+3";
    msg.fen = "rnbqkbnr/pppppppp/8/8/4P3/8/8/RNBQKBNR b KQkq - 0 1";
    msg.sans = {"e4", "e5", "Nf3"};
    msg.whiteRemainingMs = 300000;
    msg.blackRemainingMs = 291250;
    sf::Packet p = freshPacket(serializeServer(ServerMessage{msg}));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ResumeStateMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->color, Color::Black);
    EXPECT_EQ(m->opponent, "Alice");
    EXPECT_DOUBLE_EQ(m->rating, 1727.5);
    EXPECT_DOUBLE_EQ(m->deviation, 88.0);
    EXPECT_TRUE(m->rated);
    EXPECT_EQ(m->timeControl, "5+3");
    EXPECT_EQ(m->fen, msg.fen);
    ASSERT_EQ(m->sans.size(), 3u);
    EXPECT_EQ(m->sans[2], "Nf3");
    EXPECT_EQ(m->whiteRemainingMs, 300000);
    EXPECT_EQ(m->blackRemainingMs, 291250);
}

TEST(ProtocolRoundTrip, ServerResumeStateUnlimitedEmptySanList)
{
    ResumeStateMsg msg; // defaults: unlimited, no moves
    sf::Packet p = freshPacket(serializeServer(ServerMessage{msg}));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ResumeStateMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_TRUE(m->sans.empty());
    EXPECT_EQ(m->whiteRemainingMs, 0);
    EXPECT_EQ(m->blackRemainingMs, 0);
}

TEST(ProtocolGarbage, ServerResumeStateMissingClockFields)
{
    // A packet cut before both clock figures must not parse.
    sf::Packet p;
    p << ProtocolVersion << static_cast<uint8_t>(14)
      << static_cast<int>(Color::White) << std::string("Bob")
      << 1500.0 << 350.0 << static_cast<uint8_t>(0)
      << std::string() << std::string() << static_cast<int32_t>(0);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, ServerResumeStateMovesTooMany)
{
    // Declared SAN count beyond the cap must be refused without reading.
    sf::Packet p;
    p << ProtocolVersion << static_cast<uint8_t>(14)
      << static_cast<int>(Color::White) << std::string("Bob")
      << 1500.0 << 350.0 << static_cast<uint8_t>(0)
      << std::string() << std::string() << static_cast<int32_t>(513);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, ServerResumeStateNegativeMoves)
{
    sf::Packet p;
    p << ProtocolVersion << static_cast<uint8_t>(14)
      << static_cast<int>(Color::White) << std::string("Bob")
      << 1500.0 << 350.0 << static_cast<uint8_t>(0)
      << std::string() << std::string() << static_cast<int32_t>(-1);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, ServerResumeStateNegativeClock)
{
    // Clocks never carry negative time on the wire.
    sf::Packet p;
    p << ProtocolVersion << static_cast<uint8_t>(14)
      << static_cast<int>(Color::White) << std::string("Bob")
      << 1500.0 << 350.0 << static_cast<uint8_t>(0)
      << std::string() << std::string() << static_cast<int32_t>(0)
      << static_cast<int32_t>(-1) << static_cast<int32_t>(0);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolRoundTrip, ServerOpponentReconnecting)
{
    ServerMessage msg = OpponentReconnectingMsg{30};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<OpponentReconnectingMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->graceSeconds, 30);
}

TEST(ProtocolGarbage, ServerOpponentReconnectingNegativeGrace)
{
    sf::Packet p;
    p << ProtocolVersion << static_cast<uint8_t>(15)
      << static_cast<int32_t>(-5);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolRoundTrip, ServerOpponentResumed)
{
    ServerMessage msg = OpponentResumedMsg{};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<OpponentResumedMsg>(*result));
}

TEST(ProtocolRoundTrip, ServerMoveCarriesRemainingMs)
{
    ServerMessage msg = ServerMoveMsg{"e4", 298500};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ServerMoveMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->san, "e4");
    EXPECT_EQ(m->remainingMs, 298500);
}

// Negative remaining is corrupt/hostile: the wire never produces it.
TEST(ProtocolRoundTrip, ServerMoveRejectsNegativeRemainingMs)
{
    sf::Packet p;
    p << ProtocolVersion << static_cast<uint8_t>(3) << std::string("e4")
      << static_cast<int32_t>(-1);
    auto result = deserializeServer(p);
    EXPECT_FALSE(result.has_value());
}

// ── time-control parsing ────────────────────────────────────────────────────

TEST(ParseTimeControl, ValidForms)
{
    auto tc = parseTimeControl("5+3");
    ASSERT_FALSE(tc.unlimited);
    EXPECT_EQ(tc.baseSeconds, 5);
    EXPECT_EQ(tc.incrementSeconds, 3);

    tc = parseTimeControl("90+0");
    ASSERT_FALSE(tc.unlimited);
    EXPECT_EQ(tc.baseSeconds, 90);
    EXPECT_EQ(tc.incrementSeconds, 0);

    tc = parseTimeControl("0+3"); // increment-only: the clock still runs
    ASSERT_FALSE(tc.unlimited);
    EXPECT_EQ(tc.baseSeconds, 0);
    EXPECT_EQ(tc.incrementSeconds, 3);
}

TEST(ParseTimeControl, MalformedIsUnlimited)
{
    EXPECT_TRUE(parseTimeControl("").unlimited);
    EXPECT_TRUE(parseTimeControl("5").unlimited);
    EXPECT_TRUE(parseTimeControl("5+").unlimited);
    EXPECT_TRUE(parseTimeControl("+3").unlimited);
    EXPECT_TRUE(parseTimeControl("5+a").unlimited);
    EXPECT_TRUE(parseTimeControl("a+3").unlimited);
    EXPECT_TRUE(parseTimeControl("5+3 ").unlimited);
    EXPECT_TRUE(parseTimeControl("0+0").unlimited); // zeroed = no clock
}

TEST(ProtocolRoundTrip, ServerGameOverDraw)
{
    ServerMessage msg = GameOverMsg{GameResult::Draw, GameOverReason::Stalemate};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<GameOverMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->result, GameResult::Draw);
    EXPECT_EQ(m->reason, GameOverReason::Stalemate);
}

TEST(ProtocolRoundTrip, ServerChat)
{
    ServerMessage msg = ServerChatMsg{"Alice", "good game"};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ServerChatMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->name, "Alice");
    EXPECT_EQ(m->text, "good game");
}

TEST(ProtocolRoundTrip, ServerPong)
{
    ServerMessage msg = PongMsg{};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<PongMsg>(*result));
}

TEST(ProtocolRoundTrip, ServerError)
{
    ServerMessage msg = ErrorMsg{"illegal move"};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ErrorMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->message, "illegal move");
}

TEST(ProtocolRoundTrip, ServerProfile)
{
    ServerMessage msg = ProfileMsg{"Alice", 1487.25, 201.75, false};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ProfileMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->name, "Alice");
    EXPECT_DOUBLE_EQ(m->rating, 1487.25);
    EXPECT_DOUBLE_EQ(m->deviation, 201.75);
    EXPECT_FALSE(m->isGuest);
}

TEST(ProtocolRoundTrip, ServerLeaderboardListEmpty)
{
    ServerMessage msg = LeaderboardListMsg{};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<LeaderboardListMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_TRUE(m->players.empty());
}

TEST(ProtocolRoundTrip, ServerLeaderboardListEntries)
{
    LeaderboardEntryMsg a{"Ada", 1731.5, 82.25, 12, 3, 2};
    LeaderboardEntryMsg b{"Bob", 1213.0, 154.75, 1, 9, 0};
    ServerMessage msg = LeaderboardListMsg{{a, b}};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<LeaderboardListMsg>(&*result);
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->players.size(), 2u);
    EXPECT_EQ(m->players[0].name, "Ada");
    EXPECT_DOUBLE_EQ(m->players[0].rating, 1731.5);
    EXPECT_DOUBLE_EQ(m->players[0].deviation, 82.25);
    EXPECT_EQ(m->players[0].wins, 12);
    EXPECT_EQ(m->players[0].losses, 3);
    EXPECT_EQ(m->players[0].draws, 2);
    EXPECT_EQ(m->players[1].name, "Bob");
    EXPECT_DOUBLE_EQ(m->players[1].rating, 1213.0);
    EXPECT_EQ(m->players[1].losses, 9);
}

TEST(ProtocolGarbage, LeaderboardListCountTooLarge)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(13); // LeaderboardList tag
    p << static_cast<std::int32_t>(1 << 20);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, LeaderboardListNegativeCount)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(13);
    p << static_cast<std::int32_t>(-1);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, LeaderboardListNegativeCounter)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(13);
    p << static_cast<std::int32_t>(1);
    p << std::string("Ada") << 1500.0 << 350.0
      << static_cast<std::int32_t>(-1)
      << static_cast<std::int32_t>(0)
      << static_cast<std::int32_t>(0);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, LeaderboardListTruncatedEntry)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(13);
    p << static_cast<std::int32_t>(1);
    p << std::string("Ada") << 1500.0 << 350.0
      << static_cast<std::int32_t>(1); // cut off before losses/draws
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolRoundTrip, ServerProfileGuest)
{
    ServerMessage msg = ProfileMsg{"Guest-1", 1500.0, 350.0, true};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<ProfileMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_TRUE(m->isGuest);
    EXPECT_DOUBLE_EQ(m->rating, 1500.0);
}

// ── Takeback (11.3) ─────────────────────────────────────────────────────────

TEST(ProtocolRoundTrip, ClientTakebackRequest)
{
    ClientMessage msg = TakebackRequestMsg{};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<TakebackRequestMsg>(*result));
}

TEST(ProtocolRoundTrip, ClientTakebackAccept)
{
    ClientMessage msg = TakebackAcceptMsg{};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<TakebackAcceptMsg>(*result));
}

TEST(ProtocolRoundTrip, ClientTakebackDecline)
{
    ClientMessage msg = TakebackDeclineMsg{};
    sf::Packet p = freshPacket(serializeClient(msg));
    auto result = deserializeClient(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<TakebackDeclineMsg>(*result));
}

TEST(ProtocolRoundTrip, ServerTakebackRequest)
{
    ServerMessage msg = ServerTakebackRequestMsg{};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<ServerTakebackRequestMsg>(*result));
}

TEST(ProtocolRoundTrip, ServerTakebackAccepted)
{
    TakebackAcceptedMsg msg;
    msg.sans = {"e4", "e5", "Nf3"};
    sf::Packet p = freshPacket(serializeServer(ServerMessage{msg}));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<TakebackAcceptedMsg>(&*result);
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->sans.size(), 3u);
    EXPECT_EQ(m->sans[0], "e4");
    EXPECT_EQ(m->sans[1], "e5");
    EXPECT_EQ(m->sans[2], "Nf3");
}

TEST(ProtocolRoundTrip, ServerTakebackAcceptedEmpty)
{
    // Accepting with no plies left = the start position again.
    ServerMessage msg = TakebackAcceptedMsg{};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<TakebackAcceptedMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_TRUE(m->sans.empty());
}

TEST(ProtocolRoundTrip, ServerTakebackDecline)
{
    ServerMessage msg = ServerTakebackDeclineMsg{};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<ServerTakebackDeclineMsg>(*result));
}

TEST(ProtocolRoundTrip, ClientRematchTrio)
{
    ClientMessage request = RematchRequestMsg{};
    sf::Packet p = freshPacket(serializeClient(request));
    auto r = deserializeClient(p);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(std::holds_alternative<RematchRequestMsg>(*r));

    ClientMessage accept = RematchAcceptMsg{};
    sf::Packet p2 = freshPacket(serializeClient(accept));
    auto r2 = deserializeClient(p2);
    ASSERT_TRUE(r2.has_value());
    EXPECT_TRUE(std::holds_alternative<RematchAcceptMsg>(*r2));

    ClientMessage decline = RematchDeclineMsg{};
    sf::Packet p3 = freshPacket(serializeClient(decline));
    auto r3 = deserializeClient(p3);
    ASSERT_TRUE(r3.has_value());
    EXPECT_TRUE(std::holds_alternative<RematchDeclineMsg>(*r3));
}

TEST(ProtocolRoundTrip, ServerRematchRequest)
{
    ServerMessage msg = ServerRematchRequestMsg{};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<ServerRematchRequestMsg>(*result));
}

TEST(ProtocolRoundTrip, ServerRematchDeclineCarriesWithdrawn)
{
    ServerMessage plain = ServerRematchDeclineMsg{};
    sf::Packet p = freshPacket(serializeServer(plain));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* decline = std::get_if<ServerRematchDeclineMsg>(&*result);
    ASSERT_NE(decline, nullptr);
    EXPECT_FALSE(decline->withdrawn) << "default decline is not a withdraw";

    ServerMessage withdrawn = ServerRematchDeclineMsg{true};
    sf::Packet p2 = freshPacket(serializeServer(withdrawn));
    auto result2 = deserializeServer(p2);
    ASSERT_TRUE(result2.has_value());
    auto* decline2 = std::get_if<ServerRematchDeclineMsg>(&*result2);
    ASSERT_NE(decline2, nullptr);
    EXPECT_TRUE(decline2->withdrawn) << "the withdrawn flag round-trips";
}

TEST(ProtocolGarbage, TakebackAcceptedCountTooLarge)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(18); // ServerMsgType::TakebackAccepted
    p << static_cast<std::int32_t>(513); // MaxResumeMoves (512) + 1
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, TakebackAcceptedNegativeCount)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(18);
    p << static_cast<std::int32_t>(-1);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, TakebackAcceptedTruncatedSans)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(18);
    p << static_cast<std::int32_t>(2);
    p << std::string("e4"); // cut off before the second SAN
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolDebug, TakebackMessages)
{
    EXPECT_EQ(debugString(ClientMessage{TakebackRequestMsg{}}),
              "TakebackRequest{}");
    EXPECT_EQ(debugString(ClientMessage{TakebackAcceptMsg{}}),
              "TakebackAccept{}");
    EXPECT_EQ(debugString(ClientMessage{TakebackDeclineMsg{}}),
              "TakebackDecline{}");
    EXPECT_EQ(debugString(ServerMessage{ServerTakebackRequestMsg{}}),
              "TakebackRequest{}");
    TakebackAcceptedMsg accepted;
    accepted.sans = {"e4", "e5"};
    EXPECT_EQ(debugString(ServerMessage{accepted}), "TakebackAccepted{plies=2}");
    EXPECT_EQ(debugString(ServerMessage{ServerTakebackDeclineMsg{}}),
              "TakebackDecline{}");
}

TEST(ProtocolDebug, RematchMessages)
{
    EXPECT_EQ(debugString(ClientMessage{RematchRequestMsg{}}),
              "RematchRequest{}");
    EXPECT_EQ(debugString(ClientMessage{RematchAcceptMsg{}}),
              "RematchAccept{}");
    EXPECT_EQ(debugString(ClientMessage{RematchDeclineMsg{}}),
              "RematchDecline{}");
    EXPECT_EQ(debugString(ServerMessage{ServerRematchRequestMsg{}}),
              "RematchRequest{}");
    EXPECT_EQ(debugString(ServerMessage{ServerRematchDeclineMsg{}}),
              "RematchDecline{}");
    EXPECT_EQ(debugString(ServerMessage{ServerRematchDeclineMsg{true}}),
              "RematchDecline{withdrawn}");
}

// ── Garbage / truncated packets ─────────────────────────────────────────────

TEST(ProtocolGarbage, EmptyPacket)
{
    sf::Packet empty;
    EXPECT_FALSE(deserializeClient(empty).has_value());
}

TEST(ProtocolGarbage, EmptyPacketServer)
{
    sf::Packet empty;
    EXPECT_FALSE(deserializeServer(empty).has_value());
}

TEST(ProtocolGarbage, SingleByte)
{
    sf::Packet p;
    p << static_cast<uint8_t>(0xFF);
    EXPECT_FALSE(deserializeClient(p).has_value());
}

TEST(ProtocolGarbage, WrongVersion)
{
    sf::Packet p;
    p << static_cast<uint8_t>(99) << static_cast<uint8_t>(0);
    EXPECT_FALSE(deserializeClient(p).has_value());
}

TEST(ProtocolGarbage, UnknownClientTag)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion) << static_cast<uint8_t>(0xFF);
    EXPECT_FALSE(deserializeClient(p).has_value());
}

TEST(ProtocolGarbage, UnknownServerTag)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion) << static_cast<uint8_t>(0xFF);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, TruncatedClientPayload)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion) << static_cast<uint8_t>(0);
    EXPECT_FALSE(deserializeClient(p).has_value());
}

TEST(ProtocolGarbage, TruncatedServerPayload)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion) << static_cast<uint8_t>(0);
    p << static_cast<int>(Color::White);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, RandomNoiseClient)
{
    sf::Packet p;
    for (int i = 0; i < 64; ++i)
        p << static_cast<uint8_t>(i & 0xFF);
    EXPECT_FALSE(deserializeClient(p).has_value());
}

TEST(ProtocolGarbage, RandomNoiseServer)
{
    sf::Packet p;
    for (int i = 0; i < 64; ++i)
        p << static_cast<uint8_t>(i & 0xFF);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, InvalidColorValue)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(0)
      << static_cast<int>(2)
      << std::string("opponent");
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, InvalidGameResultValue)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(6)
      << static_cast<int>(99)
      << static_cast<int>(0);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, ProfileBadFlag)
{
    // Valid Profile header but flag byte out of range: crafted manually.
    // Tag 10 = Profile.
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(10);
    p << std::string("Alice") << 1400.0 << 300.0;
    p << static_cast<uint8_t>(0xFF);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, WelcomeBadRatedFlag)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(0)
      << static_cast<int>(Color::White)
      << std::string("Bob") << 1650.0 << 132.5;
    p << static_cast<uint8_t>(2)
      << std::string("10+0") << std::string("token");
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, Version1Rejected)
{
    // Protocol bump: v1 packets (e.g. old Welcome shape) must not parse.
    sf::Packet p;
    p << static_cast<uint8_t>(1)
      << static_cast<uint8_t>(0)
      << static_cast<int>(Color::White)
      << std::string("Bob");
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, ArchiveListCountTooLarge)
{
    // Declared entry count beyond the cap must be refused without reading.
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(11); // ArchiveList tag
    p << static_cast<std::int32_t>(1 << 20);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, ArchiveListNegativeCount)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(11);
    p << static_cast<std::int32_t>(-1);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, ArchiveListBadResultValue)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(11);
    p << static_cast<std::int32_t>(1);
    p << static_cast<std::int64_t>(1) << std::string("Ada")
      << std::string("Bob") << static_cast<int>(99)
      << static_cast<std::int64_t>(0);
    p << static_cast<uint8_t>(0);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, ArchiveGameBadReasonValue)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(12); // ArchiveGame tag
    p << static_cast<std::int64_t>(1) << std::string("Ada")
      << std::string("Bob") << static_cast<int>(1)
      << static_cast<int>(77)
      << std::string("") << std::string("") << std::string("e4 e5")
      << static_cast<std::int64_t>(0);
    p << static_cast<uint8_t>(0);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

TEST(ProtocolGarbage, InvalidGameOverReasonValue)
{
    sf::Packet p;
    p << static_cast<uint8_t>(ProtocolVersion)
      << static_cast<uint8_t>(6)
      << static_cast<int>(0)
      << static_cast<int>(99);
    EXPECT_FALSE(deserializeServer(p).has_value());
}

// ── debugString ─────────────────────────────────────────────────────────────

TEST(ProtocolDebug, ClientMessages)
{
    EXPECT_FALSE(debugString(ClientMessage{JoinMsg{"test"}}).empty());
    EXPECT_FALSE(debugString(ClientMessage{MoveMsg{"e4"}}).empty());
    EXPECT_FALSE(debugString(ClientMessage{DrawOfferMsg{}}).empty());
    EXPECT_FALSE(debugString(ClientMessage{DrawAcceptMsg{}}).empty());
    EXPECT_FALSE(debugString(ClientMessage{DrawDeclineMsg{}}).empty());
    EXPECT_FALSE(debugString(ClientMessage{ResignMsg{}}).empty());
    EXPECT_FALSE(debugString(ClientMessage{ChatMsg{"hi"}}).empty());
    EXPECT_FALSE(debugString(ClientMessage{PingMsg{}}).empty());
    EXPECT_FALSE(debugString(ClientMessage{LoginMsg{"alice", "topsecret"}}).empty());
    EXPECT_FALSE(debugString(ClientMessage{ArchiveRequestMsg{50, false}}).empty());
    EXPECT_FALSE(debugString(ClientMessage{ArchiveGetMsg{3}}).empty());
    EXPECT_FALSE(debugString(ClientMessage{LeaderboardRequestMsg{64}}).empty());
}

TEST(ProtocolDebug, ServerMessages)
{
    EXPECT_FALSE(debugString(ServerMessage{WelcomeMsg{Color::White, "Bob"}}).empty());
    EXPECT_FALSE(debugString(ServerMessage{OpponentJoinedMsg{"Eve"}}).empty());
    EXPECT_FALSE(debugString(ServerMessage{OpponentLeftMsg{}}).empty());
    EXPECT_FALSE(debugString(ServerMessage{ServerMoveMsg{"Nf3"}}).empty());
    EXPECT_FALSE(debugString(ServerMessage{ServerDrawOfferMsg{}}).empty());
    EXPECT_FALSE(debugString(ServerMessage{GameOverMsg{GameResult::Draw, GameOverReason::Stalemate}}).empty());
    EXPECT_FALSE(debugString(ServerMessage{ServerChatMsg{"A", "B"}}).empty());
    EXPECT_FALSE(debugString(ServerMessage{PongMsg{}}).empty());
    EXPECT_FALSE(debugString(ServerMessage{ErrorMsg{"nope"}}).empty());
    EXPECT_FALSE(debugString(ServerMessage{ProfileMsg{"Alice", 1500, 350, false}}).empty());
    EXPECT_FALSE(debugString(ServerMessage{ArchiveListMsg{}}).empty());
    EXPECT_FALSE(debugString(ServerMessage{ArchiveGameMsg{
        1, "Ada", "Bob", static_cast<int>(GameResult::Draw),
        static_cast<int>(GameOverReason::AgreedDraw), "", "", "e4 e5", 0, false}}).empty());
    EXPECT_FALSE(debugString(ServerMessage{LeaderboardListMsg{}}).empty());
}

TEST(ProtocolDebug, LoginMasksPassword)
{
    const std::string debug = debugString(ClientMessage{LoginMsg{"alice", "topsecret"}});
    EXPECT_EQ(debug.find("topsecret"), std::string::npos);
}

TEST(ProtocolDebug, ClientContainsTypeName)
{
    EXPECT_NE(debugString(ClientMessage{JoinMsg{"x"}}).find("Join"), std::string::npos);
    EXPECT_NE(debugString(ClientMessage{MoveMsg{"x"}}).find("Move"), std::string::npos);
}

TEST(ProtocolDebug, ServerContainsTypeName)
{
    EXPECT_NE(debugString(ServerMessage{WelcomeMsg{Color::White, "x"}}).find("Welcome"), std::string::npos);
    EXPECT_NE(debugString(ServerMessage{GameOverMsg{GameResult::Draw, GameOverReason::Checkmate}}).find("GameOver"), std::string::npos);
    EXPECT_NE(debugString(ServerMessage{ErrorMsg{"x"}}).find("Error"), std::string::npos);
}

} // namespace
} // namespace net
} // namespace chess
