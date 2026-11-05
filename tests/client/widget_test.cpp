#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"
#include "widgets/layout.h"
#include "widgets/player_card.h"
#include "widgets/text_field.h"

#include "screens/promo_state.h"

#include <gtest/gtest.h>

#include "ui/theme.h"

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

// --- 15.2 player card -------------------------------------------------------
// The 9.3 residual "player-card composition untested" closes here: the
// card's pre-render decisions (clock text, heat thresholds, rating
// suffix, slot geometry) are pure math, exercised headlessly — only the
// glyph/sprite drawing needs a window.

namespace {

std::string clockAt(double ms) { return formatClockText(ms); }

} // namespace

TEST(WidgetPlayerCard, ClockFormatsMssAndUnderMinuteTenths)
{
    // Zero floors at 0.0, never negative text.
    EXPECT_EQ(clockAt(0.0), "0.0");
    EXPECT_EQ(clockAt(-5000.0), "0.0");
    // Under a minute: whole tenths, no fractional noise.
    EXPECT_EQ(clockAt(1234.0), "1.2");
    EXPECT_EQ(clockAt(42000.0), "42.0");
    EXPECT_EQ(clockAt(59900.0), "59.9");
    EXPECT_EQ(clockAt(59949.9), "59.9");
    // Rounding: past 59.95 s the half-up tenth hits 600 -> "1:00".
    EXPECT_EQ(clockAt(59991.9), "1:00");
    // A minute and beyond: m:ss with zero padding.
    EXPECT_EQ(clockAt(60000.0), "1:00");
    EXPECT_EQ(clockAt(61000.0), "1:01");
    EXPECT_EQ(clockAt(60200.0), "1:00");
    EXPECT_EQ(clockAt(3600000.0), "60:00");
}

TEST(WidgetPlayerCard, ClockHeatFollowsTheElevenOneThresholds)
{
    EXPECT_EQ(clockHeatColor(9999.0), ui::pal::dangerRed);
    EXPECT_EQ(clockHeatColor(10000.0), ui::pal::dangerAmber);
    EXPECT_EQ(clockHeatColor(29999.0), ui::pal::dangerAmber);
    EXPECT_EQ(clockHeatColor(30000.0), ui::pal::textCell);
}

TEST(WidgetPlayerCard, RatingSuffixRoundsAndOmitsWhenUnknown)
{
    EXPECT_EQ(cardRatingSuffix(std::nullopt), "");
    EXPECT_EQ(cardRatingSuffix(
                  std::optional<std::pair<double, double>>({1650.0, 132.0})),
              "  (1650 \xc2\xb1 132)");
    // Half-up rounding of the Glicko numbers.
    EXPECT_EQ(cardRatingSuffix(
                  std::optional<std::pair<double, double>>({1649.6, 131.4})),
              "  (1650 \xc2\xb1 131)");
}

TEST(WidgetPlayerCard, SlotsTrackTheComposerBand)
{
    PlayerCard card;
    card.setRect(sf::FloatRect({640.f, 14.f}, {312.f, 44.f}));
    EXPECT_FLOAT_EQ(card.rect().position.y, 14.f);

    // Name row sits 6 px into the band at its pre-15.1 height;
    // captured glyphs sit 28 px in, inset by the card's side padding.
    const auto name = card.nameSlot();
    EXPECT_FLOAT_EQ(name.position.x, 640.f);
    EXPECT_FLOAT_EQ(name.position.y, 20.f);
    EXPECT_FLOAT_EQ(name.size.y, 15.f);
    const auto caps = card.capturedSlot();
    EXPECT_FLOAT_EQ(caps.position.x, 647.f);
    EXPECT_FLOAT_EQ(caps.position.y, 42.f);
    EXPECT_FLOAT_EQ(caps.size.x, 312.f - 14.f);
    EXPECT_FLOAT_EQ(caps.size.y, 15.f);
}

TEST(WidgetPlayerCard, StateDefaultsAreQuiet)
{
    PlayerCardState state;
    EXPECT_FALSE(state.thinking);
    EXPECT_FALSE(state.myTurn);
    EXPECT_EQ(state.connection, 0);
    EXPECT_FALSE(state.premovePending);
    EXPECT_EQ(state.watcherCount, 0);
    EXPECT_FALSE(state.remainingMs.has_value()); // unlimited game
    EXPECT_EQ(state.captured, nullptr);
    EXPECT_EQ(state.materialDiff, 0);
}

// --- 15.6 shared promotion-picker geometry -----------------------------------

TEST(WidgetPromoStack, CellsCenterOnTheDestinationColumnAndStackContiguously)
{
    const float sq = 75.f;
    const sf::Vector2f origin{20.f, 20.f};

    // White promoting on the h-file (toFile 7, toRank 7): visual column 7,
    // visual row 0 — the first cell centers on the destination square.
    const PromoCell first = promoStackCell(sq, origin, false, 7, 7, 0);
    EXPECT_FLOAT_EQ(first.size, sq * ui::overlay::promoCellScale);
    EXPECT_FLOAT_EQ(first.pos.x, origin.x + 7.f * sq
        + (sq - first.size) * 0.5f);
    EXPECT_FLOAT_EQ(first.pos.y, origin.y - (first.size - sq) * 0.5f);

    // Contiguous stack: cell k+1 starts exactly where cell k ends.
    const PromoCell second = promoStackCell(sq, origin, false, 7, 7, 1);
    EXPECT_FLOAT_EQ(second.pos.y, first.pos.y + first.size);
    EXPECT_FLOAT_EQ(second.pos.x, first.pos.x);
}

TEST(WidgetPromoStack, FlippedMirrorsFileAndRankTowardTheCenter)
{
    const float sq = 60.f;
    const sf::Vector2f origin{0.f, 0.f};

    // Black flipped, promoting on file a/toRank 0: mirrored column 7,
    // mirrored row 0 (promotion rank sits visually at the top when
    // flipped) — the stack grows downward into the board, same as white's.
    const PromoCell first = promoStackCell(sq, origin, true, 0, 0, 0);
    EXPECT_FLOAT_EQ(first.pos.x, origin.x + 7.f * sq
        + (sq - first.size) * 0.5f);
    EXPECT_FLOAT_EQ(first.pos.y, origin.y - (first.size - sq) * 0.5f);
}

TEST(WidgetPromoStack, StackFitsInsideTheBoardColumn)
{
    // Four cells from a top-anchored promotion square stay inside the
    // board (the checkbox row hangs off the last cell beneath, so the
    // stack must never overflow the playing field).
    const float sq = 75.f;
    const sf::Vector2f origin{20.f, 20.f};
    const float boardBottom = origin.y + 8.f * sq;
    for (int i = 0; i < 4; ++i) {
        const PromoCell c = promoStackCell(sq, origin, false, 0, 7, i);
        EXPECT_GE(c.pos.y, origin.y - sq); // one-square headroom for the inset
        EXPECT_LT(c.pos.y + c.size, boardBottom + sq);
    }
}

} // namespace
} // namespace chess::client