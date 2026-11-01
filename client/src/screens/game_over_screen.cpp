#include "game_over_screen.h"

#include "ui/theme.h"
#include "analysis_screen.h"
#include "game_screen.h"
#include "history_screen.h"
#include "menu_screen.h"
#include "tournament_lobby_screen.h"
#include "widgets/layout.h"

namespace chess::client {

namespace {
constexpr float BtnW = 200.f;
constexpr float BtnH = 40.f;
constexpr float OfferBtnW = 180.f;
constexpr float ResultY = 165.f;
constexpr float ReasonY = 210.f;
constexpr float StatusY = 250.f;
constexpr float OfferY = 285.f;
constexpr float RematchY = 335.f;
constexpr float ArchiveBtnY = 385.f;
constexpr float AnalyzeBtnY = 435.f;
constexpr float MenuBtnY = 485.f;
}

GameOverScreen::GameOverScreen(App& app,
                               net::GameResult result,
                               net::GameOverReason reason,
                               Board initialBoard,
                               std::vector<chess::Move> moves,
                               std::vector<std::string> sanMoves,
                               bool arena,
                               const std::string& arenaTimeControl,
                               const std::string& arenaVariant)
    : app_(app)
    , initialBoard_(std::move(initialBoard))
    , moves_(std::move(moves))
    , sanMoves_(std::move(sanMoves))
    , arena_(arena)
    , arenaTimeControl_(arenaTimeControl)
    , arenaVariant_(arenaVariant)
{
    std::string reasonStr;
    switch (reason) {
        case net::GameOverReason::Checkmate:          reasonStr = "Checkmate"; break;
        case net::GameOverReason::Stalemate:          reasonStr = "Stalemate"; break;
        case net::GameOverReason::FiftyMove:          reasonStr = "Fifty-move rule"; break;
        case net::GameOverReason::Repetition:         reasonStr = "Threefold repetition"; break;
        case net::GameOverReason::InsufficientMaterial: reasonStr = "Insufficient material"; break;
        case net::GameOverReason::Resignation:        reasonStr = "Resignation"; break;
        case net::GameOverReason::Disconnection:      reasonStr = "Disconnection"; break;
        case net::GameOverReason::Abort:              reasonStr = "Aborted"; break;
        case net::GameOverReason::AgreedDraw:         reasonStr = "Draw by agreement"; break;
        case net::GameOverReason::Timeout:            reasonStr = "Won on time"; break;
        default:                                      reasonStr = "Unknown"; break;
    }

    std::string resultStr;
    switch (result) {
        case net::GameResult::WhiteWins:   resultStr = "White wins"; break;
        case net::GameResult::BlackWins:   resultStr = "Black wins"; break;
        case net::GameResult::Draw:        resultStr = "Draw"; break;
        case net::GameResult::Resignation: resultStr = "Resignation"; break;
        case net::GameResult::Abort:       resultStr = "Game aborted"; break;
        default:                           resultStr = "Unknown"; break;
    }

    switch (result) {
        case net::GameResult::Draw:
            resultText_ = resultStr;
            reasonText_ = reasonStr;
            break;
        case net::GameResult::Abort:
            resultText_ = resultStr;
            break;
        default:
            resultText_ = reasonStr + " — " + resultStr;
            break;
    }

    resultLabel_.setText(resultText_);
    resultLabel_.setFontSize(ui::type::Result);
    resultLabel_.setColor(ui::pal::white);

    reasonLabel_.setText("(" + reasonText_ + ")");
    reasonLabel_.setFontSize(ui::type::Heading);
    reasonLabel_.setColor(ui::pal::textMuted);

    statusLabel_.setFontSize(16); // offer banner row size
    statusLabel_.setColor(ui::pal::warnSoft);

    hint_.setText("Tab to switch, Enter to select");
    hint_.setFontSize(ui::type::Body);
    hint_.setColor(ui::pal::textGhost);

    rematchBtn_.setLabel("Rematch");
    rematchBtn_.setStyle(ui::buttonSuccess());
    rematchBtn_.setOnClick([this] { activateRematch(); });

    // Arena game (12.3): the rematch row is the lobby return instead —
    // the server refuses rematch offers on tournament games; the next
    // arena pairing is the "again" channel and arrives in the lobby (or
    // right here, pulling this seat back into GameScreen automatically).
    if (arena_) {
        rematchBtn_.setLabel("Return to arena");
        rematchBtn_.setOnClick([this] { activateArenaLobby(); });
    }

    offerAcceptBtn_.setLabel("Accept");
    offerAcceptBtn_.setStyle(ui::buttonSuccess());
    offerAcceptBtn_.setOnClick([this] { acceptRematch(); });

    offerDeclineBtn_.setLabel("Decline");
    offerDeclineBtn_.setStyle(ui::buttonDangerMuted());
    offerDeclineBtn_.setOnClick([this] { declineRematch(); });

    analyzeBtn_.setLabel("Analyze");
    analyzeBtn_.setStyle(ui::buttonInfo());
    analyzeBtn_.setOnClick([this] { activateAnalyze(); });

    archiveBtn_.setLabel("Match archive");
    archiveBtn_.setStyle(ui::buttonBlue());
    archiveBtn_.setOnClick([this] { activateArchive(); });
    // Browsing needs the live socket of the just-finished game.
    archiveBtn_.setEnabled(
        app_.connection().state() == ConnectionState::Connected);

    menuBtn_.setLabel("Menu");
    menuBtn_.setIcon(ui::icons::IconKind::ArrowBack);
    menuBtn_.setStyle(ui::buttonPlain());
    menuBtn_.setOnClick([this] { activateMenu(); });

    layoutWidgets();

    rematchBtn_.setFocused(true);
}

void GameOverScreen::layoutWidgets()
{
    const sf::FloatRect area(
        sf::Vector2f(0.f, 0.f),
        sf::Vector2f(App::WindowWidth, App::WindowHeight));
    (void)area;
    const sf::Vector2f btnSize(BtnW, BtnH);

    auto rb = resultLabel_.bounds(app_.font());
    resultLabel_.setPosition({
        (App::WindowWidth - rb.size.x) / 2.f - rb.position.x, ResultY
    });

    auto rr = reasonLabel_.bounds(app_.font());
    reasonLabel_.setPosition({
        (App::WindowWidth - rr.size.x) / 2.f - rr.position.x, ReasonY
    });

    auto sb = statusLabel_.bounds(app_.font());
    statusLabel_.setPosition({
        (App::WindowWidth - sb.size.x) / 2.f - sb.position.x, StatusY
    });

    // The two answer buttons sit side by side on the offer row.
    const float offerGap = 20.f;
    const float offerTotal = OfferBtnW * 2.f + offerGap;
    offerAcceptBtn_.setRect(sf::FloatRect(
        sf::Vector2f((App::WindowWidth - offerTotal) / 2.f, OfferY),
        sf::Vector2f(OfferBtnW, BtnH)));
    offerDeclineBtn_.setRect(sf::FloatRect(
        sf::Vector2f((App::WindowWidth - offerTotal) / 2.f + OfferBtnW
                     + offerGap, OfferY),
        sf::Vector2f(OfferBtnW, BtnH)));

    rematchBtn_.setRect(layout::centerIn(
        sf::FloatRect(sf::Vector2f(0.f, RematchY),
                      sf::Vector2f(App::WindowWidth, BtnH)), btnSize));

    archiveBtn_.setRect(layout::centerIn(
        sf::FloatRect(sf::Vector2f(0.f, ArchiveBtnY),
                      sf::Vector2f(App::WindowWidth, BtnH)), btnSize));

    if (!moves_.empty()) {
        analyzeBtn_.setRect(layout::centerIn(
            sf::FloatRect(sf::Vector2f(0.f, AnalyzeBtnY),
                          sf::Vector2f(App::WindowWidth, BtnH)), btnSize));
    } else {
        analyzeBtn_.setEnabled(false);
    }

    menuBtn_.setRect(layout::centerIn(
        sf::FloatRect(sf::Vector2f(0.f, MenuBtnY),
                      sf::Vector2f(App::WindowWidth, BtnH)), btnSize));

    const float hintY = MenuBtnY + BtnH + 14.f;
    auto hb = hint_.bounds(app_.font());
    hint_.setPosition({
        (App::WindowWidth - hb.size.x) / 2.f - hb.position.x, hintY
    });
}

void GameOverScreen::setStatusText(const std::string& text)
{
    statusLabel_.setText(text);
    if (!text.empty()) {
        auto sb = statusLabel_.bounds(app_.font());
        statusLabel_.setPosition({
            (App::WindowWidth - sb.size.x) / 2.f - sb.position.x, StatusY
        });
    }
}

void GameOverScreen::enterIdleRematch(const std::string& note)
{
    rematchPhase_ = RematchPhase::Idle;
    rematchBtn_.setEnabled(true);
    offerAcceptBtn_.setEnabled(false);
    offerDeclineBtn_.setEnabled(false);
    setStatusText(note);
}

void GameOverScreen::activateRematch()
{
    if (rematchPhase_ != RematchPhase::Idle) return;
    if (app_.connection().state() != ConnectionState::Connected) {
        setStatusText("Not connected — no rematch");
        return;
    }
    if (app_.connection().send(chess::net::RematchRequestMsg{})) {
        rematchPhase_ = RematchPhase::Sent;
        rematchBtn_.setEnabled(false);
        setStatusText("Rematch offered — waiting for the opponent...");
    }
}

void GameOverScreen::activateArenaLobby()
{
    if (!arena_) return;
    // The join name rides along for guests (the server ignores it on
    // authenticated connections); members re-affirm idempotently.
    std::string joinName;
    if (app_.profile().has_value() && !app_.profile()->isGuest)
        joinName = app_.profile()->name;
    else
        joinName = app_.lastName();
    app_.switchScreen(std::make_unique<TournamentLobbyScreen>(
        app_, arenaTimeControl_, arenaVariant_, std::move(joinName)));
}

void GameOverScreen::activateMenu()
{
    app_.connection().disconnect();
    app_.switchScreen(std::make_unique<MenuScreen>(app_));
}

void GameOverScreen::acceptRematch()
{
    if (rematchPhase_ != RematchPhase::Offered) return;
    // The fresh WelcomeMsg of the server's rematched game does the switch;
    // both answer paths are disabled immediately to keep a double click
    // from sending two accepts.
    if (app_.connection().send(chess::net::RematchAcceptMsg{})) {
        rematchPhase_ = RematchPhase::Sent;
        offerAcceptBtn_.setEnabled(false);
        offerDeclineBtn_.setEnabled(false);
        setStatusText("Starting the rematch...");
    }
}

void GameOverScreen::declineRematch()
{
    if (rematchPhase_ != RematchPhase::Offered) return;
    if (app_.connection().send(chess::net::RematchDeclineMsg{})) {
        // No echo comes back for a plain decline (it is not the request).
        enterIdleRematch("Rematch declined");
    }
}

void GameOverScreen::activateAnalyze()
{
    if (moves_.empty()) return;
    app_.connection().disconnect();
    app_.pushScreen(std::make_unique<AnalysisScreen>(
        app_, initialBoard_, moves_, sanMoves_, resultText_));
}

void GameOverScreen::activateArchive()
{
    if (app_.connection().state() != ConnectionState::Connected) return;
    const bool mine = app_.profile().has_value()
        && !app_.profile()->isGuest;
    // Pushed so Esc from the archive drops back to this game-over screen.
    app_.pushScreen(std::make_unique<HistoryScreen>(app_, mine));
}

void GameOverScreen::cycleFocus()
{
    Button* order[] = {&rematchBtn_, &offerAcceptBtn_, &offerDeclineBtn_,
                       &archiveBtn_, &analyzeBtn_, &menuBtn_};
    for (int step = 1; step <= 6; ++step) {
        const int next = (focus_ + step) % 6;
        const bool visible = order[next] != &offerAcceptBtn_
            && order[next] != &offerDeclineBtn_;
        if (visible && order[next]->isEnabled()) {
            focus_ = next;
            for (int i = 0; i < 6; ++i)
                order[i]->setFocused(false);
            order[focus_]->setFocused(true);
            return;
        }
    }
}

void GameOverScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            activateMenu();
            return;
        }
        if (kp->code == sf::Keyboard::Key::Tab) {
            cycleFocus();
            return;
        }
    }

    sf::Vector2f local(0.f, 0.f);
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
        local = app_.toLocal(mm->position);
    else if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
        local = app_.toLocal(mb->position);
    else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
        local = app_.toLocal(rb->position);

    if (rematchBtn_.handleEvent(event, local)) return;
    if (rematchPhase_ == RematchPhase::Offered) {
        if (offerAcceptBtn_.handleEvent(event, local)) return;
        if (offerDeclineBtn_.handleEvent(event, local)) return;
    }
    if (archiveBtn_.handleEvent(event, local)) return;
    if (moves_.empty()) return;
    if (analyzeBtn_.handleEvent(event, local)) return;
    menuBtn_.handleEvent(event, local);
}

