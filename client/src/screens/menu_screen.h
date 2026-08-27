#pragma once

#include "app.h"

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
    int hovered_ = -1;

    float entryX() const;
    float entryY(int index) const;
    bool isDisabled(int index) const;
    void activateEntry(int index);
};

} // namespace chess::client
