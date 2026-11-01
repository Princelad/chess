#include "panel.h"

#include <ui/rounded_rect.h>

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
    // 14.5 chrome: rounded panel from the shared shape.
    ui::RoundedRectShape shape(rect_.size, ui::shape::panelRadius);
    shape.setPosition(rect_.position);
    shape.setFillColor(fill_);
    shape.setOutlineColor(outline_);
    shape.setOutlineThickness(outlineThickness_);
    window.draw(shape);
}

} // namespace chess::client