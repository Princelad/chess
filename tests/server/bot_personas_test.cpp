// File-relative include on purpose: client/src and server/src both expose
// a bot_personas.h on the include path, and the single chess-tests binary
// links both libraries — the quoted path pins the SERVER table.
#include "../../server/src/bot_personas.h"

#include <gtest/gtest.h>

using chess::server::BotPersona;
using chess::server::parseBotPersonas;

TEST(BotPersonas, FullEntryParses)
{
    auto v = parseBotPersonas("Sable=4:500:6");
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].name, "Sable");
    EXPECT_EQ(v[0].depth, 4);
    EXPECT_EQ(v[0].movetimeMs, 500);
    EXPECT_EQ(v[0].skill, 6);
}

TEST(BotPersonas, SkillDefaultsWhenOmitted)
{
    auto v = parseBotPersonas("Oracle=0:2000");
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].name, "Oracle");
    EXPECT_EQ(v[0].depth, 0);
    EXPECT_EQ(v[0].movetimeMs, 2000);
    EXPECT_EQ(v[0].skill, 20); // engine default level
}

TEST(BotPersonas, MultipleEntriesKeepOrder)
{
    auto v = parseBotPersonas("Sable=4:500:6,Oracle=0:2000,Pip=2:250:1");
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0].name, "Sable");
    EXPECT_EQ(v[1].name, "Oracle");
    EXPECT_EQ(v[2].name, "Pip");
    EXPECT_EQ(v[2].depth, 2);
    EXPECT_EQ(v[2].skill, 1);
}

TEST(BotPersonas, MalformedEntriesAreSkipped)
{
    // No '=', empty name, missing movetime, junk depth, negative depth,
    // junk skill — each skipped, the valid ones survive (a bad operator
    // flag must never kill server startup).
    auto v = parseBotPersonas(
        "Broken,Sable=4:500:6,=9:9:9,Half=3,Bad=abc:500,Neg=2:500:-3,"
        "JunkSkill=2:500:abc,Good=1:0:12");
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0].name, "Sable");
    EXPECT_EQ(v[1].name, "Good");
    EXPECT_EQ(v[1].skill, 12);
}

TEST(BotPersonas, EmptyAndDegenerateSpecs)
{
    EXPECT_TRUE(parseBotPersonas("").empty());
    EXPECT_TRUE(parseBotPersonas(",,,").empty());
    EXPECT_TRUE(parseBotPersonas("   ").empty());

    // Extra colon fields after skill are ignored (best-effort parse).
    auto v = parseBotPersonas("Name=1:2:3:4:5");
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].skill, 3);
}
