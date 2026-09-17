#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"
#include "widgets/layout.h"
#include "widgets/text_field.h"

#include <gtest/gtest.h>

namespace chess::client {
namespace {

sf::Event keyPress(sf::Keyboard::Key code)
{
    return sf::Event::KeyPressed{ code, sf::Keyboard::Scan::Unknown, false,
                                  false, false, false };
}

sf::Event mousePressInside(const sf::FloatRect& rect)
{
    sf::Vector2i pos{ static_cast<int>(rect.position.x + 2.f),
                      static_cast<int>(rect.position.y + 2.f) };
    return sf::Event::MouseButtonPressed{ sf::Mouse::Button::Left, pos };
}

sf::Event mouseRelease(const sf::FloatRect& rect)
{
    sf::Vector2i pos{ static_cast<int>(rect.position.x + 2.f),
                      static_cast<int>(rect.position.y + 2.f) };
    return sf::Event::MouseButtonReleased{ sf::Mouse::Button::Left, pos };
}

sf::Event mouseMoveInto(const sf::FloatRect& rect)
{
    sf::Vector2i pos{ static_cast<int>(rect.position.x + 2.f),
                      static_cast<int>(rect.position.y + 2.f) };
    return sf::Event::MouseMoved{ pos };
}

sf::Event mouseMoveTo(float x, float y)
{
    return sf::Event::MouseMoved{ sf::Vector2i{ static_cast<int>(x),
                                                static_cast<int>(y) } };
}

TEST(WidgetButton, ClickActivatesOnPressThenReleaseInside)
{
    Button btn(sf::FloatRect({10.f, 10.f}, {80.f, 30.f}), "OK");
    int clicks = 0;
    btn.setOnClick([&] { ++clicks; });

    btn.handleEvent(mouseMoveInto(btn.rect()), {12.f, 12.f});
    EXPECT_TRUE(btn.isHovered());

    EXPECT_TRUE(btn.handleEvent(mousePressInside(btn.rect()), {11.f, 11.f}));
    EXPECT_TRUE(btn.isPressed());

    EXPECT_TRUE(btn.handleEvent(mouseRelease(btn.rect()), {11.f, 11.f}));
    EXPECT_FALSE(btn.isPressed());
    EXPECT_EQ(clicks, 1);
}

TEST(WidgetButton, ReleaseOutsideCancelsClick)
{
    Button btn(sf::FloatRect({10.f, 10.f}, {80.f, 30.f}), "OK");
    int clicks = 0;
    btn.setOnClick([&] { ++clicks; });

    btn.handleEvent(mousePressInside(btn.rect()), {11.f, 11.f});
    EXPECT_FALSE(btn.handleEvent(mouseRelease(btn.rect()), {200.f, 200.f}));
    EXPECT_EQ(clicks, 0);
}

TEST(WidgetButton, DisabledIgnoresHoverPressAndKeyboard)
{
    Button btn(sf::FloatRect({10.f, 10.f}, {80.f, 30.f}), "OK");
    int clicks = 0;
    btn.setOnClick([&] { ++clicks; });
    btn.setEnabled(false);

    btn.handleEvent(mouseMoveInto(btn.rect()), {12.f, 12.f});
    EXPECT_FALSE(btn.isHovered());

    EXPECT_FALSE(btn.handleEvent(mousePressInside(btn.rect()), {11.f, 11.f}));
    EXPECT_FALSE(btn.isPressed());
    EXPECT_FALSE(btn.handleEvent(mouseRelease(btn.rect()), {11.f, 11.f}));
    EXPECT_EQ(clicks, 0);

    btn.setFocused(true);
    EXPECT_FALSE(btn.isFocused());
    EXPECT_FALSE(btn.handleEvent(keyPress(sf::Keyboard::Key::Enter), {11.f, 11.f}));
    EXPECT_EQ(clicks, 0);
}

TEST(WidgetButton, EnterAndSpaceActivateWhenFocused)
{
    Button btn(sf::FloatRect({10.f, 10.f}, {80.f, 30.f}), "OK");
    int clicks = 0;
    btn.setOnClick([&] { ++clicks; });

    EXPECT_FALSE(btn.handleEvent(keyPress(sf::Keyboard::Key::Enter), {11.f, 11.f}));
    EXPECT_EQ(clicks, 0);

    btn.setFocused(true);
    EXPECT_TRUE(btn.isFocused());
    EXPECT_TRUE(btn.handleEvent(keyPress(sf::Keyboard::Key::Enter), {11.f, 11.f}));
    EXPECT_TRUE(btn.handleEvent(keyPress(sf::Keyboard::Key::Space), {11.f, 11.f}));
    EXPECT_EQ(clicks, 2);
}

TEST(WidgetCheckbox, ClickTogglesAndFiresCallback)
{
    Checkbox box;
    box.setRect(sf::FloatRect({10.f, 10.f}, {120.f, 24.f}));
    std::vector<bool> calls;
    box.setOnToggle([&](bool v) { calls.push_back(v); });

    EXPECT_TRUE(box.handleEvent(mousePressInside(box.rect()), {11.f, 11.f}));
    EXPECT_TRUE(box.isChecked());
    EXPECT_TRUE(box.handleEvent(mousePressInside(box.rect()), {12.f, 12.f}));
    EXPECT_FALSE(box.isChecked());
    ASSERT_EQ(calls.size(), 2u);
    EXPECT_TRUE(calls[0]);
    EXPECT_FALSE(calls[1]);
}

TEST(WidgetCheckbox, SpaceTogglesWhenFocused)
{
    Checkbox box;
    box.setRect(sf::FloatRect({10.f, 10.f}, {120.f, 24.f}));
    box.setFocused(true);
    EXPECT_TRUE(box.handleEvent(keyPress(sf::Keyboard::Key::Space), {11.f, 11.f}));
    EXPECT_TRUE(box.isChecked());
    EXPECT_TRUE(box.handleEvent(keyPress(sf::Keyboard::Key::Enter), {11.f, 11.f}));
    EXPECT_FALSE(box.isChecked());
}

TEST(WidgetCheckbox, DisabledIgnoresClicks)
{
    Checkbox box;
    box.setRect(sf::FloatRect({10.f, 10.f}, {120.f, 24.f}));
    box.setEnabled(false);
    EXPECT_FALSE(box.handleEvent(mousePressInside(box.rect()), {11.f, 11.f}));
    EXPECT_FALSE(box.isChecked());
}

TEST(WidgetCheckbox, SetCheckedFiresCallbackOnlyOnChange)
{
    Checkbox box;
    int calls = 0;
    box.setOnToggle([&](bool) { ++calls; });

    box.setChecked(true);
    box.setChecked(true);
    EXPECT_EQ(calls, 1);
    box.setChecked(false);
    EXPECT_EQ(calls, 2);
}

TEST(WidgetLabel, AccessorsRoundTrip)
{
    Label label("Moves", 16, sf::Color(10, 20, 30));
    EXPECT_EQ(label.text(), "Moves");
    EXPECT_EQ(label.fontSize(), 16u);
    EXPECT_EQ(label.color(), sf::Color(10, 20, 30));

    label.setText("Captured");
    label.setFontSize(18);
    label.setColor(sf::Color(1, 2, 3));
    label.setPosition({25.f, 40.f});
    EXPECT_EQ(label.text(), "Captured");
    EXPECT_EQ(label.fontSize(), 18u);
    EXPECT_EQ(label.color(), sf::Color(1, 2, 3));
    EXPECT_EQ(label.position(), sf::Vector2f(25.f, 40.f));
}

TEST(WidgetLayout, InsetShrinksUniformlyAndClamps)
{
    const sf::FloatRect r({10.f, 20.f}, {100.f, 80.f});
    const auto in = layout::inset(r, 5.f);
    EXPECT_EQ(in.position, sf::Vector2f(15.f, 25.f));
    EXPECT_EQ(in.size, sf::Vector2f(90.f, 70.f));

    const auto clamped = layout::inset(r, 1000.f);
    EXPECT_EQ(clamped.size, sf::Vector2f(0.f, 0.f));
}

TEST(WidgetLayout, CenterInCentersExactRect)
{
    const sf::FloatRect area({0.f, 0.f}, {200.f, 100.f});
    const auto c = layout::centerIn(area, {50.f, 20.f});
    EXPECT_EQ(c.position, sf::Vector2f(75.f, 40.f));
    EXPECT_EQ(c.size, sf::Vector2f(50.f, 20.f));
}

TEST(WidgetLayout, VStackSpansAndCenters)
{
    const sf::FloatRect area({0.f, 0.f}, {100.f, 60.f});
    auto rects = layout::vstack(area, 10.f, { {40.f, 10.f}, {60.f, 10.f} });

    ASSERT_EQ(rects.size(), 2u);
    // Total height 30 (10+10+gap), centered vertically: top edge y=15.
    EXPECT_EQ(rects[0].position.y, 15.f);
    EXPECT_EQ(rects[0].position.x, 30.f); // (100-40)/2
    EXPECT_EQ(rects[0].size, sf::Vector2f(40.f, 10.f));
    EXPECT_EQ(rects[1].position.y, 35.f);
    EXPECT_EQ(rects[1].position.x, 20.f); // (100-60)/2
    EXPECT_EQ(rects[1].size, sf::Vector2f(60.f, 10.f));
}

TEST(WidgetLayout, HStackSpansAndCenters)
{
    const sf::FloatRect area({0.f, 0.f}, {100.f, 50.f});
    auto rects = layout::hstack(area, 10.f, { {30.f, 20.f}, {40.f, 10.f} });

    ASSERT_EQ(rects.size(), 2u);
    // Total width 80, centered horizontally: left edge x=10.
    EXPECT_EQ(rects[0].position.x, 10.f);
    EXPECT_EQ(rects[0].position.y, 15.f); // (50-20)/2
    EXPECT_EQ(rects[0].size, sf::Vector2f(30.f, 20.f));
    EXPECT_EQ(rects[1].position.x, 50.f);
    EXPECT_EQ(rects[1].size, sf::Vector2f(40.f, 10.f));
}

sf::Event textEntered(char32_t code)
{
    return sf::Event::TextEntered{ code };
}

TEST(WidgetTextField, ClickFocusesAndOutsideClickBlurs)
{
    TextField field(sf::FloatRect({10.f, 10.f}, {200.f, 28.f}));
    EXPECT_FALSE(field.isFocused());

    EXPECT_TRUE(field.handleEvent(mousePressInside(field.rect()), {12.f, 12.f}));
    EXPECT_TRUE(field.isFocused());

    field.handleEvent(mousePressInside(sf::FloatRect({10.f, 60.f}, {50.f, 20.f})),
                      {12.f, 62.f});
    EXPECT_FALSE(field.isFocused());
}

TEST(WidgetTextField, TextEnteredAppendsAndBackspaceDeletes)
{
    TextField field(sf::FloatRect({10.f, 10.f}, {200.f, 28.f}));
    field.setFocused(true);

    field.handleEvent(textEntered(U'h'), {10.f, 10.f});
    field.handleEvent(textEntered(U'i'), {10.f, 10.f});
    EXPECT_EQ(field.text(), "hi");

    field.handleEvent(textEntered(U'\b'), {10.f, 10.f});
    EXPECT_EQ(field.text(), "h");
}

TEST(WidgetTextField, TextEnteredHonorsMaxLength)
{
    TextField field(sf::FloatRect({10.f, 10.f}, {200.f, 28.f}));
    field.setMaxLength(3);
    field.setFocused(true);

    for (char32_t c : { U'a', U'b', U'c', U'd' })
        field.handleEvent(textEntered(c), {10.f, 10.f});
    EXPECT_EQ(field.text(), "abc");
}

TEST(WidgetTextField, EnterFiresCommitOnceWhileFocused)
{
    TextField field(sf::FloatRect({10.f, 10.f}, {200.f, 28.f}));
    int commits = 0;
    field.setOnCommit([&] { ++commits; });

    // Enter before focus is a no-op.
    EXPECT_FALSE(field.handleEvent(keyPress(sf::Keyboard::Key::Enter), {12.f, 12.f}));
    EXPECT_EQ(commits, 0);

    field.setFocused(true);
    EXPECT_TRUE(field.handleEvent(keyPress(sf::Keyboard::Key::Enter), {12.f, 12.f}));
    EXPECT_EQ(commits, 1);

    // Non-Enter keys are not consumed while focused.
    EXPECT_FALSE(field.handleEvent(keyPress(sf::Keyboard::Key::A), {12.f, 12.f}));
    EXPECT_EQ(commits, 1);
}

TEST(WidgetTextField, DisabledIgnoresFocusClickTypeAndEnter)
{
    TextField field(sf::FloatRect({10.f, 10.f}, {200.f, 28.f}));
    int commits = 0;
    field.setOnCommit([&] { ++commits; });
    field.setEnabled(false);

    EXPECT_FALSE(field.handleEvent(mousePressInside(field.rect()), {12.f, 12.f}));
    EXPECT_FALSE(field.handleEvent(textEntered(U'x'), {12.f, 12.f}));
    field.setFocused(true);
    EXPECT_FALSE(field.handleEvent(keyPress(sf::Keyboard::Key::Enter), {12.f, 12.f}));
    EXPECT_EQ(commits, 0);
}

} // namespace
} // namespace chess::client