#include "checkbox.h"

#include <algorithm>

namespace chess::client {

void Checkbox::setChecked(bool checked)
{
    if (checked_ == checked) return;
    checked_ = checked;
    if (onToggle_) onToggle_(checked_);
}

void Checkbox::toggle()
{
    setChecked(!checked_);
}

bool Checkbox::handleEvent(const sf::Event& event, const sf::Vector2f& localMouse)
{
    if (!enabled_) return false;

    if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
        hovered_ = rect_.contains(localMouse);

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return false;
        if (!rect_.contains(localMouse)) { focused_ = false; return false; }
        focused_ = true;
        toggle();
        return true;
    }

    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (!focused_) return false;
        if (kp->code == sf::Keyboard::Key::Space ||
            kp->code == sf::Keyboard::Key::Enter) {
            toggle();
            return true;
        }
    }

    return false;
}

void Checkbox::draw(sf::RenderWindow& window, const sf::Font& font) const
{
    float boxSide = std::min(static_cast<float>(fontSize_) + 5.f, rect_.size.y - 2.f);
    sf::Vector2f boxPos = { rect_.position.x, rect_.position.y + (rect_.size.y - boxSide) * 0.5f };

    // Box background
    sf::RectangleShape bg({ boxSide, boxSide });
    bg.setPosition(boxPos);
    bg.setFillColor(checked_ ? sf::Color(60, 140, 60) : sf::Color(50, 50, 50));
    bg.setOutlineThickness(1.f);
    bg.setOutlineColor(focused_ ? sf::Color(180, 180, 180) :
                       hovered_  ? sf::Color(120, 120, 120) :
                                   sf::Color(80, 80, 80));
    window.draw(bg);

    // Check mark (white filled square inside when checked)
    if (checked_) {
        float pad = boxSide * 0.25f;
        sf::RectangleShape mark({ boxSide - 2.f * pad, boxSide - 2.f * pad });
        mark.setPosition({ boxPos.x + pad, boxPos.y + pad });
        mark.setFillColor(sf::Color(240, 240, 240));
        window.draw(mark);
    }

    // Label
    float labelX = boxPos.x + boxSide + 6.f;
    sf::Text text(font, label_, fontSize_);
    text.setFillColor(focused_ ? sf::Color(240, 240, 240) :
                      enabled_ ? sf::Color(200, 200, 200) :
                                  sf::Color(90, 90, 90));
    auto lb = text.getLocalBounds();
    text.setPosition({ labelX - lb.position.x,
                       rect_.position.y + (rect_.size.y - lb.size.y) * 0.5f - lb.position.y });
    window.draw(text);
}

} // namespace chess::client