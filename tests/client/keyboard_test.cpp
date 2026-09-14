#include "keyboard.h"

#include <gtest/gtest.h>

namespace chess::client {
namespace {

TEST(KeyboardCursor, MovesInAllDirections)
{
    EXPECT_EQ(moveCursorStep(4, 4, 1, 0), std::make_pair(5, 4));
    EXPECT_EQ(moveCursorStep(4, 4, -1, 0), std::make_pair(3, 4));
    EXPECT_EQ(moveCursorStep(4, 4, 0, 1), std::make_pair(4, 5));
    EXPECT_EQ(moveCursorStep(4, 4, 0, -1), std::make_pair(4, 3));
}

TEST(KeyboardCursor, WrapsAtEdges)
{
    EXPECT_EQ(moveCursorStep(7, 0, 1, 0), std::make_pair(0, 0));
    EXPECT_EQ(moveCursorStep(0, 0, -1, 0), std::make_pair(7, 0));
    EXPECT_EQ(moveCursorStep(0, 7, 0, 1), std::make_pair(0, 0));
    EXPECT_EQ(moveCursorStep(0, 0, 0, -1), std::make_pair(0, 7));
    // Corner diagonal wrap via two steps.
    EXPECT_EQ(moveCursorStep(7, 7, 1, 1), std::make_pair(0, 0));
}

TEST(KeyboardCursor, DeltaMatchesKeys)
{
    EXPECT_EQ(cursorDelta(sf::Keyboard::Key::Up), std::make_pair(0, 1));
    EXPECT_EQ(cursorDelta(sf::Keyboard::Key::Down), std::make_pair(0, -1));
    EXPECT_EQ(cursorDelta(sf::Keyboard::Key::Left), std::make_pair(-1, 0));
    EXPECT_EQ(cursorDelta(sf::Keyboard::Key::Right), std::make_pair(1, 0));
}

TEST(KeyboardAction, MapsBoundKeys)
{
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::Up, false), KeyboardAction::MoveCursor);
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::Down, false), KeyboardAction::MoveCursor);
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::Left, false), KeyboardAction::MoveCursor);
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::Right, false), KeyboardAction::MoveCursor);
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::Enter, false), KeyboardAction::Enter);
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::Space, false), KeyboardAction::Deselect);
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::Escape, false), KeyboardAction::Deselect);
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::F, false), KeyboardAction::Flip);
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::LBracket, false), KeyboardAction::PrevMove);
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::RBracket, false), KeyboardAction::NextMove);
}

TEST(KeyboardAction, QuestionMarkTogglesHelpWithOrWithoutShift)
{
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::Slash, true), KeyboardAction::Help);
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::Slash, false), KeyboardAction::Help);
}

TEST(KeyboardAction, UnboundKeysReturnNone)
{
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::A, false), KeyboardAction::None);
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::Num1, false), KeyboardAction::None);
    EXPECT_EQ(keyToAction(sf::Keyboard::Key::Home, true), KeyboardAction::None);
}

} // namespace
} // namespace chess::client