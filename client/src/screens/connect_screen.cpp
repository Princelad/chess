#include "connect_screen.h"
#include "game_screen.h"

#include <chess/net/messages.h>

namespace chess::client {

namespace {
constexpr float StartX = 500.f;
constexpr float StartY = 140.f;
constexpr float FieldX = StartX + 80.f;
constexpr float FieldW = 280.f;
constexpr float FieldH = 36.f;
constexpr float RowGap = 56.f;
constexpr float BtnY = 148.f + 3.f * RowGap + 10.f;
}

ConnectScreen::ConnectScreen(App& app)
    : app_(app)
{
    static const char* FieldLabels[FieldCount] = { "Host:", "Port:", "Name:" };
    fields_[0].setText(app.lastHost());
    fields_[1].setText(app.lastPort());
    fields_[2].setText(app.lastName());
    for (int i = 0; i < static_cast<int>(FieldCount); ++i) {
        labels_[i].setText(FieldLabels[i]);
        labels_[i].setFontSize(20);
        labels_[i].setColor(sf::Color(200, 200, 200));
        fields_[i].setMaxLength(32);
    }
    if (app.lastName().empty())
        activeField_ = 0;
    else
        activeField_ = 2;
    fields_[activeField_].setFocused(true);
    fields_[2].setOnCommit([this] { tryConnect(); });

    connectBtn_.setLabel("Connect");
    connectBtn_.setRect(sf::FloatRect(sf::Vector2f(FieldX, BtnY),
                                      sf::Vector2f(200.f, 40.f)));
    connectBtn_.setColors(sf::Color(76, 175, 80), sf::Color(90, 190, 95),
                          sf::Color(60, 150, 70), sf::Color(40, 80, 45),
                          sf::Color(150, 150, 150), sf::Color(255, 255, 255),
                          sf::Color(60, 140, 65), sf::Color(160, 220, 160),
                          sf::Color(220, 255, 220));
    connectBtn_.setOnClick([this] { tryConnect(); });

    title_.setText("Chess");
    title_.setFontSize(36);
    title_.setColor(sf::Color(255, 255, 255));

    hint_.setText("Tab to switch fields, Enter to connect, Esc to go back");
    hint_.setFontSize(14);
    hint_.setColor(sf::Color(120, 120, 120));

    status_.setText("Enter your name and press Enter to connect.");
    status_.setFontSize(18);
    status_.setColor(sf::Color(180, 180, 180));

    layoutWidgets();
}

void ConnectScreen::layoutWidgets()
{
    title_.setPosition({50.f, 50.f});
    hint_.setPosition({50.f, 590.f});
    status_.setPosition({StartX, BtnY + 44.f});
    error_.setPosition({StartX, BtnY + 44.f});

    float y = StartY;
    for (int i = 0; i < static_cast<int>(FieldCount); ++i) {
        labels_[i].setPosition({StartX, y + 6.f});
        fields_[i].setRect(sf::FloatRect(sf::Vector2f(FieldX, y),
                                         sf::Vector2f(FieldW, FieldH)));
        y += RowGap;
    }
    connectBtn_.setRect(sf::FloatRect(sf::Vector2f(FieldX, BtnY),
                                      sf::Vector2f(200.f, 40.f)));
}

void ConnectScreen::cycleField(bool backward)
{
    int next = static_cast<int>(activeField_);
    if (backward)
        next = (next - 1 + static_cast<int>(FieldCount)) % static_cast<int>(FieldCount);
    else
        next = (next + 1) % static_cast<int>(FieldCount);

    fields_[activeField_].setFocused(false);
    activeField_ = static_cast<std::size_t>(next);
    fields_[activeField_].setFocused(true);
}

void ConnectScreen::tryConnect()
{
    if (name().empty() || phase_ != ConnectPhase::Idle) return;

    unsigned short portNum = 0;
    try {
        portNum = static_cast<unsigned short>(std::stoi(fields_[1].text()));
    } catch (...) {
        error_.setText("Invalid port number");
        return;
    }

    app_.setLastConnection(fields_[0].text(), fields_[1].text(), fields_[2].text());
    error_.setText("");
    phase_ = ConnectPhase::Connecting;
    status_.setText("Connecting...");
    for (auto& field : fields_) field.setEnabled(false);
    app_.connection().connect(fields_[0].text(), portNum);
}

const std::string& ConnectScreen::name() const
{
    return fields_[2].text();
}

void ConnectScreen::handleEvent(const sf::Event& event)
{
    if (phase_ == ConnectPhase::WaitingForOpponent) return;

    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            if (phase_ != ConnectPhase::Idle) app_.connection().disconnect();
            app_.goBack();
            return;
        }
        if (kp->code == sf::Keyboard::Key::Tab) {
            cycleField(kp->shift);
            return;
        }
        if (kp->code == sf::Keyboard::Key::Enter) {
            if (!name().empty() && phase_ == ConnectPhase::Idle) tryConnect();
            return;
        }
    }

    sf::Vector2f local(0.f, 0.f);
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
        local = app_.toLocal(mb->position);
    else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
        local = app_.toLocal(rb->position);

    for (int i = 0; i < static_cast<int>(FieldCount); ++i) {
        if (fields_[i].handleEvent(event, local)) {
            if (event.is<sf::Event::MouseButtonPressed>())
                activeField_ = static_cast<std::size_t>(i);
            return;
        }
    }

    if (phase_ == ConnectPhase::Idle && !name().empty())
        connectBtn_.handleEvent(event, local);
}

