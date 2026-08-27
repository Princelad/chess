#include "local_game_screen.h"
#include "connect_screen.h"
#include "game_over_screen.h"

#include <chess/fen.h>
#include <chess/movegen.h>
#include <chess/san.h>

namespace chess::client {

namespace {
std::pair<int, int> findKingSquare(const Board& board, Color color)
{
    Piece king = Piece::of(color, PieceType::King);
    for (int file = 0; file < 8; ++file)
        for (int rank = 0; rank < 8; ++rank)
            if (board.pieceAt(squareOf(file, rank)) == king)
                return { file, rank };
    return { 4, color == Color::White ? 0 : 7 };
}

constexpr float BtnH = 30.f;

void drawBtn(sf::RenderWindow& window, float x, float y, float w, float h,
             sf::Color fill, const sf::Font& font, const std::string& label)
{
    sf::RectangleShape rect({w, h});
    rect.setPosition({x, y});
    rect.setFillColor(fill);
    rect.setOutlineColor(sf::Color(100, 100, 100));
    rect.setOutlineThickness(1.f);
    window.draw(rect);

    sf::Text txt(font, label, 14);
    txt.setFillColor(sf::Color(240, 240, 240));
    auto lb = txt.getGlobalBounds();
    txt.setPosition({x + (w - lb.size.x) / 2.f - lb.position.x,
                     y + (h - lb.size.y) / 2.f - lb.position.y});
    window.draw(txt);
}

net::GameOverReason detectDrawReason(const Board& board)
{
    if (board.halfmoveClock() >= 100)
        return net::GameOverReason::FiftyMove;
    if (chess::threefoldRepetition(board))
        return net::GameOverReason::Repetition;
    if (chess::insufficientMaterial(board))
        return net::GameOverReason::InsufficientMaterial;
    return net::GameOverReason::Repetition;
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
    , board_(Board::fromStartPos())
    , myColor_(myColor)
    , boardView_(static_cast<float>(App::WindowWidth),
                 static_cast<float>(App::WindowHeight),
                 myColor)
    , hud_(boardView_.panelX(),
           static_cast<float>(App::WindowWidth) - boardView_.panelX() - 8.f)
    , myTurn_(myColor == Color::White)
    , engineDepth_(depth)
    , initialBoard_(Board::fromStartPos())
{
    inCheck_ = chess::inCheck(board_, myColor_);
    hud_.setInfo("Computer", myColor_, myTurn_, gameOver_);

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
    auto moves = chess::generateLegalMoves(board_);
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
    Square from = squareOf(hl_.selectedSquare->first, hl_.selectedSquare->second);
    Square to = squareOf(targetFile, targetRank);

    auto moves = chess::generateLegalMoves(board_);
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

    std::string san = chess::san::toSan(board_, *found);
    moves_.push_back(*found);
    sanMoves_.push_back(san);
    board_.makeMove(*found);
    hl_.lastMoveFrom = { static_cast<int>(chess::fileOf(found->from)),
                         static_cast<int>(chess::rankOf(found->from)) };
    hl_.lastMoveTo = { static_cast<int>(chess::fileOf(found->to)),
                       static_cast<int>(chess::rankOf(found->to)) };
    deselect();
    hud_.addMove(san);

    inCheck_ = chess::inCheck(board_, opposite(myColor_));
    hl_.checkSquare = inCheck_
        ? findKingSquare(board_, opposite(myColor_))
        : std::optional<std::pair<int,int>>{};

    auto state = chess::evaluateGameState(board_);
    if (state != GameState::Ongoing) {
        gameOver_ = true;
        myTurn_ = false;
        hud_.setGameOver(true);
        hud_.setInfo("Computer", myColor_, myTurn_, gameOver_);
        app_.switchScreen(std::make_unique<GameOverScreen>(
            app_, toResult(state, board_.sideToMove()),
            toReason(state, board_),
            initialBoard_, moves_, sanMoves_));
        return;
    }

    myTurn_ = false;
    engineThinking_ = true;
    hud_.setStatus("Computer thinking...", 999.f);
    engine_->position(board_.toFen());
    engine_->go(engineDepth_);
    hud_.setInfo("Computer", myColor_, myTurn_, gameOver_);
}

void LocalGameScreen::deselect()
{
    hl_.selectedSquare.reset();
    hl_.legalMoveTargets.clear();
}

void LocalGameScreen::applyPromotionMove(chess::PieceType type)
{
    if (!promo_) return;
    for (const auto& m : promo_->candidates) {
        if (m.promotion == type) {
            std::string san = chess::san::toSan(board_, m);
            moves_.push_back(m);
            sanMoves_.push_back(san);
            board_.makeMove(m);
            hl_.lastMoveFrom = { static_cast<int>(chess::fileOf(m.from)),
                                 static_cast<int>(chess::rankOf(m.from)) };
            hl_.lastMoveTo = { static_cast<int>(chess::fileOf(m.to)),
                               static_cast<int>(chess::rankOf(m.to)) };
            hud_.addMove(san);
            promo_.reset();
            deselect();

            inCheck_ = chess::inCheck(board_, opposite(myColor_));
            hl_.checkSquare = inCheck_
                ? findKingSquare(board_, opposite(myColor_))
                : std::optional<std::pair<int,int>>{};

            auto state = chess::evaluateGameState(board_);
            if (state != GameState::Ongoing) {
                gameOver_ = true;
                myTurn_ = false;
                hud_.setGameOver(true);
                hud_.setInfo("Computer", myColor_, myTurn_, gameOver_);
                app_.switchScreen(std::make_unique<GameOverScreen>(
                    app_, toResult(state, board_.sideToMove()),
                    toReason(state, board_),
                    initialBoard_, moves_, sanMoves_));
                return;
            }

            myTurn_ = false;
            engineThinking_ = true;
            hud_.setStatus("Computer thinking...", 999.f);
            engine_->position(board_.toFen());
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
    auto move = engine_->tryGetBestMove(board_);
    if (!move) return false;

    std::string san = chess::san::toSan(board_, *move);
    moves_.push_back(*move);
    sanMoves_.push_back(san);
    board_.makeMove(*move);
    hl_.lastMoveFrom = { static_cast<int>(chess::fileOf(move->from)),
                         static_cast<int>(chess::rankOf(move->from)) };
    hl_.lastMoveTo = { static_cast<int>(chess::fileOf(move->to)),
                       static_cast<int>(chess::rankOf(move->to)) };
    hl_.selectedSquare.reset();
    hl_.legalMoveTargets.clear();
    hud_.addMove(san);

    inCheck_ = chess::inCheck(board_, myColor_);
    hl_.checkSquare = inCheck_
        ? findKingSquare(board_, myColor_)
        : std::optional<std::pair<int,int>>{};

    engineThinking_ = false;
    hud_.setStatus("", 0.f);
    myTurn_ = true;
    hud_.setInfo("Computer", myColor_, myTurn_, gameOver_);
    return true;
}

void LocalGameScreen::checkGameOver()
{
    auto state = chess::evaluateGameState(board_);
    if (state == GameState::Ongoing) return;

    gameOver_ = true;
    myTurn_ = false;
    hud_.setGameOver(true);
    hud_.setInfo("Computer", myColor_, myTurn_, gameOver_);

    app_.switchScreen(std::make_unique<GameOverScreen>(
        app_, toResult(state, board_.sideToMove()),
        toReason(state, board_),
        initialBoard_, moves_, sanMoves_));
}

void LocalGameScreen::returnToConnect()
{
    if (engine_) engine_->quit();
    app_.switchScreen(std::make_unique<ConnectScreen>(app_));
}

LocalGameScreen::PromoCell LocalGameScreen::promoCell(int index) const
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
        return;
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return;
        int mx = mb->position.x;
        int my = mb->position.y;

        if (!gameOver_) {
            float px = boardView_.panelX();
            float btnY = hud_.contentBottom();
            auto inRect = [mx, my](float x, float y, float w, float h) {
                return mx >= x && mx < x + w && my >= y && my < y + h;
            };
            if (inRect(px, btnY, 140.f, BtnH)) {
                returnToConnect();
                return;
            }
        }

        if (gameOver_ || !myTurn_ || engineThinking_ || engineFailed_) return;

        if (promo_) {
            auto pos = static_cast<sf::Vector2f>(mb->position);
            for (int i = 0; i < static_cast<int>(promo_->candidates.size()); ++i) {
                auto cell = promoCell(i);
                sf::FloatRect rect(cell.pos, {cell.size, cell.size});
                if (rect.contains(pos)) {
                    applyPromotionMove(promo_->candidates[i].promotion);
                    return;
                }
            }
            cancelPromotion();
            return;
        }

        auto square = boardView_.pixelToSquare(
            static_cast<sf::Vector2f>(mb->position));
        if (!square) return;

        auto [file, rank] = *square;
        Piece piece = board_.pieceAt(squareOf(file, rank));

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

    boardView_.drawSquares(window);
    boardView_.drawHighlights(window, hl_, board_);
    boardView_.drawLabels(window, font);
    boardView_.drawPieces(window, font, board_, app_);

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

    float px = boardView_.panelX();
    float btnY = hud_.contentBottom();
    drawBtn(window, px, btnY, 140.f, BtnH, sf::Color(180, 60, 60), font, "Back to Menu");
}

} // namespace chess::client
