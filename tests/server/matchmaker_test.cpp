#include <gtest/gtest.h>

#include "client.h"
#include "matchmaker.h"

TEST(MatchmakerTest, EnqueueOneReturnsNullopt)
{
    Matchmaker mm;
    Client c;
    c.socket = std::make_unique<sf::TcpSocket>();
    c.name = "Alice";

    auto result = mm.enqueue(c);
    EXPECT_FALSE(result.has_value());
}

TEST(MatchmakerTest, EnqueueTwoReturnsPair)
{
    Matchmaker mm;
    Client a, b;
    a.socket = std::make_unique<sf::TcpSocket>();
    a.name = "Alice";
    b.socket = std::make_unique<sf::TcpSocket>();
    b.name = "Bob";

    mm.enqueue(a);
    auto result = mm.enqueue(b);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, &a);
    EXPECT_EQ(result->second, &b);
}

TEST(MatchmakerTest, FirstJoinedIsWhite)
{
    Matchmaker mm;
    Client a, b;
    a.socket = std::make_unique<sf::TcpSocket>();
    a.name = "Alice";
    b.socket = std::make_unique<sf::TcpSocket>();
    b.name = "Bob";

    mm.enqueue(a);
    auto [white, black] = *mm.enqueue(b);
    EXPECT_EQ(white->name, "Alice");
    EXPECT_EQ(black->name, "Bob");
}

TEST(MatchmakerTest, EnqueueThreeReturnsNulloptThenPairs)
{
    Matchmaker mm;
    Client a, b, c, d;
    a.socket = std::make_unique<sf::TcpSocket>();
    a.name = "A";
    b.socket = std::make_unique<sf::TcpSocket>();
    b.name = "B";
    c.socket = std::make_unique<sf::TcpSocket>();
    c.name = "C";
    d.socket = std::make_unique<sf::TcpSocket>();
    d.name = "D";

    mm.enqueue(a);
    auto pair1 = mm.enqueue(b);
    ASSERT_TRUE(pair1.has_value());

    mm.enqueue(c);
    auto pair2 = mm.enqueue(d);
    ASSERT_TRUE(pair2.has_value());
    EXPECT_EQ(pair2->first, &c);
    EXPECT_EQ(pair2->second, &d);
}

TEST(MatchmakerTest, DuplicateEnqueueReturnsNullopt)
{
    Matchmaker mm;
    Client a, b;
    a.socket = std::make_unique<sf::TcpSocket>();
    a.name = "Alice";
    b.socket = std::make_unique<sf::TcpSocket>();
    b.name = "Bob";

    mm.enqueue(a);
    auto result = mm.enqueue(a);
    EXPECT_FALSE(result.has_value());

    auto pair = mm.enqueue(b);
    ASSERT_TRUE(pair.has_value());
    EXPECT_EQ(pair->first, &a);
    EXPECT_EQ(pair->second, &b);
}

TEST(MatchmakerTest, RemoveQueuedClient)
{
    Matchmaker mm;
    Client a, b;
    a.socket = std::make_unique<sf::TcpSocket>();
    a.name = "Alice";
    b.socket = std::make_unique<sf::TcpSocket>();
    b.name = "Bob";

    mm.enqueue(a);
    mm.remove(a);
    auto result = mm.enqueue(b);
    EXPECT_FALSE(result.has_value());
}

TEST(MatchmakerTest, RemoveNotQueuedIsNoop)
{
    Matchmaker mm;
    Client a, b;
    a.socket = std::make_unique<sf::TcpSocket>();
    a.name = "Alice";
    b.socket = std::make_unique<sf::TcpSocket>();
    b.name = "Bob";

    mm.remove(a);
    mm.enqueue(a);
    auto result = mm.enqueue(b);
    ASSERT_TRUE(result.has_value());
}

// ── 12.1: variant-aware pairing ─────────────────────────────────────────────

TEST(MatchmakerTest, VariantMismatchStaysQueued)
{
    Matchmaker mm;
    Client a, b, c;
    a.socket = std::make_unique<sf::TcpSocket>();
    a.name = "Alice";
    b.socket = std::make_unique<sf::TcpSocket>();
    b.name = "Bob";
    b.variant = "chess960";
    c.socket = std::make_unique<sf::TcpSocket>();
    c.name = "Carol";

    mm.enqueue(a);
    // Alice (standard) vs Bob (Chess960): never pair despite the empty gap.
    EXPECT_FALSE(mm.enqueue(b).has_value());
    // Carol joins two incompatible-but-present clients: her standard twin is
    // Alice, never Bob.
    auto pair = mm.enqueue(c);
    ASSERT_TRUE(pair.has_value());
    EXPECT_EQ(pair->first, &a);
    EXPECT_EQ(pair->second, &c);
}

TEST(MatchmakerTest, VariantMismatchDoesNotStealOtherQueued)
{
    Matchmaker mm;
    Client a, b, c;
    a.socket = std::make_unique<sf::TcpSocket>();
    a.name = "Alice";
    b.socket = std::make_unique<sf::TcpSocket>();
    b.name = "Bob";
    b.variant = "chess960";
    c.socket = std::make_unique<sf::TcpSocket>();
    c.name = "Carol";
    c.variant = "chess960";

    mm.enqueue(b);
    mm.enqueue(c);
    // Alice joins a queue that already holds a matching 960 pair — she must
    // not be paired against either of them.
    auto result = mm.enqueue(a);
    EXPECT_FALSE(result.has_value());
}

TEST(MatchmakerTest, MatchingVariantPairs)
{
    Matchmaker mm;
    Client a, b;
    a.socket = std::make_unique<sf::TcpSocket>();
    a.name = "Alice";
    a.variant = "chess960";
    b.socket = std::make_unique<sf::TcpSocket>();
    b.name = "Bob";
    b.variant = "chess960";

    mm.enqueue(a);
    auto result = mm.enqueue(b);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first->variant, "chess960");
    EXPECT_EQ(result->second->variant, "chess960");
}
