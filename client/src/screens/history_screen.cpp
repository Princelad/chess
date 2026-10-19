#include "history_screen.h"

#include "analysis_screen.h"
#include "game_screen.h"

#include <SFML/Window/Clipboard.hpp>

#include <chess/fen.h>

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace chess::client {

namespace {

constexpr float PanelX = 170.f;
constexpr float PanelY = 40.f;
constexpr float PanelW = 620.f;
constexpr float PanelH = 560.f;
constexpr float RowH = 30.f;

std::string formatDate(std::int64_t playedAt)
{
    const std::time_t t = static_cast<std::time_t>(playedAt);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << tm.tm_year + 1900 << '.'
        << std::setw(2) << tm.tm_mon + 1 << '.' << std::setw(2) << tm.tm_mday;
    return out.str();
}

} // namespace

HistoryScreen::HistoryScreen(App& app, bool mineOnlyDefault)
    : app_(app)
    , mineOnly_(mineOnlyDefault)
    , bg_(sf::FloatRect(sf::Vector2f(PanelX, PanelY),
                        sf::Vector2f(PanelW, PanelH)),
          sf::Color(28, 28, 30), sf::Color(90, 90, 90), 1.f)
    , detailPanel_(sf::FloatRect(sf::Vector2f(PanelX + 20.f, 170.f),
                                 sf::Vector2f(PanelW - 40.f, 330.f)),
                   sf::Color(24, 24, 26), sf::Color(70, 70, 70), 1.f)
{
    title_.setText("Match archive");
    title_.setFontSize(30);
    title_.setColor(sf::Color(240, 240, 240));

    status_.setFontSize(15);
    status_.setColor(sf::Color(170, 170, 170));

    detailTitle_.setFontSize(18);
    detailTitle_.setColor(sf::Color(235, 235, 235));
    detailMeta_.setFontSize(14);
    detailMeta_.setColor(sf::Color(160, 160, 160));
    detailSans_.setFontSize(13);
    detailSans_.setColor(sf::Color(140, 140, 140));
    detailSans_.setText("");

    mineToggleable_ = app_.profile().has_value() && !app_.profile()->isGuest;
    mineCheck_.setLabel("Only my games");
    mineCheck_.setFontSize(15);
    mineCheck_.setChecked(mineOnly_);
    mineCheck_.setOnToggle([this](bool checked) {
        if (mineToggleable_ && checked != mineOnly_) {
            mineOnly_ = checked;
            request();
        }
    });

    backBtn_.setLabel("OK");
    backBtn_.setOnClick([this] { app_.goBack(); });

    watchBtn_.setLabel("Watch");
    watchBtn_.setOnClick([this] { watchGame(); });

    copyBtn_.setLabel("Copy PGN");
    copyBtn_.setOnClick([this] { copyPgn(); });

    layoutWidgets();
    request();
}

void HistoryScreen::request()
{
    phase_ = Phase::Busy;
    entries_.clear();
    record_.reset();
    status_.setText("");
    app_.connection().send(
        chess::net::ArchiveRequestMsg{50 + PageRows, mineOnly_});
}

void HistoryScreen::openDetail(std::int64_t id)
{
    status_.setText("");
    phase_ = Phase::Busy;
    app_.connection().send(chess::net::ArchiveGetMsg{id});
}

void HistoryScreen::backToList()
{
    record_.reset();
    phase_ = Phase::List;
    layoutWidgets();
}

void HistoryScreen::copyPgn()
{
    if (!record_.has_value())
        return;
    sf::Clipboard::setString(gameToPgn(*record_));
    status_.setText("PGN copied to the clipboard.");
}

HistoryScreen::Replay HistoryScreen::buildReplay(
    const chess::net::ArchiveGameMsg& record)
{
    Replay replay;
    if (record.startFen.empty()) {
        replay.initialBoard = Board::fromStartPos();
    } else {
        auto board = Board::fromFen(record.startFen);
        replay.initialBoard = board.has_value() ? *board : Board::fromStartPos();
    }

    auto parts = archivePgnParts(record);
    Board running = replay.initialBoard;
    for (const auto& san : parts.sans) {
        replay.sans.push_back(san);
        auto move = chess::san::fromSan(running, san);
        if (!move.has_value())
            break; // corrupt record: keep what we parsed
        replay.moves.push_back(*move);
        running.makeMove(*move);
    }

    std::ostringstream out;
    out << archiveResultText(record.result)
        << " (" << archiveResultToken(record.result) << ")";
    replay.resultText = out.str();
    return replay;
}

void HistoryScreen::watchGame()
{
    if (!record_.has_value())
        return;
    const Replay replay = buildReplay(*record_);
    // Pushed (not switched) so "Esc" returns into the archive detail.
    app_.pushScreen(std::make_unique<AnalysisScreen>(
        app_, replay.initialBoard, replay.moves, replay.sans,
        replay.resultText));
}

void HistoryScreen::layoutWidgets()
{
    title_.setPosition({PanelX + 20.f, PanelY + 16.f});

    mineCheck_.setRect(sf::FloatRect(
        sf::Vector2f(PanelX + PanelW - 200.f, PanelY + 24.f),
        sf::Vector2f(180.f, 24.f)));

    status_.setPosition({PanelX + 20.f, PanelY + PanelH - 34.f});
    backBtn_.setRect(sf::FloatRect(
        sf::Vector2f(PanelX + PanelW - 90.f, PanelY + PanelH - 42.f),
        sf::Vector2f(70.f, 30.f)));

    rowButtons_.clear();
    if (phase_ == Phase::List) {
        const std::size_t visible = std::min<std::size_t>(
            PageRows, entries_.size() - topRow_);
        rowButtons_.reserve(visible);
        float y = PanelY + 64.f;
        for (std::size_t i = topRow_; i < topRow_ + visible
                && i < entries_.size(); ++i) {
            const auto& entry = entries_[i];
            std::ostringstream label;
            label << entry.whiteName << "  vs  " << entry.blackName
                  << "    " << archiveResultToken(entry.result);
            if (!entry.rated)
                label << "   casual";
            rowButtons_.emplace_back(
                sf::FloatRect(sf::Vector2f(PanelX + 20.f, y),
                              sf::Vector2f(PanelW - 40.f, RowH)),
                label.str());
            Button& row = rowButtons_.back();
            const std::int64_t id = entry.id;
            row.setOnClick([this, id] { openDetail(id); });
            row.setColors(sf::Color(42, 42, 44), sf::Color(58, 58, 62),
                          sf::Color(52, 52, 56), sf::Color(42, 42, 44),
                          sf::Color(120, 120, 120), sf::Color(220, 220, 220),
                          sf::Color(90, 90, 90), sf::Color(200, 200, 120),
                          sf::Color(220, 220, 140));
            y += RowH + 2.f;
        }
    }

    watchBtn_.setRect(sf::FloatRect(
        sf::Vector2f(PanelX + 30.f, PanelY + PanelH - 90.f),
        sf::Vector2f(130.f, 34.f)));
    copyBtn_.setRect(sf::FloatRect(
        sf::Vector2f(PanelX + 180.f, PanelY + PanelH - 90.f),
        sf::Vector2f(130.f, 34.f)));
    detailTitle_.setPosition({PanelX + 30.f, 190.f});
    detailMeta_.setPosition({PanelX + 30.f, 230.f});
    detailSans_.setPosition({PanelX + 30.f, 260.f});
    detailPanel_.setRect(sf::FloatRect(
        sf::Vector2f(PanelX + 20.f, 170.f),
        sf::Vector2f(PanelW - 40.f, 330.f)));
}

void HistoryScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            if (phase_ == Phase::Detail)
                backToList();
            else
                app_.goBack();
            return;
        }
        if (phase_ == Phase::List) {
            if (kp->code == sf::Keyboard::Key::Up && topRow_ > 0)
                --topRow_;
            if (kp->code == sf::Keyboard::Key::Down
                && entries_.size() > PageRows
                && topRow_ + PageRows < entries_.size())
                ++topRow_;
            if (kp->code == sf::Keyboard::Key::Up
                || kp->code == sf::Keyboard::Key::Down) {
                layoutWidgets();
                return;
            }
        }
    }

    if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>();
        wheel && phase_ == Phase::List) {
        const float delta = wheel->delta;
        int steps = static_cast<int>(delta > 0.f ? delta : -delta);
        while (steps-- > 0) {
            if (delta > 0.f && topRow_ > 0)
                --topRow_;
            else if (delta < 0.f && entries_.size() > PageRows
                     && topRow_ + PageRows < entries_.size())
                ++topRow_;
        }
        layoutWidgets();
        return;
    }

    sf::Vector2f local(0.f, 0.f);
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
        local = app_.toLocal(mb->position);
    else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
        local = app_.toLocal(rb->position);
    else if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
        local = app_.toLocal(mm->position);

    if (phase_ == Phase::List) {
        for (auto& row : rowButtons_)
            if (row.handleEvent(event, local)) return;
        if (mineToggleable_ && mineCheck_.handleEvent(event, local)) {
            layoutWidgets();
            return;
        }
        backBtn_.handleEvent(event, local);
    } else if (phase_ == Phase::Detail) {
        watchBtn_.handleEvent(event, local);
        copyBtn_.handleEvent(event, local);
        backBtn_.handleEvent(event, local);
    } else {
        backBtn_.handleEvent(event, local);
    }
}