void ConnectScreen::update(float dtSec)
{
    for (auto& field : fields_) field.update(dtSec);

    if (phase_ == ConnectPhase::Idle) return;

    app_.connection().poll();

    while (app_.connection().hasMessages()) {
        auto msg = app_.connection().nextMessage();
        if (auto* welcome = std::get_if<chess::net::WelcomeMsg>(&msg)) {
            app_.switchScreen(std::make_unique<GameScreen>(
                app_, welcome->color, welcome->opponent));
            return;
        }
        if (auto* err = std::get_if<chess::net::ErrorMsg>(&msg)) {
            error_.setText(err->message);
            phase_ = ConnectPhase::Idle;
            status_.setText("");
            for (auto& field : fields_) field.setEnabled(true);
            app_.connection().disconnect();
            return;
        }
        if (std::holds_alternative<chess::net::OpponentLeftMsg>(msg)) {
            phase_ = ConnectPhase::Idle;
            status_.setText("Opponent left. Try again.");
            for (auto& field : fields_) field.setEnabled(true);
            app_.connection().disconnect();
            return;
        }
    }

    if (phase_ == ConnectPhase::Connecting &&
        app_.connection().state() == ConnectionState::Connected)
    {
        phase_ = ConnectPhase::WaitingForOpponent;
        status_.setText("Waiting for opponent...");
        app_.connection().join(name());
    }

    if (app_.connection().state() == ConnectionState::Disconnected) {
        if (phase_ == ConnectPhase::Connecting ||
            phase_ == ConnectPhase::WaitingForOpponent) {
            error_.setText(app_.connection().error().empty()
                ? "Connection failed" : app_.connection().error());
            phase_ = ConnectPhase::Idle;
            status_.setText("");
            for (auto& field : fields_) field.setEnabled(true);
        }
    }
}

void ConnectScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    for (int i = 0; i < static_cast<int>(FieldCount); ++i) {
        labels_[i].draw(window, font);
        fields_[i].draw(window, font);
    }

    if (phase_ != ConnectPhase::WaitingForOpponent && !name().empty())
        connectBtn_.draw(window, font);

    float statusY = BtnY + 44.f;
    if (!error_.text().empty()) {
        error_.setPosition({StartX, statusY});
        error_.draw(window, font);
        statusY += 30.f;
    }
    if (!status_.text().empty()) {
        status_.setPosition({StartX, statusY});
        status_.draw(window, font);
    }

    title_.draw(window, font);
    hint_.draw(window, font);
}

} // namespace chess::client