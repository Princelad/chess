#include "connect_screen.h"
#include "game_screen.h"

#include <chess/net/messages.h>

#include <algorithm>
#include <cctype>

namespace chess::client {

ConnectScreen::ConnectScreen(App& app)
    : app_(app)
    , host_(app.lastHost())
    , port_(app.lastPort())
    , name_(app.lastName())
    , activeField_(app.lastName().empty() ? 2 : 2)
{
    status_ = "Enter your name and press Enter to connect.";
}

void ConnectScreen::handleEvent(const sf::Event& event)
{
    if (phase_ == ConnectPhase::WaitingForOpponent) return;

    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Tab) {
            activeField_ = kp->shift
                ? (activeField_ + 2) % 3
                : (activeField_ + 1) % 3;
            return;
        }
        if (kp->code == sf::Keyboard::Key::Enter) {
            if (!name_.empty() && phase_ == ConnectPhase::Idle) tryConnect();
            return;
        }
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left &&
            phase_ == ConnectPhase::Idle && !name_.empty()) {
            float btnX = 580.f, btnY = 298.f, btnW = 200.f, btnH = 40.f;
            float mx = static_cast<float>(mb->position.x);
            float my = static_cast<float>(mb->position.y);
            if (mx >= btnX && mx <= btnX + btnW && my >= btnY && my <= btnY + btnH) {
                tryConnect();
                return;
            }
        }
    }

    if (const auto* te = event.getIf<sf::Event::TextEntered>()) {
        error_.clear();
        char c = static_cast<char>(te->unicode);
        if (c == '\b') {
            auto& field = activeField_ == 0 ? host_ : activeField_ == 1 ? port_ : name_;
            if (!field.empty()) field.pop_back();
        } else if (c >= 32 && c < 127) {
            auto& field = activeField_ == 0 ? host_ : activeField_ == 1 ? port_ : name_;
            if (field.size() < 32) field += c;
        }
        cursorBlink_ = 0.f;
        cursorVisible_ = true;
    }
}

void ConnectScreen::update(float dtSec)
{
    cursorBlink_ += dtSec;
    if (cursorBlink_ >= 0.5f) {
        cursorBlink_ -= 0.5f;
        cursorVisible_ = !cursorVisible_;
    }

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
            error_ = err->message;
            phase_ = ConnectPhase::Idle;
            status_ = "";
            app_.connection().disconnect();
            return;
        }
        if (std::holds_alternative<chess::net::OpponentLeftMsg>(msg)) {
            phase_ = ConnectPhase::Idle;
            status_ = "Opponent left. Try again.";
            app_.connection().disconnect();
            return;
        }
    }

    if (phase_ == ConnectPhase::Connecting &&
        app_.connection().state() == ConnectionState::Connected)
    {
        phase_ = ConnectPhase::WaitingForOpponent;
        status_ = "Waiting for opponent...";
        app_.connection().join(name_);
    }

    if (app_.connection().state() == ConnectionState::Disconnected) {
        if (phase_ == ConnectPhase::Connecting ||
            phase_ == ConnectPhase::WaitingForOpponent) {
            error_ = app_.connection().error().empty()
                ? "Connection failed" : app_.connection().error();
            phase_ = ConnectPhase::Idle;
            status_ = "";
        }
    }
}

void ConnectScreen::tryConnect()
{
    unsigned short portNum = 0;
    try {
        portNum = static_cast<unsigned short>(std::stoi(port_));
    } catch (...) {
        error_ = "Invalid port number";
        return;
    }

    app_.setLastConnection(host_, port_, name_);
    error_.clear();
    phase_ = ConnectPhase::Connecting;
    status_ = "Connecting...";
    app_.connection().connect(host_, portNum);
}

void ConnectScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    const char* labels[] = {"Host:", "Port:", "Name:"};
    std::string* fields[] = {&host_, &port_, &name_};

    float x = 500.f;
    float y = 120.f;
    float labelX = x;
    float fieldX = x + 80.f;
    float fieldW = 280.f;
    float fieldH = 36.f;
    float rowGap = 56.f;

    for (int i = 0; i < 3; ++i) {
        sf::Text label(font, labels[i], 20);
        label.setFillColor(sf::Color(200, 200, 200));
        label.setPosition({labelX, y + 6.f});
        window.draw(label);

        bool dimmed = phase_ != ConnectPhase::Idle;
        bool active = (i == activeField_) && !dimmed;
        sf::RectangleShape fieldBox({fieldW, fieldH});
        fieldBox.setPosition({fieldX, y});
        fieldBox.setFillColor(active
            ? sf::Color(70, 68, 66) : dimmed
            ? sf::Color(48, 46, 43) : sf::Color(58, 56, 54));
        fieldBox.setOutlineColor(active
            ? sf::Color(180, 180, 180) : dimmed
            ? sf::Color(68, 66, 64) : sf::Color(100, 100, 100));
        fieldBox.setOutlineThickness(active ? 2.f : 1.f);
        window.draw(fieldBox);

        sf::Text fieldText(font, *fields[i], 20);
        fieldText.setFillColor(phase_ != ConnectPhase::Idle
            ? sf::Color(100, 100, 100) : sf::Color(255, 255, 255));
        fieldText.setPosition({fieldX + 8.f, y + 6.f});
        window.draw(fieldText);

        if (i == activeField_ && cursorVisible_ && phase_ == ConnectPhase::Idle) {
            float cursorX = fieldX + 8.f + fieldText.getGlobalBounds().size.x + 2.f;
            sf::RectangleShape cursor({2.f, fieldH - 8.f});
            cursor.setPosition({cursorX, y + 4.f});
            cursor.setFillColor(sf::Color(255, 255, 255));
            window.draw(cursor);
        }

        y += rowGap;
    }

    if (phase_ != ConnectPhase::WaitingForOpponent && !name_.empty()) {
        sf::RectangleShape btn({200.f, 40.f});
        btn.setPosition({fieldX, y + 10.f});
        btn.setFillColor(sf::Color(76, 175, 80));
        window.draw(btn);

        sf::Text btnText(font, "Connect", 20);
        btnText.setFillColor(sf::Color(255, 255, 255));
        auto bounds = btnText.getGlobalBounds();
        btnText.setPosition({fieldX + 100.f - bounds.size.x / 2.f - bounds.position.x, y + 16.f});
        window.draw(btnText);
    }

    float statusY = y + 80.f;
    if (!error_.empty()) {
        sf::Text errText(font, error_, 18);
        errText.setFillColor(sf::Color(244, 67, 54));
        errText.setPosition({x, statusY});
        window.draw(errText);
        statusY += 30.f;
    }
    if (!status_.empty()) {
        sf::Text statusText(font, status_, 18);
        statusText.setFillColor(sf::Color(180, 180, 180));
        statusText.setPosition({x, statusY});
        window.draw(statusText);
    }

    sf::Text title(font, "Chess", 36);
    title.setFillColor(sf::Color(255, 255, 255));
    title.setPosition({50.f, 50.f});
    window.draw(title);

    sf::Text hint(font, "Tab to switch fields, Enter to connect", 14);
    hint.setFillColor(sf::Color(120, 120, 120));
    hint.setPosition({50.f, 590.f});
    window.draw(hint);
}

} // namespace chess::client
