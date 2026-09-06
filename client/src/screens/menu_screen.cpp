#include "menu_screen.h"
#include "connect_screen.h"
#include "local_game_screen.h"
#include "widgets/layout.h"

#include <cstdlib>

namespace chess::client {

namespace {
constexpr float TitleY = 80.f;
constexpr float MenuStartY = 200.f;
}

MenuScreen::MenuScreen(App& app)
    : app_(app)
{
    static const char* Labels[EntryCount] = {
        "Play Online",
        "Play vs Computer",
        "Puzzles",
        "Archive",
        "Settings"
    };
    for (int i = 0; i < EntryCount; ++i) {
        entries_[i].setLabel(Labels[i]);
        const bool disabled = i >= 2;
        entries_[i].setEnabled(!disabled);
        entries_[i].setOnClick([this, i] {
            if (i == 0) {
                app_.pushScreen(std::make_unique<ConnectScreen>(app_));
            } else if (i == 1) {
                const char* env = std::getenv("CHESS_ENGINE_PATH");
                std::string enginePath = env ? env : "stockfish";
                app_.pushScreen(std::make_unique<LocalGameScreen>(
                    app_, Color::White, std::move(enginePath), 5));
            }
        });
    }
    entries_[0].setFocused(true);

    title_.setText("Chess");
    title_.setFontSize(36);
    title_.setColor(sf::Color(255, 255, 255));

    hint_.setText("Mouse or arrow keys + Enter");
    hint_.setFontSize(14);
    hint_.setColor(sf::Color(100, 100, 100));

    layoutButtons();
}

void MenuScreen::layoutButtons()
{
    const float areaTop = MenuStartY - EntryH / 2.f;
    const float areaH = EntryH * EntryCount + EntryGap * (EntryCount - 1);
    const sf::FloatRect area(
        sf::Vector2f(0.f, areaTop),
        sf::Vector2f(App::WindowWidth, areaH));
    const auto rects = layout::vstack(
        area, EntryGap,
        std::vector<sf::Vector2f>(EntryCount, {EntryW, EntryH}));
    for (int i = 0; i < EntryCount; ++i)
        entries_[i].setRect(rects[i]);
}

void MenuScreen::focusNext(bool down)
{
    int next = focused_;
    for (int i = 0; i < EntryCount; ++i) {
        next = down ? (next + 1) : (next - 1);
        if (next < 0) next = EntryCount - 1;
        if (next >= EntryCount) next = 0;
        if (entries_[next].isEnabled()) {
            entries_[focused_].setFocused(false);
            focused_ = next;
            entries_[focused_].setFocused(true);
            return;
        }
    }
}

void MenuScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) return;
        if (kp->code == sf::Keyboard::Key::Up) { focusNext(false); return; }
        if (kp->code == sf::Keyboard::Key::Down) { focusNext(true); return; }
    }

    sf::Vector2f local(0.f, 0.f);
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
        local = app_.toLocal(mm->position);
    else if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
        local = app_.toLocal(mb->position);
    else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
        local = app_.toLocal(rb->position);

    for (int i = 0; i < EntryCount; ++i) {
        if (entries_[i].isEnabled() && entries_[i].handleEvent(event, local))
            return;
    }
}

void MenuScreen::update(float /*dtSec*/) {}

void MenuScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    auto titleBounds = title_.bounds(font);
    title_.setPosition({
        (App::WindowWidth - titleBounds.size.x) / 2.f - titleBounds.position.x,
        TitleY
    });
    title_.draw(window, font);

    for (int i = 0; i < EntryCount; ++i)
        entries_[i].draw(window, font);

    const float hintY = MenuStartY + EntryH * EntryCount
                        + EntryGap * (EntryCount - 1) + 40.f;
    auto hintBounds = hint_.bounds(font);
    hint_.setPosition({
        (App::WindowWidth - hintBounds.size.x) / 2.f - hintBounds.position.x,
        hintY
    });
    hint_.draw(window, font);
}

} // namespace chess::client