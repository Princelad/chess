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
    p << static_cast<uint8_t>(2);
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
