#include <gtest/gtest.h>

#include "offer_prompt.h"

namespace offer = chess::client::offer;

TEST(OfferPrompt, StartsEmpty)
{
    const offer::State state;
    EXPECT_TRUE(state.empty());
    EXPECT_FALSE(offer::showsDraw(state));
    EXPECT_FALSE(offer::showsTakeback(state));
}

TEST(OfferPrompt, DrawOfferShowsUntilAnswered)
{
    const auto offered = offer::apply({}, offer::Event::DrawOffered);
    EXPECT_TRUE(offered.state.draw);
    EXPECT_FALSE(offered.dissolvedDraw);
    EXPECT_TRUE(offer::showsDraw(offered.state));
    EXPECT_FALSE(offer::showsTakeback(offered.state));

    const auto declined = offer::apply(offered.state, offer::Event::DrawDeclined);
    EXPECT_TRUE(declined.dissolvedDraw);
    EXPECT_TRUE(declined.state.empty());
}

TEST(OfferPrompt, DrawWinsVisibilityWhileBothPending)
{
    auto state = offer::apply({}, offer::Event::DrawOffered).state;
    state = offer::apply(state, offer::Event::TakebackRequested).state;

    EXPECT_TRUE(state.draw);
    EXPECT_TRUE(state.takeback);
    EXPECT_TRUE(offer::showsDraw(state));
    EXPECT_FALSE(offer::showsTakeback(state));

    // Declining the draw falls through to the still-pending takeback.
    const auto afterDraw = offer::apply(state, offer::Event::DrawDeclined);
    EXPECT_TRUE(afterDraw.dissolvedDraw);
    EXPECT_FALSE(afterDraw.dissolvedTakeback);
    EXPECT_TRUE(offer::showsTakeback(afterDraw.state));
}

TEST(OfferPrompt, MoveDissolvesBothAndReportsThem)
{
    auto state = offer::apply({}, offer::Event::DrawOffered).state;
    state = offer::apply(state, offer::Event::TakebackRequested).state;

    const auto moved = offer::apply(state, offer::Event::PositionChanged);
    EXPECT_TRUE(moved.dissolvedDraw);
    EXPECT_TRUE(moved.dissolvedTakeback);
    EXPECT_TRUE(moved.state.empty());
}

TEST(OfferPrompt, SeatResetDissolvesPendingOffers)
{
    const auto state = offer::apply({}, offer::Event::DrawOffered).state;
    const auto dropped = offer::apply(state, offer::Event::SeatReset);
    EXPECT_TRUE(dropped.dissolvedDraw);
    EXPECT_TRUE(dropped.state.empty());
}

TEST(OfferPrompt, MoveWithNothingPendingReportsNothing)
{
    const auto settled = offer::apply({}, offer::Event::PositionChanged);
    EXPECT_FALSE(settled.dissolvedDraw);
    EXPECT_FALSE(settled.dissolvedTakeback);
    EXPECT_TRUE(settled.state.empty());
}

TEST(OfferPrompt, OffersCanReturnAfterDissolution)
{
    auto state = offer::apply({}, offer::Event::DrawOffered).state;
    state = offer::apply(state, offer::Event::PositionChanged).state;
    EXPECT_TRUE(state.empty());

    state = offer::apply(state, offer::Event::DrawOffered).state;
    EXPECT_TRUE(offer::showsDraw(state));
}

TEST(OfferPrompt, TakebackDeclineLeavesDrawPending)
{
    auto state = offer::apply({}, offer::Event::TakebackRequested).state;
    const auto declined = offer::apply(state, offer::Event::TakebackDeclined);
    EXPECT_TRUE(declined.dissolvedTakeback);
    EXPECT_TRUE(declined.state.empty());

    state = offer::apply({}, offer::Event::DrawOffered).state;
    state = offer::apply(state, offer::Event::TakebackRequested).state;
    const auto declinedWhileDraw = offer::apply(
        state, offer::Event::TakebackDeclined);
    EXPECT_TRUE(declinedWhileDraw.dissolvedTakeback);
    EXPECT_FALSE(declinedWhileDraw.dissolvedDraw);
    EXPECT_TRUE(offer::showsDraw(declinedWhileDraw.state));
}

TEST(OfferPrompt, GameEndedClearsEverything)
{
    const auto state = offer::apply({}, offer::Event::DrawOffered).state;
    const auto over = offer::apply(state, offer::Event::GameEnded);
    EXPECT_TRUE(over.state.empty());
    EXPECT_TRUE(over.dissolvedDraw);
}

TEST(OfferPrompt, StateEquality)
{
    offer::State a;
    offer::State b;
    EXPECT_TRUE(a == b);
    a.draw = true;
    EXPECT_TRUE(a != b);
    b.takeback = true;
    b.draw = true;
    a.takeback = true;
    EXPECT_TRUE(a == b);
}
