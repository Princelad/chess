#include "app.h"

#include "screens/connect_screen.h"

#include <iostream>

namespace chess::client {

App::App()
    : window_(sf::VideoMode({WindowWidth, WindowHeight}),
              "Chess",
              sf::Style::Default)
{
    window_.setFramerateLimit(120);
    loadAssets();
    screen_ = std::make_unique<ConnectScreen>(*this);
}

void App::loadAssets()
{
    font_.emplace();
    const char* paths[] = {
        "assets/fonts/Inter-Regular.ttf",
        "../assets/fonts/Inter-Regular.ttf",
        "../../assets/fonts/Inter-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
    };
    for (const char* path : paths) {
        if (font_->openFromFile(path)) return;
    }
    std::cerr << "Warning: could not load Inter-Regular.ttf\n";
}

void App::switchScreen(std::unique_ptr<Screen> screen)
{
    screen_ = std::move(screen);
}

void App::run()
{
    constexpr float FixedDt = 1.f / 60.f;
    float accumulator = 0.f;
    sf::Clock clock;

    while (window_.isOpen()) {
        float frameTime = clock.restart().asSeconds();
        if (frameTime > 0.25f) frameTime = 0.25f;
        accumulator += frameTime;

        while (const auto event = window_.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window_.close();
                return;
            }
            if (screen_) screen_->handleEvent(*event);
        }

        while (accumulator >= FixedDt) {
            if (screen_) screen_->update(FixedDt);
            accumulator -= FixedDt;
        }

        window_.clear(sf::Color(48, 46, 43));
        if (screen_) screen_->draw(window_);
        window_.display();
    }
}

} // namespace chess::client
