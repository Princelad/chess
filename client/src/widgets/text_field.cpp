#include "text_field.h"

#include <ui/rounded_rect.h>

#include <algorithm>

namespace chess::client {

namespace {

std::string truncateForWidth(const std::string& s, const sf::Font& font,
                             unsigned int charSize, float maxWidth)
{
    sf::Text full(font, s, charSize);
    if (full.getLocalBounds().size.x <= maxWidth) return s;

    std::string out = s;
    while (out.size() > 1) {
        out.pop_back();
        if (sf::Text(font, out + "...", charSize).getLocalBounds().size.x <= maxWidth)
            break;
    }
    if (out.size() > 1) out += "...";
    return out;
}

} // namespace

TextField::TextField(sf::FloatRect rect, std::string text)
    : rect_(rect)
    , text_(std::move(text))
{
}

bool TextField::handleEvent(const sf::Event& event, const sf::Vector2f& localMouse)
{
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return false;
        if (enabled_ && rect_.contains(localMouse)) {
            focused_ = true;
            caretVisible_ = true;
            caretBlink_ = 0.f;
            return true;
        }
        if (focused_) focused_ = false;
        return false;
    }

    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (!focused_ || !enabled_) return false;
        if (kp->code == sf::Keyboard::Key::Enter) {
            if (onCommit_) onCommit_();
            return true;
        }
        return false;
    }

    if (const auto* te = event.getIf<sf::Event::TextEntered>()) {
        if (!focused_ || !enabled_) return false;
        const char32_t code = te->unicode;
        if (code == '\b') {
            if (!text_.empty()) text_.pop_back();
        } else if (code >= 32 && code < 127 && text_.size() < maxLength_) {
            text_ += static_cast<char>(code);
        }
        caretVisible_ = true;
        caretBlink_ = 0.f;
        return true;
    }

    return false;
}

void TextField::update(float dtSec)
{
    if (!focused_ || !enabled_) return;
    caretBlink_ += dtSec;
    if (caretBlink_ >= 0.5f) {
        caretBlink_ -= 0.5f;
        caretVisible_ = !caretVisible_;
    }
}

void TextField::draw(sf::RenderWindow& window, const sf::Font& font) const
{
    const bool dimmed = !enabled_;

    // 14.5 field look: dark inset fill (small rounding), no side outline —
    // the state accent is the underline bar below the box.
    ui::RoundedRectShape box(rect_.size, ui::shape::fieldRadius);
    box.setPosition(rect_.position);
    box.setFillColor(focused_ && !dimmed ? style_.fillFocused
                                         : (dimmed ? style_.fillDimmed
                                                   : style_.fillIdle));
    box.setOutlineColor(sf::Color::Transparent);
    box.setOutlineThickness(0.f);
    window.draw(box);

    const float underlineH = focused_ && !dimmed ? 2.5f : 1.5f;
    sf::RectangleShape underline(
        {rect_.size.x - 16.f, underlineH});
    underline.setPosition({rect_.position.x + 8.f,
                           rect_.position.y + rect_.size.y - 5.f});
    underline.setFillColor(dimmed ? style_.strokeDimmed
        : (focused_ ? style_.strokeFocused : style_.strokeIdle));
    window.draw(underline);

    const bool showPlaceholder = text_.empty() && !focused_;
    const std::string shownText = masked_
        ? std::string(text_.size(), '*') : text_;
    const std::string displayText = showPlaceholder ? placeholder_ : shownText;

    sf::Text text(font, truncateForWidth(displayText, font, 20, rect_.size.x - 16.f), 20);
    text.setFillColor(dimmed
        ? style_.textDisabled
        : (showPlaceholder ? style_.placeholder : style_.text));
    text.setPosition({rect_.position.x + 8.f, rect_.position.y + 6.f});
    window.draw(text);

    if (focused_ && !dimmed && caretVisible_ && !showPlaceholder) {
        float caretX = rect_.position.x + 8.f + text.getLocalBounds().size.x + 2.f;
        sf::RectangleShape caret({2.f, rect_.size.y - 8.f});
        caret.setPosition({caretX, rect_.position.y + 4.f});
        caret.setFillColor(style_.caret);
        window.draw(caret);
    }
}

} // namespace chess::client