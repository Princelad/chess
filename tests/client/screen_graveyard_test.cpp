#include <gtest/gtest.h>

#include "app.h"

#include <memory>

// Audit 7.4: the ScreenGraveyard parks screens killed by App's screen
// transitions so a handler that keeps touching `this` after calling
// switchScreen/goBack stays valid; App::run() reaps once per frame after
// draw. These tests cover the parking contract headlessly (App itself
// owns a RenderWindow and stays untested).

namespace {

using chess::client::Screen;
using chess::client::ScreenGraveyard;

struct FlagScreen : Screen {
    bool* destroyed;
    // Optional hook: retires another screen into the same graveyard when
    // THIS one is destroyed (the retire-during-reap defensive shape).
    ScreenGraveyard* onDeathGraveyard = nullptr;
    std::unique_ptr<Screen> onDeathRetire;

    explicit FlagScreen(bool* flag) : destroyed(flag) {}
    ~FlagScreen() override
    {
        *destroyed = true;
        if (onDeathGraveyard != nullptr)
            onDeathGraveyard->retire(std::move(onDeathRetire));
    }
    void handleEvent(const sf::Event&) override {}
    void update(float) override {}
    void draw(sf::RenderWindow&) override {}
};

TEST(ScreenGraveyard, RetireParksAndReapDestroys)
{
    ScreenGraveyard graveyard;
    bool destroyed = false;
    graveyard.retire(std::make_unique<FlagScreen>(&destroyed));
    EXPECT_FALSE(destroyed); // parked, not dead
    EXPECT_FALSE(graveyard.empty());

    graveyard.reap();
    EXPECT_TRUE(destroyed);
    EXPECT_TRUE(graveyard.empty());
}

TEST(ScreenGraveyard, NullRetireIsANoop)
{
    ScreenGraveyard graveyard;
    graveyard.retire(nullptr);
    EXPECT_TRUE(graveyard.empty());
    graveyard.reap(); // must not crash
    EXPECT_TRUE(graveyard.empty());
}

TEST(ScreenGraveyard, RetireDuringReapSurvivesToTheNextReap)
{
    ScreenGraveyard graveyard;
    bool outerDestroyed = false;
    bool innerDestroyed = false;
    auto inner = std::make_unique<FlagScreen>(&innerDestroyed);
    auto outer = std::make_unique<FlagScreen>(&outerDestroyed);
    outer->onDeathGraveyard = &graveyard;
    outer->onDeathRetire = std::move(inner);

    graveyard.retire(std::move(outer));
    graveyard.reap();
    EXPECT_TRUE(outerDestroyed);
    // The inner screen was retired by the outer's destructor during the
    // reap — it must be parked for the NEXT reap, not destroyed inside
    // the same iteration (and not leaked).
    EXPECT_FALSE(innerDestroyed);
    EXPECT_FALSE(graveyard.empty());

    graveyard.reap();
    EXPECT_TRUE(innerDestroyed);
    EXPECT_TRUE(graveyard.empty());
}

TEST(ScreenGraveyard, ReapWithNothingParkedIsANoop)
{
    ScreenGraveyard graveyard;
    graveyard.reap();
    EXPECT_TRUE(graveyard.empty());
}

} // namespace
