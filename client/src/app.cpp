#include "app.h"

#include "screens/connect_screen.h"

#include <iostream>

namespace chess::client {

App::App()
    : window_(sf::VideoMode({WindowWidth, WindowHeight}),
              "Chess",
              sf::Style::Titlebar | sf::Style::Close)
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
    bool fontOk = false;
    for (const char* path : paths) {
        if (font_->openFromFile(path)) { fontOk = true; break; }
    }
    if (!fontOk)
        std::cerr << "Warning: no font loaded, text may be invisible\n";
    loadPieceTextures();
}

void App::loadPieceTextures()
{
    const char* dirs[] = {
        "assets/pieces/",
        "../assets/pieces/",
        "../../assets/pieces/",
    };

    const char names[2][6] = {
        { 'w','w','w','w','w','w' },
        { 'b','b','b','b','b','b' },
    };
    const char types[6] = { 'p', 'n', 'b', 'r', 'q', 'k' };

    bool allLoaded = true;
    for (int c = 0; c < 2; ++c) {
        for (int t = 0; t < 6; ++t) {
            std::string filename(1, names[c][t]);
            filename += types[t];
            filename += ".png";

            bool loaded = false;
            for (const char* dir : dirs) {
                if (pieceTextures_[PieceIndex(
                        static_cast<Color>(c),
                        static_cast<PieceType>(t))].loadFromFile(dir + filename)) {
                    loaded = true;
                    break;
                }
            }
            if (!loaded) allLoaded = false;
        }
    }
    for (auto& tex : pieceTextures_)
        tex.setSmooth(true);
    piecesLoaded_ = allLoaded;
    if (!allLoaded)
        std::cerr << "Warning: some piece textures failed to load\n";
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
