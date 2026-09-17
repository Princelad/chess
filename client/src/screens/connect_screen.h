#pragma once

#include "app.h"
#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/text_field.h"

#include <array>
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
    static constexpr std::size_t FieldCount = 3;

    void tryConnect();
    void cycleField(bool backward);
    void layoutWidgets();
    const std::string& name() const;

    App& app_;
    ConnectPhase phase_ = ConnectPhase::Idle;

    std::array<TextField, FieldCount> fields_;
    std::array<Label, FieldCount> labels_;
    std::size_t activeField_ = 0;
    Button connectBtn_;
    Label title_;
    Label hint_;
    Label status_;
    Label error_;
};

} // namespace chess::client