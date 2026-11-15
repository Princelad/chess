#pragma once

#include <SFML/Graphics.hpp>
#include <ui/theme.h>

#include <cstddef>
#include <functional>
#include <string>

namespace chess::client {

// Text entry charset (audit): Ascii accepts printable ASCII only — names,
// passwords and hosts are hard-ASCII server-side (validAccountName /
// validPassword refuse anything else). Utf8 accepts any printable Unicode
// codepoint encoded to UTF-8, for chat lines and file paths.
enum class InputMode { Ascii, Utf8 };

// Encodes one Unicode codepoint as UTF-8 bytes. Pure — the headless test
// suite covers the sequence shapes without an event or a font.
inline std::string utf8Encode(char32_t cp)
{
    std::string out;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

// Pops the last UTF-8 sequence (one backspace step): trailing continuation
// bytes first, then their lead byte. A byte-only tail (truncated sequence
// carried in via setText) still shrinks by one byte.
inline void utf8PopBack(std::string& text)
{
    while (!text.empty()
           && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80)
        text.pop_back();
    if (!text.empty()) text.pop_back();
}

// The charset gate a TextEntered codepoint must pass. Both modes refuse
// control characters and DEL; Utf8 also refuses surrogate halves (never a
// valid codepoint) so the encoder cannot emit them.
inline bool inputAccepts(InputMode mode, char32_t cp)
{
    if (cp < 0x20 || cp == 0x7F) return false;
    if (cp >= 0xD800 && cp <= 0xDFFF) return false;
    return mode == InputMode::Utf8 || cp < 0x80;
}

class TextField {
public:
    TextField() = default;
    TextField(sf::FloatRect rect, std::string text = {});

    void setRect(sf::FloatRect rect) { rect_ = rect; }
    sf::FloatRect rect() const { return rect_; }

    const std::string& text() const { return text_; }
    void setText(std::string text) { text_ = std::move(text); }

    void setMaxLength(std::size_t maxLength) { maxLength_ = maxLength; }
    void setPlaceholder(std::string placeholder) { placeholder_ = std::move(placeholder); }

    // Defaults to Ascii; chat input and path fields opt into Utf8.
    void setInputMode(InputMode mode) { inputMode_ = mode; }
    InputMode inputMode() const { return inputMode_; }

    // Masks typed characters as '*' (password entry). text() stays readable.
    void setMasked(bool masked) { masked_ = masked; }
    bool isMasked() const { return masked_; }

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

    void setFocused(bool focused)
    {
        focused_ = focused;
        if (focused) caretVisible_ = true;
    }
    bool isFocused() const { return focused_; }

    // Optional callback fired when Enter is pressed while focused.
    void setOnCommit(std::function<void()> onCommit) { onCommit_ = std::move(onCommit); }

    void setStyle(const ui::TextFieldStyle& style) { style_ = style; }

    // Returns true if the event was consumed by the field.
    bool handleEvent(const sf::Event& event, const sf::Vector2f& localMouse);

    void update(float dtSec);
    void draw(sf::RenderWindow& window, const sf::Font& font) const;

private:
    sf::FloatRect rect_;
    ui::TextFieldStyle style_;
    std::string text_;
    std::size_t maxLength_ = 64;
    std::string placeholder_;
    InputMode inputMode_ = InputMode::Ascii;
    bool enabled_ = true;
    bool masked_ = false;
    bool focused_ = false;
    bool caretVisible_ = true;
    float caretBlink_ = 0.f;
    std::function<void()> onCommit_;
};

} // namespace chess::client