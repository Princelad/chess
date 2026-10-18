#include <chess/db/auth.h>

#include <algorithm>

#include <gtest/gtest.h>

namespace chess::db {
namespace {

TEST(PasswordHash, RoundTripVerifies)
{
    const std::string stored = hashPassword("correct horse");
    EXPECT_TRUE(verifyPassword("correct horse", stored));
    EXPECT_FALSE(verifyPassword("wrong horse", stored));
    EXPECT_FALSE(verifyPassword("", stored));
}

TEST(PasswordHash, PrefixDescribesParameters)
{
    const std::string stored = hashPassword("pw");
    EXPECT_EQ(stored.rfind("pbkdf2-sha256$", 0), 0u);
    // three variable parts after the scheme name
    EXPECT_EQ(std::count(stored.begin(), stored.end(), '$'), 3);
}

TEST(PasswordHash, FreshSaltEveryCall)
{
    const auto first = hashPassword("same");
    const auto second = hashPassword("same");
    EXPECT_NE(first, second);
    // Both still verify.
    EXPECT_TRUE(verifyPassword("same", first));
    EXPECT_TRUE(verifyPassword("same", second));
}

TEST(PasswordHash, RejectsMalformedStoredHashes)
{
    for (const std::string& stored : {
             "",                                  // empty
             "shasomething",                      // no prefix
             "pbkdf2-sha256$",                    // no parts
             "pbkdf2-sha256$0$AAAA$BBBB",         // iteration count < 1
             "pbkdf2-sha256$abc$AAAA$BBBB",       // non-numeric iterations
             "pbkdf2-sha256$100000$$$"            // empty salt/hash
         }) {
        EXPECT_FALSE(verifyPassword("pw", stored)) << "stored: " << stored;
    }
}

TEST(PasswordHash, TamperedStorageFails)
{
    std::string stored = hashPassword("pw");
    // Flip one character of the hash part.
    stored.back() = stored.back() == 'A' ? 'B' : 'A';
    EXPECT_FALSE(verifyPassword("pw", stored));
}

TEST(PasswordHash, ConstantTimeEquals)
{
    EXPECT_TRUE(constantTimeEquals("", ""));
    EXPECT_TRUE(constantTimeEquals("abc", "abc"));
    EXPECT_FALSE(constantTimeEquals("abc", "abd"));
    EXPECT_FALSE(constantTimeEquals("abc", "ab"));
    EXPECT_FALSE(constantTimeEquals("abc", "abcd"));
}

TEST(AccountRules, NameValidation)
{
    EXPECT_TRUE(validAccountName("Alice"));
    EXPECT_TRUE(validAccountName("chip_chip_42"));
    EXPECT_FALSE(validAccountName(""));
    EXPECT_FALSE(validAccountName(std::string(33, 'a')));
    EXPECT_TRUE(validAccountName(std::string(32, 'a')));
    EXPECT_TRUE(validAccountName("sp ace")); // spaces are printable characters
}

TEST(AccountRules, PasswordValidation)
{
    EXPECT_TRUE(validPassword("pw"));
    EXPECT_TRUE(validPassword("with space"));
    EXPECT_FALSE(validPassword(""));
    EXPECT_FALSE(validPassword(std::string(65, 'a')));
    EXPECT_TRUE(validPassword(std::string(64, 'a')));
    EXPECT_FALSE(validPassword(std::string("\nline", 5)));
}

} // namespace
} // namespace chess::db
