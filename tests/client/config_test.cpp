#include "config.h"
#include "themes.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include <gtest/gtest.h>

namespace chess::client {
namespace {

std::filesystem::path tempPath(const std::string& tag)
{
    static std::mt19937 rng(1234);
    std::string name = "chess_config_test_" + tag + "_"
        + std::to_string(rng());
    return std::filesystem::temp_directory_path() / name;
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream in(path);
    std::string out;
    std::string line;
    while (std::getline(in, line)) out += line + "\n";
    return out;
}

TEST(Config, SaveLoadRoundTrip)
{
    const auto path = tempPath("roundtrip");
    {
        Config c(path.string());
        c.set("general.auto_queen", "true");
        c.set("board.colors", "green");
        c.set("board.show_coordinates", "false");
        c.set("pieces.path", "/tmp/pieces");
        c.setInt("sound.volume", 42);
        ASSERT_TRUE(c.save());
    }
    {
        Config c(path.string());
        ASSERT_TRUE(c.load());
        EXPECT_EQ(c.get("general.auto_queen"), "true");
        EXPECT_EQ(c.get("board.colors"), "green");
        EXPECT_EQ(c.get("board.show_coordinates"), "false");
        EXPECT_EQ(c.get("pieces.path"), "/tmp/pieces");
        EXPECT_EQ(c.getInt("sound.volume", -1), 42);
        EXPECT_EQ(c.getBool("general.auto_queen", false), true);
        EXPECT_EQ(c.getBool("board.show_coordinates", true), false);
    }
    std::filesystem::remove(path);
}

TEST(Config, SaveGroupsBySection)
{
    const auto path = tempPath("sections");
    {
        Config c(path.string());
        c.set("general.auto_queen", "true");
        c.set("sound.muted", "false");
        c.set("board.colors", "classic");
        c.set("general.name", "x");
        ASSERT_TRUE(c.save());
    }
    const std::string text = readFile(path);
    const std::size_t general = text.find("[general]");
    const std::size_t sound = text.find("[sound]");
    const std::size_t board = text.find("[board]");
    EXPECT_NE(general, std::string::npos);
    EXPECT_NE(sound, std::string::npos);
    EXPECT_NE(board, std::string::npos);
    // Grouped: all [general] keys appear before [sound].
    // Grouped and sorted alphabetically by section.
    EXPECT_LT(board, general);
    EXPECT_LT(general, sound);
    EXPECT_NE(text.find("auto_queen = true"), std::string::npos);
    std::filesystem::remove(path);
}

TEST(Config, LoadIgnoresCommentsAndMalformedLines)
{
    const auto path = tempPath("malformed");
    {
        std::ofstream out(path);
        out << "# comment\n"
            << "; another\n"
            << "[general]\n"
            << "auto_queen = true   ; trailing\n"
            << "bogus line without equals\n"
            << "= no-key\n"
            << "  picky  = cheddar\t\n"
            << "\n";
    }
    Config c(path.string());
    ASSERT_TRUE(c.load());
    EXPECT_EQ(c.get("general.auto_queen"), "true");
    EXPECT_FALSE(c.has("general.bogus"));
    EXPECT_FALSE(c.has("general."));
    EXPECT_EQ(c.get("general.picky"), "cheddar");
    std::filesystem::remove(path);
}

TEST(Config, BoolIntParsingFallsBackToDefault)
{
    const auto path = tempPath("parse");
    {
        std::ofstream out(path);
        out << "[general]\n"
            << "a = true\nb = 1\nc = yes\nd = on\n"
            << "e = false\nf = 0\ng = no\nh = off\n"
            << "i = maybe\n"
            << "vol = 7\n"
            << "bad = twelve\n";
    }
    Config c(path.string());
    ASSERT_TRUE(c.load());
    EXPECT_TRUE(c.getBool("general.a", false));
    EXPECT_TRUE(c.getBool("general.b", false));
    EXPECT_TRUE(c.getBool("general.c", false));
    EXPECT_TRUE(c.getBool("general.d", false));
    EXPECT_FALSE(c.getBool("general.e", true));
    EXPECT_FALSE(c.getBool("general.f", true));
    EXPECT_FALSE(c.getBool("general.g", true));
    EXPECT_FALSE(c.getBool("general.h", true));
    EXPECT_TRUE(c.getBool("general.i", true)); // unknown -> default
    EXPECT_EQ(c.getInt("general.vol", -1), 7);
    EXPECT_EQ(c.getInt("general.bad", -1), -1); // non-numeric -> default
    std::filesystem::remove(path);
}

TEST(Config, FloatParsingFallsBackToDefault)
{
    const auto path = tempPath("float");
    {
        std::ofstream out(path);
        out << "[animation]\n"
            << "duration = 0.3\n"
            << "bad = fast\n";
    }
    Config c(path.string());
    ASSERT_TRUE(c.load());
    EXPECT_DOUBLE_EQ(c.getFloat("animation.duration", 0.15), 0.3);
    EXPECT_DOUBLE_EQ(c.getFloat("animation.bad", 0.15), 0.15); // non-numeric -> default
    EXPECT_DOUBLE_EQ(c.getFloat("animation.missing", 0.15), 0.15);
    std::filesystem::remove(path);
}

TEST(Config, MissingKeysReturnDefaults)
{
    Config c(tempPath("missing").string());
    EXPECT_FALSE(c.load());
    EXPECT_EQ(c.get("nothing.at", "dflt"), "dflt");
    EXPECT_EQ(c.getBool("nothing.at", true), true);
    EXPECT_EQ(c.getInt("nothing.at", 9), 9);
}

TEST(Config, RemoveErasesKeys)
{
    Config c(tempPath("remove").string());
    c.set("board.colors", "blue");
    c.remove("board.colors");
    EXPECT_FALSE(c.has("board.colors"));
    EXPECT_EQ(c.get("board.colors", "classic"), "classic");
}

TEST(Config, SetRequiresDottedKey)
{
    Config c(tempPath("dotted").string());
    c.set("nokey", "x");
    c.set("section.key", "y");
    EXPECT_FALSE(c.has("nokey"));
    EXPECT_EQ(c.get("section.key"), "y");
}

TEST(Config, EmptyPathSaveFails)
{
    Config c("");
    c.set("a.b", "c");
    EXPECT_FALSE(c.save());
}

TEST(Themes, ColorsDistinctPerTheme)
{
    for (int i = 0; i < static_cast<int>(BoardTheme::Count); ++i) {
        auto theme = static_cast<BoardTheme>(i);
        auto colors = boardColorsFor(theme);
        EXPECT_NE(colors.light, colors.dark)
            << boardThemeName(theme);
        for (int j = i + 1; j < static_cast<int>(BoardTheme::Count); ++j) {
            auto other = boardColorsFor(static_cast<BoardTheme>(j));
            EXPECT_NE(colors.light, other.light)
                << boardThemeName(theme) << " vs "
                << boardThemeName(static_cast<BoardTheme>(j));
        }
    }
}

TEST(Themes, NextCyclesThroughCount)
{
    BoardTheme t = BoardTheme::Classic;
    for (int i = 1; i <= static_cast<int>(BoardTheme::Count); ++i)
        t = nextBoardTheme(t);
    EXPECT_EQ(t, BoardTheme::Classic);
}

TEST(Themes, NameRoundTrip)
{
    for (int i = 0; i < static_cast<int>(BoardTheme::Count); ++i) {
        auto theme = static_cast<BoardTheme>(i);
        EXPECT_EQ(boardThemeFromName(boardThemeName(theme)), theme);
    }
    EXPECT_EQ(boardThemeFromName("bogus"), BoardTheme::Classic);
}

TEST(Themes, LabelColorContrasts)
{
    sf::Color all[4] = { {0, 0, 0}, {255, 255, 255}, {60, 120, 180}, {240, 217, 181} };
    for (auto c : all) {
        auto label = labelColorFor(c);
        int l = label.r + label.g + label.b;
        int s = c.r + c.g + c.b;
        // Label luminance must move opposite to the square's.
        EXPECT_TRUE((l > 500) != (s > 500));
    }
}

} // namespace
} // namespace chess::client