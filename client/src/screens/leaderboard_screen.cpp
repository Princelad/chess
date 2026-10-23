#include "leaderboard_screen.h"

#include "game_screen.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace chess::client {

namespace {

constexpr float PanelX = 170.f;
constexpr float PanelY = 40.f;
constexpr float PanelW = 620.f;
constexpr float PanelH = 560.f;
constexpr float RowH = 30.f;

std::string toLower(const std::string& text)
{
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// One leaderboard row: rank, name, rating with deviation, rated-game record.
std::string rowLabel(std::size_t rank, const chess::net::LeaderboardEntryMsg& entry)
{
    std::ostringstream out;
    out << (rank + 1) << ".  " << entry.name
        << "    " << std::lround(entry.rating) << " ±" << std::lround(entry.deviation)
        << "    " << entry.wins << "W " << entry.losses << "L " << entry.draws << "D";
    return out.str();
}

} // namespace

LeaderboardScreen::LeaderboardScreen(App& app)
    : app_(app)
    , bg_(sf::FloatRect(sf::Vector2f(PanelX, PanelY),
                        sf::Vector2f(PanelW, PanelH)),
          sf::Color(28, 28, 30), sf::Color(90, 90, 90), 1.f)
{
    if (const auto& profile = app_.profile(); profile.has_value() && !profile->isGuest)
        ownName_ = toLower(profile->name);

    title_.setText("Leaderboard");
    title_.setFontSize(30);
    title_.setColor(sf::Color(240, 240, 240));

    status_.setFontSize(15);
    status_.setColor(sf::Color(170, 170, 170));

    backBtn_.setLabel("OK");
    backBtn_.setOnClick([this] { app_.goBack(); });

    layoutRows();
    request();
}

void LeaderboardScreen::request()
{
    phase_ = Phase::Busy;
    entries_.clear();
    topRow_ = 0;
    layoutRows();
    status_.setText("Loading the leaderboard...");
    app_.connection().send(chess::net::LeaderboardRequestMsg{50 + PageRows});
}

void LeaderboardScreen::layoutRows()
{
    title_.setPosition({PanelX + 20.f, PanelY + 16.f});
    status_.setPosition({PanelX + 20.f, PanelY + 64.f});
    backBtn_.setRect(sf::FloatRect(
        sf::Vector2f(PanelX + PanelW - 90.f, PanelY + PanelH - 42.f),
        sf::Vector2f(70.f, 30.f)));

    rowButtons_.clear();
    if (phase_ != Phase::List)
        return;

    const std::size_t visible = std::min<std::size_t>(
        PageRows, entries_.size() - topRow_);
    rowButtons_.reserve(visible);
    float y = PanelY + 64.f;
    for (std::size_t i = topRow_; i < topRow_ + visible && i < entries_.size(); ++i) {
        // Rank is the position in the list: highest rating first.
        std::string label = rowLabel(i, entries_[i]);
        const bool own = !ownName_.empty()
            && toLower(entries_[i].name) == ownName_;
        if (own)
            label += "  (you)";
        rowButtons_.emplace_back(
            sf::FloatRect(sf::Vector2f(PanelX + 20.f, y),
                          sf::Vector2f(PanelW - 40.f, RowH)),
            label);
        Button& row = rowButtons_.back();
        // Rows are not clickable; muted colors like the archive list, with a
        // distinct tint for the viewer's own account.
        if (own)
            row.setColors(sf::Color(46, 62, 58), sf::Color(58, 78, 72),
                          sf::Color(52, 70, 64), sf::Color(46, 62, 58),
                          sf::Color(150, 150, 150), sf::Color(230, 245, 230),
                          sf::Color(70, 110, 90), sf::Color(120, 190, 150),
                          sf::Color(150, 220, 180));
        else
            row.setColors(sf::Color(42, 42, 44), sf::Color(58, 58, 62),
                          sf::Color(52, 52, 56), sf::Color(42, 42, 44),
                          sf::Color(120, 120, 120), sf::Color(220, 220, 220),
                          sf::Color(90, 90, 90), sf::Color(200, 200, 120),
                          sf::Color(220, 220, 140));
        y += RowH + 2.f;
    }

    if (entries_.empty())
        status_.setText("No rated games on this server yet.");
}

void LeaderboardScreen::handleEvent(const sf::Event& event)
{
    // Rows are informational; only scrolling and leaving apply here.
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            app_.goBack();
            return;
        }
        if (kp->code == sf::Keyboard::Key::Up && topRow_ > 0) {
            --topRow_;
            layoutRows();
            return;
        }
        if (kp->code == sf::Keyboard::Key::Down
            && entries_.size() > PageRows
            && topRow_ + PageRows < entries_.size()) {
            ++topRow_;
            layoutRows();
            return;
        }
    }

    if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>()) {
        const float delta = wheel->delta;
        int steps = static_cast<int>(delta > 0.f ? delta : -delta);
        bool moved = false;
        while (steps-- > 0) {
            if (delta > 0.f && topRow_ > 0) {
                --topRow_;
                moved = true;
            } else if (delta < 0.f && entries_.size() > PageRows
                       && topRow_ + PageRows < entries_.size()) {
                ++topRow_;
                moved = true;
            }
        }
        if (moved) {
            layoutRows();
            return;
        }
    }

    sf::Vector2f local(0.f, 0.f);
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
        local = app_.toLocal(mb->position);
    else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
        local = app_.toLocal(rb->position);
    else if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
        local = app_.toLocal(mm->position);

    for (auto& row : rowButtons_)
        row.handleEvent(event, local);
    backBtn_.handleEvent(event, local);
}

void LeaderboardScreen::update(float)
{
    app_.connection().poll();

    while (app_.connection().hasMessages()) {
        auto msg = app_.connection().nextMessage();
        if (auto* list = std::get_if<chess::net::LeaderboardListMsg>(&msg)) {
            entries_ = std::move(list->players);
            topRow_ = 0;
            phase_ = Phase::List;
            status_.setText("");
            layoutRows();
        }
        else if (auto* err = std::get_if<chess::net::ErrorMsg>(&msg)) {
            status_.setText(err->message);
            if (phase_ == Phase::Busy)
                phase_ = Phase::List; // stay with an explanatory note
            layoutRows();
        }
        else if (auto* welcome = std::get_if<chess::net::WelcomeMsg>(&msg)) {
            // A match found us while browsing: fall back into play.
            std::optional<std::pair<double, double>> opponentRating;
            if (welcome->rated)
                opponentRating =
                    std::make_pair(welcome->rating, welcome->deviation);
            app_.switchScreen(std::make_unique<GameScreen>(
                app_, welcome->color, welcome->opponent, opponentRating,
                welcome->rated, welcome->timeControl, welcome->resumeToken,
                welcome->startFen));
            return;
        }
    }

    // The server dropped us while browsing.
    if (app_.connection().state() != ConnectionState::Connected
        && app_.connection().state() != ConnectionState::Connecting) {
        status_.setText("Not connected. Leave and connect again.");
        if (phase_ == Phase::Busy)
            phase_ = Phase::List;
        layoutRows();
    }
}

void LeaderboardScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    bg_.draw(window);
    title_.draw(window, font);
    status_.draw(window, font);

    for (auto& row : rowButtons_)
        row.draw(window, font);
    backBtn_.draw(window, font);
}

} // namespace chess::client
