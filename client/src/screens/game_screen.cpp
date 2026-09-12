#include "game_screen.h"
#include "game_over_screen.h"
#include "ui_helpers.h"
#include "widgets/layout.h"

#include <chess/movegen.h>
#include <chess/san.h>
#include <chess/net/messages.h>

namespace chess::client {

namespace {
constexpr float BtnH = 30.f;
constexpr float InputH = 28.f;
constexpr std::size_t MaxChatLog = 50;
constexpr std::size_t MaxChatInput = 200;
}

GameScreen::GameScreen(App& app, Color myColor, const std::string& opponentName)
    : app_(app)
    , myColor_(myColor)
    , opponentName_(opponentName)
    , boardView_(static_cast<float>(App::WindowWidth),
                 static_cast<float>(App::WindowHeight),
                 myColor)
    , hud_(app_, boardView_.panelX(),
           static_cast<float>(App::WindowWidth) - boardView_.panelX() - 8.f)
    , myTurn_(myColor == Color::White)
{
    hud_.setGame(Board::fromStartPos(), {}, {});
    hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);

    chatInput_.setMaxLength(MaxChatInput);
    chatInput_.setPlaceholder("Type a message...");
    chatInput_.setOnCommit([this] {
        sendChat();
        chatInput_.setFocused(false);
    });

    autoQueenCheck_.setOnToggle([this](bool checked) {
        app_.setAutoQueen(checked);
    });

    chatLabel_.setText("Chat:");
    chatLabel_.setFontSize(14);
    chatLabel_.setColor(sf::Color(160, 160, 160));

    drawOfferLabel_.setText("Draw offered:");
    drawOfferLabel_.setFontSize(14);
    drawOfferLabel_.setColor(sf::Color(255, 200, 60));

    resignBtn_.setLabel("Resign");
    resignBtn_.setColors(sf::Color(160, 55, 55), sf::Color(185, 65, 65),
                         sf::Color(135, 45, 45), sf::Color(70, 45, 45),
                         sf::Color(140, 90, 90), sf::Color(240, 240, 240),
                         sf::Color(90, 60, 60), sf::Color(210, 120, 120),
                         sf::Color(180, 140, 140));
    resignBtn_.setOnClick([this] {
        if (!gameOver_) app_.connection().send(chess::net::ResignMsg{});
    });

    offerDrawBtn_.setLabel("Offer Draw");
    offerDrawBtn_.setColors(sf::Color(70, 70, 95), sf::Color(85, 85, 115),
                            sf::Color(58, 58, 78), sf::Color(45, 45, 58),
                            sf::Color(105, 105, 120), sf::Color(230, 230, 235),
                            sf::Color(80, 80, 100), sf::Color(150, 150, 175),
                            sf::Color(180, 180, 205));
    offerDrawBtn_.setOnClick([this] {
        if (gameOver_) return;
        app_.connection().send(chess::net::DrawOfferMsg{});
        chatLog_.push_back("Draw offer sent");
        if (chatLog_.size() > MaxChatLog)
            chatLog_.erase(chatLog_.begin());
    });

    declineBtn_.setLabel("Decline");
    declineBtn_.setColors(sf::Color(160, 55, 55), sf::Color(185, 65, 65),
                          sf::Color(135, 45, 45), sf::Color(70, 45, 45),
                          sf::Color(140, 90, 90), sf::Color(240, 240, 240),
                          sf::Color(90, 60, 60), sf::Color(210, 120, 120),
                          sf::Color(180, 140, 140));
    declineBtn_.setOnClick([this] {
        app_.connection().send(chess::net::DrawDeclineMsg{});
        drawOfferPending_ = false;
        chatLog_.push_back("Draw declined");
        if (chatLog_.size() > MaxChatLog)
            chatLog_.erase(chatLog_.begin());
    });

