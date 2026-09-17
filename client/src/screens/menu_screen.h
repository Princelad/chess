#pragma once

#include "app.h"
#include "widgets/button.h"
#include "widgets/label.h"

#include <array>

namespace chess::client {

class MenuScreen : public Screen {
public:
    explicit MenuScreen(App& app);
    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    static constexpr int EntryCount = 5;
    static constexpr float EntryW = 280.f;
    static constexpr float EntryH = 44.f;
    static constexpr float EntryGap = 8.f;

    App& app_;
    std::array<Button, EntryCount> entries_;
    int focused_ = 0;
    Label title_;
    Label hint_;

    void focusNext(bool down);
    void layoutButtons();
};

} // namespace chess::client