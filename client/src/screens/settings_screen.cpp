#include "settings_screen.h"
#include "widgets/layout.h"

#include <algorithm>
#include <cmath>

namespace chess::client {

namespace {
constexpr float RowsX = 230.f;
constexpr float RowsW = 500.f;
constexpr float RowsTop = 130.f;
constexpr float RowGap = 14.f;
}

SettingsScreen::SettingsScreen(App& app)
    : app_(app)
{
    title_.setText("Settings");
    title_.setFontSize(28);
    title_.setColor(sf::Color(255, 255, 255));

    hint_.setText("Arrows/Tab: navigate · Space/Enter: activate · Esc: back");
    hint_.setFontSize(13);
    hint_.setColor(sf::Color(100, 100, 100));

    autoQueenCheck_.setLabel("Auto-queen (skip promotion picker)");
    autoQueenCheck_.setChecked(app_.autoQueen());
    autoQueenCheck_.setOnToggle([this](bool on) { app_.setAutoQueen(on); });

    coordsCheck_.setLabel("Show board coordinates");
    coordsCheck_.setChecked(app_.showCoordinates());
    coordsCheck_.setOnToggle([this](bool on) { app_.setShowCoordinates(on); });

    animCheck_.setLabel("Move animation");
    animCheck_.setChecked(app_.config().getBool("animation.enabled", true));
    animCheck_.setOnToggle([this](bool on) {
        app_.config().setBool("animation.enabled", on);
        app_.config().save();
    });

    animDurPrev_.setLabel("Previous duration");
    animDurNext_.setLabel("Next duration");
    animDurPrev_.setFocusable(false);
    animDurNext_.setFocusable(false);
    animDurPrev_.setOnClick([this] { cycleAnimDur(-1); });
    animDurNext_.setOnClick([this] { cycleAnimDur(1); });

    soundCheck_.setLabel("Enable sounds");
    soundCheck_.setChecked(!app_.config().getBool("sound.muted", false));
    soundCheck_.setOnToggle([this](bool on) {
        app_.config().setBool("sound.muted", !on);
        app_.config().save();
        app_.sounds().setEnabled(on);
    });

    soundVolPrev_.setLabel("Previous volume");
    soundVolNext_.setLabel("Next volume");
    soundVolPrev_.setFocusable(false);
    soundVolNext_.setFocusable(false);
    soundVolPrev_.setOnClick([this] { cycleSoundVol(-1); });
    soundVolNext_.setOnClick([this] { cycleSoundVol(1); });

    themePrev_.setLabel("Previous theme");
    themeNext_.setLabel("Next theme");
    themePrev_.setFocusable(false);
    themeNext_.setFocusable(false);
    themePrev_.setOnClick([this] {
        app_.setBoardTheme(nextBoardTheme(nextBoardTheme(app_.boardTheme())));
        updateThemeLabel();
    });
    themeNext_.setOnClick([this] {
        app_.setBoardTheme(nextBoardTheme(app_.boardTheme()));
        updateThemeLabel();
    });

    piecesCaption_.setText("Piece set path");
    piecesField_.setText(app_.config().get("pieces.path", ""));
    piecesField_.setMaxLength(256);
    piecesField_.setPlaceholder("directory with pieces (empty = bundled)");
    applyBtn_.setLabel("Apply");
    applyBtn_.setOnClick([this] { applyPiecesPath(); });

    resetBtn_.setLabel("Reset to defaults");
    resetBtn_.setOnClick([this] { resetDefaults(); });

    backBtn_.setLabel("Back");
    backBtn_.setOnClick([this] { app_.goBack(); });

    status_.setColor(sf::Color(150, 150, 150));

    const std::string configured = app_.config().get("pieces.path", "");
    setStatus(configured.empty() ? "Using bundled piece set"
                                 : "Piece set: " + configured);

    updateThemeLabel();
    updateAnimDurLabel();
    updateSoundVolLabel();
    layoutRows();
    applyFocus();
}

void SettingsScreen::layoutRows()
{
    const auto rows = layout::vstack(
        sf::FloatRect(sf::Vector2f(RowsX, RowsTop), sf::Vector2f(RowsW, 300.f)),
        RowGap,
        { sf::Vector2f(RowsW, 34.f),  // auto-queen
          sf::Vector2f(RowsW, 34.f),  // coords
          sf::Vector2f(RowsW, 34.f),  // animation
          sf::Vector2f(RowsW, 34.f),  // sound
          sf::Vector2f(RowsW, 34.f),  // theme
          sf::Vector2f(RowsW, 36.f),  // piece set path
          sf::Vector2f(RowsW, 18.f),  // status
          sf::Vector2f(RowsW, 40.f) }); // buttons

    autoQueenCheck_.setRect(rows[0]);
    coordsCheck_.setRect(rows[1]);
    animCheck_.setRect(rows[2]);

    const float btnW = 48.f;
    const float btnH = 30.f;

    auto placeCycleRow = [&](Checkbox& check, Button& prev, Button& next,
                             Label& name, int rowIdx) {
        check.setRect(rows[rowIdx]);
        const float yy = rows[rowIdx].position.y + (rows[rowIdx].size.y - btnH) / 2.f;
        prev.setRect(sf::FloatRect(
            sf::Vector2f(rows[rowIdx].position.x, yy), sf::Vector2f(btnW, btnH)));
        next.setRect(sf::FloatRect(
            sf::Vector2f(rows[rowIdx].position.x + rows[rowIdx].size.x - btnW, yy),
            sf::Vector2f(btnW, btnH)));
        auto nb = name.bounds(app_.font());
        name.setPosition({
            rows[rowIdx].position.x + (rows[rowIdx].size.x - nb.size.x) / 2.f
                - nb.position.x,
            yy + (btnH - nb.size.y) / 2.f - nb.position.y
        });
    };
    placeCycleRow(animCheck_, animDurPrev_, animDurNext_, animDurName_, 2);
    placeCycleRow(soundCheck_, soundVolPrev_, soundVolNext_, soundVolName_, 3);

    const float yTheme = rows[4].position.y + (rows[4].size.y - btnH) / 2.f;
    themePrev_.setRect(sf::FloatRect(
        sf::Vector2f(rows[4].position.x, yTheme), sf::Vector2f(btnW, btnH)));
    themeNext_.setRect(sf::FloatRect(
        sf::Vector2f(rows[4].position.x + rows[4].size.x - btnW, yTheme),
        sf::Vector2f(btnW, btnH)));

    auto tf = themeName_.bounds(app_.font());
    themeName_.setPosition({
        rows[4].position.x + (rows[4].size.x - tf.size.x) / 2.f - tf.position.x,
        yTheme + (btnH - tf.size.y) / 2.f - tf.position.y
    });

    piecesCaption_.setPosition({ rows[5].position.x, rows[5].position.y - 20.f });

    const float fieldW = 300.f;
    const float applyW = 70.f;
    const float y3 = rows[5].position.y + (rows[5].size.y - 32.f) / 2.f;
    piecesField_.setRect(sf::FloatRect(
        sf::Vector2f(rows[5].position.x, y3), sf::Vector2f(fieldW, 32.f)));
    applyBtn_.setRect(sf::FloatRect(
        sf::Vector2f(rows[5].position.x + rows[5].size.x - applyW, y3),
        sf::Vector2f(applyW, 32.f)));

    auto sb = status_.bounds(app_.font());
    status_.setPosition({ rows[6].position.x, rows[6].position.y - sb.position.y });

    const float resetW = 150.f;
    const float backW = 90.f;
    const float y5 = rows[7].position.y + (rows[7].size.y - 34.f) / 2.f;
    resetBtn_.setRect(sf::FloatRect(
        sf::Vector2f(rows[7].position.x, y5), sf::Vector2f(resetW, 34.f)));
    backBtn_.setRect(sf::FloatRect(
        sf::Vector2f(rows[7].position.x + rows[7].size.x - backW, y5),
        sf::Vector2f(backW, 34.f)));
}

void SettingsScreen::updateThemeLabel()
{
    themeName_.setText(boardThemeName(app_.boardTheme()));
    themeName_.setFontSize(15);
    themeName_.setColor(sf::Color(230, 230, 230));
}

void SettingsScreen::applyPiecesPath()
{
    std::string path = piecesField_.text();
    const auto notSpace = [](char c) { return c != ' ' && c != '\t'; };
    const auto first = std::find_if_not(path.begin(), path.end(), notSpace);
    path.erase(path.begin(), first);
    if (!path.empty()) {
        const auto last = std::find_if_not(path.rbegin(), path.rend(), notSpace).base();
        path.erase(last, path.end());
    }

    app_.config().set("pieces.path", path);
    app_.config().save();
    piecesField_.setText(path);

    if (app_.reloadPieces())
        setStatus(path.empty() ? "Using bundled piece set"
                               : "Loaded piece set: " + path);
    else
        setStatus(path.empty() ? "Bundled set unavailable"
                               : "Path not found — kept previous pieces");
}

void SettingsScreen::resetDefaults()
{
    app_.config().setBool("general.auto_queen", true);
    app_.config().set("board.colors", "classic");
    app_.config().setBool("board.show_coordinates", true);
    app_.config().set("pieces.path", "");
    app_.config().setInt("sound.volume", 100);
    app_.config().setBool("sound.muted", false);
    app_.config().setBool("animation.enabled", true);
    app_.config().set("animation.duration", "0.3");
    app_.config().save();

    autoQueenCheck_.setChecked(true);
    coordsCheck_.setChecked(true);
    animCheck_.setChecked(true);
    updateAnimDurLabel();
    soundCheck_.setChecked(true);
    updateSoundVolLabel();
    piecesField_.setText("");
    piecesField_.setFocused(false);
    applyPiecesPath();
    updateThemeLabel();
    setStatus(app_.reloadPieces() ? "Defaults restored"
                                  : "Defaults restored (pieces unavailable)");
}

void SettingsScreen::setStatus(const std::string& text)
{
    status_.setText(text);
}

namespace {
constexpr float kAnimDurs[] = { 0.15f, 0.3f, 0.5f };
constexpr const char* kAnimDurLabels[] = { "Fast  0.15", "Normal  0.3", "Slow  0.5" };
constexpr int kVols[] = { 25, 60, 100 };
constexpr const char* kVolLabels[] = { "Quiet  25", "Medium  60", "Loud  100" };
} // anonymous namespace

void SettingsScreen::updateAnimDurLabel()
{
    const double d = app_.config().getFloat("animation.duration", 0.3);
    int idx = 1;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(static_cast<float>(d) - kAnimDurs[i]) < 0.01f) {
            idx = i;
            break;
        }
    }
    animDurName_.setText(kAnimDurLabels[idx]);
    animDurName_.setFontSize(15);
    animDurName_.setColor(sf::Color(230, 230, 230));
}

