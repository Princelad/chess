#include "app.h"

#include "asset_paths.h"
#include "screens/menu_screen.h"
#include "ui/theme.h"

#include <algorithm>
#include <iostream>

namespace chess::client {

App::App()
    : window_(sf::VideoMode({WindowWidth, WindowHeight}),
              "Chess",
              sf::Style::Titlebar | sf::Style::Close | sf::Style::Resize)
    , viewport_(WindowWidth, WindowHeight)
    , config_(Config::defaultPath())
{
    window_.setFramerateLimit(120);
    sounds_.setEnabled(!config_.getBool("sound.muted", false));
    sounds_.setVolume(std::clamp(config_.getInt("sound.volume", 100), 0, 100));
    loadAssets();
    buildView(viewport_.x, viewport_.y);
    screen_ = std::make_unique<MenuScreen>(*this);
}

sf::Vector2f App::toLocal(sf::Vector2i pixel) const
{
    return window_.mapPixelToCoords(pixel, view_);
}

void App::buildView(unsigned int width, unsigned int height)
{
    const float logicalW = static_cast<float>(WindowWidth);
    const float logicalH = static_cast<float>(WindowHeight);

    float scale = std::min(static_cast<float>(width) / logicalW,
                           static_cast<float>(height) / logicalH);
    if (scale <= 0.f) scale = 1.f;

    view_ = sf::View({logicalW / 2.f, logicalH / 2.f}, {logicalW, logicalH});
    const float left = (static_cast<float>(width) - logicalW * scale)
                       / (2.f * static_cast<float>(width));
    const float top = (static_cast<float>(height) - logicalH * scale)
                      / (2.f * static_cast<float>(height));
    view_.setViewport(sf::FloatRect(
        sf::Vector2f(left, top),
        sf::Vector2f(logicalW * scale / static_cast<float>(width),
                     logicalH * scale / static_cast<float>(height))));
}

void App::loadAssets()
{
    config_.load();
    if (!config_.has("general.auto_queen")) config_.setBool("general.auto_queen", true);
    if (!config_.has("board.colors")) config_.set("board.colors", "classic");
    if (!config_.has("board.show_coordinates")) config_.setBool("board.show_coordinates", true);
    if (!config_.has("pieces.path")) config_.set("pieces.path", "");
    if (!config_.has("pieces.set")) config_.set("pieces.set", "cburnett");
    if (!config_.has("sound.volume")) config_.setInt("sound.volume", 100);
    if (!config_.has("sound.muted")) config_.setBool("sound.muted", false);
    if (!config_.has("animation.enabled")) config_.setBool("animation.enabled", true);
    if (!config_.has("general.last_host")) config_.set("general.last_host", "localhost");
    if (!config_.has("general.last_port")) config_.set("general.last_port", "5555");
    if (!config_.has("general.last_name")) config_.set("general.last_name", "");
    if (!config_.has("animation.duration")) config_.set("animation.duration", "0.3");
    config_.save();

    lastHost_ = config_.get("general.last_host", "localhost");
    lastPort_ = config_.get("general.last_port", "5555");
    lastName_ = config_.get("general.last_name", "");

    font_.emplace();
    // Asset roots (CHESS_ASSETS env override + the three relative roots)
    // first, then the common system font locations as a fallback.
    bool fontOk = false;
    for (const std::string& root : assetRoots()) {
        if (font_->openFromFile(root + "fonts/Inter-Regular.ttf")) {
            fontOk = true;
            break;
        }
    }
    if (!fontOk) {
        // Asset roots missed and no bundled candidate — fall back through
        // likely system fonts on each platform (openFromFile fails silently,
        // ordered Linux → Windows → macOS).
        const char* paths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/consola.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            "/System/Library/Fonts/HelveticaNeue.ttc",
        };
        for (const char* p : paths)
            if (font_->openFromFile(p)) { fontOk = true; break; }
    }
    if (!fontOk)
        std::cerr << "Warning: no font loaded, text may be invisible\n";
    loadPieceTextures();
}

bool App::loadPieceTextures()
{
    const std::string customDir = config_.get("pieces.path", "");
    std::vector<std::string> dirs;
    if (!customDir.empty()) dirs.push_back(customDir);
    // Named set dirs (11.6), then the legacy flat root that predates the
    // per-set subdirectories (keeps hand-assembled trees loadable), each
    // under every asset root the runtime knows about.
    for (const std::string& root : assetRoots())
        dirs.push_back(root + "pieces/" + pieceSetDirName(pieceSet()) + "/");
    for (const std::string& root : assetRoots())
        dirs.push_back(root + "pieces/");

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
            for (const auto& dir : dirs) {
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
    return allLoaded;
}

bool App::reloadPieces()
{
    std::array<sf::Texture, 12> backup = pieceTextures_;
    const bool wasLoaded = piecesLoaded_;
    if (loadPieceTextures()) return true;

    pieceTextures_ = std::move(backup);
    piecesLoaded_ = wasLoaded;
    return false;
}

void App::switchScreen(std::unique_ptr<Screen> screen)
{
    stack_.clear();
    screen_ = std::move(screen);
}

void App::pushScreen(std::unique_ptr<Screen> screen)
{
    stack_.push_back(std::move(screen_));
    screen_ = std::move(screen);
}

void App::goBack()
{
    if (stack_.empty()) return;
    screen_ = std::move(stack_.back());
    stack_.pop_back();
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
            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                viewport_ = resized->size;
                buildView(viewport_.x, viewport_.y);
            }
            if (screen_) screen_->handleEvent(*event);
        }

        while (accumulator >= FixedDt) {
            if (screen_) screen_->update(FixedDt);
            accumulator -= FixedDt;
        }

        window_.clear(ui::pal::bgApp);
        window_.setView(view_);
        if (screen_) screen_->draw(window_);
        window_.display();
    }
}

} // namespace chess::client
