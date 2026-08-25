#pragma once

#include "app.h"

#include <string>

namespace chess::client {

enum class ConnectPhase { Idle, Connecting, WaitingForOpponent };

class ConnectScreen : public Screen {
public:
    explicit ConnectScreen(App& app);
    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    void tryConnect();

    App& app_;
    ConnectPhase phase_ = ConnectPhase::Idle;

    std::string host_;
    std::string port_;
    std::string name_;
    std::string status_;
    std::string error_;

    int activeField_ = 0;
    float cursorBlink_ = 0.f;
    bool cursorVisible_ = true;
};

} // namespace chess::client