void SettingsScreen::cycleAnimDur(int dir)
{
    const double d = app_.config().getFloat("animation.duration", 0.3);
    int idx = 1;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(static_cast<float>(d) - kAnimDurs[i]) < 0.01f) {
            idx = i;
            break;
        }
    }
    idx = (idx + dir) % 3;
    if (idx < 0) idx += 3;
    app_.config().set("animation.duration", std::to_string(kAnimDurs[idx]));
    app_.config().save();
    updateAnimDurLabel();
    layoutRows();
}

void SettingsScreen::updateSoundVolLabel()
{
    const int v = std::clamp(app_.config().getInt("sound.volume", 100), 0, 100);
    int idx = 2;
    if (v <= 25) idx = 0;
    else if (v <= 60) idx = 1;
    soundVolName_.setText(kVolLabels[idx]);
    soundVolName_.setFontSize(15);
    soundVolName_.setColor(sf::Color(230, 230, 230));
}

void SettingsScreen::cycleSoundVol(int dir)
{
    const int v = std::clamp(app_.config().getInt("sound.volume", 100), 0, 100);
    int idx = 2;
    if (v <= 25) idx = 0;
    else if (v <= 60) idx = 1;
    idx = (idx + dir) % 3;
    if (idx < 0) idx += 3;
    app_.config().setInt("sound.volume", kVols[idx]);
    app_.config().save();
    app_.sounds().setVolume(kVols[idx]);
    updateSoundVolLabel();
    layoutRows();
}

