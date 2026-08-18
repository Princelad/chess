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

} // namespace
} // namespace net
} // namespace chess
