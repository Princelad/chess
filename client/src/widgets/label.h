#pragma once

#include <SFML/Graphics.hpp>

#include <string>

namespace chess::client {

class Label {
public:
    Label() = default;
    Label(std::string text, unsigned int fontSize = 14,
          sf::Color color = sf::Color(220, 220, 220));

    void setText(std::string text) { text_ = std::move(text); }
    const std::string& text() const { return text_; }

    void setFontSize(unsigned int size) { fontSize_ = size; }
    unsigned int fontSize() const { return fontSize_; }

    void setColor(sf::Color color) { color_ = color; }
    sf::Color color() const { return color_; }

    void setPosition(sf::Vector2f pos) { position_ = pos; }
    sf::Vector2f position() const { return position_; }

    // Natural width/height given the font actually used for drawing.
    sf::Vector2f size(const sf::Font& font) const;
    sf::FloatRect bounds(const sf::Font& font) const;

    void draw(sf::RenderWindow& window, const sf::Font& font) const;

private:
    std::string text_;
    unsigned int fontSize_ = 14;
    sf::Color color_ = sf::Color(220, 220, 220);
    sf::Vector2f position_ = {0.f, 0.f};
};

} // namespace chess::client