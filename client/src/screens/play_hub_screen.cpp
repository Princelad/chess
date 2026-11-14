#include "play_hub_screen.h"

#include "avatars.h"
#include "local_game_screen.h"
#include "menu_screen.h"
#include "persona_select_screen.h"
#include "puzzle_screen.h"
#include "rush_screen.h"
#include "settings_screen.h"
#include "ui/theme.h"
#include "version.h"
#include "widgets/player_card.h"

#include <chess/net/messages.h>

#include <cstdlib>
#include <string>

namespace chess::client {

namespace {

// Left zone (quick pair + connect drawer toggle).
constexpr float LeftX = 40.f;
constexpr float QuickCardW = 190.f;
constexpr float QuickCardH = 64.f;
constexpr float QuickGap = 12.f;
constexpr float QuickTopY = 145.f;
constexpr float VariantY = 300.f;
constexpr float DrawerRowY = 356.f;

// Right zone (persona rows + tool cards), visible while the drawer is
// closed; the drawer covers it when open.
constexpr float RightX = 480.f;
constexpr float PersonaRowW = 300.f;
constexpr float PersonaRowH = 44.f;
constexpr float PersonaStep = 50.f;
constexpr float PersonaTopY = 188.f;
constexpr float ToolW = 200.f;
constexpr float ToolH = 44.f;
constexpr float ToolGap = 10.f;
constexpr float ToolTopY = 432.f;

// Quick-pair cards: label + the exact 11.1 preset each queues with.
struct QuickCardSpec {
    const char* label;
    const char* preset;
    ui::ButtonStyle (*style)();
};

constexpr QuickCardSpec QuickSpecs[4] = {
    {"Bullet - 1+0", "1+0", &ui::buttonBlue},
    {"Blitz - 3+2", "3+2", &ui::buttonAmber},
    {"Rapid - 10+0", "10+0", &ui::buttonGreen},
    {"Classical - 30+0", "30+0", &ui::buttonPlum},
};

} // namespace

PlayHubScreen::PlayHubScreen(App& app)
    : app_(app)
    , drawer_(std::make_unique<ConnectScreen>(app))
{
    drawer_->embedAsDrawer();

    for (int i = 0; i < QuickCount; ++i) {
        quickBtns_[i].setLabel(QuickSpecs[i].label);
        quickBtns_[i].setStyle(QuickSpecs[i].style());
        const float col = static_cast<float>(i % 2);
        const float row = static_cast<float>(i / 2);
        quickBtns_[i].setRect(sf::FloatRect(
            {LeftX + col * (QuickCardW + QuickGap),
             QuickTopY + row * (QuickCardH + QuickGap)},
            {QuickCardW, QuickCardH}));
        quickBtns_[i].setOnClick([this, i] {
            quickQueue(QuickSpecs[i].preset);
        });
    }

    variantBtn_.setLabel(app_.config().get("general.variant", "").empty()
            ? "Variant: Standard"
            : "Variant: Chess960");
    variantBtn_.setStyle(ui::buttonCycle());
    variantBtn_.setRect(
        sf::FloatRect({LeftX, VariantY}, {QuickCardW, 40.f}));
    variantBtn_.setOnClick([this] {
        drawer_->cycleVariant();
        variantBtn_.setLabel(
            app_.config().get("general.variant", "").empty()
                ? "Variant: Standard"
                : "Variant: Chess960");
    });

    arenaBtn_.setLabel("Arena lobby");
    arenaBtn_.setStyle(ui::buttonBronze());
    arenaBtn_.setRect(sf::FloatRect(
        {LeftX + QuickCardW + QuickGap, VariantY}, {QuickCardW, 40.f}));
    arenaBtn_.setOnClick([this] { quickBrowse(BrowseTarget::Arena); });

    drawerBtn_.setLabel("Connect...");
    drawerBtn_.setStyle(ui::buttonConnect());
    drawerBtn_.setRect(
        sf::FloatRect({LeftX, DrawerRowY}, {QuickCardW, 40.f}));
    drawerBtn_.setOnClick([this] { toggleDrawer(); });

    sideBtn_.setLabel("Play as White");
    sideBtn_.setStyle(ui::buttonCycle());
    sideBtn_.setRect(sf::FloatRect({RightX, 145.f}, {180.f, 34.f}));
    sideBtn_.setOnClick([this] {
        playBlack_ = !playBlack_;
        sideBtn_.setLabel(playBlack_ ? "Play as Black" : "Play as White");
    });

    layoutPersonaRows();

    // Tool cards (rows of two): Daily / Puzzle Rush, Puzzles / Settings,
    // Archive / Leaderboard, Watch.
    const char* toolLabels[ToolCount] = {
        "Daily puzzle", "Puzzle Rush", "Puzzles",
        "Archive", "Leaderboard", "Settings", "Watch"};
    const ui::ButtonStyle toolStyles[ToolCount] = {
        ui::buttonInfo(), ui::buttonAmber(), ui::buttonPlain(),
        ui::buttonBlue(), ui::buttonMint(), ui::buttonSlate(),
        ui::buttonSlate()};
    for (int i = 0; i < ToolCount; ++i) {
        toolBtns_[i].setLabel(toolLabels[i]);
        toolBtns_[i].setStyle(toolStyles[i]);
        const float col = static_cast<float>(i % 2);
        const float row = static_cast<float>(i / 2);
        toolBtns_[i].setRect(sf::FloatRect(
            {RightX + col * (ToolW + ToolGap),
             ToolTopY + row * (ToolH + ToolGap)},
            {ToolW, ToolH}));
    }
    toolBtns_[0].setOnClick([this] {
        app_.switchScreen(std::make_unique<PuzzleScreen>(app_, true));
    });
    toolBtns_[1].setOnClick([this] {
        app_.switchScreen(std::make_unique<RushScreen>(app_));
    });
    toolBtns_[2].setOnClick([this] {
        app_.switchScreen(std::make_unique<PuzzleScreen>(app_));
    });
    toolBtns_[3].setOnClick([this] { quickBrowse(BrowseTarget::Archive); });
    toolBtns_[4].setOnClick(
        [this] { quickBrowse(BrowseTarget::Leaderboard); });
    toolBtns_[5].setOnClick([this] {
        app_.switchScreen(std::make_unique<SettingsScreen>(app_));
    });
    // Watch (12.4): the drawer connects and hands over to the spectator
    // game view (the name field doubles as the follow target).
    toolBtns_[6].setOnClick([this] { quickBrowse(BrowseTarget::Watch); });
}

void PlayHubScreen::layoutPersonaRows()
{
    const auto& cast = personas::all();
    personaRows_.clear();
    for (std::size_t i = 0; i < cast.size(); ++i) {
        personaRows_.emplace_back();
        personaRows_.back().setRect(sf::FloatRect(
            {RightX,
             PersonaTopY + static_cast<float>(i) * PersonaStep},
            {PersonaRowW, PersonaRowH}));
    }
}

void PlayHubScreen::toggleDrawer()
{
    drawerOpen_ = !drawerOpen_;
}

void PlayHubScreen::quickQueue(const std::string& tc)
{
    // The drawer picks the preset and joins with the persisted host/port/
    // name; without a name it just opens for manual entry (audit 2.10:
    // the drawer only expands when the quick attempt could not start —
    // a successful queue hands the screen over on pairing anyway).
    if (!drawer_->quickQueueTc(tc))
        drawerOpen_ = true;
}

void PlayHubScreen::quickBrowse(BrowseTarget target)
{
    drawer_->quickBrowse(target);
    drawerOpen_ = true;
}

void PlayHubScreen::startPersona(std::size_t index)
{
    const char* env = std::getenv("CHESS_ENGINE_PATH");
    std::string enginePath = env ? env : "stockfish";
    const Color myColor = playBlack_ ? Color::Black : Color::White;
    app_.switchScreen(std::make_unique<LocalGameScreen>(
        app_, myColor, std::move(enginePath), personas::all()[index]));
}

void PlayHubScreen::handleEvent(const sf::Event& event)
{
    // Esc collapses the drawer (cancelling an in-flight attempt); the
    // drawer never sees Esc, so it cannot leave the hub by accident.
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape && drawerOpen_) {
            drawer_->cancelAttempt();
            drawerOpen_ = false;
            return;
        }
    }

    if (drawerOpen_) {
        // The drawer owns the event; it may switch screens (the queue
        // pairing), and nothing of `this` may be touched after (12.3).
        drawer_->handleEvent(event);
        return;
    }

    sf::Vector2f local(0.f, 0.f);
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
        local = app_.toLocal(mm->position);
    else if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
        local = app_.toLocal(mb->position);
    else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
        local = app_.toLocal(rb->position);

    if (const auto* mm = event.getIf<sf::Event::MouseMoved>()) {
        pickedPersona_ = -1;
        for (std::size_t i = 0; i < personaRows_.size(); ++i) {
            if (personaRows_[i].rect().contains(local)) {
                pickedPersona_ = static_cast<int>(i);
                break;
            }
        }
    }
    if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (rb->button == sf::Mouse::Button::Left) {
            for (std::size_t i = 0; i < personaRows_.size(); ++i) {
                if (personaRows_[i].rect().contains(local)) {
                    startPersona(i);
                    return;
                }
            }
        }
    }

    for (auto& b : quickBtns_)
        if (b.handleEvent(event, local)) return;
    if (variantBtn_.handleEvent(event, local)) return;
    if (arenaBtn_.handleEvent(event, local)) return;
    if (drawerBtn_.handleEvent(event, local)) return;
    if (sideBtn_.handleEvent(event, local)) return;
    for (auto& b : toolBtns_)
        if (b.handleEvent(event, local)) return;
}

