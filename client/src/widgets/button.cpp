#include "button.h"

#include <SFML/Graphics/RenderWindow.hpp>

namespace chess::client {

Button::Button(sf::FloatRect rect, std::string label)
    : rect_(rect)
    , label_(std::move(label))
{
}

void Button::activate()
{
    if (!enabled_) return;
    if (onClick_) onClick_();
}

bool Button::handleEvent(const sf::Event& event, const sf::Vector2f& localMouse)
{
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>()) {
        (void)mm;
        hovered_ = enabled_ && rect_.contains(localMouse);
        return false;
    }
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (!enabled_ || mb->button != sf::Mouse::Button::Left) return false;
        if (rect_.contains(localMouse)) {
            pressedIn_ = true;
            return true;
        }
        return false;
    }
    if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (rb->button != sf::Mouse::Button::Left) return false;
        bool wasPressedIn = pressedIn_;
        pressedIn_ = false;
        if (wasPressedIn && rect_.contains(localMouse)) {
            activate();
            return true;
        }
        return false;
    }
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (!focused_ || !enabled_) return false;
        if (kp->code == sf::Keyboard::Key::Enter ||
            kp->code == sf::Keyboard::Key::Space) {
            activate();
            return true;
        }
        return false;
    }
    return false;
}

void Button::draw(sf::RenderWindow& window, const sf::Font& font) const
{
    sf::Color fill;
    sf::Color outline;
    sf::Color labelColor;
    float thickness = 1.f;

    if (!enabled_) {
        fill = style_.disabledFill;
        outline = style_.outline;
        labelColor = style_.disabledLabel;
    } else if (pressedIn_) {
        fill = style_.pressedFill;
        outline = style_.activeOutline;
        labelColor = style_.label;
    } else if (hovered_) {
        fill = style_.hoverFill;
        outline = style_.activeOutline;
        labelColor = style_.label;
    } else {
        fill = style_.idleFill;
        outline = style_.outline;
        labelColor = style_.label;
    }

    if (focused_) {
        outline = style_.focusOutline;
        thickness = 2.f;
    }

    sf::RectangleShape shape(rect_.size);
    shape.setPosition(rect_.position);
    shape.setFillColor(fill);
    shape.setOutlineColor(outline);
    shape.setOutlineThickness(thickness);
    window.draw(shape);

    const sf::FloatRect* textArea = &rect_;
    if (icon_) {
        const sf::FloatRect slot = ui::icons::iconSlotRect(rect_, style_.textSize);
        const sf::FloatRect remaining = ui::icons::iconTextRect(rect_, style_.textSize);

        // Probe the label against the remaining space first — a label too
        // wide there keeps the whole rect (icon drops, text never overlaps).
        sf::Text probe(font, label_, style_.textSize);
        if (probe.getLocalBounds().size.x <= remaining.size.x - 4.f) {
            ui::icons::drawIcon(window, *icon_, slot.position, slot.size.x,
                                labelColor);
            textArea = &remaining;
        }
    }

    sf::Text text(font, label_, style_.textSize);
    text.setFillColor(labelColor);
    const auto bounds = text.getLocalBounds();
    text.setPosition({
        textArea->position.x + (textArea->size.x - bounds.size.x) / 2.f - bounds.position.x,
        textArea->position.y + (textArea->size.y - bounds.size.y) / 2.f - bounds.position.y
    });
    window.draw(text);
}

} // namespace chess::client