    acceptBtn_.setLabel("Accept");
    acceptBtn_.setColors(sf::Color(60, 140, 60), sf::Color(75, 165, 75),
                         sf::Color(48, 115, 48), sf::Color(45, 80, 45),
                         sf::Color(110, 140, 110), sf::Color(240, 240, 240),
                         sf::Color(70, 100, 70), sf::Color(140, 200, 140),
                         sf::Color(180, 220, 180));
    acceptBtn_.setOnClick([this] {
        app_.connection().send(chess::net::DrawAcceptMsg{});
        drawOfferPending_ = false;
    });

    layoutPanel();
}

void GameScreen::layoutPanel()
{
    float px = boardView_.panelX();

    resignBtn_.setRect(sf::FloatRect(sf::Vector2f(px, hud_.contentBottom()),
                                     sf::Vector2f(140.f, BtnH)));
    offerDrawBtn_.setRect(sf::FloatRect(sf::Vector2f(px + 148.f, hud_.contentBottom()),
                                        sf::Vector2f(140.f, BtnH)));

    declineBtn_.setRect(sf::FloatRect(sf::Vector2f(px, hud_.contentBottom()),
                                      sf::Vector2f(90.f, BtnH)));
    acceptBtn_.setRect(sf::FloatRect(sf::Vector2f(px + 98.f, hud_.contentBottom()),
                                     sf::Vector2f(90.f, BtnH)));

    float btnY = hud_.contentBottom();
    float chatSepY = btnY + BtnH + 8.f;
    chatLabel_.setPosition({px, chatSepY + 6.f});

    float logTop = chatSepY + 24.f;
    float inputY = static_cast<float>(App::WindowHeight) - 20.f - InputH;
    float logBottom = inputY - 8.f;
    chatLogBg_.setRect(sf::FloatRect(
        sf::Vector2f(px, logTop),
        sf::Vector2f(static_cast<float>(App::WindowWidth) - px - 8.f, logBottom - logTop)));
    chatLogBg_.setFill(sf::Color(30, 30, 30));
    chatLogBg_.setOutline(sf::Color(70, 70, 70));

    chatInput_.setRect(sf::FloatRect(
        sf::Vector2f(px, inputY),
        sf::Vector2f(static_cast<float>(App::WindowWidth) - px - 8.f, InputH)));
}

void GameScreen::selectPiece(int file, int rank)
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

void GameScreen::trySendMove(int targetFile, int targetRank)
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
            commitMove(*qm);
            return;
        }
        buildPromotion(hl_.selectedSquare->first, hl_.selectedSquare->second,
                       targetFile, targetRank);
        return;
    }

    commitMove(*found);
}

void GameScreen::buildPromotion(int fromFile, int fromRank, int toFile, int toRank)
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

void GameScreen::deselect()
{
    hl_.selectedSquare.reset();
    hl_.legalMoveTargets.clear();
}

void GameScreen::syncViewHighlights()
{
    const Board& shown = hud_.navigator().board();
    hl_.lastMoveFrom = hud_.navigator().lastMoveFrom();
    hl_.lastMoveTo = hud_.navigator().lastMoveTo();
    Color stm = shown.sideToMove();
    hl_.checkSquare = chess::inCheck(shown, stm)
        ? findKingSquare(shown, stm)
        : std::optional<std::pair<int, int>>{};
}

void GameScreen::commitMove(const chess::Move& m)
{
    std::string san = chess::san::toSan(hud_.navigator().finalBoard(), m);
    app_.connection().send(chess::net::MoveMsg{san});
    myTurn_ = false;
    hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
    deselect();
}

void GameScreen::sendPromotionMove(chess::PieceType type)
{
    if (!promo_) return;
    for (const auto& m : promo_->candidates) {
        if (m.promotion == type) {
            promo_.reset();
            promoHover_ = -1;
            commitMove(m);
            return;
        }
    }
    promo_.reset();
    promoHover_ = -1;
    deselect();
}

void GameScreen::cancelPromotion()
{
    promo_.reset();
    promoHover_ = -1;
    deselect();
}

void GameScreen::clearBoardInput()
{
    drag_.cancel();
    dragFrom_.reset();
    rightPress_.reset();
}

