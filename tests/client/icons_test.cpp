#include "ui/icons.h"
#include "ui/theme.h"

#include <gtest/gtest.h>

#include <vector>

namespace chess::client {
namespace {

// Pure registry completeness: every declared kind is inside the enum range
// so a new icon added without a drawIcon case breaks this cast-free table.
TEST(IconsRegistry, DeclaresExactlyTheDocumentedSet)
{
    const ui::icons::IconKind all[] = {
        ui::icons::IconKind::Clock,   ui::icons::IconKind::Flag,
        ui::icons::IconKind::HandDraw, ui::icons::IconKind::Flip,
        ui::icons::IconKind::Bulb,    ui::icons::IconKind::Medal,
        ui::icons::IconKind::Eye,     ui::icons::IconKind::ArrowBack,
    };
    EXPECT_EQ(std::size(all), ui::icons::iconKindCount());
    for (int i = 0; i < ui::icons::iconKindCount(); ++i)
        EXPECT_EQ(static_cast<int>(all[i]), i);
}

TEST(IconsSizing, BoxScalesWithTypeScaleAndIsMonotone)
{
    // Small sizes stay usable on screen, big ones cap reasonably.
    EXPECT_GE(ui::icons::iconBox(ui::type::Caption), 12.f);
    EXPECT_LE(ui::icons::iconBox(ui::type::Display), 45.f);

    float prev = 0.f;
    for (unsigned size :
         {ui::type::Caption, ui::type::Small, ui::type::Body,
          ui::type::Status, ui::type::Heading, ui::type::Big,
          ui::type::Result, ui::type::Display}) {
        const float box = ui::icons::iconBox(size);
        EXPECT_GT(box, prev);
        EXPECT_NEAR(box, static_cast<float>(size) * 1.15f, 0.5f);
        prev = box;
    }
}

TEST(IconsLayout, SlotAlwaysFitsInsideTheControlRect)
{
    const std::vector<sf::FloatRect> rects = {
        sf::FloatRect({0.f, 0.f}, {23.f, 23.f}),   // tiny square control
        sf::FloatRect({10.f, 10.f}, {36.f, 36.f}), // standard button
        sf::FloatRect({0.f, 0.f}, {120.f, 28.f}),  // wide flat row
        sf::FloatRect({5.f, 5.f}, {200.f, 18.f}),  // short height
    };
    for (const auto rect : rects) {
        for (unsigned textSize : {ui::type::Caption, ui::type::Body,
                                  ui::type::Heading, ui::type::Display}) {
            const auto slot = ui::icons::iconSlotRect(rect, textSize);
            EXPECT_GE(slot.position.x, rect.position.x);
            EXPECT_GE(slot.position.y, rect.position.y);
            EXPECT_LE(slot.position.x + slot.size.x,
                      rect.position.x + rect.size.x);
            EXPECT_LE(slot.position.y + slot.size.y,
                      rect.position.y + rect.size.y);
            EXPECT_GT(slot.size.x, 0.f);
            EXPECT_EQ(slot.size.x, slot.size.y); // square icon box
        }
    }
}

TEST(IconsLayout, TextRectIsTheControlMinusTheSlot)
{
    const sf::FloatRect rect({100.f, 50.f}, {200.f, 36.f});
    const auto slot = ui::icons::iconSlotRect(rect, ui::type::Heading);
    const auto text = ui::icons::iconTextRect(rect, ui::type::Heading);

    EXPECT_FLOAT_EQ(text.position.x, slot.position.x + slot.size.x);
    EXPECT_FLOAT_EQ(text.position.y, rect.position.y);
    EXPECT_FLOAT_EQ(text.size.y, rect.size.y);
    // Remaining region leaves at least the majority of the button to the
    // label for a sensible short label.
    EXPECT_GT(text.size.x, rect.size.x * 0.5f);
}

TEST(IconsLayout, IconDropsOnNarrowControlsNotOverlaps)
{
    // Button::draw drops the icon when the label cannot fit beside it; the
    // pure contract behind that decision: for a control barely wider than
    // the icon box, the remaining region is smaller than a Heading-sized
    // word, so icon+text cannot coexist.
    const sf::FloatRect rect({0.f, 0.f}, {23.f, 23.f});
    const auto text = ui::icons::iconTextRect(rect, ui::type::Heading);
    EXPECT_LT(text.size.x, 20.f);
}

} // namespace
} // namespace chess::client
