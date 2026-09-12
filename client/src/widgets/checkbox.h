#pragma once

#include <SFML/Graphics.hpp>

#include <functional>
#include <string>

namespace chess::client {

class Checkbox {
public:
    Checkbox() = default;

    void setRect(sf::FloatRect rect) { rect_ = rect; }
    sf::FloatRect rect() const { return rect_; }

    void setLabel(std::string label) { label_ = std::move(label); }
    const std::string& label() const { return label_; }

    void setFontSize(unsigned int size) { fontSize_ = size; }

    void setChecked(bool checked);
    bool isChecked() const { return checked_; }

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

    void setFocused(bool focused) { focused_ = focused && enabled_; }
    bool isFocused() const { return focused_; }

    void setOnToggle(std::function<void(bool)> onToggle) { onToggle_ = std::move(onToggle); }

    // Returns true if the event was consumed (click toggle or Space/Enter while focused).
    bool handleEvent(const sf::Event& event, const sf::Vector2f& localMouse);

    void draw(sf::RenderWindow& window, const sf::Font& font) const;

private:
    void toggle();

    sf::FloatRect rect_;
    std::string label_;
    unsigned int fontSize_ = 13;
    bool checked_ = false;
    bool enabled_ = true;
    bool focused_ = false;
    bool hovered_ = false;
    std::function<void(bool)> onToggle_;
};

} // namespace chess::client