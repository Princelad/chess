#pragma once

#include <SFML/Graphics.hpp>

#include <functional>
#include <string>

namespace chess::client {

class Button {
public:
    Button() = default;
    Button(sf::FloatRect rect, std::string label);

    void setRect(sf::FloatRect rect) { rect_ = rect; }
    sf::FloatRect rect() const { return rect_; }

    void setLabel(std::string label) { label_ = std::move(label); }
    const std::string& label() const { return label_; }

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

    void setFocusable(bool focusable)
    {
        focusable_ = focusable;
        if (!focusable_) focused_ = false;
    }
    void setFocused(bool focused)
    {
        focused_ = focused && focusable_ && enabled_;
    }
    bool isFocused() const { return focused_; }

    void setOnClick(std::function<void()> onClick) { onClick_ = std::move(onClick); }

    void setColors(sf::Color idleFill, sf::Color hoverFill, sf::Color pressedFill,
                   sf::Color disabledFill, sf::Color disabledLabel,
                   sf::Color label, sf::Color outline, sf::Color activeOutline,
                   sf::Color focusOutline);

    bool isHovered() const { return hovered_; }
    bool isPressed() const { return pressedIn_; }

    // Returns true if the event was consumed by the button (click activation,
    // press start inside, or Enter/Space while focused).
    bool handleEvent(const sf::Event& event, const sf::Vector2f& localMouse);

    void draw(sf::RenderWindow& window, const sf::Font& font) const;

private:
    void activate();

    sf::FloatRect rect_;
    std::string label_;
    bool enabled_ = true;
    bool focusable_ = true;
    bool focused_ = false;
    bool hovered_ = false;
    bool pressedIn_ = false;
    std::function<void()> onClick_;

    sf::Color idleFill_ = sf::Color(58, 56, 54);
    sf::Color hoverFill_ = sf::Color(70, 68, 66);
    sf::Color pressedFill_ = sf::Color(50, 48, 46);
    sf::Color disabledFill_ = sf::Color(40, 38, 36);
    sf::Color disabledLabel_ = sf::Color(90, 90, 90);
    sf::Color labelColor_ = sf::Color(220, 220, 220);
    sf::Color outline_ = sf::Color(80, 78, 76);
    sf::Color activeOutline_ = sf::Color(140, 140, 140);
    sf::Color focusOutline_ = sf::Color(180, 180, 180);
};

} // namespace chess::client