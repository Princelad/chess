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
    ServerMessage msg = WelcomeMsg{Color::White, "Bob"};
    sf::Packet p = freshPacket(serializeServer(msg));
    auto result = deserializeServer(p);
    ASSERT_TRUE(result.has_value());
    auto* m = std::get_if<WelcomeMsg>(&*result);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->color, Color::White);
    EXPECT_EQ(m->opponent, "Bob");
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
