#pragma once

#include <SFML/Window/Keyboard.hpp>

#include <utility>

namespace chess::client {

// Key commands for keyboard play (phase 9.6). Mapped from KeyPressed events
// by `keyToAction`; pure and unit-testable.
enum class KeyboardAction {
    None,
    MoveCursor,
    Enter,
    Deselect,
    Flip,
    Help,
    PrevMove,
    NextMove,
};

inline KeyboardAction keyToAction(sf::Keyboard::Key key, bool shift)
{
    switch (key) {
        case sf::Keyboard::Key::Up:
        case sf::Keyboard::Key::Down:
        case sf::Keyboard::Key::Left:
        case sf::Keyboard::Key::Right:
            return KeyboardAction::MoveCursor;
        case sf::Keyboard::Key::Enter:
            return KeyboardAction::Enter;
        case sf::Keyboard::Key::Space:
            return KeyboardAction::Deselect;
        case sf::Keyboard::Key::Escape:
            return KeyboardAction::Deselect;
        case sf::Keyboard::Key::F:
            return KeyboardAction::Flip;
        case sf::Keyboard::Key::Slash:
            // '?' is Shift+'/' on many layouts; accept either so all
            // keyboards can open the help overlay.
            return KeyboardAction::Help;
        case sf::Keyboard::Key::LBracket:
            return KeyboardAction::PrevMove;
        case sf::Keyboard::Key::RBracket:
            return KeyboardAction::NextMove;
        default:
            (void)shift;
            return KeyboardAction::None;
    }
}

// Move a cursor one step along `file` or `rank`, wrapping at the board edges.
// `dx`/`dy` are in board-logical terms: `dx=1` increases the file (toward the
// h-file), `dy=1` increases the rank.
inline std::pair<int, int> moveCursorStep(int file, int rank, int dx, int dy)
{
    return {
        (file + dx + 8) % 8,
        (rank + dy + 8) % 8,
    };
}

// Map an arrow key to a (dx, dy) logical board step.
inline std::pair<int, int> cursorDelta(sf::Keyboard::Key key)
{
    switch (key) {
        case sf::Keyboard::Key::Up:    return { 0, 1 };
        case sf::Keyboard::Key::Down:  return { 0, -1 };
        case sf::Keyboard::Key::Left:  return { -1, 0 };
        case sf::Keyboard::Key::Right: return { 1, 0 };
        default:                       return { 0, 0 };
    }
}

} // namespace chess::client