void PlayHubScreen::update(float dtSec)
{
    // The drawer runs even while collapsed so an in-flight queue keeps
    // progressing; it may switch screens (opponent found) and this screen
    // is destroyed in place — nothing of `this` after the call (12.3).
    drawer_->update(dtSec);
}

void PlayHubScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    // ── Left zone: always visible ───────────────────────────────────────
    sf::Text title(font, "Chess", ui::type::Display);
    title.setFillColor(ui::pal::white);
    title.setPosition({LeftX, 30.f});
    window.draw(title);

    sf::Text sub(font, "Play hub", ui::type::Heading);
    sub.setFillColor(ui::pal::textGhost);
    sub.setPosition({LeftX, 92.f});
    window.draw(sub);

    sf::Text quickHeader(font, "Quick pair", ui::type::Body);
    quickHeader.setFillColor(ui::pal::textSecondary);
    quickHeader.setPosition({LeftX, 122.f});
    window.draw(quickHeader);

    for (auto& b : quickBtns_) b.draw(window, font);
    variantBtn_.draw(window, font);
    arenaBtn_.draw(window, font);

    drawerBtn_.draw(window, font);
    std::string summary = app_.lastHost().empty()
        ? "Not connected - no server yet"
        : app_.lastHost() + ":" + app_.lastPort()
            + (app_.lastName().empty() ? "" : "  -  " + app_.lastName());
    sf::Text summaryText(font, summary, ui::type::Body);
    summaryText.setFillColor(ui::pal::textGhost);
    summaryText.setPosition(
        {LeftX + QuickCardW + QuickGap, DrawerRowY + 11.f});
    window.draw(summaryText);

    sf::Text version(font, std::string("chess v") + kAppVersion, 12);
    version.setFillColor(ui::pal::textGhost);
    version.setPosition({LeftX, 614.f});
    window.draw(version);

    if (drawerOpen_) {
        // The drawer covers the right zone entirely: draw it last so its
        // fields/statuses sit on top.
        drawer_->draw(window);
        return;
    }

    // ── Right zone: persona rows + tool cards ───────────────────────────
    sf::Text botHeader(font, "Play vs Computer", ui::type::Body);
    botHeader.setFillColor(ui::pal::textSecondary);
    botHeader.setPosition({RightX, 122.f});
    window.draw(botHeader);
    sideBtn_.draw(window, font);

    const auto& cast = personas::all();
    for (std::size_t i = 0; i < cast.size(); ++i) {
        const auto& p = cast[i];
        const sf::FloatRect band(
            {RightX,
             PersonaTopY + static_cast<float>(i) * PersonaStep},
            {PersonaRowW, PersonaRowH});

        if (pickedPersona_ == static_cast<int>(i)) {
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

        std::string blurb =
            "~" + std::to_string(p.approxElo);
        sf::Text blurbText(font, blurb, 14);
        blurbText.setFillColor(ui::pal::textGhost);
        const auto lb = blurbText.getLocalBounds();
        blurbText.setPosition({
            band.position.x + band.size.x + 12.f,
            band.position.y + (band.size.y - lb.size.y) / 2.f - lb.position.y});
        window.draw(blurbText);
    }

    for (auto& b : toolBtns_) b.draw(window, font);
}

} // namespace chess::client
