#include "button.h"

namespace chess::client {

Button::Button(sf::FloatRect rect, std::string label)
    : rect_(rect)
    , label_(std::move(label))
{
}

void Button::setColors(sf::Color idleFill, sf::Color hoverFill, sf::Color pressedFill,
                       sf::Color disabledFill, sf::Color disabledLabel,
                       sf::Color label, sf::Color outline, sf::Color activeOutline,
                       sf::Color focusOutline)
{
    idleFill_ = idleFill;
    hoverFill_ = hoverFill;
    pressedFill_ = pressedFill;
    disabledFill_ = disabledFill;
    disabledLabel_ = disabledLabel;
    labelColor_ = label;
    outline_ = outline;
    activeOutline_ = activeOutline;
    focusOutline_ = focusOutline;
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
        fill = disabledFill_;
        outline = outline_;
        labelColor = disabledLabel_;
    } else if (pressedIn_) {
        fill = pressedFill_;
        outline = activeOutline_;
        labelColor = labelColor_;
    } else if (hovered_) {
        fill = hoverFill_;
        outline = activeOutline_;
        labelColor = labelColor_;
    } else {
        fill = idleFill_;
        outline = outline_;
        labelColor = labelColor_;
    }

    if (focused_) {
        outline = focusOutline_;
        thickness = 2.f;
    }

    sf::RectangleShape shape(rect_.size);
    shape.setPosition(rect_.position);
    shape.setFillColor(fill);
    shape.setOutlineColor(outline);
    shape.setOutlineThickness(thickness);
    window.draw(shape);

    sf::Text text(font, label_, 20);
    text.setFillColor(labelColor);
    const auto bounds = text.getLocalBounds();
    text.setPosition({
        rect_.position.x + (rect_.size.x - bounds.size.x) / 2.f - bounds.position.x,
        rect_.position.y + (rect_.size.y - bounds.size.y) / 2.f - bounds.position.y
    });
    window.draw(text);
}

} // namespace chess::client