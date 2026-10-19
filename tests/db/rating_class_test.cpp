#include <chess/db/rating_class.h>

#include <gtest/gtest.h>

namespace chess::db {
namespace {

TEST(ClassifyTimeControl, EmptyOrGarbageIsUnlimited)
{
    EXPECT_STREQ(classifyTimeControl(""), "unlimited");
    EXPECT_STREQ(classifyTimeControl("abc"), "unlimited");
    EXPECT_STREQ(classifyTimeControl("5 x 3"), "unlimited");
    EXPECT_STREQ(classifyTimeControl("+3"), "unlimited");
    EXPECT_STREQ(classifyTimeControl("5 + 3"), "unlimited");
    EXPECT_STREQ(classifyTimeControl("0+0"), "unlimited");
}

TEST(ClassifyTimeControl, BulletBelowThreeMinutes)
{
    EXPECT_STREQ(classifyTimeControl("1+0"), "bullet");   // 60s
    EXPECT_STREQ(classifyTimeControl("1+1"), "bullet");   // 100s
    EXPECT_STREQ(classifyTimeControl("0+2"), "bullet");   // 80s
    EXPECT_STREQ(classifyTimeControl("2+1"), "bullet");   // 160s
}

TEST(ClassifyTimeControl, BlitzBelowEightMinutes)
{
    EXPECT_STREQ(classifyTimeControl("3+0"), "blitz");    // 180s boundary
    EXPECT_STREQ(classifyTimeControl("5+3"), "blitz");    // 420s
    EXPECT_STREQ(classifyTimeControl("7+0"), "blitz");    // 420s
}

TEST(ClassifyTimeControl, RapidBelowTwentyFiveMinutes)
{
    EXPECT_STREQ(classifyTimeControl("8+0"), "rapid");    // 480s boundary
    EXPECT_STREQ(classifyTimeControl("10+0"), "rapid");   // 600s
    EXPECT_STREQ(classifyTimeControl("15+10"), "rapid");  // 1300s
}

TEST(ClassifyTimeControl, ClassicalBeyondTwentyFiveMinutes)
{
    EXPECT_STREQ(classifyTimeControl("25+0"), "classical"); // 1500s boundary
    EXPECT_STREQ(classifyTimeControl("90+0"), "classical");
    EXPECT_STREQ(classifyTimeControl("30+30"), "classical");
}

} // namespace
} // namespace chess::db
