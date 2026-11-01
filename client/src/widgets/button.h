#pragma once

#include <SFML/Graphics.hpp>
#include <ui/icons.h>
#include <ui/theme.h>

#include <functional>
#include <optional>
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

    void setStyle(const ui::ButtonStyle& style) { style_ = style; }
    const ui::ButtonStyle& style() const { return style_; }

    // Decorative leading icon (14.2). The label always keeps rendering
    // alongside it (letter-fallback discipline); if the remaining text
    // space would collide, the icon silently drops instead.
    void setIcon(std::optional<ui::icons::IconKind> icon) { icon_ = icon; }
    void clearIcon() { icon_.reset(); }

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
    std::optional<ui::icons::IconKind> icon_;

    ui::ButtonStyle style_; // zero-init == theme::buttonNeutral() values
};

} // namespace chess::client