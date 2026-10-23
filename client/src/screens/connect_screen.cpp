#include "connect_screen.h"
#include "game_screen.h"
#include "history_screen.h"
#include "leaderboard_screen.h"

#include <chess/net/messages.h>

namespace chess::client {

namespace {
constexpr float StartX = 500.f;
constexpr float StartY = 140.f;
constexpr float FieldX = StartX + 80.f;
constexpr float FieldW = 280.f;
constexpr float FieldH = 36.f;
constexpr float RowGap = 56.f;
constexpr float BtnY = 148.f + 6.f * RowGap + 10.f;
}

ConnectScreen::ConnectScreen(App& app)
    : app_(app)
{
    static const char* FieldLabels[FieldCount] = { "Host:", "Port:", "Name:", "Password:" };
    fields_[0].setText(app.lastHost());
    fields_[1].setText(app.lastPort());
    fields_[2].setText(app.lastName());
    for (int i = 0; i < static_cast<int>(FieldCount); ++i) {
        labels_[i].setText(FieldLabels[i]);
        labels_[i].setFontSize(20);
        labels_[i].setColor(sf::Color(200, 200, 200));
        fields_[i].setMaxLength(32);
    }
    // Time control presets (11.1): cycled by the picker button, first entry
    // stays the classic unlimited game. The choice persists in the config.
    static constexpr const char* TimeControlPresets[TimeControlCount] = {
        "", "1+0", "3+2", "5+3", "10+0", "15+10", "30+0",
    };
    for (int i = 0; i < TimeControlCount; ++i)
        timeControls_[i] = TimeControlPresets[i];
    const std::string savedTc = app_.config().get("general.time_control", "");
    for (int i = TimeControlCount; i-- > 0;) {
        if (savedTc == timeControls_[i]) {
            tcIndex_ = static_cast<std::size_t>(i);
            break;
        }
    }
    tcLabel_.setText("Time control:");
    tcLabel_.setFontSize(20);
    tcLabel_.setColor(sf::Color(200, 200, 200));
    tcBtn_.setColors(sf::Color(70, 74, 96), sf::Color(84, 88, 112),
                     sf::Color(58, 62, 82), sf::Color(45, 48, 66),
                     sf::Color(150, 150, 150), sf::Color(255, 255, 255),
                     sf::Color(60, 64, 86), sf::Color(160, 168, 200),
                     sf::Color(200, 208, 240));
    tcBtn_.setOnClick([this] { selectTimeControl(); });
    updateTimeControlLabel();
    // Game variant presets (12.1): cycled by the picker beside the time
    // control; "" is the standard game, "chess960" selects a rolled Start
    // Position via the server, and the choice persists in the config. Only
    // the exact tag is ever sent — the server degrades anything else.
    static constexpr const char* VariantPresets[VariantCount] = {
        "", chess::net::Chess960Variant,
    };
    for (int i = 0; i < VariantCount; ++i)
        variants_[i] = VariantPresets[i];
    const std::string savedVariant = app_.config().get("general.variant", "");
    for (int i = VariantCount; i-- > 0;) {
        if (savedVariant == variants_[i]) {
            variantIndex_ = static_cast<std::size_t>(i);
            break;
        }
    }
    if (savedVariant != variants_[variantIndex_]) {
        // Hand-edited/unknown tags re-normalize to the standard game (the
        // same exact-match rule the server applies to the join payload).
        app_.config().set("general.variant", variants_[0]);
    }
    variantLabel_.setText("Variant:");
    variantLabel_.setFontSize(20);
    variantLabel_.setColor(sf::Color(200, 200, 200));
    variantBtn_.setColors(sf::Color(70, 74, 96), sf::Color(84, 88, 112),
                          sf::Color(58, 62, 82), sf::Color(45, 48, 66),
                          sf::Color(150, 150, 150), sf::Color(255, 255, 255),
                          sf::Color(60, 64, 86), sf::Color(160, 168, 200),
                          sf::Color(200, 208, 240));
    variantBtn_.setOnClick([this] { selectVariant(); });
    updateVariantLabel();
    // Password: masked input, longer allowed than names, guest by default.
    fields_[3].setMasked(true);
    fields_[3].setMaxLength(64);
    fields_[3].setPlaceholder("empty = guest");
    for (auto& field : fields_) field.setOnCommit([this] { tryConnectOrRetry(); });
    if (app.lastName().empty())
        activeField_ = 0;
    else
        activeField_ = 3;
    fields_[activeField_].setFocused(true);

    connectBtn_.setLabel("Connect");
    connectBtn_.setRect(sf::FloatRect(sf::Vector2f(FieldX, BtnY),
                                      sf::Vector2f(200.f, 40.f)));
    connectBtn_.setColors(sf::Color(76, 175, 80), sf::Color(90, 190, 95),
                          sf::Color(60, 150, 70), sf::Color(40, 80, 45),
                          sf::Color(150, 150, 150), sf::Color(255, 255, 255),
                          sf::Color(60, 140, 65), sf::Color(160, 220, 160),
                          sf::Color(220, 255, 220));
    connectBtn_.setOnClick([this] { tryConnectOrRetry(); });

    archiveBtn_.setLabel("Match archive");
    archiveBtn_.setColors(sf::Color(60, 90, 160), sf::Color(75, 105, 180),
                          sf::Color(52, 80, 140), sf::Color(45, 60, 90),
                          sf::Color(150, 150, 150), sf::Color(255, 255, 255),
                          sf::Color(52, 78, 135), sf::Color(160, 190, 240),
                          sf::Color(210, 230, 255));
    archiveBtn_.setOnClick([this] { startBrowse(BrowseTarget::Archive); });

    leaderboardBtn_.setLabel("Leaderboard");
    leaderboardBtn_.setColors(sf::Color(46, 62, 58), sf::Color(58, 78, 72),
                              sf::Color(52, 70, 64), sf::Color(40, 55, 52),
                              sf::Color(150, 150, 150), sf::Color(240, 250, 240),
                              sf::Color(70, 110, 90), sf::Color(140, 200, 170),
                              sf::Color(180, 235, 205));
    leaderboardBtn_.setOnClick([this] { startBrowse(BrowseTarget::Leaderboard); });

    title_.setText("Chess");
    title_.setFontSize(36);
    title_.setColor(sf::Color(255, 255, 255));

    hint_.setText("Tab to switch fields, Enter to connect, Esc to go back");
    hint_.setFontSize(14);
    hint_.setColor(sf::Color(120, 120, 120));

    status_.setText("Enter your name (password optional) and press Enter to connect.");
    status_.setFontSize(18);
    status_.setColor(sf::Color(180, 180, 180));

    layoutWidgets();
}

void ConnectScreen::layoutWidgets()
{
    title_.setPosition({50.f, 50.f});
    hint_.setPosition({50.f, 590.f});
    // Below both button rows (Connect + archive on row one, leaderboard
    // on row two) so statuses never render underneath a button.
    status_.setPosition({StartX, BtnY + 104.f});
    error_.setPosition({StartX, BtnY + 104.f});

    float y = StartY;
    for (int i = 0; i < static_cast<int>(FieldCount); ++i) {
        labels_[i].setPosition({StartX, y + 6.f});
        fields_[i].setRect(sf::FloatRect(sf::Vector2f(FieldX, y),
                                         sf::Vector2f(FieldW, FieldH)));
        y += RowGap;
    }
    connectBtn_.setRect(sf::FloatRect(sf::Vector2f(FieldX, BtnY),
                                      sf::Vector2f(200.f, 40.f)));
    // Side button: browse the server's game archive without queueing.
    archiveBtn_.setRect(sf::FloatRect(sf::Vector2f(FieldX + 210.f, BtnY),
                                      sf::Vector2f(170.f, 40.f)));
    // Row two: leaderboard browsing, same connect-without-queueing flow.
    leaderboardBtn_.setRect(sf::FloatRect(sf::Vector2f(FieldX, BtnY + 50.f),
                                          sf::Vector2f(200.f, 40.f)));
    // TC picker picks up the row after the last field, buttons shifted one
    // row down: the preset is chosen before pairing, so it needs a slot of
    // its own among the fields. The variant picker (12.1) rides the row
    // below it.
    tcLabel_.setPosition({StartX, StartY + 4.f * RowGap + 6.f});
    tcBtn_.setRect(sf::FloatRect(sf::Vector2f(FieldX, StartY + 4.f * RowGap),
                                 sf::Vector2f(FieldW, FieldH)));
    variantLabel_.setPosition({StartX, StartY + 5.f * RowGap + 6.f});
    variantBtn_.setRect(sf::FloatRect(
        sf::Vector2f(FieldX, StartY + 5.f * RowGap),
        sf::Vector2f(FieldW, FieldH)));
}

// Cycles the preset forward and stamps the choice into the config, so the
// next visit starts where the player left off.
void ConnectScreen::selectTimeControl()
{
    tcIndex_ = (tcIndex_ + 1) % timeControls_.size();
    updateTimeControlLabel();
    app_.config().set("general.time_control", timeControls_[tcIndex_]);
}

void ConnectScreen::updateTimeControlLabel()
{
    tcBtn_.setLabel(timeControls_[tcIndex_].empty()
        ? "Time control: unlimited"
        : "Time control: " + timeControls_[tcIndex_]);
}

// Cycles Standard <-> Chess960 and stamps the choice into the config, like
// the time control picker.
void ConnectScreen::selectVariant()
{
    variantIndex_ = (variantIndex_ + 1) % variants_.size();
    updateVariantLabel();
    app_.config().set("general.variant", variants_[variantIndex_]);
}

void ConnectScreen::updateVariantLabel()
{
    variantBtn_.setLabel(variants_[variantIndex_].empty()
        ? "Variant: Standard"
        : "Variant: " + variants_[variantIndex_]);
}

void ConnectScreen::cycleField(bool backward){
    int next = static_cast<int>(activeField_);
    if (backward)
        next = (next - 1 + static_cast<int>(FieldCount)) % static_cast<int>(FieldCount);
    else
        next = (next + 1) % static_cast<int>(FieldCount);

    fields_[activeField_].setFocused(false);
    activeField_ = static_cast<std::size_t>(next);
    fields_[activeField_].setFocused(true);
}

void ConnectScreen::tryConnectOrRetry()
{
    if (phase_ == ConnectPhase::Idle) {
        if (name().empty()) return;

        unsigned short portNum = 0;
        try {
            portNum = static_cast<unsigned short>(std::stoi(fields_[1].text()));
        } catch (...) {
            error_.setText("Invalid port number");
            return;
        }

        app_.setLastConnection(fields_[0].text(), fields_[1].text(), fields_[2].text());
        app_.clearProfile();
        error_.setText("");
        phase_ = ConnectPhase::Connecting;
        status_.setText("Connecting...");
        for (auto& field : fields_) field.setEnabled(false);
        app_.connection().connect(fields_[0].text(), portNum);
        return;
    }

    // Login retry after a failed attempt: the socket is still connected, so
    // just take the (possibly edited) credentials again.
    if (phase_ == ConnectPhase::AwaitingProfile) {
        error_.setText("");
        if (!password().empty()) {
            status_.setText("Logging in...");
            app_.connection().send(chess::net::LoginMsg{name(), password()});
        } else if (browseTarget_ != BrowseTarget::None) {
            // Switched to guest-mode browsing after an error.
            openBrowseTarget();
        } else {
            // Switched to guest play after an account error.
            phase_ = ConnectPhase::WaitingForOpponent;
            status_.setText("Waiting for opponent...");
            for (auto& field : fields_) field.setEnabled(false);
            app_.clearProfile();
            app_.connection().join(name(), timeControl(), variant());
        }
    }
}

const std::string& ConnectScreen::name() const
{
    return fields_[2].text();
}

const std::string& ConnectScreen::password() const
{
    return fields_[3].text();
}

void ConnectScreen::openBrowseTarget()
{
    const bool mine = app_.profile().has_value()
        && !app_.profile()->isGuest;
    // Hand over to the browse target; this screen gets destroyed by the
    // switch (Idle-state entry point only).
    if (browseTarget_ == BrowseTarget::Leaderboard)
        app_.switchScreen(std::make_unique<LeaderboardScreen>(app_));
    else
        app_.switchScreen(std::make_unique<HistoryScreen>(app_, mine));
}

// Browsing: connect WITHOUT joining a queue; the archive list filters
// itself when this connection is (or will be) a logged-in account, the
// leaderboard is the global top-N either way.
void ConnectScreen::startBrowse(BrowseTarget target)
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
    app_.clearProfile();
    error_.setText("");
    browseTarget_ = target;
    phase_ = ConnectPhase::ConnectingBrowse;
    status_.setText("Connecting...");
    for (auto& field : fields_) field.setEnabled(false);
    app_.connection().connect(fields_[0].text(), portNum);
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
            if (!name().empty()
                && (phase_ == ConnectPhase::Idle
                    || phase_ == ConnectPhase::AwaitingProfile))
                tryConnectOrRetry();
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

