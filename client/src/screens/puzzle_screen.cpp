#include "puzzle_screen.h"

#include "daily.h"
#include "ui/theme.h"
#include "asset_paths.h"
#include "keyboard.h"
#include "ui_helpers.h"

#include <chess/movegen.h>
#include <chess/san.h>

#include <algorithm>
#include <chrono>
#include <ctime>

namespace chess::client {

PuzzleScreen::PuzzleScreen(App& app, bool daily)
    : app_(app)
    , boardView_(static_cast<float>(App::WindowWidth),
                 static_cast<float>(App::WindowHeight),
                 Color::White,
                 app.boardTheme(),
                 app.showCoordinates(),
                 app.coordinateStyle())
    , panelLayout_(layout::composeBoardLayout(
          static_cast<float>(App::WindowWidth),
          static_cast<float>(App::WindowHeight), boardView_,
          layout::BoardLayoutOptions{false, false, false, true}))
{
    puzzles_ = loadPuzzlesFromDirs(assetRoots());
    streak_.best = app_.config().getInt("puzzle.best_streak", 0);

    if (!puzzles_.empty())
        loadPuzzle(0);
    else
        setStatus("No puzzles found (missing puzzles.pgn)", 999.f);

    // 17.2: session day + the Daily button (normal mode only).
    {
        const auto hours = std::chrono::duration_cast<std::chrono::hours>(
            std::chrono::system_clock::now().time_since_epoch());
        today_ = static_cast<int>(hours.count() / 24);
    }
    dailyBtn_.setLabel("Daily puzzle");
    dailyBtn_.setIcon(std::nullopt);
    dailyBtn_.setStyle(ui::buttonInfo());
    dailyBtn_.setRect(sf::FloatRect(
        {panelLayout_.panelX + panelLayout_.buttonInsetX,
         panelLayout_.buttonY(4)},
        {panelLayout_.buttonWidth, panelLayout_.buttonHeight}));
    dailyBtn_.setOnClick([this] { enterDailyMode(); });

    auto place = [&](Button& b, int fromBottom) {
        b.setRect(sf::FloatRect(
            {panelLayout_.panelX + panelLayout_.buttonInsetX,
             panelLayout_.buttonY(fromBottom)},
            {panelLayout_.buttonWidth, panelLayout_.buttonHeight}));
    };

    hintBtn_.setLabel("Hint");
    hintBtn_.setIcon(ui::icons::IconKind::Bulb);
    hintBtn_.setStyle(ui::buttonHint());
    place(hintBtn_, 3);
    hintBtn_.setOnClick([this] { hint(); });

    solutionBtn_.setLabel("Show solution");
    solutionBtn_.setStyle(ui::buttonAmber());
    place(solutionBtn_, 2);
    solutionBtn_.setOnClick([this] { revealSolution(); });

    nextBtn_.setLabel("Next puzzle");
    nextBtn_.setStyle(ui::buttonGreen());
    place(nextBtn_, 1);
    nextBtn_.setOnClick([this] { nextPuzzle(); });

    backBtn_.setLabel("Menu");
    backBtn_.setIcon(ui::icons::IconKind::ArrowBack);
    backBtn_.setStyle(ui::buttonDanger());
    place(backBtn_, 0);
    backBtn_.setOnClick([this] { backToMenu(); });

    if (daily)
        enterDailyMode();
}

const Puzzle* PuzzleScreen::current() const
{
    if (index_ < 0 || index_ >= static_cast<int>(puzzles_.size()))
        return nullptr;
    return &puzzles_[index_];
}

void PuzzleScreen::loadPuzzle(int index)
{
    index_ = index;
    resetPosition();

    const auto* puzzle = current();
    if (!puzzle)
        return;

    board_ = *chess::Board::fromFen(puzzle->startFen);
    boardView_.setFlipped(puzzle->solverColor == chess::Color::Black);
    keyCursor_ = {4, 4};
}

void PuzzleScreen::resetPosition()
{
    const auto* puzzle = current();
    if (puzzle)
        board_ = *chess::Board::fromFen(puzzle->startFen);
    solutionPly_ = 0;
    playedSans_.clear();
    solved_ = false;
    revealed_ = false;
    hintLevel_ = 0;
    pendingReply_.reset();
    replyTimer_ = 0.f;
    promo_.reset();
    promoHover_ = -1;
    anim_.cancel();
    annotations_.clear();
    deselect();
    lastFrom_.reset();
    lastTo_.reset();
    refreshStatus();
}

bool PuzzleScreen::solverTurn() const
{
    const auto* puzzle = current();
    if (!puzzle || solved_)
        return false;
    return board_.sideToMove() == puzzle->solverColor && !pendingReply_;
}

bool PuzzleScreen::solverPieceAt(int file, int rank) const
{
    chess::Piece piece = board_.pieceAt(chess::squareOf(file, rank));
    const auto* puzzle = current();
    if (!puzzle)
        return false;
    return !piece.isNone() && piece.color == puzzle->solverColor;
}

void PuzzleScreen::selectPiece(int file, int rank)
{
    hl_.selectedSquare = {file, rank};
    hl_.legalMoveTargets.clear();
    chess::Square from = chess::squareOf(file, rank);
    for (const auto& m : chess::generateLegalMoves(board_)) {
        if (m.from == from) {
            hl_.legalMoveTargets.push_back({
                static_cast<int>(chess::fileOf(m.to)),
                static_cast<int>(chess::rankOf(m.to))});
        }
    }
}

void PuzzleScreen::tryMove(int targetFile, int targetRank)
{
    if (!hl_.selectedSquare || !solverTurn())
        return;

    chess::Square from =
        chess::squareOf(hl_.selectedSquare->first, hl_.selectedSquare->second);
    chess::Square to = chess::squareOf(targetFile, targetRank);

    const chess::Move* found = nullptr;
    std::vector<chess::Move> promos;
    // Named local: the pointer below must not dangle into a range-for
    // temporary (the game/local-game siblings hoist the vector too).
    const auto moves = chess::generateLegalMoves(board_);
    for (const auto& m : moves) {
        if (m.from == from && m.to == to) {
            found = &m;
            if (m.isPromotion()) promos.push_back(m);
        }
    }

    if (!found) {
        setStatus("Illegal move", 2.f);
        deselect();
        return;
    }

    // Puzzle mode always shows the picker: a stored line may need a
    // specific promotion piece, so auto-queen must not pre-empt it.
    if (found->isPromotion()) {
        buildPromotion(hl_.selectedSquare->first, hl_.selectedSquare->second,
                       targetFile, targetRank);
        return;
    }

    applySolverMove(*found);
}

void PuzzleScreen::buildPromotion(int fromFile, int fromRank, int toFile,
                                  int toRank)
{
    PromotionState ps;
    ps.fromFile = fromFile;
    ps.fromRank = fromRank;
    ps.toFile = toFile;
    ps.toRank = toRank;

    chess::Square from = chess::squareOf(fromFile, fromRank);
    chess::Square to = chess::squareOf(toFile, toRank);
    for (const auto& m : chess::generateLegalMoves(board_)) {
        if (m.from == from && m.to == to && m.isPromotion())
            ps.candidates.push_back(m);
    }
    promo_ = std::move(ps);
    promoHover_ = -1;
}

void PuzzleScreen::applyPromotionMove(chess::PieceType type)
{
    if (!promo_) return;
    for (const auto& m : promo_->candidates) {
        if (m.promotion == type) {
            promo_.reset();
            promoHover_ = -1;
            applySolverMove(m);
            return;
        }
    }
    cancelPromotion();
}

void PuzzleScreen::cancelPromotion()
{
    promo_.reset();
    promoHover_ = -1;
    deselect();
}

void PuzzleScreen::applySolverMove(const chess::Move& m)
{
    const auto* puzzle = current();
    if (!puzzle || !solverTurn())
        return;

    const chess::Move& expected = puzzle->solution[solutionPly_];
    if (!(m == expected)) {
        onWrongMove();
        return;
    }

    startMovePresentation(m);
    playedSans_.push_back(chess::san::toSan(board_, m));
    board_.makeMove(m);
    ++solutionPly_;
    deselect();
    refreshStatus();
    if (solutionPly_ >= static_cast<int>(puzzle->solution.size())) {
        onSolved();
        return;
    }
    scheduleReply();
}

void PuzzleScreen::startMovePresentation(const chess::Move& m)
{
    const auto& cfg = app_.config();
    const float duration = static_cast<float>(cfg.getFloat("animation.duration", 0.3));
    if (cfg.getBool("animation.enabled", true) && duration > 0.f)
        anim_.start(m, board_, duration);
    app_.sounds().playMove(board_, m);
    lastFrom_ = {static_cast<int>(chess::fileOf(m.from)),
                 static_cast<int>(chess::rankOf(m.from))};
    lastTo_ = {static_cast<int>(chess::fileOf(m.to)),
               static_cast<int>(chess::rankOf(m.to))};
}

void PuzzleScreen::scheduleReply()
{
    const auto* puzzle = current();
    if (!puzzle)
        return;
    if (solutionPly_ >= static_cast<int>(puzzle->solution.size()))
        return;

    const auto& cfg = app_.config();
    const float duration = static_cast<float>(cfg.getFloat("animation.duration", 0.3));
    if (cfg.getBool("animation.enabled", true) && duration > 0.f) {
        // Let the solver's slide finish before the defender answers.
        pendingReply_ = puzzle->solution[solutionPly_];
        replyTimer_ = duration;
        return;
    }
    fireReply();
}

void PuzzleScreen::fireReply()
{
    if (!pendingReply_)
        return;
    const chess::Move reply = *pendingReply_;
    pendingReply_.reset();
    replyTimer_ = 0.f;

    if (solutionPly_ >= static_cast<int>(current()->solution.size())) {
        refreshStatus();
        return;
    }

    startMovePresentation(reply);
    playedSans_.push_back(chess::san::toSan(board_, reply));
    board_.makeMove(reply);
    ++solutionPly_;
    refreshStatus();
}

void PuzzleScreen::onSolved()
{
    solved_ = true;
    streak_.onSolved();
    app_.config().setInt("puzzle.best_streak", streak_.best);
    app_.config().save();
    setStatus("Solved! Streak " + std::to_string(streak_.current), 5.f);

    // 17.2: every solve counts toward all-time; the daily also marks its
    // day + streak (a skipped day resets via daily::solveStreak).
    const int solvedTotal = app_.config().getInt("puzzle.solved_total", 0) + 1;
    app_.config().setInt("puzzle.solved_total", solvedTotal);
    if (daily_) {
        const int last = app_.config().getInt("puzzle.daily_last", -1);
        const int streak = daily::solveStreak(
            last, app_.config().getInt("puzzle.daily_streak", 0), today_);
        app_.config().setInt("puzzle.daily_last", today_);
        app_.config().setInt("puzzle.daily_streak", streak);
        dailyStreak_ = streak;
        setStatus("Daily solved! Streak " + std::to_string(streak)
                      + " - come back tomorrow.", 999.f);
    }
    app_.config().save();
}

void PuzzleScreen::onWrongMove()
{
    streak_.onFail();
    setStatus("Not the move - puzzle restarted. Streak reset to 0.", 4.f);
    deselect();
    clearBoardInput();
    const std::string text = status_;
    resetPosition();
    status_ = text;
    statusTimer_ = 4.f;
}

void PuzzleScreen::hint()
{
    const auto* puzzle = current();
    if (!puzzle || solved_ || revealed_)
        return;
    if (puzzle->solution.empty())
        return;

    if (hintLevel_ == 0) {
        ++hintLevel_;
        const auto& m = puzzle->solution[solutionPly_];
        annotations_.clear();
        annotations_.toggleCircle(static_cast<int>(chess::fileOf(m.from)),
                                  static_cast<int>(chess::rankOf(m.from)));
        setStatus("Hint: the mouse may circle the piece", 4.f);
    } else if (hintLevel_ == 1) {
        ++hintLevel_;
        const auto& m = puzzle->solution[solutionPly_];
        annotations_.addArrow(static_cast<int>(chess::fileOf(m.from)),
                              static_cast<int>(chess::rankOf(m.from)),
                              static_cast<int>(chess::fileOf(m.to)),
                              static_cast<int>(chess::rankOf(m.to)));
        setStatus("Arrow shows the full move", 4.f);
    } else {
        setStatus("The move is marked - play it!", 2.f);
    }
}

void PuzzleScreen::revealLine()
{
    // Shared by the manual reveal and the daily's already-solved replay:
    // applies the whole stored line, no status or streak side effects.
    const auto* puzzle = current();
    if (!puzzle)
        return;
    deselect();
    clearBoardInput();

    while (solutionPly_ < static_cast<int>(puzzle->solution.size())) {
        const chess::Move& m = puzzle->solution[solutionPly_];
        playedSans_.push_back(chess::san::toSan(board_, m));
        lastFrom_ = {static_cast<int>(chess::fileOf(m.from)),
                     static_cast<int>(chess::rankOf(m.from))};
        lastTo_ = {static_cast<int>(chess::fileOf(m.to)),
                   static_cast<int>(chess::rankOf(m.to))};
        board_.makeMove(m);
        ++solutionPly_;
    }
    app_.sounds().play(chess::client::Sfx::Checkmate);
}

void PuzzleScreen::revealSolution()
{
    const auto* puzzle = current();
    if (!puzzle || solved_ || revealed_)
        return;

    streak_.onFail();
    revealed_ = true;
    hintLevel_ = 2;
    revealLine();

    if (daily_)
        setStatus("Solution shown - come back tomorrow for a new daily.", 6.f);
    else
        setStatus("Solution shown - streak reset. Press Next.", 6.f);
}

void PuzzleScreen::nextPuzzle()
{
    if (daily_) {
        leaveDailyMode();
        return;
    }
    if (puzzles_.empty())
        return;
    loadPuzzle((index_ + 1) % static_cast<int>(puzzles_.size()));
}

void PuzzleScreen::enterDailyMode()
{
    if (puzzles_.empty())
        return;
    daily_ = true;
    refreshDailyStats();
    loadPuzzle(daily::dailyIndex(today_, static_cast<int>(puzzles_.size())));
    nextBtn_.setLabel("Back to puzzles");

    if (daily::solvedToday(app_.config().getInt("puzzle.daily_last", -1),
                           today_)) {
        // Already done today: replay the line for viewing, no new marks.
        revealed_ = true;
        hintLevel_ = 2;
        revealLine();
        setStatus("Daily solved - streak "
                      + std::to_string(dailyStreak_)
                      + ". Come back tomorrow!", 999.f);
    } else {
        refreshStatus();
    }
}

void PuzzleScreen::leaveDailyMode()
{
    daily_ = false;
    nextBtn_.setLabel("Next puzzle");
    loadPuzzle(0);
}

void PuzzleScreen::refreshDailyStats()
{
    const int last = app_.config().getInt("puzzle.daily_last", -1);
    dailyStreak_ = daily::displayStreak(
        last, app_.config().getInt("puzzle.daily_streak", 0), today_);
}

void PuzzleScreen::backToMenu()
{
    // Audit 9.3: return to the app home — the hub when a hub card opened
    // this puzzle set, the legacy menu only for `general.entry` opt-outs.
    // RushScreen's override persists a beaten best before delegating.
    app_.goHome();
}

void PuzzleScreen::deselect()
{
    hl_.selectedSquare.reset();
    hl_.legalMoveTargets.clear();
}

void PuzzleScreen::clearBoardInput()
{
    drag_.cancel();
    dragFrom_.reset();
    rightPress_.reset();
}

void PuzzleScreen::syncViewHighlights()
{
    hl_.hoverSquare = boardView_.pixelToSquare(cursor_);
    hl_.lastMoveFrom = lastFrom_;
    hl_.lastMoveTo = lastTo_;
    hl_.cursorSquare = keyCursor_;
    chess::Color stm = board_.sideToMove();
    hl_.checkSquare = chess::inCheck(board_, stm)
        ? findKingSquare(board_, stm)
        : std::optional<std::pair<int, int>>{};
}

void PuzzleScreen::setStatus(std::string text, float seconds)
{
    status_ = std::move(text);
    statusTimer_ = seconds;
}

void PuzzleScreen::refreshStatus()
{
    const auto* puzzle = current();
    if (!puzzle) {
        setStatus("No puzzles found (missing puzzles.pgn)", 999.f);
        return;
    }
    if (daily_) {
        refreshDailyStats();
        if (daily::solvedToday(app_.config().getInt("puzzle.daily_last", -1),
                               today_))
            setStatus("Daily solved - streak "
                          + std::to_string(dailyStreak_)
                          + ". Come back tomorrow!", 999.f);
        else {
            const char* side =
                puzzle->solverColor == chess::Color::White ? "White" : "Black";
            setStatus(std::string(side) + " to move - mate in " +
                          std::to_string(puzzle->mateIn()), 999.f);
        }
        return;
    }
    if (solved_) {
        setStatus("Solved! Streak " + std::to_string(streak_.current) +
                      "  -  next puzzle?", 999.f);
        return;
    }
    const char* side =
        puzzle->solverColor == chess::Color::White ? "White" : "Black";
    setStatus(std::string(side) + " to move - mate in " +
                  std::to_string(puzzle->mateIn()), 999.f);
}

PromoCell PuzzleScreen::promoCell(int index) const
{
    if (!promo_)
        return {{0.f, 0.f}, 0.f};
    return promoStackCell(boardView_.squareSize(), boardView_.boardOrigin(),
                          boardView_.isFlipped(), promo_->toFile,
                          promo_->toRank, index);
}

std::optional<chess::PieceType>
PuzzleScreen::promoTypeForKey(sf::Keyboard::Key key) const
{
    switch (key) {
        case sf::Keyboard::Key::Q:
        case sf::Keyboard::Key::Num1: return chess::PieceType::Queen;
        case sf::Keyboard::Key::R:
        case sf::Keyboard::Key::Num2: return chess::PieceType::Rook;
        case sf::Keyboard::Key::B:
        case sf::Keyboard::Key::Num3: return chess::PieceType::Bishop;
        case sf::Keyboard::Key::N:
        case sf::Keyboard::Key::Num4: return chess::PieceType::Knight;
        default: return std::nullopt;
    }
}

void PuzzleScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            if (showingHelp_) { showingHelp_ = false; return; }
            if (promo_) { cancelPromotion(); return; }
            clearBoardInput();
            deselect();
            return;
        }

        // Tier 4 (v2.0.3 rule, round-4 audit): the help overlay gates every
        // other input — keys and clicks beneath it must not drive the board
        // (a cursor move or a click under the dim was a real move attempt).
        // Escape above and the Help toggle here are the only exits.
        if (showingHelp_) {
            if (keyToAction(kp->code, kp->shift) == KeyboardAction::Help)
                showingHelp_ = false;
            return;
        }

        if (promo_) {
            if (kp->code == sf::Keyboard::Key::Space) { cancelPromotion(); return; }
            if (auto t = promoTypeForKey(kp->code)) {
                applyPromotionMove(*t);
                return;
            }
            return;
        }

        switch (keyToAction(kp->code, kp->shift)) {
            case KeyboardAction::Deselect:
                clearBoardInput();
                deselect();
                return;
            case KeyboardAction::Flip:
                boardView_.toggleFlipped();
                return;
            case KeyboardAction::Help:
                showingHelp_ = !showingHelp_;
                return;
            case KeyboardAction::MoveCursor: {
                auto [df, dr] = cursorDelta(kp->code, boardView_.isFlipped());
                keyCursor_ = moveCursorStep(keyCursor_.first, keyCursor_.second, df, dr);
                return;
            }
            case KeyboardAction::Enter: {
                if (!solverTurn()) return;
                if (hl_.selectedSquare) {
                    if (*hl_.selectedSquare == keyCursor_) {
                        deselect();
                    } else if (solverPieceAt(keyCursor_.first, keyCursor_.second)) {
                        selectPiece(keyCursor_.first, keyCursor_.second);
                    } else {
                        tryMove(keyCursor_.first, keyCursor_.second);
                    }
                } else {
                    if (solverPieceAt(keyCursor_.first, keyCursor_.second))
                        selectPiece(keyCursor_.first, keyCursor_.second);
                }
                return;
            }
            case KeyboardAction::PrevMove:
            case KeyboardAction::NextMove:
            case KeyboardAction::None:
            default:
                break;
        }
        return;
    }

    // Tier 4: clicks beneath the help overlay do nothing (buttons, promo
    // cells, and board squares all sit under the dim).
    if (showingHelp_) return;

    sf::Vector2f local(0.f, 0.f);
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
        local = app_.toLocal(mm->position);
    else if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
        local = app_.toLocal(mb->position);
    else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
        local = app_.toLocal(rb->position);

    if (const auto* mm = event.getIf<sf::Event::MouseMoved>()) {
        (void)mm;
        cursor_ = local;
        if (promo_) {
            promoHover_ = -1;
            for (int i = 0; i < static_cast<int>(promo_->candidates.size()); ++i) {
                auto cell = promoCell(i);
                if (sf::FloatRect(cell.pos, {cell.size, cell.size}).contains(local)) {
                    promoHover_ = i;
                    break;
                }
            }
            return;
        }
        if (drag_.isActive()) drag_.move(local.x, local.y);
    }

    if (backBtn_.handleEvent(event, local)) return;
    if (hintBtn_.handleEvent(event, local)) return;
    if (solutionBtn_.handleEvent(event, local)) return;
    if (nextBtn_.handleEvent(event, local)) return;
    if (dailyBtn_.handleEvent(event, local)) return;

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            if (promo_) {
                for (int i = 0; i < static_cast<int>(promo_->candidates.size()); ++i) {
                    auto cell = promoCell(i);
                    if (sf::FloatRect(cell.pos, {cell.size, cell.size}).contains(local)) {
                        applyPromotionMove(promo_->candidates[i].promotion);
                        return;
                    }
                }
                cancelPromotion();
                return;
            }

            annotations_.clear();
            hintLevel_ = 0;
            if (!solverTurn()) return;

            auto square = boardView_.pixelToSquare(local);
            if (!square) return;

            keyCursor_ = *square;
            drag_.press(local.x, local.y);
            cursor_ = local;
            dragFrom_ = *square;

            if (solverPieceAt(square->first, square->second))
                selectPiece(square->first, square->second);
        } else if (mb->button == sf::Mouse::Button::Right) {
            if (promo_) cancelPromotion();
            clearBoardInput();
            deselect();
            rightPress_ = boardView_.pixelToSquare(local);
        }
        return;
    }

    if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (rb->button == sf::Mouse::Button::Left) {
            bool wasDragging = drag_.release();
            auto square = boardView_.pixelToSquare(local);
            cursor_ = local;

            if (dragFrom_) {
                if (wasDragging) {
                    if (square && *square != *dragFrom_
                        && solverPieceAt(dragFrom_->first, dragFrom_->second)) {
                        selectPiece(dragFrom_->first, dragFrom_->second);
                        tryMove(square->first, square->second);
                    }
                } else if (hl_.selectedSquare) {
                    if (square && *square == *hl_.selectedSquare) {
                        deselect();
                    } else if (square && solverPieceAt(square->first, square->second)) {
                        selectPiece(square->first, square->second);
                    } else if (square) {
                        tryMove(square->first, square->second);
                    } else {
                        deselect();
                    }
                }
            }
            dragFrom_.reset();
            return;
        }

        if (rb->button == sf::Mouse::Button::Right) {
            auto square = boardView_.pixelToSquare(local);
            if (rightPress_) {
                if (square && *square == *rightPress_) {
                    annotations_.toggleCircle(square->first, square->second);
                } else if (square) {
                    annotations_.addArrow(rightPress_->first, rightPress_->second,
                                          square->first, square->second);
                }
            }
            rightPress_.reset();
            return;
        }
    }
}

