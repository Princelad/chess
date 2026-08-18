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

} // namespace
} // namespace net
} // namespace chess
