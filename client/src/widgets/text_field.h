#pragma once

#include <SFML/Graphics.hpp>

#include <cstddef>
#include <functional>
#include <string>

namespace chess::client {

class TextField {
public:
    TextField() = default;
    TextField(sf::FloatRect rect, std::string text = {});

    void setRect(sf::FloatRect rect) { rect_ = rect; }
    sf::FloatRect rect() const { return rect_; }

    const std::string& text() const { return text_; }
    void setText(std::string text) { text_ = std::move(text); }

    void setMaxLength(std::size_t maxLength) { maxLength_ = maxLength; }
    void setPlaceholder(std::string placeholder) { placeholder_ = std::move(placeholder); }

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

    void setFocused(bool focused)
    {
        focused_ = focused;
        if (focused) caretVisible_ = true;
    }
    bool isFocused() const { return focused_; }

    // Optional callback fired when Enter is pressed while focused.
    void setOnCommit(std::function<void()> onCommit) { onCommit_ = std::move(onCommit); }

    // Returns true if the event was consumed by the field.
    bool handleEvent(const sf::Event& event, const sf::Vector2f& localMouse);

    void update(float dtSec);
    void draw(sf::RenderWindow& window, const sf::Font& font) const;

private:
    sf::FloatRect rect_;
    std::string text_;
    std::size_t maxLength_ = 64;
    std::string placeholder_;
    bool enabled_ = true;
    bool focused_ = false;
    bool caretVisible_ = true;
    float caretBlink_ = 0.f;
    std::function<void()> onCommit_;
};

} // namespace chess::client