#include "local_game_screen.h"
#include "game_over_screen.h"
#include "menu_screen.h"
#include "ui_helpers.h"

#include <chess/movegen.h>
#include <chess/net/messages.h>
#include <chess/san.h>

namespace chess::client {

namespace {
constexpr float BtnH = 30.f;

net::GameOverReason detectDrawReason(const Board& board)
{
    if (board.halfmoveClock() >= 100)
        return net::GameOverReason::FiftyMove;
    if (chess::threefoldRepetition(board))
        return net::GameOverReason::Repetition;
    if (chess::insufficientMaterial(board))
        return net::GameOverReason::InsufficientMaterial;
    return net::GameOverReason::Abort;
}

net::GameOverReason toReason(GameState state, const Board& board)
{
    switch (state) {
        case GameState::Checkmate:  return net::GameOverReason::Checkmate;
        case GameState::Stalemate:  return net::GameOverReason::Stalemate;
        case GameState::Draw:       return detectDrawReason(board);
        default:                    return net::GameOverReason::Abort;
    }
}

net::GameResult toResult(GameState state, Color sideToMove)
{
    switch (state) {
        case GameState::Checkmate:
            return sideToMove == Color::White
                ? net::GameResult::BlackWins
                : net::GameResult::WhiteWins;
        case GameState::Stalemate:
        case GameState::Draw:
            return net::GameResult::Draw;
        default:
            return net::GameResult::Abort;
    }
}
}

LocalGameScreen::LocalGameScreen(App& app, Color myColor,
                                 std::string enginePath, int depth)
    : app_(app)
    , myColor_(myColor)
    , boardView_(static_cast<float>(App::WindowWidth),
                 static_cast<float>(App::WindowHeight),
                 myColor)
    , hud_(app_, boardView_.panelX(),
           static_cast<float>(App::WindowWidth) - boardView_.panelX() - 8.f)
    , myTurn_(myColor == Color::White)
    , engineDepth_(depth)
{
    hud_.setGame(Board::fromStartPos(), {}, {});
    hud_.setInfo("Computer", myColor_, myTurn_, gameOver_);

    backBtn_.setLabel("Back to Menu");
    backBtn_.setColors(sf::Color(160, 55, 55), sf::Color(185, 65, 65),
                       sf::Color(135, 45, 45), sf::Color(70, 45, 45),
                       sf::Color(140, 90, 90), sf::Color(240, 240, 240),
                       sf::Color(90, 60, 60), sf::Color(210, 120, 120),
                       sf::Color(180, 140, 140));
    backBtn_.setRect(sf::FloatRect(
        sf::Vector2f(boardView_.panelX(), hud_.contentBottom()),
        sf::Vector2f(140.f, BtnH)));
    backBtn_.setOnClick([this] { returnToMenu(); });

    autoQueenCheck_.setOnToggle([this](bool checked) {
        app_.setAutoQueen(checked);
    });

    engine_ = std::make_unique<uci::UciEngine>(std::move(enginePath));
    auto info = engine_->init();
    if (info.name.empty() && !engine_->isRunning()) {
        engineFailed_ = true;
        hud_.setStatus("Engine not found. Set CHESS_ENGINE_PATH.", 999.f);
        return;
    }

    engine_->setOption("Threads", "1");
    engine_->setOption("Hash", "64");
    engine_->newGame();
    engine_->position("startpos");

    if (myColor_ == Color::Black) {
        myTurn_ = false;
        engineThinking_ = true;
        hud_.setStatus("Computer thinking...", 999.f);
        engine_->go(engineDepth_);
    }
}

LocalGameScreen::~LocalGameScreen()
{
    if (engine_) engine_->quit();
}

void LocalGameScreen::selectPiece(int file, int rank)
{
    hl_.selectedSquare = { file, rank };
    hl_.legalMoveTargets.clear();
    Square from = squareOf(file, rank);
    auto moves = chess::generateLegalMoves(hud_.navigator().finalBoard());
    for (const auto& m : moves) {
        if (m.from == from) {
            hl_.legalMoveTargets.push_back({
                static_cast<int>(chess::fileOf(m.to)),
                static_cast<int>(chess::rankOf(m.to))
            });
        }
    }
}

void LocalGameScreen::tryMove(int targetFile, int targetRank)
{
    const Board& live = hud_.navigator().finalBoard();
    Square from = squareOf(hl_.selectedSquare->first, hl_.selectedSquare->second);
    Square to = squareOf(targetFile, targetRank);

    auto moves = chess::generateLegalMoves(live);
    const chess::Move* found = nullptr;
    std::vector<chess::Move> promos;
    for (const auto& m : moves) {
        if (m.from == from && m.to == to) {
            found = &m;
            if (m.isPromotion()) promos.push_back(m);
        }
    }

    if (!found) {
        hud_.setStatus("Illegal move", 2.0f);
        deselect();
        return;
    }

    if (found->isPromotion()) {
        if (auto qm = autoQueenMove(promos)) {
            applyMove(*qm);
            return;
        }
        buildPromotion(hl_.selectedSquare->first, hl_.selectedSquare->second,
                       targetFile, targetRank);
        return;
    }

    applyMove(*found);
}

void LocalGameScreen::buildPromotion(int fromFile, int fromRank, int toFile, int toRank)
{
    PromotionState ps;
    ps.fromFile = fromFile;
    ps.fromRank = fromRank;
    ps.toFile = toFile;
    ps.toRank = toRank;

    const Board& live = hud_.navigator().finalBoard();
    Square from = squareOf(ps.fromFile, ps.fromRank);
    Square to = squareOf(ps.toFile, ps.toRank);
    auto moves = chess::generateLegalMoves(live);
    for (const auto& m : moves) {
        if (m.from == from && m.to == to && m.isPromotion())
            ps.candidates.push_back(m);
    }
    promo_ = std::move(ps);
    promoHover_ = -1;
}

void LocalGameScreen::applyMove(const chess::Move& m)
{
    std::string san = chess::san::toSan(hud_.navigator().finalBoard(), m);
    hud_.navigator().appendMove(m, san);
    deselect();

    auto state = chess::evaluateGameState(hud_.navigator().finalBoard());
    if (state != GameState::Ongoing) {
        gameOver_ = true;
        myTurn_ = false;
        hud_.setGameOver(true);
        hud_.setInfo("Computer", myColor_, myTurn_, gameOver_);
        app_.switchScreen(std::make_unique<GameOverScreen>(
            app_, toResult(state, hud_.navigator().finalBoard().sideToMove()),
            toReason(state, hud_.navigator().finalBoard()),
            hud_.navigator().initialBoard(), hud_.navigator().moves(),
            hud_.navigator().sans()));
        return;
    }

    myTurn_ = false;
    engineThinking_ = true;
    hud_.setStatus("Computer thinking...", 999.f);
    engine_->position(hud_.navigator().finalBoard().toFen());
    engine_->go(engineDepth_);
    hud_.setInfo("Computer", myColor_, myTurn_, gameOver_);
}

void LocalGameScreen::deselect()
{
    hl_.selectedSquare.reset();
    hl_.legalMoveTargets.clear();
}

void LocalGameScreen::syncViewHighlights()
{
    const Board& shown = hud_.navigator().board();
    hl_.lastMoveFrom = hud_.navigator().lastMoveFrom();
    hl_.lastMoveTo = hud_.navigator().lastMoveTo();
    Color stm = shown.sideToMove();
    hl_.checkSquare = chess::inCheck(shown, stm)
        ? findKingSquare(shown, stm)
        : std::optional<std::pair<int, int>>{};
}

void LocalGameScreen::applyPromotionMove(chess::PieceType type)
{
    if (!promo_) return;
    for (const auto& m : promo_->candidates) {
        if (m.promotion == type) {
            promo_.reset();
            promoHover_ = -1;
            applyMove(m);
            return;
        }
    }
    promo_.reset();
    promoHover_ = -1;
    deselect();
}

void LocalGameScreen::cancelPromotion()
{
    promo_.reset();
    promoHover_ = -1;
    deselect();
}

void LocalGameScreen::clearBoardInput()
{
    drag_.cancel();
    dragFrom_.reset();
    rightPress_.reset();
}

bool LocalGameScreen::ownPieceAt(int file, int rank)
{
    Piece piece = hud_.navigator().finalBoard().pieceAt(squareOf(file, rank));
    return !piece.isNone() && piece.color == myColor_;
}

sf::FloatRect LocalGameScreen::autoQueenRect() const
{
    auto cell = promoCell(static_cast<int>(promo_->candidates.size()) - 1);
    float sq = boardView_.squareSize();
    return { { cell.pos.x, cell.pos.y + cell.size + 3.f },
             { sq * 1.6f, 22.f } };
}

std::optional<chess::PieceType> LocalGameScreen::promoTypeForKey(sf::Keyboard::Key key) const
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

bool LocalGameScreen::applyEngineMove()
{
    auto move = engine_->tryGetBestMove(hud_.navigator().finalBoard());
    if (!move) return false;

    std::string san = chess::san::toSan(hud_.navigator().finalBoard(), *move);
    hud_.navigator().appendMove(*move, san);
    hl_.selectedSquare.reset();
    hl_.legalMoveTargets.clear();

    engineThinking_ = false;
    hud_.setStatus("", 0.f);
    myTurn_ = true;
    hud_.setInfo("Computer", myColor_, myTurn_, gameOver_);
    return true;
}

void LocalGameScreen::checkGameOver()
{
    auto state = chess::evaluateGameState(hud_.navigator().finalBoard());
    if (state == GameState::Ongoing) return;

    gameOver_ = true;
    myTurn_ = false;
    hud_.setGameOver(true);
    hud_.setInfo("Computer", myColor_, myTurn_, gameOver_);

    app_.switchScreen(std::make_unique<GameOverScreen>(
        app_, toResult(state, hud_.navigator().finalBoard().sideToMove()),
        toReason(state, hud_.navigator().finalBoard()),
        hud_.navigator().initialBoard(), hud_.navigator().moves(),
        hud_.navigator().sans()));
}

void LocalGameScreen::returnToMenu()
{
    app_.switchScreen(std::make_unique<MenuScreen>(app_));
}

PromoCell LocalGameScreen::promoCell(int index) const
{
    float sq = boardView_.squareSize();
    sf::Vector2f origin = boardView_.boardOrigin();
    bool flipped = boardView_.isFlipped();
    int col = flipped ? (7 - promo_->toFile) : promo_->toFile;
    int rowStart = flipped ? promo_->toRank : (7 - promo_->toRank);
    return {
        {origin.x + static_cast<float>(col) * sq,
         origin.y + static_cast<float>(rowStart + index) * sq},
        sq
    };
}

void LocalGameScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            if (promo_) { cancelPromotion(); return; }
            clearBoardInput();
            deselect();
            return;
        }
        if (kp->code == sf::Keyboard::Key::Space) {
            if (promo_) { cancelPromotion(); return; }
            clearBoardInput();
            deselect();
            return;
        }

        if (promo_) {
            if (auto t = promoTypeForKey(kp->code)) {
                applyPromotionMove(*t);
                return;
            }
        }

        if (hud_.navigator().handleEvent(event, {0.f, 0.f})) {
            clearBoardInput();
            return;
        }
        return;
    }

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
            autoQueenCheck_.setRect(autoQueenRect());
            autoQueenCheck_.handleEvent(event, local);
            return;
        }
        if (drag_.isActive()) drag_.move(local.x, local.y);
    }

    if (const auto* we = event.getIf<sf::Event::MouseWheelScrolled>()) {
        float px = boardView_.panelX();
        if (app_.toLocal(we->position).x >= px) {
            hud_.handleScroll(we->delta);
            return;
        }
    }

    if (hud_.navigator().handleEvent(event, local)) {
        clearBoardInput();
        return;
    }

    if (!gameOver_ && backBtn_.handleEvent(event, local)) return;

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            if (promo_) {
                autoQueenCheck_.setRect(autoQueenRect());
                if (autoQueenCheck_.handleEvent(*mb, local)) return;
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
            if (gameOver_ || !myTurn_ || engineThinking_ || engineFailed_) return;

            if (!hud_.navigator().atEnd()) {
                hud_.navigator().goEnd();
                deselect();
            }

            auto square = boardView_.pixelToSquare(local);
            if (!square) return;

            drag_.press(local.x, local.y);
            cursor_ = local;
            dragFrom_ = *square;

            if (ownPieceAt(square->first, square->second))
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
                        && ownPieceAt(dragFrom_->first, dragFrom_->second)) {
                        selectPiece(dragFrom_->first, dragFrom_->second);
                        tryMove(square->first, square->second);
                    }
                } else if (hl_.selectedSquare) {
                    if (square && *square == *hl_.selectedSquare) {
                        deselect();
                    } else if (square && ownPieceAt(square->first, square->second)) {
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

void LocalGameScreen::update(float /*dtSec*/)
{
    hud_.update(0.f);

    if (gameOver_ || engineFailed_) return;

    if (engineThinking_) {
        if (applyEngineMove()) {
            checkGameOver();
        }
    }
}

void LocalGameScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();
    const Board& shown = hud_.navigator().board();

    if (!hud_.navigator().atEnd() && !annotations_.empty())
        annotations_.clear();

    boardView_.drawSquares(window);
    syncViewHighlights();
    boardView_.drawHighlights(window, hl_, shown);
    boardView_.drawLabels(window, font);
    boardView_.drawPieces(window, font, shown, app_);
    boardView_.drawAnnotations(window, annotations_.arrows(), annotations_.circles());

    if (drag_.isDragging() && dragFrom_) {
        Piece dragged = hud_.navigator().finalBoard().pieceAt(
            squareOf(dragFrom_->first, dragFrom_->second));
        if (!dragged.isNone() && dragged.color == myColor_)
            boardView_.drawDraggedPiece(window, font, dragged, cursor_, app_);
    }

    if (promo_) {
        for (int i = 0; i < static_cast<int>(promo_->candidates.size()); ++i) {
            auto c = promoCell(i);
            sf::RectangleShape cell({c.size, c.size});
            cell.setPosition(c.pos);
            cell.setFillColor(sf::Color(40, 38, 35, 220));
            cell.setOutlineColor(sf::Color(180, 180, 180, 180));
            cell.setOutlineThickness(1.f);
            window.draw(cell);

            if (i == promoHover_) {
                sf::RectangleShape hover({c.size, c.size});
                hover.setPosition(c.pos);
                hover.setFillColor(sf::Color(255, 255, 255, 30));
                window.draw(hover);
            }

            PieceType pt = promo_->candidates[i].promotion;
            float pieceSize = c.size * 0.8f;
            float offset = (c.size - pieceSize) / 2.f;

            if (app_.piecesLoaded()) {
                const auto& tex = app_.pieceTexture(myColor_, pt);
                sf::Sprite sprite(tex);
                float scale = pieceSize / static_cast<float>(tex.getSize().x);
                sprite.setScale({scale, scale});
                sprite.setPosition({c.pos.x + offset, c.pos.y + offset});
                window.draw(sprite);
            } else {
                const char letters[] = { 'P', 'N', 'B', 'R', 'Q', 'K' };
                unsigned int letterSize = static_cast<unsigned int>(c.size * 0.5f);
                if (letterSize < 12) letterSize = 12;
                sf::Text letter(font, std::string(1, letters[static_cast<int>(pt)]), letterSize);
                letter.setFillColor(sf::Color(240, 240, 240));
                auto lb = letter.getLocalBounds();
                letter.setPosition({
                    c.pos.x + (c.size - lb.size.x) / 2.f - lb.position.x,
                    c.pos.y + (c.size - lb.size.y) / 2.f - lb.position.y
                });
                window.draw(letter);
            }
        }

        autoQueenCheck_.setLabel("Auto-queen");
        autoQueenCheck_.setChecked(app_.autoQueen());
        autoQueenCheck_.setRect(autoQueenRect());
        autoQueenCheck_.draw(window, font);
    }

    hud_.draw(window, font);

    if (!gameOver_)
        backBtn_.draw(window, font);
}

} // namespace chess::client