void PuzzleScreen::update(float dtSec)
{
    anim_.update(dtSec);

    if (statusTimer_ < 999.f) {
        statusTimer_ -= dtSec;
        if (statusTimer_ <= 0.f)
            refreshStatus();
    }

    if (pendingReply_) {
        replyTimer_ -= dtSec;
        if (replyTimer_ <= 0.f)
            fireReply();
    }
}

void PuzzleScreen::drawBoardLayer(sf::RenderWindow& window)
{
    auto& font = app_.font();

    boardView_.drawSquares(window);
    syncViewHighlights();
    boardView_.drawHighlights(window, hl_, board_);
    boardView_.drawLabels(window, font);
    boardView_.drawPieces(window, font, board_, app_, &anim_);
    boardView_.drawAnnotations(window, annotations_.arrows(), annotations_.circles());

    if (drag_.isDragging() && dragFrom_) {
        chess::Piece dragged = board_.pieceAt(
            chess::squareOf(dragFrom_->first, dragFrom_->second));
        const auto* puzzle = current();
        if (!dragged.isNone() && puzzle && dragged.color == puzzle->solverColor)
            boardView_.drawDraggedPiece(window, font, dragged, cursor_, app_);
    }

    if (!promo_)
        return;
    const auto* puzzle = current();
    for (int i = 0; i < static_cast<int>(promo_->candidates.size()); ++i) {
        auto c = promoCell(i);
        sf::RectangleShape cell({c.size, c.size});
        cell.setPosition(c.pos);
        cell.setFillColor(ui::overlay::cellFill);
        cell.setOutlineColor(ui::overlay::cellStroke);
        cell.setOutlineThickness(ui::overlay::promoStrokeW);
        window.draw(cell);

        if (i == promoHover_) {
            sf::RectangleShape hover({c.size, c.size});
            hover.setPosition(c.pos);
            hover.setFillColor(ui::overlay::cellHover);
            window.draw(hover);
        }

        chess::PieceType pt = promo_->candidates[i].promotion;
        float pieceSize = c.size * 0.8f;
        float offset = (c.size - pieceSize) / 2.f;

        if (app_.piecesLoaded() && puzzle) {
            const auto& tex = app_.pieceTexture(puzzle->solverColor, pt);
            sf::Sprite sprite(tex);
            float scale = pieceSize / static_cast<float>(tex.getSize().x);
            sprite.setScale({scale, scale});
            sprite.setPosition({c.pos.x + offset, c.pos.y + offset});
            window.draw(sprite);
        } else {
            const char letters[] = {'P', 'N', 'B', 'R', 'Q', 'K'};
            unsigned int letterSize = static_cast<unsigned int>(c.size * 0.5f);
            if (letterSize < 12) letterSize = 12;
            sf::Text letter(font,
                            std::string(1, letters[static_cast<int>(pt)]),
                            letterSize);
            letter.setFillColor(ui::overlay::cellGlyph);
            auto lb = letter.getLocalBounds();
            letter.setPosition({
                c.pos.x + (c.size - lb.size.x) / 2.f - lb.position.x,
                c.pos.y + (c.size - lb.size.y) / 2.f - lb.position.y});
            window.draw(letter);
        }
    }
}

