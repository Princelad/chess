#include "settings_screen.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "widgets/layout.h"

#include <algorithm>
#include <cmath>

namespace chess::client {

namespace {

// 14.4: settings status doubles as the theme preview — the two board
// colors as hex, so the cycler shows what landed without a live board.
std::string hexColor(const sf::Color& color)
{
    static const char* digits = "0123456789ABCDEF";
    std::string out = "#";
    for (unsigned channel : {color.r, color.g, color.b}) {
        out += digits[(channel >> 4) & 0xF];
        out += digits[channel & 0xF];
    }
    return out;
}
}

SettingsScreen::SettingsScreen(App& app)
    : app_(app)
{
    title_.setText("Settings");
    title_.setFontSize(ui::type::Big);
    title_.setColor(ui::pal::white);

    hint_.setText("Arrows/Tab: navigate · Space/Enter: activate · Esc: back");
    hint_.setFontSize(ui::type::Small);
    hint_.setColor(ui::pal::textGhost);

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

    animDurPrev_.setLabel("");
    animDurPrev_.setIcon(ui::icons::IconKind::ArrowLeft);
    animDurNext_.setLabel("");
    animDurNext_.setIcon(ui::icons::IconKind::ArrowRight);
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

    soundVolPrev_.setLabel("");
    soundVolPrev_.setIcon(ui::icons::IconKind::ArrowLeft);
    soundVolNext_.setLabel("");
    soundVolNext_.setIcon(ui::icons::IconKind::ArrowRight);
    soundVolPrev_.setFocusable(false);
    soundVolNext_.setFocusable(false);
    soundVolPrev_.setOnClick([this] { cycleSoundVol(-1); });
    soundVolNext_.setOnClick([this] { cycleSoundVol(1); });

    themePrev_.setLabel("");
    themePrev_.setIcon(ui::icons::IconKind::ArrowLeft);
    themeNext_.setLabel("");
    themeNext_.setIcon(ui::icons::IconKind::ArrowRight);
    themePrev_.setFocusable(false);
    themeNext_.setFocusable(false);
    themePrev_.setOnClick([this] {
        app_.setBoardTheme(nextBoardTheme(nextBoardTheme(app_.boardTheme())));
        updateThemeLabel();
        const auto c = boardColorsFor(app_.boardTheme());
        setStatus("Theme preview: " + hexColor(c.light) + " / " + hexColor(c.dark));
    });
    themeNext_.setOnClick([this] {
        app_.setBoardTheme(nextBoardTheme(app_.boardTheme()));
        updateThemeLabel();
        const auto c = boardColorsFor(app_.boardTheme());
        setStatus("Theme preview: " + hexColor(c.light) + " / " + hexColor(c.dark));
    });

    coordStylePrev_.setLabel("");
    coordStylePrev_.setIcon(ui::icons::IconKind::ArrowLeft);
    coordStyleNext_.setLabel("");
    coordStyleNext_.setIcon(ui::icons::IconKind::ArrowRight);
    coordStylePrev_.setFocusable(false);
    coordStyleNext_.setFocusable(false);
    coordStylePrev_.setOnClick([this] { cycleCoordStyle(-1); });
    coordStyleNext_.setOnClick([this] { cycleCoordStyle(1); });

    pieceSetPrev_.setLabel("");
    pieceSetPrev_.setIcon(ui::icons::IconKind::ArrowLeft);
    pieceSetNext_.setLabel("");
    pieceSetNext_.setIcon(ui::icons::IconKind::ArrowRight);
    pieceSetPrev_.setFocusable(false);
    pieceSetNext_.setFocusable(false);
    pieceSetPrev_.setOnClick([this] { cyclePieceSet(-1); });
    pieceSetNext_.setOnClick([this] { cyclePieceSet(1); });

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

    status_.setColor(ui::pal::textDimmed);

    const std::string configured = app_.config().get("pieces.path", "");
    setStatus(configured.empty() ? "Using bundled piece set"
                                 : "Piece set: " + configured);

    updateThemeLabel();
    updateCoordStyleLabel();
    updatePieceSetLabel();
    updateAnimDurLabel();
    updateSoundVolLabel();
    layoutRows();
    applyFocus();
}

void SettingsScreen::layoutRows()
{
    // 14.5: sectioned panels — Gameplay + Sound stack down the left
    // column, Board & pieces owns the right one; rows grow to touch size
    // inside the panels.
    constexpr float RowH = 34.f;
    constexpr float RowGap = 8.f;
    constexpr float PadX = 14.f;
    constexpr float ColLX = 80.f;
    constexpr float ColLW = 380.f;
    constexpr float ColRX = 500.f;
    constexpr float ColRW = 380.f;
    constexpr float BtnW = 48.f;
    constexpr float BtnH = 30.f;
    constexpr float LabelH = 24.f;   // section caption strip above a panel
    constexpr float PanelPad = 12.f; // row inset inside a panel

    // Left column band: Gameplay panel (3 rows) + gap + Sound panel (2
    // rows); the right column holds Board & pieces (4 cycler/checkbox
    // rows + the custom-path field block).
    constexpr float GameH = 2 * PanelPad + 3 * RowH + 2 * RowGap;
    constexpr float SoundH = 2 * PanelPad + 2 * RowH + RowGap;
    constexpr float LeftTop = 110.f;
    constexpr float SoundTop = LeftTop + LabelH + GameH + 18.f;
    constexpr float BoardH = 2 * PanelPad + 4 * RowH + 3 * RowGap
        + 18.f + 30.f + 6.f; // + caption + field row inside the panel

    gameplayLabel_.setText("Gameplay");
    gameplayLabel_.setFontSize(ui::type::Status);
    gameplayLabel_.setColor(ui::pal::textSecondary);
    gameplayLabel_.setPosition({ColLX + PadX,
        LeftTop + (LabelH - gameplayLabel_.bounds(app_.font()).size.y) * 0.5f
            - gameplayLabel_.bounds(app_.font()).position.y});
    gameplayPanel_.setRect(sf::FloatRect(
        sf::Vector2f(ColLX, LeftTop + LabelH), sf::Vector2f(ColLW, GameH)));

    soundLabel_.setText("Sound");
    soundLabel_.setFontSize(ui::type::Status);
    soundLabel_.setColor(ui::pal::textSecondary);
    soundLabel_.setPosition({ColLX + PadX,
        SoundTop + (LabelH - soundLabel_.bounds(app_.font()).size.y) * 0.5f
            - soundLabel_.bounds(app_.font()).position.y});
    soundPanel_.setRect(sf::FloatRect(
        sf::Vector2f(ColLX, SoundTop + LabelH), sf::Vector2f(ColLW, SoundH)));

    boardLabel_.setText("Board & pieces");
    boardLabel_.setFontSize(ui::type::Status);
    boardLabel_.setColor(ui::pal::textSecondary);
    boardLabel_.setPosition({ColRX + PadX,
        LeftTop + (LabelH - boardLabel_.bounds(app_.font()).size.y) * 0.5f
            - boardLabel_.bounds(app_.font()).position.y});
    boardPanel_.setRect(sf::FloatRect(
        sf::Vector2f(ColRX, LeftTop + LabelH), sf::Vector2f(ColRW, BoardH)));

    const float leftRowX = ColLX + PadX;
    const float leftRowW = ColLW - 2 * PadX;
    const float gameRowY = LeftTop + LabelH + PanelPad;
    const auto leftRowRect = [&](int row) {
        return sf::FloatRect(
            sf::Vector2f(leftRowX, gameRowY + row * (RowH + RowGap)),
            sf::Vector2f(leftRowW, RowH));
    };

    autoQueenCheck_.setRect(leftRowRect(0));
    animCheck_.setRect(leftRowRect(1));

    const auto placeCycler = [&](const sf::FloatRect& r, Button& prev,
                                 Button& next, Label& name) {
        prev.setRect(sf::FloatRect(
            sf::Vector2f(r.position.x, r.position.y + (r.size.y - BtnH) / 2.f),
            sf::Vector2f(BtnW, BtnH)));
        next.setRect(sf::FloatRect(
            sf::Vector2f(r.position.x + r.size.x - BtnW,
                         r.position.y + (r.size.y - BtnH) / 2.f),
            sf::Vector2f(BtnW, BtnH)));
        auto nb = name.bounds(app_.font());
        name.setPosition({
            r.position.x + (r.size.x - nb.size.x) / 2.f - nb.position.x,
            r.position.y + (r.size.y - nb.size.y) / 2.f - nb.position.y,
        });
    };
    placeCycler(leftRowRect(2), animDurPrev_, animDurNext_, animDurName_);

    const float soundRowY = SoundTop + LabelH + PanelPad;
    const auto soundRowRect = [&](int row) {
        return sf::FloatRect(
            sf::Vector2f(leftRowX, soundRowY + row * (RowH + RowGap)),
            sf::Vector2f(leftRowW, RowH));
    };
    soundCheck_.setRect(soundRowRect(0));
    placeCycler(soundRowRect(1), soundVolPrev_, soundVolNext_, soundVolName_);

    const float rightRowX = ColRX + PadX;
    const float rightRowW = ColRW - 2 * PadX;
    const float boardRowY = LeftTop + LabelH + PanelPad;
    const auto boardRowRect = [&](int row) {
        return sf::FloatRect(
            sf::Vector2f(rightRowX, boardRowY + row * (RowH + RowGap)),
            sf::Vector2f(rightRowW, RowH));
    };
    coordsCheck_.setRect(boardRowRect(0));
    placeCycler(boardRowRect(1),
                coordStylePrev_, coordStyleNext_, coordStyleName_);
    placeCycler(boardRowRect(2), themePrev_, themeNext_, themeName_);
    placeCycler(boardRowRect(3), pieceSetPrev_, pieceSetNext_, pieceSetName_);

    // Custom piece-set path block rides the Board panel's tail.
    const float pathCaptionY = boardRowY + 4 * (RowH + RowGap);
    piecesCaption_.setPosition({rightRowX, pathCaptionY});
    const float fieldW = 250.f;
    const float applyW = 70.f;
    const float fieldY = pathCaptionY + 18.f + 6.f;
    piecesField_.setRect(sf::FloatRect(
        sf::Vector2f(rightRowX, fieldY), sf::Vector2f(fieldW, 30.f)));
    applyBtn_.setRect(sf::FloatRect(
        sf::Vector2f(ColRX + ColRW - PadX - applyW, fieldY),
        sf::Vector2f(applyW, 30.f)));

    // Footer: status line + buttons ride the left column's tail.
    auto sb = status_.bounds(app_.font());
    const float footerY = SoundTop + LabelH + SoundH + 22.f;
    status_.setPosition({ColLX + PadX - sb.position.x,
                         footerY - sb.position.y});

    const float resetW = 150.f;
    const float backW = 90.f;
    const float btnY = footerY + 24.f;
    resetBtn_.setRect(sf::FloatRect(
        sf::Vector2f(ColLX + PadX, btnY), sf::Vector2f(resetW, 34.f)));
    backBtn_.setRect(sf::FloatRect(
        sf::Vector2f(ColRX + ColRW - backW, btnY), sf::Vector2f(backW, 34.f)));
}

void SettingsScreen::updateThemeLabel()
{
    themeName_.setText(boardThemeName(app_.boardTheme()));
    themeName_.setFontSize(15);
    themeName_.setColor(ui::pal::textBright);
}

// 14.4: settings status doubles as the theme preview — the two board
// colors as hex, so the cycler shows what landed without a live board.
void SettingsScreen::updateCoordStyleLabel()
{
    coordStyleName_.setText(coordinateStyleName(app_.coordinateStyle()));
    coordStyleName_.setFontSize(15);
    coordStyleName_.setColor(ui::pal::textBright);
}

void SettingsScreen::cycleCoordStyle(int dir)
{
    int idx = static_cast<int>(app_.coordinateStyle());
    idx = (idx + dir) % static_cast<int>(CoordinateStyle::Count);
    if (idx < 0) idx += static_cast<int>(CoordinateStyle::Count);
    app_.setCoordinateStyle(static_cast<CoordinateStyle>(idx));
    updateCoordStyleLabel();
    setStatus(coordinateStyleName(app_.coordinateStyle())
                  == coordinateStyleName(CoordinateStyle::Inside)
          ? "Coordinates inside the squares (chess.com style)"
          : "Coordinates at the square corners");
}

void SettingsScreen::updatePieceSetLabel()
{
    pieceSetName_.setText(pieceSetDirName(app_.pieceSet()));
    pieceSetName_.setFontSize(15);
    pieceSetName_.setColor(ui::pal::textBright);
}

void SettingsScreen::cyclePieceSet(int dir)
{
    int idx = static_cast<int>(app_.pieceSet());
    idx = (idx + dir) % static_cast<int>(PieceSet::Count);
    if (idx < 0) idx += static_cast<int>(PieceSet::Count);
    app_.setPieceSet(static_cast<PieceSet>(idx));
    if (!app_.config().get("pieces.path", "").empty()) {
        setStatus("Custom piece path overrides the named set");
    } else if (app_.reloadPieces()) {
        setStatus(std::string(pieceSetDirName(app_.pieceSet()))
            + " piece set loaded");
    } else {
        setStatus("Piece set unavailable — kept previous pieces");
    }
    updatePieceSetLabel();
    layoutRows();
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
    app_.config().set("board.coordinates", "corner");
    app_.config().set("pieces.path", "");
    app_.config().set("pieces.set", "cburnett");
    app_.config().setInt("sound.volume", 100);
    app_.config().setBool("sound.muted", false);
    app_.config().setBool("animation.enabled", true);
    app_.config().set("animation.duration", "0.3");
    app_.config().save();

    app_.sounds().setEnabled(true);
    app_.sounds().setVolume(100);

    autoQueenCheck_.setChecked(true);
    coordsCheck_.setChecked(true);
    animCheck_.setChecked(true);
    updateAnimDurLabel();
    soundCheck_.setChecked(true);
    updateSoundVolLabel();
    updateCoordStyleLabel();
    piecesField_.setText("");
    piecesField_.setFocused(false);
    applyPiecesPath();
    updateThemeLabel();
    updatePieceSetLabel();
    setStatus(app_.reloadPieces() ? "Defaults restored"
                                  : "Defaults restored (pieces unavailable)");
}

void SettingsScreen::setStatus(const std::string& text)
{
    status_.setText(text);
}

namespace {
constexpr float kAnimDurs[] = { 0.15f, 0.3f, 0.5f };
constexpr const char* kAnimDurStrs[] = { "0.15", "0.3", "0.5" };
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
    animDurName_.setColor(ui::pal::textBright);
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
    app_.config().set("animation.duration", kAnimDurStrs[idx]);
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
    soundVolName_.setColor(ui::pal::textBright);
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
            next == FocusCoordStylePrev || next == FocusCoordStyleNext ||
            next == FocusPieceSetPrev || next == FocusPieceSetNext ||
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
    if (coordStylePrev_.handleEvent(event, local)) return;
    if (coordStyleNext_.handleEvent(event, local)) return;
    if (pieceSetPrev_.handleEvent(event, local)) return;
    if (pieceSetNext_.handleEvent(event, local)) return;
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

    // 14.5: section panels under their captions, then the widgets on top.
    gameplayPanel_.draw(window);
    soundPanel_.draw(window);
    boardPanel_.draw(window);
    gameplayLabel_.draw(window, font);
    soundLabel_.draw(window, font);
    boardLabel_.draw(window, font);

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
    coordStylePrev_.draw(window, font);
    coordStyleNext_.draw(window, font);
    coordStyleName_.draw(window, font);
    pieceSetPrev_.draw(window, font);
    pieceSetNext_.draw(window, font);
    pieceSetName_.draw(window, font);

    piecesCaption_.draw(window, font);
    piecesField_.draw(window, font);
    applyBtn_.draw(window, font);
    status_.draw(window, font);

    resetBtn_.draw(window, font);
    backBtn_.draw(window, font);
}

} // namespace chess::client