void HistoryScreen::update(float)
{
    app_.connection().poll();

    while (app_.connection().hasMessages()) {
        auto msg = app_.connection().nextMessage();
        if (auto* list = std::get_if<chess::net::ArchiveListMsg>(&msg)) {
            entries_ = std::move(list->games);
            topRow_ = 0;
            phase_ = Phase::List;
            if (mineToggleable_ && entries_.empty() && mineOnly_) {
                status_.setText("No games of yours yet. Uncheck below to see "
                    "the server archive.");
                mineCheck_.setChecked(false);
                mineOnly_ = false;
                request(); // fall back to showing something
            }
            layoutWidgets();
        }
        else if (auto* record = std::get_if<chess::net::ArchiveGameMsg>(&msg)) {
            record_ = std::move(*record);
            phase_ = Phase::Detail;
            const Replay replay = buildReplay(*record_);
            detailTitle_.setText("Game #" + std::to_string(record_->id)
                + ": " + record_->whiteName + " vs " + record_->blackName);
            std::ostringstream meta;
            meta << archiveResultText(record_->result)
                 << "  " << formatDate(record_->playedAt);
            if (record_->rated)
                meta << "  rated";
            else
                meta << "  casual";
            meta << "  " << replay.sans.size() << " halfmoves";
            detailMeta_.setText(meta.str());
            detailSans_.setText(record_->sanMoves.empty()
                ? "(no moves recorded)" : record_->sanMoves);
            layoutWidgets();
        }
        else if (auto* err = std::get_if<chess::net::ErrorMsg>(&msg)) {
            status_.setText(err->message);
            if (phase_ == Phase::Busy)
                phase_ = Phase::List; // stay on the list with a status note
            layoutWidgets();
        }
        else if (auto* welcome = std::get_if<chess::net::WelcomeMsg>(&msg)) {
            // An opponent matched us while browsing: fall back into play.
            std::optional<std::pair<double, double>> opponentRating;
            if (welcome->rated)
                opponentRating =
                    std::make_pair(welcome->rating, welcome->deviation);
            app_.switchScreen(std::make_unique<GameScreen>(
                app_, welcome->color, welcome->opponent, opponentRating,
                welcome->rated));
            return;
        }
    }

    // The server dropped us while browsing.
    if (app_.connection().state() != ConnectionState::Connected
        && app_.connection().state() != ConnectionState::Connecting) {
        status_.setText("Not connected. Leave and connect again.");
        if (phase_ == Phase::Busy)
            phase_ = Phase::List;
        layoutWidgets();
    }
}

void HistoryScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    bg_.draw(window);
    title_.draw(window, font);
    status_.draw(window, font);

    if (phase_ == Phase::List || phase_ == Phase::Busy) {
        for (auto& row : rowButtons_)
            row.draw(window, font);
        if (mineToggleable_)
            mineCheck_.draw(window, font);
        backBtn_.draw(window, font);

        if (phase_ == Phase::Busy) {
            status_.setPosition({PanelX + 20.f, PanelY + 100.f});
            status_.draw(window, font);
            status_.setPosition({PanelX + 20.f, PanelY + PanelH - 34.f});
        }
        return;
    }

    // Detail view
    detailPanel_.draw(window);
    detailTitle_.draw(window, font);
    detailMeta_.draw(window, font);
    detailSans_.draw(window, font);
    watchBtn_.draw(window, font);
    copyBtn_.draw(window, font);
    backBtn_.draw(window, font);
}

} // namespace chess::client
