#include "report_screen.h"

#include "connection_pump.h"
#include "eco.h"
#include "screens/ui_helpers.h"
#include "ui/theme.h"
#include "widgets/panel.h"

#include <chess/board.h>
#include <chess/movegen.h>
#include <chess/types.h>

#include <algorithm>
#include <cstdio>

namespace chess::client {

namespace {
// The black accuracy bar rides the window (the white bar uses the shared
// textPrimary); a screened-unique accent tone with a naming comment.
constexpr sf::Color kBlackAccuracyBar{58, 58, 62};

std::string formatPercent(float v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f%%", static_cast<double>(v));
    return buf;
}

// Legend: "Blunder x1, Mistake x2", zero counts skipped (decl order).
// ASCII-only separators: the bundled Inter renders middle dots as tofu.
std::string legendLine(const quality::SideQuality& side)
{
    std::string line;
    auto appendClass = [&](quality::MoveClass klass) {
        const int count =
            side.counts[static_cast<std::size_t>(klass)];
        if (count == 0) return;
        if (!line.empty()) line += ", ";
        line += std::string(quality::moveClassName(klass)) + " x"
            + std::to_string(count);
    };
    appendClass(quality::MoveClass::Book);
    appendClass(quality::MoveClass::Brilliant);
    appendClass(quality::MoveClass::Best);
    appendClass(quality::MoveClass::Inaccuracy);
    appendClass(quality::MoveClass::Mistake);
    appendClass(quality::MoveClass::Blunder);
    return line;
}
} // namespace

ReportScreen::ReportScreen(App& app,
                           Board initialBoard,
                           std::vector<chess::Move> moves,
                           std::vector<std::string> sanMoves,
                           std::string result,
                           std::int64_t archiveId)
    : app_(app)
    , resultText_(std::move(result))
    , archiveId_(archiveId)
    , boardView_(static_cast<float>(App::WindowWidth),
                 static_cast<float>(App::WindowHeight),
                 Color::White,
                 app.boardTheme(),
                 app.showCoordinates(),
                 app.coordinateStyle())
    , panelLayout_(layout::composeBoardLayout(
          static_cast<float>(App::WindowWidth),
          static_cast<float>(App::WindowHeight), boardView_,
          layout::BoardLayoutOptions{false, false, false, false, true}))
{
    key_ = evalKeyFor(archiveId_, initialBoard.toFen(), sanMoves);

    navigator_.setGame(std::move(initialBoard), std::move(moves),
                       std::move(sanMoves));

    reportTitleLabel_.setText("Game report");
    reportTitleLabel_.setFontSize(ui::type::Result);
    reportTitleLabel_.setColor(ui::pal::textHeader);

    resultLabel_.setText(resultText_);
    resultLabel_.setFontSize(ui::type::Body);
    resultLabel_.setColor(ui::pal::textSecondary);

    // 11.5 lookup: a listed opening labels the header, once per entry.
    if (auto opening = ecoLookup(navigator_.sans())) {
        openingLabel_.setText("Opening: " + opening->name + " ("
            + opening->code + ")");
    }
    openingLabel_.setFontSize(ui::type::Body);
    openingLabel_.setColor(ui::pal::textSecondary);

    plyLabel_.setFontSize(ui::type::Body);
    plyLabel_.setColor(ui::pal::textDimmed);

    statusLabel_.setFontSize(ui::type::Status);
    statusLabel_.setColor(ui::pal::textMuted);
    hintLabel_.setText("Esc: back");
    hintLabel_.setFontSize(ui::type::Small);
    hintLabel_.setColor(ui::pal::textFaded);

    layoutPanel();
    refreshWorstList();

    // Engine contract: user-visible post-game demand (the report was
    // opened), so the lazy spawn happens here once; engine-less machines
    // see the notice and the panel stays inert (16.1 discipline).
    EvalHarness& harness = app_.evalHarness();
    if (harness.ensureEngine() && harness.hasEngine()) {
        auto nodes = evalNodes(navigator_.initialBoard().toFen(),
                               navigator_.sans());
        if (nodes) {
            // One worker job at a time: start() on a busy harness is a
            // silent no-op (its own contract says "stop() to switch
            // keys"), so a report opened while another job still ran
            // would sit inert forever. stop() parks the old job Idle —
            // its cached nodes resume on the next start.
            harness.stop();
            harness.start(key_, std::move(*nodes), kEvalMovetimeMs);
            statusLabel_.setText("");
        } else {
            statusLabel_.setText("This game's moves cannot be analyzed");
        }
    } else {
        statusLabel_.setText("No chess engine found - report unavailable");
    }
}

ReportScreen::~ReportScreen()
{
    // Leaving the report parks the job (cached nodes persist, so a
    // re-entry resumes); an untouched harness stop() is a quiet no-op.
    app_.evalHarness().stop();
}

void ReportScreen::layoutPanel()
{
    reportTitleLabel_.setPosition({panelLayout_.panelX,
        panelLayout_.reportTitleY});
    resultLabel_.setPosition({panelLayout_.panelX, panelLayout_.reportResultY});
    openingLabel_.setPosition({panelLayout_.panelX,
        panelLayout_.reportOpeningY});
    statusLabel_.setPosition({panelLayout_.panelX, panelLayout_.legendY});
    hintLabel_.setPosition({panelLayout_.panelX, panelLayout_.hintY});

    navigator_.setLayout(panelLayout_.moveList, panelLayout_.navRowA);
}

void ReportScreen::refreshWorstList()
{
    worstLines_.clear();
    worstRects_.clear();
    worstCount_ = 0;
    if (!quality_ || quality_->moves.empty()) return;

    // {Blunder, Mistake, Inaccuracy} descending by win% given up; bounded
    // by the worstStep rows that fit under the header pile.
    struct Entry
    {
        int ply = 0;
        float givenUp = 0.f;
    };
    std::vector<Entry> entries;
    for (std::size_t i = 0; i < quality_->moves.size(); ++i) {
        const auto klass = quality_->moves[i].klass;
        if (klass == quality::MoveClass::Blunder
            || klass == quality::MoveClass::Mistake
            || klass == quality::MoveClass::Inaccuracy) {
            entries.push_back({static_cast<int>(i),
                               quality_->moves[i].givenUp});
        }
    }
    std::stable_sort(entries.begin(), entries.end(),
        [](const Entry& a, const Entry& b) {
            return a.givenUp > b.givenUp;
        });

    const int maxRows = static_cast<int>(
        (panelLayout_.hintY - panelLayout_.worstTop - 10.f)
        / panelLayout_.worstStep);
    for (int row = 0; row < std::min<int>(entries.size(), maxRows); ++row) {
        const int ply = entries[row].ply;
        const auto& quality = quality_->moves[ply];
        const std::string san = navigator_.sans()[ply];
        // The move number as played: ply/(side) from the SAN history:
        // ply counts half-moves, white starts.
        const int moveNo = ply / 2 + 1;
        const Color mover = ply % 2 == 0 ? Color::White : Color::Black;
        std::string line = mover == Color::White
            ? std::to_string(moveNo) + ". "
            : std::to_string(moveNo) + "... ";
        line += san + "  " + formatPercent(-quality.givenUp);

        const float y = panelLayout_.worstTop
            + static_cast<float>(row) * panelLayout_.worstStep;
        worstRects_.push_back({sf::FloatRect(
            sf::Vector2f(panelLayout_.panelX, y),
            sf::Vector2f(panelLayout_.panelWidth, panelLayout_.worstStep)),
            ply + 1}); // jump lands on the position AFTER the move
        worstLines_.push_back(std::move(line));
    }
    worstCount_ = static_cast<int>(worstLines_.size());
}

void ReportScreen::buildQuality()
{
    if (computed_) return;
    EvalHarness& harness = app_.evalHarness();
    const auto progress = harness.progress(key_);
    if (progress.state != EvalState::Done) return;

    auto report = harness.report(key_);
    if (!report || report->failed) {
        statusLabel_.setText("Evaluation failed - leave and reopen to retry");
        return;
    }

    statusLabel_.setText("");

    const auto book = quality::bookPlies(navigator_.sans());
    const auto sacrifices = quality::sacrificePlies(
        navigator_.initialBoard(), navigator_.moves());
    auto stream = quality::streamFromReport(*report, navigator_.sans(),
                                            sacrifices);
    // Caller-side flags stamp onto the stream: 16.2 keeps the classifier
    // free of the ECO/board dependencies.
    if (stream.size() == book.size()) {
        for (std::size_t i = 0; i < stream.size(); ++i)
            stream[i].isBook = book[i];
    }
    if (stream.empty()) {
        statusLabel_.setText("Evaluation failed - leave and reopen to retry");
        return;
    }
    quality_ = quality::evaluateGame(stream);
    computed_ = true;
    // 16.4 chip layer: the same classification colors the navigator's SAN
    // cells (Good stays unmarked).
    navigator_.setClassLayer(quality::classLayerFor(
        *report, navigator_.initialBoard(), navigator_.moves(),
        navigator_.sans(), book));
    refreshWorstList();
}

void ReportScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            app_.goBack();
            return;
        }
        if (navigator_.handleEvent(event, {0.f, 0.f})) return;
        return;
    }

    sf::Vector2f local(0.f, 0.f);
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
        local = app_.toLocal(mm->position);
    else if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
        local = app_.toLocal(mb->position);
    else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
        local = app_.toLocal(rb->position);

    if (navigator_.handleEvent(event, local)) return;

    // Worst-move rows jump the navigator (click in the row rect).
    for (const auto& [rect, ply] : worstRects_) {
        if (rect.contains(local)) {
            navigator_.goToPly(ply);
            return;
        }
    }
}