void PuzzleScreen::draw(sf::RenderWindow& window)
{
    drawBoardLayer(window);

    // Panel
    const float px = panelLayout_.panelX;
    const float height = static_cast<float>(App::WindowHeight);
    sf::RectangleShape panel({panelLayout_.panelEdgeWidth, height});
    panel.setPosition({px, 0.f});
    panel.setFillColor(ui::pal::puzzlePanel);
    window.draw(panel);

    auto& font = app_.font();
    const auto* puzzle = current();
    std::string title = "Puzzles";
    if (daily_) {
        title = "Daily puzzle";
        std::time_t t = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        std::tm tm{};
        localtime_r(&t, &tm);
        char buf[32];
        if (std::strftime(buf, sizeof(buf), "%b %d", &tm) > 0)
            title += std::string("  -  ") + buf;
    } else if (puzzle) {
        const char* side =
            puzzle->solverColor == chess::Color::White ? "White" : "Black";
        title = "Puzzle " + std::to_string(index_ + 1) + " of " +
                std::to_string(puzzles_.size()) + "  -  " + side +
                " mates in " + std::to_string(puzzle->mateIn());
    }
    sf::Text titleText(font, title, 15);
    titleText.setFillColor(ui::pal::textPrimary);
    titleText.setPosition({px + panelLayout_.buttonInsetX,
                           panelLayout_.puzzleTitleY});
    window.draw(titleText);

    sf::Text streakText(font,
                        "Streak " + std::to_string(streak_.current) +
                            "   best " + std::to_string(streak_.best),
                        14);
    streakText.setFillColor(ui::pal::streakSage);
    streakText.setPosition({px + panelLayout_.buttonInsetX,
                            panelLayout_.puzzleStreakY});
    window.draw(streakText);

    sf::Text statusText(font, status_, 14);
    statusText.setFillColor(solved_ ? ui::pal::ok
                                    : ui::pal::warnBanner);
    statusText.setPosition({px + panelLayout_.buttonInsetX,
                            panelLayout_.puzzleStatusY});
    window.draw(statusText);

    if (!playedSans_.empty()) {
        std::string line;
        for (const auto& s : playedSans_) {
            if (!line.empty()) line += ' ';
            line += s;
        }
        sf::Text lineText(font, safeTruncate(line, 32), 14);
        lineText.setFillColor(ui::pal::solutionSlate);
        lineText.setPosition({px + panelLayout_.buttonInsetX,
                              panelLayout_.puzzleSolutionY});
        window.draw(lineText);
    }

    // 17.2 daily stats line: chain-alive streak + all-time solve count.
    {
        const int allTime = app_.config().getInt("puzzle.solved_total", 0);
        sf::Text dailyText(font,
                           "Daily streak " + std::to_string(dailyStreak_)
                               + "  -  all-time " + std::to_string(allTime),
                           13);
        dailyText.setFillColor(ui::pal::textGhost);
        dailyText.setPosition({px + panelLayout_.buttonInsetX,
                               panelLayout_.puzzleDailyY});
        window.draw(dailyText);
    }

    hintBtn_.setEnabled(puzzle && !solved_);
    solutionBtn_.setEnabled(puzzle && !solved_ && !revealed_);
    nextBtn_.setEnabled(puzzle);
    dailyBtn_.setEnabled(puzzle && !daily_);
    hintBtn_.draw(window, font);
    solutionBtn_.draw(window, font);
    nextBtn_.draw(window, font);
    if (!daily_) dailyBtn_.draw(window, font);
    backBtn_.draw(window, font);

    if (showingHelp_) drawHelpOverlay(window);
}

