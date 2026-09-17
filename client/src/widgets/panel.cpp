#include "panel.h"

namespace chess::client {

Panel::Panel(sf::FloatRect rect, sf::Color fill, sf::Color outline,
             float outlineThickness)
    : rect_(rect)
    , fill_(fill)
    , outline_(outline)
    , outlineThickness_(outlineThickness)
{
}

void Panel::draw(sf::RenderWindow& window) const
{
    sf::RectangleShape shape(rect_.size);
    shape.setPosition(rect_.position);
    shape.setFillColor(fill_);
    shape.setOutlineColor(outline_);
    shape.setOutlineThickness(outlineThickness_);
    window.draw(shape);
}

} // namespace chess::client