    if (phase_ == ConnectPhase::Idle && !name().empty()
        || phase_ == ConnectPhase::AwaitingProfile)
        connectBtn_.handleEvent(event, local);
    if (phase_ == ConnectPhase::Idle && !name().empty()) {
        archiveBtn_.handleEvent(event, local);
        leaderboardBtn_.handleEvent(event, local);
        tcBtn_.handleEvent(event, local);
        variantBtn_.handleEvent(event, local);
    }
}

void ConnectScreen::update(float dtSec)
{
    for (auto& field : fields_) field.update(dtSec);

    if (phase_ == ConnectPhase::Idle) return;

    app_.connection().poll();

    while (app_.connection().hasMessages()) {
        auto msg = app_.connection().nextMessage();
        if (auto* welcome = std::get_if<chess::net::WelcomeMsg>(&msg)) {
            // Opponent rating is only meaningful for rated games; guests
            // would otherwise see the default 1500 ± 350 as noise.
            std::optional<std::pair<double, double>> opponentRating;
            if (welcome->rated)
                opponentRating = std::make_pair(welcome->rating, welcome->deviation);
            app_.switchScreen(std::make_unique<GameScreen>(
                app_, welcome->color, welcome->opponent, opponentRating,
                welcome->rated, welcome->timeControl, welcome->resumeToken,
                welcome->startFen));
            return;
        }
        if (auto* profileMsg = std::get_if<chess::net::ProfileMsg>(&msg)) {
            app_.setProfile(*profileMsg);
            if (phase_ == ConnectPhase::AwaitingProfile) {
                if (browseTarget_ != BrowseTarget::None) {
                    openBrowseTarget();
                } else {
                    phase_ = ConnectPhase::WaitingForOpponent;
                    status_.setText("Waiting for opponent...");
                    app_.connection().join(name(), timeControl(), variant());
                }
            }
            continue;
        }
        if (auto* err = std::get_if<chess::net::ErrorMsg>(&msg)) {
            if (phase_ == ConnectPhase::AwaitingProfile) {
                // Bad password / unknown name with empty password: keep the
                // connection so the user can fix the fields and retry.
                error_.setText(err->message);
                status_.setText("Fix the fields and press Enter to retry.");
                status_.setColor(sf::Color(180, 180, 180));
                for (auto& field : fields_) field.setEnabled(true);
                return;
            }
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
        if (!password().empty()) {
            phase_ = ConnectPhase::AwaitingProfile;
            status_.setText("Logging in...");
            app_.connection().send(
                chess::net::LoginMsg{name(), password()});
        } else {
            phase_ = ConnectPhase::WaitingForOpponent;
            status_.setText("Waiting for opponent...");
            app_.connection().join(name(), timeControl(), variant());
        }
    }
    else if (phase_ == ConnectPhase::ConnectingBrowse &&
        app_.connection().state() == ConnectionState::Connected)
    {
        // Archives need the profile only for the "my games" filter, the
        // leaderboard not at all; the login still refreshes last_seen/RD.
        if (!password().empty()) {
            phase_ = ConnectPhase::AwaitingProfile;
            status_.setText("Logging in...");
            app_.connection().send(
                chess::net::LoginMsg{name(), password()});
        } else {
            openBrowseTarget();
        }
    }

    if (app_.connection().state() == ConnectionState::Disconnected) {
        if (phase_ == ConnectPhase::Connecting
            || phase_ == ConnectPhase::ConnectingBrowse
            || phase_ == ConnectPhase::AwaitingProfile
            || phase_ == ConnectPhase::WaitingForOpponent) {
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
    if (phase_ == ConnectPhase::Idle && !name().empty()) {
        archiveBtn_.draw(window, font);
        leaderboardBtn_.draw(window, font);
    }
    tcLabel_.draw(window, font);
    if (phase_ == ConnectPhase::Idle)
        tcBtn_.draw(window, font);
    variantLabel_.draw(window, font);
    if (phase_ == ConnectPhase::Idle)
        variantBtn_.draw(window, font);

    float statusY = BtnY + 104.f;
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