void GameOverScreen::update(float /*dtSec*/)
{
    // The result screen now owns the live socket (11.7): it must pump the
    // connection so pings keep flowing and the rematch conversation
    // (offer, answer, the rematched game's WelcomeMsg) arrives here.
    app_.connection().poll();

    while (app_.connection().hasMessages()) {
        auto msg = app_.connection().nextMessage();

        if (auto* welcome = std::get_if<chess::net::WelcomeMsg>(&msg)) {
            // The rematched game: same construction as the connect flow's
            // pairing path — a fresh screen, colors already swapped by the
            // server, the new seat's resume token riding along.
            std::optional<std::pair<double, double>> opponentRating;
            if (welcome->rated)
                opponentRating = std::make_pair(welcome->rating,
                                                welcome->deviation);
            setStatusText("");
            app_.switchScreen(std::make_unique<GameScreen>(
                app_, welcome->color, welcome->opponent, opponentRating,
                welcome->rated, welcome->timeControl, welcome->resumeToken,
                welcome->startFen, welcome->arena));
            return;
        }
        if (std::holds_alternative<chess::net::ServerRematchRequestMsg>(msg)) {
            // The opponent asks for a rematch: the answer row replaces the
            // question mark, focus jumps to Accept for a fast yes.
            rematchPhase_ = RematchPhase::Offered;
            rematchBtn_.setEnabled(false);
            offerAcceptBtn_.setEnabled(true);
            offerDeclineBtn_.setEnabled(true);
            setStatusText("Opponent offers a rematch");
            offerAcceptBtn_.setFocused(true);
            rematchBtn_.setFocused(false);
            offerDeclineBtn_.setFocused(false);
            focus_ = 1;
            continue;
        }
        if (auto* decline = std::get_if<chess::net::ServerRematchDeclineMsg>(
                &msg)) {
            enterIdleRematch(decline->withdrawn
                                 ? "Rematch offer withdrawn"
                                 : "Rematch declined");
            continue;
        }
        if (auto* profileMsg = std::get_if<chess::net::ProfileMsg>(&msg)) {
            app_.setProfile(*profileMsg);
            continue;
        }
        if (auto* err = std::get_if<chess::net::ErrorMsg>(&msg)) {
            // A failed request/error clears whatever rematch state this
            // screen was in (the server has none pending for us).
            enterIdleRematch(err->message);
            continue;
        }
    }
}

void GameOverScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    resultLabel_.draw(window, font);
    if (!reasonText_.empty())
        reasonLabel_.draw(window, font);
    if (!statusLabel_.text().empty())
        statusLabel_.draw(window, font);

    if (rematchPhase_ == RematchPhase::Offered) {
        offerAcceptBtn_.draw(window, font);
        offerDeclineBtn_.draw(window, font);
    } else {
        rematchBtn_.draw(window, font);
    }
    archiveBtn_.draw(window, font);
    if (!moves_.empty())
        analyzeBtn_.draw(window, font);
    menuBtn_.draw(window, font);

    hint_.draw(window, font);
}

} // namespace chess::client
