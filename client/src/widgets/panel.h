#pragma once

#include <SFML/Graphics.hpp>

namespace chess::client {

class Panel {
public:
    Panel() = default;
    Panel(sf::FloatRect rect,
          sf::Color fill = sf::Color(30, 30, 30),
          sf::Color outline = sf::Color(80, 80, 80),
          float outlineThickness = 1.f);

    void setRect(sf::FloatRect rect) { rect_ = rect; }
    sf::FloatRect rect() const { return rect_; }
    void setFill(sf::Color fill) { fill_ = fill; }
    void setOutline(sf::Color outline) { outline_ = outline; }
    void setOutlineThickness(float thickness) { outlineThickness_ = thickness; }

    bool contains(sf::Vector2f point) const { return rect_.contains(point); }

    void draw(sf::RenderWindow& window) const;

private:
    sf::FloatRect rect_;
    sf::Color fill_ = sf::Color(30, 30, 30);
    sf::Color outline_ = sf::Color(80, 80, 80);
    float outlineThickness_ = 1.f;
};

} // namespace chess::client