bool GameScreen::ownPieceAt(int file, int rank)
{
    Piece piece = hud_.navigator().finalBoard().pieceAt(squareOf(file, rank));
    return !piece.isNone() && piece.color == myColor_;
}

sf::FloatRect GameScreen::autoQueenRect() const
{
    auto cell = promoCell(static_cast<int>(promo_->candidates.size()) - 1);
    float sq = boardView_.squareSize();
    return { { cell.pos.x, cell.pos.y + cell.size + 3.f },
             { sq * 1.6f, 22.f } };
}

std::optional<chess::PieceType> GameScreen::promoTypeForKey(sf::Keyboard::Key key) const
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

PromoCell GameScreen::promoCell(int index) const
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

void GameScreen::sendChat()
{
    if (chatInput_.text().empty()) return;
    app_.connection().send(chess::net::ChatMsg{chatInput_.text()});
    chatLog_.push_back("You: " + chatInput_.text());
    if (chatLog_.size() > MaxChatLog)
        chatLog_.erase(chatLog_.begin());
    chatInput_.setText("");
}

void GameScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (chatInput_.isFocused()) {
            if (kp->code == sf::Keyboard::Key::Escape) {
                chatInput_.setFocused(false);
            }
            return;
        }

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
                sendPromotionMove(*t);
                return;
            }
        }

        if (hud_.navigator().handleEvent(event, {0.f, 0.f})) {
            clearBoardInput();
            return;
        }
        return;
    }

    if (const auto* te = event.getIf<sf::Event::TextEntered>()) {
        if (chatInput_.handleEvent(*te, {0.f, 0.f})) return;
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

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            if (chatInput_.handleEvent(*mb, local)) return;
        }
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

    if (!gameOver_) {
        if (drawOfferPending_) {
            if (declineBtn_.handleEvent(event, local)) return;
            if (acceptBtn_.handleEvent(event, local)) return;
        } else {
            if (resignBtn_.handleEvent(event, local)) return;
            if (offerDrawBtn_.handleEvent(event, local)) return;
        }
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            if (promo_) {
                autoQueenCheck_.setRect(autoQueenRect());
                if (autoQueenCheck_.handleEvent(*mb, local)) return;
                for (int i = 0; i < static_cast<int>(promo_->candidates.size()); ++i) {
                    auto cell = promoCell(i);
                    if (sf::FloatRect(cell.pos, {cell.size, cell.size}).contains(local)) {
                        sendPromotionMove(promo_->candidates[i].promotion);
                        return;
                    }
                }
                cancelPromotion();
                return;
            }

            annotations_.clear();
            if (gameOver_ || !myTurn_) return;

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
                        trySendMove(square->first, square->second);
                    }
                } else if (hl_.selectedSquare) {
                    if (square && *square == *hl_.selectedSquare) {
                        deselect();
                    } else if (square && ownPieceAt(square->first, square->second)) {
                        selectPiece(square->first, square->second);
                    } else if (square) {
                        trySendMove(square->first, square->second);
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

void GameScreen::update(float dtSec)
{
    hud_.update(dtSec);
    chatInput_.update(dtSec);

    app_.connection().poll();

    while (app_.connection().hasMessages()) {
        auto msg = app_.connection().nextMessage();

        if (auto* move = std::get_if<chess::net::ServerMoveMsg>(&msg)) {
            auto parsed = chess::san::fromSan(hud_.navigator().finalBoard(), move->san);
            if (parsed) {
                hud_.navigator().appendMove(*parsed, move->san);
                hl_.selectedSquare.reset();
                hl_.legalMoveTargets.clear();
                myTurn_ = true;
                hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
            }
        }
        else if (auto* gameOver = std::get_if<chess::net::GameOverMsg>(&msg)) {
            gameOver_ = true;
            drawOfferPending_ = false;
            hud_.setGameOver(true);
            hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
            app_.switchScreen(std::make_unique<GameOverScreen>(
                app_, gameOver->result, gameOver->reason,
                hud_.navigator().initialBoard(), hud_.navigator().moves(),
                hud_.navigator().sans()));
            return;
        }
        else if (auto* drawOffer = std::get_if<chess::net::ServerDrawOfferMsg>(&msg)) {
            (void)drawOffer;
            drawOfferPending_ = true;
            hud_.setStatus("Opponent offers a draw", 3.0f);
            chatLog_.push_back("Draw offer received");
            if (chatLog_.size() > MaxChatLog)
                chatLog_.erase(chatLog_.begin());
        }
        else if (std::holds_alternative<chess::net::ServerDrawDeclineMsg>(msg)) {
            drawOfferPending_ = false;
            hud_.setStatus("Draw offer declined", 2.0f);
            chatLog_.push_back("Draw offer declined");
            if (chatLog_.size() > MaxChatLog)
                chatLog_.erase(chatLog_.begin());
        }
        else if (auto* chat = std::get_if<chess::net::ServerChatMsg>(&msg)) {
            chatLog_.push_back(chat->name + ": " + chat->text);
            if (chatLog_.size() > MaxChatLog)
                chatLog_.erase(chatLog_.begin());
        }
        else if (auto* err = std::get_if<chess::net::ErrorMsg>(&msg)) {
            hud_.setStatus(err->message, 2.0f);
            if (err->message == "Illegal move" || err->message == "Not your turn") {
                myTurn_ = true;
                hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
            }
        }
    }

    if (app_.connection().state() == ConnectionState::Disconnected) {
        gameOver_ = true;
        app_.switchScreen(std::make_unique<GameOverScreen>(
            app_, net::GameResult::Abort, net::GameOverReason::Disconnection,
            hud_.navigator().initialBoard(), hud_.navigator().moves(),
            hud_.navigator().sans()));
    }
}

void GameScreen::drawButtons(sf::RenderWindow& window)
{
    auto& font = app_.font();

    if (drawOfferPending_) {
        drawOfferLabel_.setPosition({boardView_.panelX(), hud_.contentBottom() - 20.f});
        drawOfferLabel_.draw(window, font);
        declineBtn_.draw(window, font);
        acceptBtn_.draw(window, font);
    } else if (!gameOver_) {
        resignBtn_.draw(window, font);
        offerDrawBtn_.draw(window, font);
    }
}

void GameScreen::drawChat(sf::RenderWindow& window)
{
    float px = boardView_.panelX();
    auto& font = app_.font();
    float panelW = static_cast<float>(App::WindowWidth) - px - 8.f;
    float btnY = hud_.contentBottom();
    float chatSepY = btnY + BtnH + 8.f;

    sf::RectangleShape sep({panelW, 1.f});
    sep.setPosition({px, chatSepY});
    sep.setFillColor(sf::Color(80, 80, 80));
    window.draw(sep);

    chatLabel_.draw(window, font);

    float logTop = chatSepY + 24.f;
    float inputY = static_cast<float>(App::WindowHeight) - 20.f - InputH;
    float logBottom = inputY - 8.f;

    chatLogBg_.draw(window);

    float lineH = 17.f;
    int maxLines = logBottom > logTop
        ? static_cast<int>((logBottom - logTop) / lineH) : 0;

    int start = static_cast<int>(chatLog_.size()) - maxLines;
    if (start < 0) start = 0;

    float y = logTop + 2.f;
    for (int i = start; i < static_cast<int>(chatLog_.size()); ++i) {
        if (y + lineH > logBottom) break;
        sf::Text line(font, chatLog_[i], 13);
        line.setFillColor(sf::Color(200, 200, 200));
        line.setPosition({px + 4.f, y});

        auto lb = line.getLocalBounds();
        if (lb.size.x > panelW - 8.f) {
            line.setString(sf::String(
                safeTruncate(chatLog_[i],
                    static_cast<std::size_t>((panelW - 8.f) / 6.f))));
        }
        window.draw(line);
        y += lineH;
    }

    chatInput_.draw(window, font);
}

void GameScreen::draw(sf::RenderWindow& window)
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
    drawButtons(window);
    drawChat(window);
}

} // namespace chess::client