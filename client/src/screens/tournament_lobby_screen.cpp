#include "tournament_lobby_screen.h"

#include "ui/theme.h"
#include "game_screen.h"
#include "menu_screen.h"

namespace chess::client {

namespace {
constexpr float TitleY = 60.f;
constexpr float SubY = 110.f;
constexpr float TableY = 170.f;
constexpr float RowH = 26.f;
constexpr float NameX = 380.f;
constexpr float PointsX = 640.f;
constexpr float RecordX = 720.f;
constexpr float LeaveY = 556.f;
}

TournamentLobbyScreen::TournamentLobbyScreen(App& app,
                                             std::string timeControl,
                                             std::string variant,
                                             std::string joinName)
    : app_(app)
    , timeControl_(std::move(timeControl))
    , variant_(std::move(variant))
    , joinName_(std::move(joinName))
{
    title_.setText("Arena");
    title_.setFontSize(32);
    title_.setColor(ui::pal::white);

    std::string sub = timeControl_.empty()
        ? "unlimited" : ("tc " + timeControl_);
    sub += variant_.empty()
        ? "   ·   standard" : ("   ·   " + variant_);
    subtitle_.setText(sub);
    subtitle_.setFontSize(16);
    subtitle_.setColor(sf::Color(150, 150, 155)); // arena slate tone

    headerName_.setText("Name");
    headerPoints_.setText("Pts");
    headerRecord_.setText("W   L   D");
    for (auto* h : {&headerName_, &headerPoints_, &headerRecord_}) {
        h->setFontSize(15);
        h->setColor(sf::Color(160, 170, 190)); // arena header slate
    }

    statusLabel_.setFontSize(16);
    statusLabel_.setColor(ui::pal::warnBanner);
    statusLabel_.setText("Joining the arena...");

    resultLabel_.setFontSize(30);
    resultLabel_.setColor(ui::pal::ok);

    leaveBtn_.setLabel("Leave arena");
    leaveBtn_.setStyle(ui::buttonDangerMuted());
    leaveBtn_.setOnClick([this] { leave(); });

    title_.setPosition({NameX, TitleY});
    subtitle_.setPosition({NameX, SubY});
    headerName_.setPosition({NameX, TableY - RowH + 40.f});
    headerPoints_.setPosition({PointsX, TableY - RowH + 40.f});
    headerRecord_.setPosition({RecordX, TableY - RowH + 40.f});
    statusLabel_.setPosition({NameX, 520.f});
    leaveBtn_.setRect(sf::FloatRect(sf::Vector2f(NameX, LeaveY),
                                    sf::Vector2f(200.f, 40.f)));

    // Join on entry (idempotent for a member returning here); if the
    // connection is not up yet, update() re-arms the join.
    sendJoin();
}

void TournamentLobbyScreen::sendJoin()
{
    if (joinSent_) return;
    if (app_.connection().state() != ConnectionState::Connected)
        return;
    if (app_.connection().send(chess::net::TournamentJoinMsg{
            joinName_, timeControl_, variant_}))
        joinSent_ = true;
}

void TournamentLobbyScreen::leave()
{
    // Fire-and-forget: the row drop matters to the server's next pass, not
    // to us — the player chose to go back to the menu.
    if (app_.connection().state() == ConnectionState::Connected)
        app_.connection().send(chess::net::TournamentLeaveMsg{});
    app_.connection().disconnect();
    app_.switchScreen(std::make_unique<MenuScreen>(app_));
}

void TournamentLobbyScreen::enterGame(const chess::net::WelcomeMsg& welcome)
{
    // The arena pairing arrived — exactly the construction the connect
    // and rematch flows use, with the arena flag riding along so the
    // game-over screen lands back here.
    std::optional<std::pair<double, double>> opponentRating;
    if (welcome.rated)
        opponentRating = std::make_pair(welcome.rating, welcome.deviation);
    app_.switchScreen(std::make_unique<GameScreen>(
        app_, welcome.color, welcome.opponent, opponentRating,
        welcome.rated, welcome.timeControl, welcome.resumeToken,
        welcome.startFen, welcome.arena));
}

void TournamentLobbyScreen::rebuildRows()
{
    rowWidgets_.clear();
    rowWidgets_.reserve(rows_.size());
    for (const auto& row : rows_) {
        RowWidgets widgets;
        widgets.name.setText(row.name);
        widgets.name.setFontSize(15);
        widgets.name.setColor(row.self ? ui::pal::goldSelf
                                       : sf::Color(225, 225, 235)); // arena pale row

        widgets.points.setText(std::to_string(row.points));
        widgets.points.setFontSize(15);
        widgets.points.setColor(sf::Color(210, 210, 220)); // arena row tint

        std::string rec = std::to_string(row.wins) + "   "
            + std::to_string(row.losses) + "   " + std::to_string(row.draws);
        if (row.playing)
            rec += "  (playing)";
        widgets.record.setText(rec);
        widgets.record.setFontSize(15);
        widgets.record.setColor(sf::Color(170, 175, 185)); // arena row tint

        rowWidgets_.push_back(std::move(widgets));
    }
    layoutRows();
}

void TournamentLobbyScreen::layoutRows()
{
    float y = TableY;
    for (auto& widgets : rowWidgets_) {
        widgets.name.setPosition({NameX, y});
        widgets.points.setPosition({PointsX, y});
        widgets.record.setPosition({RecordX, y});
        y += RowH;
    }

    // Status line: round progress of our own row, or the result echo.
    const chess::net::TournamentRowMsg* selfRow = nullptr;
    for (const auto& row : rows_)
        if (row.self) selfRow = &row;

    if (selfRow != nullptr && roundsTarget_ > 0
        && selfRow->wins + selfRow->losses + selfRow->draws >= roundsTarget_)
        statusLabel_.setText("You played all " + std::to_string(roundsTarget_)
            + " rounds — waiting for the standings");
    else if (selfRow != nullptr && roundsTarget_ > 0)
        statusLabel_.setText(
            "Round "
            + std::to_string(selfRow->wins + selfRow->losses
                             + selfRow->draws + 1)
            + " of " + std::to_string(roundsTarget_)
            + (selfRow->playing ? " — your game is running"
                                : " — waiting for a pairing"));
    else
        statusLabel_.setText("Waiting for the first standings...");

    resultLabel_.setText(phase_ == 1
        ? std::string("Arena finished — final standings above")
        : std::string());
    if (!resultLabel_.text().empty()) {
        auto rb = resultLabel_.bounds(app_.font());
        resultLabel_.setPosition({
            (App::WindowWidth - rb.size.x) / 2.f - rb.position.x, 500.f});
    }
}

void TournamentLobbyScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            leave();
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