void PuzzleScreen::drawHelpOverlay(sf::RenderWindow& window)
{
    auto& font = app_.font();

    sf::RectangleShape dim(
        {static_cast<float>(App::WindowWidth),
         static_cast<float>(App::WindowHeight)});
    dim.setFillColor(ui::overlay::dim);
    window.draw(dim);

    const char* lines[] = {
        "Puzzle shortcuts",
        "Drag or click - play the winning move",
        "Arrows - move cursor, Enter - play",
        "Space - deselect",
        "F - flip board",
        "Q R B N or 1-4 - choose promotion",
        "Right drag - draw arrows",
        "Hint (twice) / Show solution - assist",
        "? - toggle this help",
    };
    constexpr int Count = static_cast<int>(sizeof(lines) / sizeof(lines[0]));

    constexpr unsigned int HdrSize = 17;
    constexpr unsigned int RowSize = 14;
    constexpr float RowH = 24.f;
    constexpr float PadX = 22.f;
    constexpr float PadY = 18.f;

    float maxW = 0.f;
    for (int i = 0; i < Count; ++i) {
        sf::Text t(font, lines[i], (i == 0) ? HdrSize : RowSize);
        maxW = std::max(maxW, t.getLocalBounds().size.x);
    }

    const float panelW = maxW + 2.f * PadX;
    const float panelH = RowH * static_cast<float>(Count) + 2.f * PadY;
    const float px = (static_cast<float>(App::WindowWidth) - panelW) / 2.f;
    const float py = (static_cast<float>(App::WindowHeight) - panelH) / 2.f;

    sf::RectangleShape bg({panelW, panelH});
    bg.setPosition({px, py});
    bg.setFillColor(ui::overlay::panelFill);
    bg.setOutlineColor(ui::overlay::panelStroke);
    bg.setOutlineThickness(1.f);
    window.draw(bg);

    float y = py + PadY;
    for (int i = 0; i < Count; ++i) {
        sf::Text t(font, lines[i], (i == 0) ? HdrSize : RowSize);
        t.setFillColor(i == 0 ? ui::pal::white : ui::pal::textCell);
        auto lb = t.getLocalBounds();
        t.setPosition({
            px + (panelW - lb.size.x) / 2.f - lb.position.x,
            y - lb.position.y});
        window.draw(t);
        y += RowH;
    }
}

} // namespace chess::client
