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
    for (const auto& m : moves) {
        if (m.from == from && m.to == to) {
            found = &m;
            if (m.isPromotion()) break;
        }
    }

    if (!found) {
        hud_.setStatus("Illegal move", 2.0f);
        deselect();
        return;
    }

    if (found->isPromotion()) {
        PromotionState ps;
        ps.fromFile = hl_.selectedSquare->first;
        ps.fromRank = hl_.selectedSquare->second;
        ps.toFile = targetFile;
        ps.toRank = targetRank;
        for (const auto& m : moves) {
            if (m.from == from && m.to == to && m.isPromotion())
                ps.candidates.push_back(m);
        }
        promo_ = std::move(ps);
        return;
    }

    std::string san = chess::san::toSan(live, *found);
    hud_.navigator().appendMove(*found, san);
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
    const Board& live = hud_.navigator().finalBoard();
    for (const auto& m : promo_->candidates) {
        if (m.promotion == type) {
            std::string san = chess::san::toSan(live, m);
            hud_.navigator().appendMove(m, san);
            promo_.reset();
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
            return;
        }
    }
    promo_.reset();
    deselect();
}

void LocalGameScreen::cancelPromotion()
{
    promo_.reset();
    deselect();
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
            deselect();
            return;
        }
        if (kp->code == sf::Keyboard::Key::Space) {
            if (promo_) { cancelPromotion(); return; }
            deselect();
            return;
        }
        if (hud_.navigator().handleEvent(event, {0.f, 0.f})) return;
        return;
    }

    sf::Vector2f local(0.f, 0.f);
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>())
        local = app_.toLocal(mm->position);
    else if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>())
        local = app_.toLocal(mb->position);
    else if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>())
        local = app_.toLocal(rb->position);

    if (const auto* we = event.getIf<sf::Event::MouseWheelScrolled>()) {
        float px = boardView_.panelX();
        if (app_.toLocal(we->position).x >= px) {
            hud_.handleScroll(we->delta);
            return;
        }
    }

    if (hud_.navigator().handleEvent(event, local)) return;

    if (!gameOver_ && backBtn_.handleEvent(event, local)) return;

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return;

        if (gameOver_ || !myTurn_ || engineThinking_ || engineFailed_) return;

        if (!hud_.navigator().atEnd()) {
            hud_.navigator().goEnd();
            deselect();
        }

        if (promo_) {
            for (int i = 0; i < static_cast<int>(promo_->candidates.size()); ++i) {
                auto cell = promoCell(i);
                sf::FloatRect rect(cell.pos, {cell.size, cell.size});
                if (rect.contains(local)) {
                    applyPromotionMove(promo_->candidates[i].promotion);
                    return;
                }
            }
            cancelPromotion();
            return;
        }

        auto square = boardView_.pixelToSquare(local);
        if (!square) return;

        auto [file, rank] = *square;
        Piece piece = hud_.navigator().finalBoard().pieceAt(squareOf(file, rank));

        if (hl_.selectedSquare) {
            if (file == hl_.selectedSquare->first &&
                rank == hl_.selectedSquare->second) {
                deselect();
            } else if (!piece.isNone() && piece.color == myColor_) {
                selectPiece(file, rank);
            } else {
                tryMove(file, rank);
            }
        } else {
            if (!piece.isNone() && piece.color == myColor_) {
                selectPiece(file, rank);
            }
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

    boardView_.drawSquares(window);
    syncViewHighlights();
    boardView_.drawHighlights(window, hl_, shown);
    boardView_.drawLabels(window, font);
    boardView_.drawPieces(window, font, shown, app_);

    if (promo_) {
        for (int i = 0; i < static_cast<int>(promo_->candidates.size()); ++i) {
            auto c = promoCell(i);
            sf::RectangleShape cell({c.size, c.size});
            cell.setPosition(c.pos);
            cell.setFillColor(sf::Color(40, 38, 35, 220));
            cell.setOutlineColor(sf::Color(180, 180, 180, 180));
            cell.setOutlineThickness(1.f);
            window.draw(cell);

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
                auto lb = letter.getGlobalBounds();
                letter.setPosition({
                    c.pos.x + (c.size - lb.size.x) / 2.f - lb.position.x,
                    c.pos.y + (c.size - lb.size.y) / 2.f - lb.position.y
                });
                window.draw(letter);
            }
        }
    }

    hud_.draw(window, font);

    if (!gameOver_)
        backBtn_.draw(window, font);
}

} // namespace chess::client