void SettingsScreen::focusNext(bool down)
{
    int next = focusIdx_;
    for (int i = 0; i < FocusCount; ++i) {
        next = down ? (next + 1) : (next - 1);
        if (next < 0) next = FocusCount - 1;
        if (next >= FocusCount) next = 0;
        if (next == FocusThemePrev || next == FocusThemeNext ||
            next == FocusAnimDurPrev || next == FocusAnimDurNext ||
            next == FocusSoundVolPrev || next == FocusSoundVolNext)
            continue;
        focusIdx_ = next;
        applyFocus();
        return;
    }
}

void SettingsScreen::applyFocus()
{
    autoQueenCheck_.setFocused(focusIdx_ == FocusAutoQueen);
    coordsCheck_.setFocused(focusIdx_ == FocusCoords);
    animCheck_.setFocused(focusIdx_ == FocusAnimToggle);
    soundCheck_.setFocused(focusIdx_ == FocusSoundToggle);
    piecesField_.setFocused(focusIdx_ == FocusPiecesField);
    applyBtn_.setFocused(focusIdx_ == FocusApply);
    resetBtn_.setFocused(focusIdx_ == FocusReset);
    backBtn_.setFocused(focusIdx_ == FocusBack);
}

void SettingsScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) { app_.goBack(); return; }
        if (kp->code == sf::Keyboard::Key::Up ||
            kp->code == sf::Keyboard::Key::Tab) { focusNext(false); return; }
        if (kp->code == sf::Keyboard::Key::Down) { focusNext(true); return; }
    }

    sf::Vector2f local(0.f, 0.f);
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
        local = app_.toLocal(mm->position);
    else if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
        local = app_.toLocal(mb->position);
    else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
        local = app_.toLocal(rb->position);

    if (autoQueenCheck_.handleEvent(event, local)) return;
    if (coordsCheck_.handleEvent(event, local)) return;
    if (animCheck_.handleEvent(event, local)) return;
    if (animDurPrev_.handleEvent(event, local)) return;
    if (animDurNext_.handleEvent(event, local)) return;
    if (soundCheck_.handleEvent(event, local)) return;
    if (soundVolPrev_.handleEvent(event, local)) return;
    if (soundVolNext_.handleEvent(event, local)) return;
    if (themePrev_.handleEvent(event, local)) return;
    if (themeNext_.handleEvent(event, local)) return;
    if (piecesField_.handleEvent(event, local)) return;
    if (applyBtn_.handleEvent(event, local)) return;
    if (resetBtn_.handleEvent(event, local)) return;
    backBtn_.handleEvent(event, local);
}

void SettingsScreen::update(float dtSec)
{
    piecesField_.update(dtSec);
}

void SettingsScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    auto titleBounds = title_.bounds(font);
    title_.setPosition({
        (App::WindowWidth - titleBounds.size.x) / 2.f - titleBounds.position.x,
        60.f
    });
    title_.draw(window, font);

    autoQueenCheck_.draw(window, font);
    coordsCheck_.draw(window, font);
    animCheck_.draw(window, font);
    animDurPrev_.draw(window, font);
    animDurNext_.draw(window, font);
    animDurName_.draw(window, font);
    soundCheck_.draw(window, font);
    soundVolPrev_.draw(window, font);
    soundVolNext_.draw(window, font);
    soundVolName_.draw(window, font);
    themePrev_.draw(window, font);
    themeNext_.draw(window, font);
    themeName_.draw(window, font);

    piecesCaption_.draw(window, font);
    piecesField_.draw(window, font);
    applyBtn_.draw(window, font);
    status_.draw(window, font);

    resetBtn_.draw(window, font);
    backBtn_.draw(window, font);
}

} // namespace chess::client