#include "menu_screen.h"
#include "connect_screen.h"
#include "local_game_screen.h"

#include <cstdlib>

namespace chess::client {

namespace {
constexpr float TitleY = 80.f;
constexpr float MenuStartY = 200.f;
}

MenuScreen::MenuScreen(App& app)
    : app_(app)
{
}

float MenuScreen::entryX() const
{
    return (App::WindowWidth - EntryW) / 2.f;
}

float MenuScreen::entryY(int index) const
{
    return MenuStartY + index * (EntryH + EntryGap);
}

bool MenuScreen::isDisabled(int index) const
{
    return index >= 2;
}

void MenuScreen::activateEntry(int index)
{
    if (index < 0 || index >= EntryCount || isDisabled(index)) return;

    if (index == 0) {
        app_.pushScreen(std::make_unique<ConnectScreen>(app_));
    } else if (index == 1) {
        const char* env = std::getenv("CHESS_ENGINE_PATH");
        std::string enginePath = env ? env : "stockfish";
        app_.pushScreen(std::make_unique<LocalGameScreen>(
            app_, Color::White, std::move(enginePath), 5));
    }
}

void MenuScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) return;

        if (kp->code == sf::Keyboard::Key::Up) {
            for (int i = hovered_ - 1; i >= 0; --i) {
                if (!isDisabled(i)) { hovered_ = i; return; }
            }
            return;
        }
        if (kp->code == sf::Keyboard::Key::Down) {
            for (int i = hovered_ + 1; i < EntryCount; ++i) {
                if (!isDisabled(i)) { hovered_ = i; return; }
            }
            return;
        }

        if (kp->code == sf::Keyboard::Key::Enter) {
            if (hovered_ < 0) hovered_ = 0;
            activateEntry(hovered_);
            return;
        }
    }

    if (const auto* mm = event.getIf<sf::Event::MouseMoved>()) {
        float mx = static_cast<float>(mm->position.x);
        float my = static_cast<float>(mm->position.y);
        float x0 = entryX();
        hovered_ = -1;
        for (int i = 0; i < EntryCount; ++i) {
            float y0 = entryY(i);
            if (mx >= x0 && mx <= x0 + EntryW && my >= y0 && my <= y0 + EntryH) {
                hovered_ = i;
                break;
            }
        }
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            activateEntry(hovered_);
        }
    }
}

void MenuScreen::update(float /*dtSec*/) {}

void MenuScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();
    float cx = entryX();

    sf::Text title(font, "Chess", 36);
    title.setFillColor(sf::Color(255, 255, 255));
    auto tb = title.getGlobalBounds();
    title.setPosition({(App::WindowWidth - tb.size.x) / 2.f - tb.position.x, TitleY});
    window.draw(title);

    const char* labels[EntryCount] = {
        "Play Online",
        "Play vs Computer",
        "Puzzles",
        "Archive",
        "Settings"
    };

    for (int i = 0; i < EntryCount; ++i) {
        float y0 = entryY(i);
        bool disabled = isDisabled(i);
        bool hovered = (i == hovered_) && !disabled;

        sf::RectangleShape rect({EntryW, EntryH});
        rect.setPosition({cx, y0});
        if (disabled) {
            rect.setFillColor(sf::Color(40, 38, 36));
            rect.setOutlineColor(sf::Color(55, 53, 51));
        } else if (hovered) {
            rect.setFillColor(sf::Color(70, 68, 66));
            rect.setOutlineColor(sf::Color(140, 140, 140));
        } else {
            rect.setFillColor(sf::Color(58, 56, 54));
            rect.setOutlineColor(sf::Color(80, 78, 76));
        }
        rect.setOutlineThickness(1.f);
        window.draw(rect);

        sf::Text label(font, labels[i], 20);
        label.setFillColor(disabled
            ? sf::Color(90, 90, 90)
            : sf::Color(220, 220, 220));
        auto lb = label.getGlobalBounds();
        label.setPosition({
            cx + (EntryW - lb.size.x) / 2.f - lb.position.x,
            y0 + (EntryH - lb.size.y) / 2.f - lb.position.y
        });
        window.draw(label);
    }

    sf::Text hint(font, "Mouse or arrow keys + Enter", 14);
    hint.setFillColor(sf::Color(100, 100, 100));
    auto hb = hint.getGlobalBounds();
    hint.setPosition({
        (App::WindowWidth - hb.size.x) / 2.f - hb.position.x,
        entryY(EntryCount - 1) + EntryH + 40.f
    });
    window.draw(hint);
}

} // namespace chess::client
