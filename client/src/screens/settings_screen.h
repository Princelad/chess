#pragma once

#include "app.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"
#include "widgets/text_field.h"

namespace chess::client {

class SettingsScreen : public Screen {
public:
    explicit SettingsScreen(App& app);

    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    enum FocusId {
        FocusAutoQueen,
        FocusCoords,
        FocusAnimToggle,
        FocusAnimDurPrev,
        FocusAnimDurNext,
        FocusThemePrev,
        FocusThemeNext,
        FocusPiecesField,
        FocusApply,
        FocusReset,
        FocusBack,
        FocusCount
    };

    void layoutRows();
    void focusNext(bool down);
    void applyFocus();
    void updateThemeLabel();
    void updateAnimDurLabel();
    void cycleAnimDur(int dir);
    void applyPiecesPath();
    void resetDefaults();
    void setStatus(const std::string& text);

    App& app_;
    Label title_;
    Label hint_;

    Checkbox autoQueenCheck_;
    Checkbox coordsCheck_;

    Checkbox animCheck_;
    Button animDurPrev_;
    Button animDurNext_;
    Label animDurName_;

    Button themePrev_;
    Button themeNext_;
    Label themeName_;

    Label piecesCaption_;
    TextField piecesField_;
    Button applyBtn_;
    Label status_;

    Button resetBtn_;
    Button backBtn_;

    int focusIdx_ = 0;
};

} // namespace chess::client