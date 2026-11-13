#include "bot_personas.h"

#include <gtest/gtest.h>

namespace personas = chess::client::personas;

TEST(Personas, CastIsOrderedWeakestToStrongest)
{
    const auto& cast = personas::all();
    ASSERT_EQ(cast.size(), 5u);

    for (std::size_t i = 0; i + 1 < cast.size(); ++i) {
        EXPECT_LT(cast[i].skill, cast[i + 1].skill) << "slot " << i;
        EXPECT_LT(cast[i].approxElo, cast[i + 1].approxElo) << "slot " << i;
    }
}

TEST(Personas, FieldsAreSane)
{
    for (const auto& p : personas::all()) {
        EXPECT_GE(p.skill, 0);
        EXPECT_LE(p.skill, 20);
        EXPECT_GE(p.depth, 0);
        EXPECT_GE(p.movetimeMs, 0);
        EXPECT_GT(p.approxElo, 0);
        EXPECT_NE(p.name, nullptr);
        EXPECT_NE(p.blurb, nullptr);
    }

    // Oracle runs at full engine strength with no depth cap.
    const auto& top = personas::all().back();
    EXPECT_EQ(top.skill, 20);
    EXPECT_EQ(top.depth, 0);
}

TEST(Personas, NamesAreUnique)
{
    const auto& cast = personas::all();
    for (std::size_t i = 0; i < cast.size(); ++i)
        for (std::size_t j = i + 1; j < cast.size(); ++j)
            EXPECT_NE(std::string(cast[i].name), std::string(cast[j].name));
}

TEST(Personas, FindIsCaseInsensitive)
{
    const auto* sable = personas::find("Sable");
    ASSERT_NE(sable, nullptr);
    EXPECT_EQ(std::string(sable->name), "Sable");

    EXPECT_EQ(personas::find("sable"), sable);
    EXPECT_EQ(personas::find("SABLE"), sable);
    EXPECT_EQ(personas::find("oracle"), personas::find("Oracle"));
    EXPECT_EQ(personas::find("Meridian"), personas::find("meridian"));

    // Unknown and empty names have no persona.
    EXPECT_EQ(personas::find(""), nullptr);
    EXPECT_EQ(personas::find("Magnus"), nullptr);
    EXPECT_EQ(personas::find("Nelson"), nullptr);
}
