#include "label.h"

namespace chess::client {

Label::Label(std::string text, unsigned int fontSize, sf::Color color)
    : text_(std::move(text))
    , fontSize_(fontSize)
    , color_(color)
{
}

sf::FloatRect Label::bounds(const sf::Font& font) const
{
    return sf::Text(font, text_, fontSize_).getLocalBounds();
}

sf::Vector2f Label::size(const sf::Font& font) const
{
    const auto b = bounds(font);
    return { b.size.x, b.size.y };
}

void Label::draw(sf::RenderWindow& window, const sf::Font& font) const
{
    sf::Text drawable(font, text_, fontSize_);
    drawable.setFillColor(color_);
    drawable.setPosition(position_);
    window.draw(drawable);
}

} // namespace chess::client