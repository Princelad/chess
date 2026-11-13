#include "persona_select_screen.h"

#include "bot_personas.h"
#include "avatars.h"
#include "local_game_screen.h"
#include "menu_screen.h"
#include "ui/theme.h"
#include "widgets/player_card.h"

#include <cstdlib>

namespace chess::client {

namespace {
constexpr float CardW = 360.f;
constexpr float CardH = 48.f;
constexpr float CardStep = 62.f;
constexpr float CardsTop = 150.f;
constexpr float CardX = static_cast<float>(App::WindowWidth / 2 - 180);
constexpr float BlurbX = CardX + CardW + 14.f;
} // namespace

PersonaSelectScreen::PersonaSelectScreen(App& app)
    : app_(app)
{
    cards_.resize(personas::all().size());
    for (std::size_t i = 0; i < personas::all().size(); ++i) {
        cards_[i].setRect(sf::FloatRect(
            {CardX, CardsTop + static_cast<float>(i) * CardStep},
            {CardW, CardH}));
    }

    sideBtn_.setLabel("Play as White");
    sideBtn_.setStyle(ui::buttonConnect());
    sideBtn_.setRect(sf::FloatRect({CardX, 490.f}, {200.f, 44.f}));
    sideBtn_.setOnClick([this] {
        playBlack_ = !playBlack_;
        sideBtn_.setLabel(playBlack_ ? "Play as Black" : "Play as White");
    });

    backBtn_.setLabel("Menu");
    backBtn_.setIcon(ui::icons::IconKind::ArrowBack);
    backBtn_.setStyle(ui::buttonDanger());
    backBtn_.setRect(sf::FloatRect({CardX + 216.f, 490.f}, {160.f, 44.f}));
    backBtn_.setOnClick([this] { app_.goBack(); });
}

void PersonaSelectScreen::startGame()
{
    const char* env = std::getenv("CHESS_ENGINE_PATH");
    std::string enginePath = env ? env : "stockfish";
    const Color myColor = playBlack_ ? Color::Black : Color::White;
    app_.switchScreen(std::make_unique<LocalGameScreen>(
        app_, myColor, std::move(enginePath), personas::all()[picked_]));
}

void PersonaSelectScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            app_.goBack();
            return;
        }
        return;
    }

    sf::Vector2f local(0.f, 0.f);
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
        local = app_.toLocal(mb->position);
    else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
        local = app_.toLocal(rb->position);
    else if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
        local = app_.toLocal(mm->position);

    if (sideBtn_.handleEvent(event, local)) return;
    if (backBtn_.handleEvent(event, local)) return;

    // Card rows: hover + click pick the persona (manual hit-testing — the
    // rows are PlayerCard bands, not Buttons).
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>()) {
        picked_ = -1;
        for (std::size_t i = 0; i < cards_.size(); ++i)
            if (cards_[i].rect().contains(local)) {
                picked_ = static_cast<int>(i);
                break;
            }
        return;
    }
    if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (rb->button == sf::Mouse::Button::Left) {
            for (std::size_t i = 0; i < cards_.size(); ++i) {
                if (cards_[i].rect().contains(local)) {
                    picked_ = static_cast<int>(i);
                    startGame();
                    return;
                }
            }
        }
    }
}

void PersonaSelectScreen::update(float /*dtSec*/) {}

void PersonaSelectScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    sf::Text title(font, "Play vs Computer", ui::type::Display);
    title.setFillColor(ui::pal::textPrimary);
    title.setPosition({40.f, 60.f});
    window.draw(title);

    sf::Text sub(font, "Pick an opponent", ui::type::Body);
    sub.setFillColor(ui::pal::textGhost);
    sub.setPosition({40.f, 100.f});
    window.draw(sub);

    const auto& cast = personas::all();
    for (std::size_t i = 0; i < cast.size(); ++i) {
        const auto& p = cast[i];
        const sf::FloatRect band(
            {CardX, CardsTop + static_cast<float>(i) * CardStep},
            {CardW, CardH});

        if (picked_ == static_cast<int>(i)) {
            sf::RectangleShape hover({band.size.x, band.size.y});
            hover.setPosition({band.position.x, band.position.y});
            hover.setFillColor(ui::pal::bgCardHover);
            window.draw(hover);
        }

        PlayerCard card;
        card.setRect(band);
        PlayerCardState state;
        state.nameLine = p.name;
        state.nameColor = ui::pal::textCell;
        state.avatarSeed = avatars::seedFromName(p.name);
        state.avatarName = p.name;
        card.draw(window, font, app_, state);

        std::string blurb = std::string(p.blurb) + "  -  ~"
            + std::to_string(p.approxElo);
        sf::Text blurbText(font, blurb, 14);
        blurbText.setFillColor(ui::pal::textGhost);
        const auto lb = blurbText.getLocalBounds();
        blurbText.setPosition({
            BlurbX,
            band.position.y + (band.size.y - lb.size.y) / 2.f - lb.position.y});
        window.draw(blurbText);
    }

    sideBtn_.draw(window, font);
    backBtn_.draw(window, font);
}

} // namespace chess::client
