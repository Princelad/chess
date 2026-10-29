#include "ui/theme.h"

#include <gtest/gtest.h>

namespace chess::client {
namespace {

TEST(ThemeHelpers, MixIsMonotonicAndClamped)
{
    const sf::Color black{ 0, 0, 0 };
    const sf::Color white{ 255, 255, 255 };

    const sf::Color mid = ui::mix(black, white, 0.5f);
    EXPECT_EQ(mid.r, 127);
    EXPECT_EQ(mid.g, 127);
    EXPECT_EQ(mid.b, 127);

    // t outside [0,1] clamps to the endpoints
    EXPECT_TRUE(ui::mix(black, white, -1.f) == black);
    EXPECT_TRUE(ui::mix(black, white, 2.f) == white);
}

TEST(ThemeHelpers, ShadePreservesAlpha)
{
    const sf::Color translucent{ 100, 100, 100, 150 };
    EXPECT_EQ(ui::shade(translucent, 1.f).a, 150);
    EXPECT_EQ(ui::shade(translucent, -1.f).a, 150);
    EXPECT_EQ(ui::shade(translucent, 1.f).r, 255);
    EXPECT_EQ(ui::shade(translucent, -1.f).r, 0);

    // Shading never changes hue direction: >0 lightens, <0 darkens
    const sf::Color gray{ 100, 100, 100 };
    EXPECT_GT(ui::shade(gray, 0.5f).r, gray.r);
    EXPECT_LT(ui::shade(gray, -0.5f).r, gray.r);
}

TEST(ThemeHelpers, LuminanceAndContrast)
{
    EXPECT_NEAR(ui::relativeLuminance(sf::Color{ 0, 0, 0 }), 0.f, 1e-6);
    EXPECT_NEAR(ui::relativeLuminance(sf::Color{ 255, 255, 255 }), 1.f, 1e-6);

    // White vs black: max contrast
    const float wb = ui::contrastRatio(sf::Color{ 255, 255, 255 }, sf::Color{ 0, 0, 0 });
    EXPECT_NEAR(wb, 21.f, 0.01f);

    // Symmetric
    EXPECT_NEAR(ui::contrastRatio(sf::Color{ 120, 120, 120 }, sf::Color{ 30, 30, 30 }),
                ui::contrastRatio(sf::Color{ 30, 30, 30 }, sf::Color{ 120, 120, 120 }),
                1e-6f);
}

TEST(ThemeHelpers, ReadableTextFlipsOnBackgroundTone)
{
    EXPECT_GT(ui::relativeLuminance(ui::pal::white), 0.99f);
    const sf::Color dark = ui::readableTextOn(sf::Color{ 230, 245, 230 }); // light bg
    const sf::Color light = ui::readableTextOn(sf::Color{ 30, 30, 30 });   // dark bg
    EXPECT_LT(ui::relativeLuminance(dark), 0.25f);
    EXPECT_GT(ui::relativeLuminance(light), 0.75f);
}

TEST(ThemePalette, BackgroundFamilyIsDark)
{
    EXPECT_LT(ui::relativeLuminance(ui::pal::bgApp), 0.25f);
    EXPECT_LT(ui::relativeLuminance(ui::pal::bgDeep), 0.15f);
    EXPECT_LT(ui::relativeLuminance(ui::pal::bgPanel), 0.15f);
    EXPECT_LT(ui::relativeLuminance(ui::pal::bgCard), 0.20f);
    // Elevated/control fills sit above the panel family by design (hover/
    // disabled rows, checkbox track) — they only need to stay below mid-gray.
    EXPECT_LT(ui::relativeLuminance(ui::pal::bgElevated), 0.5f);
}

TEST(ThemePalette, TextIsReadableOnEveryPanelBackground)
{
    // Panels that actually carry text rows.
    const sf::Color family[] = {
        ui::pal::bgApp, ui::pal::bgDeep, ui::pal::bgSunken, ui::pal::bgPanel,
        ui::pal::bgCard,
    };
    for (const auto bg : family) {
        // Today's legacy values clear 4.2:1 on the darkest panel and 3.9:1 on
        // cards; 14.5's restyle should RAISE this floor (and these asserts),
        // not tighten below it.
        EXPECT_GT(ui::contrastRatio(ui::pal::textPrimary, bg), 4.0f);
        EXPECT_GT(ui::contrastRatio(ui::pal::textBody, bg), 3.8f);
        EXPECT_GT(ui::contrastRatio(ui::pal::textBright, bg), 4.0f);
    }
}

TEST(ThemeTypeScale, StrictlyIncreasing)
{
    EXPECT_LT(ui::type::Small, ui::type::Body);
    EXPECT_LT(ui::type::Body, ui::type::Status);
    EXPECT_LT(ui::type::Status, ui::type::Heading);
    EXPECT_LT(ui::type::Heading, ui::type::Big);
    EXPECT_LT(ui::type::Big, ui::type::Result);
    EXPECT_LT(ui::type::Result, ui::type::Display);
}

TEST(ThemeSpacing, ScaleIsRising)
{
    EXPECT_LT(ui::space::Xs, ui::space::Sm);
    EXPECT_LT(ui::space::Sm, ui::space::Md);
    EXPECT_LT(ui::space::Md, ui::space::Lg);
    EXPECT_LT(ui::space::Lg, ui::space::Xl);
    EXPECT_LT(ui::space::Xl, ui::space::Xxl);
}

TEST(ThemeButtons, VariantsAreStateful)
{
    // A sane button distinguishes all four interaction states (listed row
    // buttons deliberately reuse disabled == idle — they are read-only rows).
    for (auto make : { ui::buttonDanger, ui::buttonSuccessBright, ui::buttonInfo }) {
        const auto s = make();
        EXPECT_FALSE(s.idleFill == s.hoverFill);
        EXPECT_FALSE(s.hoverFill == s.pressedFill);
        EXPECT_FALSE(s.idleFill == s.disabledFill);
    }
}

TEST(ThemeButtons, SemanticFamiliesAreDistinct)
{
    // Danger/success/info + connect variants must not collide into one blob.
    const std::pair<sf::Color, sf::Color> pairs[] = {
        { ui::buttonDanger().idleFill, ui::buttonSuccess().idleFill },
        { ui::buttonSuccess().idleFill, ui::buttonInfo().idleFill },
        { ui::buttonConnect().idleFill, ui::buttonCycle().idleFill },
        { ui::buttonMint().idleFill, ui::buttonBronze().idleFill },
        { ui::buttonBlue().idleFill, ui::buttonSlate().idleFill },
        { ui::buttonPlum().idleFill, ui::buttonLeave().idleFill },
        { ui::buttonHint().idleFill, ui::buttonAmber().idleFill },
    };
    for (const auto& [a, b] : pairs)
        EXPECT_FALSE(a == b);
}

TEST(ThemeButtons, VariantDefaultsMatchTheWidgetDefaults)
{
    // The widget-and-factory baseline are the same token set.
    const ui::ButtonStyle widgetDefault{};
    EXPECT_TRUE(widgetDefault == ui::buttonNeutral());

    // Opaque fills for the primary family (schema change would break the
    // RectangleShape-based render path).
    EXPECT_EQ(ui::buttonDanger().idleFill.a, 255);
    EXPECT_EQ(ui::buttonSuccessBright().label.a, 255);
}

} // namespace
} // namespace chess::client