    leaveBtn_.handleEvent(event, local);
}

void TournamentLobbyScreen::update(float /*dtSec*/)
{
    if (!joinSent_)
        sendJoin();

    app_.connection().poll();

    while (app_.connection().hasMessages()) {
        auto msg = app_.connection().nextMessage();

        if (auto* state = std::get_if<chess::net::TournamentStateMsg>(&msg)) {
            phase_ = state->phase;
            roundsTarget_ = state->roundsTarget;
            rows_ = state->rows;
            rebuildRows();
            continue;
        }
        if (auto* welcome = std::get_if<chess::net::WelcomeMsg>(&msg)) {
            enterGame(*welcome);
            return;
        }
        if (auto* err = std::get_if<chess::net::ErrorMsg>(&msg)) {
            statusLabel_.setText(err->message);
            continue;
        }
    }
}

void TournamentLobbyScreen::draw(sf::RenderWindow& window)
{
    const auto& font = app_.font();

    title_.draw(window, font);
    subtitle_.draw(window, font);
    headerName_.draw(window, font);
    headerPoints_.draw(window, font);
    headerRecord_.draw(window, font);

    for (auto& widgets : rowWidgets_) {
        widgets.name.draw(window, font);
        widgets.points.draw(window, font);
        widgets.record.draw(window, font);
    }

    if (!statusLabel_.text().empty())
        statusLabel_.draw(window, font);
    if (!resultLabel_.text().empty())
        resultLabel_.draw(window, font);
    leaveBtn_.draw(window, font);
}

} // namespace chess::client