void ReportScreen::update(float /*dtSec*/)
{
    // Audit 9.2: pump before anything else — a pairing (the arena pass
    // re-pairs idle GameOver seats ~1 s after a game ends) hands the
    // screen over here, and the rematch conversation stays queued for
    // GameOverScreen (the dtor parks the eval job on the switch).
    if (pumpPairing(app_))
        return;

    buildQuality();

    // Status line rides every frame until the quality is in (progress
    // counter, then the legend replaces it in draw()).
    if (computed_) return;
    EvalHarness& harness = app_.evalHarness();
    const auto progress = harness.progress(key_);
    if (progress.state == EvalState::Running) {
        statusLabel_.setText("Analyzing " + std::to_string(progress.done)
            + " / " + std::to_string(progress.total));
    }
}

void ReportScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();
    const Board& shown = navigator_.board();

    boardView_.drawSquares(window);

    HighlightState hl;
    hl.lastMoveFrom = navigator_.lastMoveFrom();
    hl.lastMoveTo = navigator_.lastMoveTo();
    const Color stm = shown.sideToMove();
    hl.checkSquare = chess::inCheck(shown, stm)
        ? std::optional<std::pair<int, int>>(findKingSquare(shown, stm))
        : std::nullopt;
    boardView_.drawHighlights(window, hl, shown);
    boardView_.drawLabels(window, font);
    boardView_.drawPieces(window, font, shown, app_);

    navigator_.draw(window, font, app_);

    const float barX = panelLayout_.panelX + panelLayout_.accuracyLabelW;
    const float barW = panelLayout_.panelWidth
        - panelLayout_.accuracyLabelW;
    constexpr float kValueGutter = 44.f; // % text right of the track end

    auto drawAccuracyRow = [&](float y, const char* name, float accuracy,
                               sf::Color fill) {
        sf::Text nameText(font, name, ui::type::Small);
        nameText.setFillColor(ui::pal::textSecondary);
        nameText.setPosition({panelLayout_.panelX, y});
        window.draw(nameText);

        sf::RectangleShape track(
            {barW - kValueGutter, panelLayout_.accuracyBarH});
        track.setPosition({barX, y});
        track.setFillColor(ui::pal::bgCard);
        window.draw(track);

        sf::RectangleShape fillShape(
            {(barW - kValueGutter) * (accuracy / 100.f),
             panelLayout_.accuracyBarH});
        fillShape.setPosition({barX, y});
        fillShape.setFillColor(fill);
        window.draw(fillShape);

        sf::Text valueText(font, formatPercent(accuracy), ui::type::Small);
        valueText.setFillColor(ui::pal::textCell);
        valueText.setPosition({barX + barW - kValueGutter, y});
        window.draw(valueText);
    };

    if (quality_) {
        drawAccuracyRow(panelLayout_.accuracyWhiteY, "White",
                        quality_->white.accuracy, ui::pal::textPrimary);
        drawAccuracyRow(panelLayout_.accuracyBlackY, "Black",
                        quality_->black.accuracy, kBlackAccuracyBar);

        const std::string legend = legendLine(quality_->white) + "  |  "
            + legendLine(quality_->black);
        if (!legend.empty()) {
            sf::Text legendText(font, legend, ui::type::Caption);
            legendText.setFillColor(ui::pal::textMuted);
            legendText.setPosition(
                {panelLayout_.panelX, panelLayout_.legendY});
            window.draw(legendText);
        }

        sf::Text worstHeader(font, "Worst moves", ui::type::Small);
        worstHeader.setFillColor(ui::pal::textSecondary);
        worstHeader.setPosition({panelLayout_.panelX,
                                 panelLayout_.worstHeaderY});
        window.draw(worstHeader);

        for (int i = 0; i < worstCount_; ++i) {
            sf::Text line(font, worstLines_[i], ui::type::Small);
            line.setFillColor(ui::pal::textCell);
            line.setPosition({panelLayout_.panelX, panelLayout_.worstTop
                + static_cast<float>(i) * panelLayout_.worstStep});
            window.draw(line);
        }
    }

    plyLabel_.setText("Position " + std::to_string(navigator_.currentPly())
        + " / " + std::to_string(navigator_.totalPlies()));

    reportTitleLabel_.draw(window, font);
    resultLabel_.draw(window, font);
    openingLabel_.draw(window, font);
    plyLabel_.draw(window, font);
    statusLabel_.draw(window, font);
    hintLabel_.draw(window, font);
}

} // namespace chess::client
