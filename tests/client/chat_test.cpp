// 11.8 Chat polish helpers: local "HH:MM" timestamps, the deterministic
// light palette per sender name, and the config-persisted per-player mute
// list (client-local only — the server never sees it).
#include "chat_log.h"

#include <cstdlib>
#include <filesystem>

#include <gtest/gtest.h>

namespace chess::client {
namespace {

class ChatTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Deterministic wall-clock zone for the timestamp tests (Linux
        // target; nothing else in the suite reads local time).
        ::setenv("TZ", "UTC", 1);
        ::tzset();
    }
};

TEST_F(ChatTest, TimestampFormatsHourMinute)
{
    EXPECT_EQ(chatTimestamp(0), "00:00");
    EXPECT_EQ(chatTimestamp(59), "00:00");
    EXPECT_EQ(chatTimestamp(60), "00:01");
    EXPECT_EQ(chatTimestamp(3661), "01:01");
    EXPECT_EQ(chatTimestamp(86399), "23:59");
    EXPECT_EQ(chatTimestamp(86400), "00:00") << "hours wrap per day";
}

TEST_F(ChatTest, TimestampNeverTruncated)
{
    // strftime's %H:%M always gives two digits per field (padding via fmt).
    EXPECT_EQ(chatTimestamp(60 * 9 + 5), "00:09");
    EXPECT_EQ(chatTimestamp(60 * 61 + 5), "01:01");
}

TEST(ChatNameColor, DeterministicAndInPalette)
{
    const sf::Color a = chatNameColor("Agata");
    EXPECT_EQ(a, chatNameColor("Agata")) << "same name, same color";
    EXPECT_EQ(a, chatNameColor(std::string("Agata")));

    // The result is one of the palette entries: hash again through a small
    // wrapper is circular, so instead verify stability across both empty
    // and non-empty names and that every queried color is light enough to
    // read on the dark panel (each channel > 100 keeps it out of the
    // background's value range).
    for (const std::string& name : {"", "A", "Bot-1", "a-very-long-name-00"}) {
        const sf::Color c = chatNameColor(name);
        EXPECT_GE(c.r, 100);
        EXPECT_GE(c.g, 100);
        EXPECT_GE(c.b, 100);
    }
}

TEST(ChatMute, RoundTripsThroughConfig)
{
    const auto path = std::filesystem::temp_directory_path()
        / "chess_chat_mute_test_roundtrip";
    std::filesystem::remove(path);
    {
        Config c(path.string());
        // chatToggleMute returns the name's NEW state.
        EXPECT_TRUE(chatToggleMute(c, "Bot-1")) << "first toggle mutes";
        EXPECT_TRUE(chatIsMuted(chatMutedFromConfig(c), "Bot-1"));
        ASSERT_TRUE(c.save());
    }
    {
        Config c(path.string());
        ASSERT_TRUE(c.load());
        EXPECT_TRUE(chatIsMuted(chatMutedFromConfig(c), "Bot-1"))
            << "mute survives a restart";
        // Toggling a different name leaves the other entries untouched;
        // un-toggling drops only that name.
        EXPECT_TRUE(chatToggleMute(c, "Bot-2"));
        EXPECT_TRUE(chatIsMuted(chatMutedFromConfig(c), "Bot-1"));
        EXPECT_FALSE(chatToggleMute(c, "Bot-1"));
        EXPECT_FALSE(chatIsMuted(chatMutedFromConfig(c), "Bot-1"));
        EXPECT_TRUE(chatIsMuted(chatMutedFromConfig(c), "Bot-2"));
        ASSERT_TRUE(c.save());
    }
    {
        Config c(path.string());
        ASSERT_TRUE(c.load());
        EXPECT_FALSE(chatIsMuted(chatMutedFromConfig(c), "Bot-1"));
        EXPECT_TRUE(chatIsMuted(chatMutedFromConfig(c), "Bot-2"));
    }
    std::filesystem::remove(path);
}

TEST(ChatMute, CsvParsingSkipsBlanks)
{
    const auto path = std::filesystem::temp_directory_path()
        / "chess_chat_mute_test_csv";
    std::filesystem::remove(path);
    Config c(path.string());
    c.set("chat.muted", "Bot-1,, Bot-2,");
    auto names = chatMutedFromConfig(c);
    ASSERT_EQ(names.size(), 2u) << "blank segments never become entries";
    EXPECT_EQ(names[0], "Bot-1");
    // Segments are matched verbatim (the toggle writer always emits the
    // exact names it stored, so stray spaces only appear in hand-edited
    // configs and still behave as their own name there).
    EXPECT_TRUE(chatIsMuted(names, "Bot-1"));
    EXPECT_TRUE(chatIsMuted(names, " Bot-2"));
    EXPECT_FALSE(chatIsMuted(names, "Bot-2"));
    std::filesystem::remove(path);
}

// 11.8 audit: a name the INI store would rewrite on reload must never
// persist — Config::load strips #/; comments, splits on '=' and trims edge
// blanks, so such a name mutes a different string after a restart. The
// mute command rejects them at the boundary instead.
TEST(ChatMute, PersistableNameGate)
{
    EXPECT_TRUE(chatMuteNamePersistable("Bot-1"));
    EXPECT_TRUE(chatMuteNamePersistable("Bot (2)"));
    EXPECT_FALSE(chatMuteNamePersistable(""));
    EXPECT_FALSE(chatMuteNamePersistable("Bo#t"));   // comment strip
    EXPECT_FALSE(chatMuteNamePersistable("Bo;t"));   // comment strip
    EXPECT_FALSE(chatMuteNamePersistable("Bo=t"));   // key=value split
    EXPECT_FALSE(chatMuteNamePersistable("[section]"));
    EXPECT_FALSE(chatMuteNamePersistable("Bo\nt"));  // control char
    EXPECT_FALSE(chatMuteNamePersistable(" Bo"));    // edge blank, trimmed
    EXPECT_FALSE(chatMuteNamePersistable("Bo "));    // edge blank, trimmed
}

} // namespace
} // namespace chess::client
