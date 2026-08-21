#include "game_screen.h"
#include "game_over_screen.h"

#include <chess/movegen.h>
#include <chess/san.h>
#include <chess/net/messages.h>

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
constexpr float BtnGap = 8.f;
constexpr float ChatLogMaxH = 300.f;
constexpr float InputH = 28.f;
constexpr std::size_t MaxChatLog = 50;

sf::RectangleShape makeBtn(float x, float y, float w, float h,
                           sf::Color fill, const sf::Font& font,
                           const std::string& label)
{
    sf::RectangleShape rect({w, h});
    rect.setPosition({x, y});
    rect.setFillColor(fill);
    rect.setOutlineColor(sf::Color(100, 100, 100));
    rect.setOutlineThickness(1.f);

    sf::Text txt(font, label, 14);
    txt.setFillColor(sf::Color(240, 240, 240));
    auto lb = txt.getGlobalBounds();
    txt.setPosition({x + (w - lb.size.x) / 2.f, y + (h - lb.size.y) / 2.f});

    return rect;
}
}

GameScreen::GameScreen(App& app, Color myColor, const std::string& opponentName)
    : app_(app)
    , board_(Board::fromStartPos())
    , myColor_(myColor)
    , opponentName_(opponentName)
    , boardView_(static_cast<float>(App::WindowWidth),
                 static_cast<float>(App::WindowHeight),
                 myColor)
    , myTurn_(myColor == Color::White)
{
    inCheck_ = chess::inCheck(board_, myColor_);
}

void GameScreen::selectPiece(int file, int rank)
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

void GameScreen::trySendMove(int targetFile, int targetRank)
{
    Square from = squareOf(hl_.selectedSquare->first, hl_.selectedSquare->second);
    Square to = squareOf(targetFile, targetRank);

    auto moves = chess::generateLegalMoves(board_);
    const chess::Move* found = nullptr;
    for (const auto& m : moves) {
        if (m.from == from && m.to == to) {
            if (m.isPromotion()) {
                found = &m;
                break;
            }
            found = &m;
            break;
        }
    }

    if (!found) {
        statusMsg_ = "Illegal move";
        statusTimer_ = 2.0f;
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
    app_.connection().send(chess::net::MoveMsg{san});
    myTurn_ = false;
    deselect();
}

void GameScreen::deselect()
{
    hl_.selectedSquare.reset();
    hl_.legalMoveTargets.clear();
}

void GameScreen::sendPromotionMove(chess::PieceType type)
{
    if (!promo_) return;
    for (const auto& m : promo_->candidates) {
        if (m.promotion == type) {
            std::string san = chess::san::toSan(board_, m);
            app_.connection().send(chess::net::MoveMsg{san});
            myTurn_ = false;
            promo_.reset();
            deselect();
            return;
        }
    }
    promo_.reset();
    deselect();
}

void GameScreen::cancelPromotion()
{
    promo_.reset();
    deselect();
}

void GameScreen::sendChat()
{
    if (chatInput_.empty()) return;
    app_.connection().send(chess::net::ChatMsg{chatInput_});
    chatLog_.push_back("You: " + chatInput_);
    if (chatLog_.size() > MaxChatLog)
        chatLog_.erase(chatLog_.begin());
    chatInput_.clear();
}

void GameScreen::handleButtonClick(int mx, int my)
{
    float px = boardView_.panelX();

    auto inRect = [mx, my](float x, float y, float w, float h) {
        return mx >= x && mx < x + w && my >= y && my < y + h;
    };

    if (drawOfferPending_) {
        if (inRect(px, 195.f, 90.f, BtnH)) {
            app_.connection().send(chess::net::DrawDeclineMsg{});
            drawOfferPending_ = false;
            chatLog_.push_back("Draw declined");
            if (chatLog_.size() > MaxChatLog)
                chatLog_.erase(chatLog_.begin());
            return;
        }
        if (inRect(px + 98.f, 195.f, 90.f, BtnH)) {
            app_.connection().send(chess::net::DrawAcceptMsg{});
            drawOfferPending_ = false;
            return;
        }
    } else {
        if (inRect(px, 195.f, 140.f, BtnH)) {
            app_.connection().send(chess::net::ResignMsg{});
            return;
        }
        if (inRect(px + 148.f, 195.f, 140.f, BtnH)) {
            app_.connection().send(chess::net::DrawOfferMsg{});
            chatLog_.push_back("Draw offer sent");
            if (chatLog_.size() > MaxChatLog)
                chatLog_.erase(chatLog_.begin());
            return;
        }
    }
}

void GameScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (chatFocused_) {
            if (kp->code == sf::Keyboard::Key::Enter) {
                sendChat();
                chatFocused_ = false;
                return;
            }
            if (kp->code == sf::Keyboard::Key::Escape) {
                chatFocused_ = false;
                return;
            }
            return;
        }

        if (kp->code == sf::Keyboard::Key::Escape) {
            if (promo_) { cancelPromotion(); return; }
            app_.connection().send(chess::net::ResignMsg{});
            app_.connection().disconnect();
            return;
        }
        if (kp->code == sf::Keyboard::Key::Space) {
            if (promo_) { cancelPromotion(); return; }
            deselect();
            return;
        }
        return;
    }

    if (const auto* te = event.getIf<sf::Event::TextEntered>()) {
        if (chatFocused_) {
            auto ch = te->unicode;
            if (ch == '\b') {
                if (!chatInput_.empty())
                    chatInput_.pop_back();
            } else if (ch >= 32 && ch < 127) {
                chatInput_ += static_cast<char>(ch);
            }
            return;
        }
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return;
        int mx = mb->position.x;
        int my = mb->position.y;

        float px = boardView_.panelX();
        float inputY = static_cast<float>(App::WindowHeight) - 20.f - InputH;
        if (mx >= px && mx < px + 296.f && my >= inputY && my < inputY + InputH) {
            chatFocused_ = !chatFocused_;
            return;
        }

        if (chatFocused_) {
            chatFocused_ = false;
            return;
        }

        if (!gameOver_) {
            handleButtonClick(mx, my);
        }

        if (gameOver_ || !myTurn_) return;

        if (promo_) {
            auto pos = static_cast<sf::Vector2f>(mb->position);
            float sq = boardView_.squareSize();

            bool flipped = boardView_.isFlipped();
            int promoCol = flipped ? (7 - promo_->toFile) : promo_->toFile;
            int promoRowStart = flipped ? promo_->toRank : (7 - promo_->toRank);

            sf::Vector2f origin = boardView_.boardOrigin();
            float ppx = origin.x + static_cast<float>(promoCol) * sq;
            float ppy = origin.y + static_cast<float>(promoRowStart) * sq;

            for (int i = 0; i < static_cast<int>(promo_->candidates.size()); ++i) {
                sf::FloatRect cell({ppx, ppy + i * sq}, {sq, sq});
                if (cell.contains(pos)) {
                    sendPromotionMove(promo_->candidates[i].promotion);
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
                trySendMove(file, rank);
            }
        } else {
            if (!piece.isNone() && piece.color == myColor_) {
                selectPiece(file, rank);
            }
        }
    }
}

void GameScreen::update(float dtSec)
{
    if (statusTimer_ > 0.f) {
        statusTimer_ -= dtSec;
        if (statusTimer_ <= 0.f) statusMsg_.clear();
    }

    app_.connection().poll();

    while (app_.connection().hasMessages()) {
        auto msg = app_.connection().nextMessage();

        if (auto* move = std::get_if<chess::net::ServerMoveMsg>(&msg)) {
            auto parsed = chess::san::fromSan(board_, move->san);
            if (parsed) {
                hl_.lastMoveFrom = std::make_pair(
                    static_cast<int>(chess::fileOf(parsed->from)),
                    static_cast<int>(chess::rankOf(parsed->from)));
                hl_.lastMoveTo = std::make_pair(
                    static_cast<int>(chess::fileOf(parsed->to)),
                    static_cast<int>(chess::rankOf(parsed->to)));
                hl_.selectedSquare.reset();
                hl_.legalMoveTargets.clear();

                board_.makeMove(*parsed);
                inCheck_ = chess::inCheck(board_, myColor_);
                hl_.checkSquare = inCheck_
                    ? findKingSquare(board_, myColor_)
                    : std::optional<std::pair<int,int>>{};
                myTurn_ = true;
            }
        }
        else if (auto* gameOver = std::get_if<chess::net::GameOverMsg>(&msg)) {
            gameOver_ = true;
            drawOfferPending_ = false;
            app_.switchScreen(std::make_unique<GameOverScreen>(
                app_, gameOver->result, gameOver->reason));
            return;
        }
        else if (auto* drawOffer = std::get_if<chess::net::ServerDrawOfferMsg>(&msg)) {
            (void)drawOffer;
            drawOfferPending_ = true;
            statusMsg_ = "Opponent offers a draw";
            statusTimer_ = 3.0f;
            chatLog_.push_back("Draw offer received");
            if (chatLog_.size() > MaxChatLog)
                chatLog_.erase(chatLog_.begin());
        }
        else if (auto* chat = std::get_if<chess::net::ServerChatMsg>(&msg)) {
            chatLog_.push_back(chat->name + ": " + chat->text);
            if (chatLog_.size() > MaxChatLog)
                chatLog_.erase(chatLog_.begin());
        }
        else if (auto* err = std::get_if<chess::net::ErrorMsg>(&msg)) {
            statusMsg_ = err->message;
            statusTimer_ = 2.0f;
            myTurn_ = true;
        }
    }

    if (app_.connection().state() == ConnectionState::Disconnected) {
        gameOver_ = true;
        app_.switchScreen(std::make_unique<GameOverScreen>(
            app_, net::GameResult::Abort, net::GameOverReason::Disconnection));
    }
}

void GameScreen::drawButtons(sf::RenderWindow& window)
{
    float px = boardView_.panelX();
    auto& font = app_.font();

    if (drawOfferPending_) {
        sf::RectangleShape declineBtn = makeBtn(
            px, 195.f, 90.f, BtnH, sf::Color(180, 60, 60), font, "Decline");
        sf::RectangleShape acceptBtn = makeBtn(
            px + 98.f, 195.f, 90.f, BtnH, sf::Color(60, 140, 60), font, "Accept");
        window.draw(declineBtn);
        window.draw(acceptBtn);

        sf::Text offerText(font, "Draw offered:", 14);
        offerText.setFillColor(sf::Color(255, 200, 60));
        offerText.setPosition({px, 175.f});
        window.draw(offerText);
    } else if (!gameOver_) {
        sf::RectangleShape resignBtn = makeBtn(
            px, 195.f, 140.f, BtnH, sf::Color(180, 60, 60), font, "Resign");
        sf::RectangleShape drawBtn = makeBtn(
            px + 148.f, 195.f, 140.f, BtnH, sf::Color(80, 80, 100), font, "Offer Draw");
        window.draw(resignBtn);
        window.draw(drawBtn);
    }
}

void GameScreen::drawChat(sf::RenderWindow& window)
{
    float px = boardView_.panelX();
    auto& font = app_.font();
    float panelW = 296.f;

    sf::Vertex sep[] = {
        {sf::Vector2f(px, 240.f), sf::Color(80, 80, 80)},
        {sf::Vector2f(px + panelW, 240.f), sf::Color(80, 80, 80)}
    };
    window.draw(sep, 2, sf::PrimitiveType::Lines);

    sf::Text chatLabel(font, "Chat:", 14);
    chatLabel.setFillColor(sf::Color(160, 160, 160));
    chatLabel.setPosition({px, 248.f});
    window.draw(chatLabel);

    float logTop = 270.f;
    float inputY = static_cast<float>(App::WindowHeight) - 20.f - InputH;
    float logBottom = inputY - 8.f;
    float logH = logBottom - logTop;

    if (logH > 0.f) {
        sf::RectangleShape logBg({panelW, logH});
        logBg.setPosition({px, logTop});
        logBg.setFillColor(sf::Color(30, 30, 30));
        window.draw(logBg);

        unsigned int lineSize = 13;
        float lineH = 17.f;
        int maxLines = static_cast<int>(logH / lineH);

        int start = static_cast<int>(chatLog_.size()) - maxLines;
        if (start < 0) start = 0;

        float y = logTop + 2.f;
        for (int i = start; i < static_cast<int>(chatLog_.size()); ++i) {
            if (y + lineH > logBottom) break;
            sf::Text line(font, chatLog_[i], lineSize);
            line.setFillColor(sf::Color(200, 200, 200));
            line.setPosition({px + 4.f, y});

            auto lb = line.getGlobalBounds();
            if (lb.size.x > panelW - 8.f) {
                line.setString(sf::String(
                    chatLog_[i].substr(0,
                        static_cast<std::size_t>((panelW - 8.f) / 6.f)) + "..."));
            }
            window.draw(line);
            y += lineH;
        }
    }

    sf::RectangleShape inputBg({panelW, InputH});
    inputBg.setPosition({px, inputY});
    inputBg.setFillColor(chatFocused_ ? sf::Color(50, 50, 60) : sf::Color(40, 40, 40));
    inputBg.setOutlineColor(chatFocused_ ? sf::Color(100, 140, 200) : sf::Color(80, 80, 80));
    inputBg.setOutlineThickness(1.f);
    window.draw(inputBg);

    std::string displayText = chatInput_.empty() && !chatFocused_
        ? "Type a message..." : chatInput_;
    sf::Text inputText(font, displayText, 14);
    inputText.setFillColor(chatInput_.empty() && !chatFocused_
        ? sf::Color(120, 120, 120) : sf::Color(220, 220, 220));
    inputText.setPosition({px + 6.f, inputY + 6.f});
    window.draw(inputText);

    if (chatFocused_) {
        auto lb = inputText.getGlobalBounds();
        float cursorX = px + 6.f + lb.size.x + 1.f;
        sf::Vertex cursor[] = {
            {sf::Vector2f(cursorX, inputY + 4.f), sf::Color(220, 220, 220)},
            {sf::Vector2f(cursorX, inputY + InputH - 4.f), sf::Color(220, 220, 220)}
        };
        window.draw(cursor, 2, sf::PrimitiveType::Lines);
    }
}

void GameScreen::draw(sf::RenderWindow& window)
{
    auto& font = app_.font();

    boardView_.drawSquares(window);
    boardView_.drawHighlights(window, hl_, board_);
    boardView_.drawLabels(window, font);
    boardView_.drawPieces(window, font, board_, app_);

    if (promo_) {
        float sq = boardView_.squareSize();
        sf::Vector2f origin = boardView_.boardOrigin();
        bool flipped = boardView_.isFlipped();

        int promoCol = flipped ? (7 - promo_->toFile) : promo_->toFile;
        int promoRowStart = flipped ? promo_->toRank : (7 - promo_->toRank);

        float ppx = origin.x + static_cast<float>(promoCol) * sq;
        float ppy = origin.y + static_cast<float>(promoRowStart) * sq;

        for (int i = 0; i < static_cast<int>(promo_->candidates.size()); ++i) {
            sf::Vector2f cellPos(ppx, ppy + i * sq);
            sf::RectangleShape cell({sq, sq});
            cell.setPosition(cellPos);
            cell.setFillColor(sf::Color(40, 38, 35, 220));
            cell.setOutlineColor(sf::Color(180, 180, 180, 180));
            cell.setOutlineThickness(1.f);
            window.draw(cell);

            PieceType pt = promo_->candidates[i].promotion;
            float pieceSize = sq * 0.8f;
            float offset = (sq - pieceSize) / 2.f;

            if (app_.piecesLoaded()) {
                const auto& tex = app_.pieceTexture(myColor_, pt);
                sf::Sprite sprite(tex);
                float scale = pieceSize / 160.f;
                sprite.setScale({scale, scale});
                sprite.setPosition({cellPos.x + offset, cellPos.y + offset});
                window.draw(sprite);
            } else {
                const char letters[] = { 'P', 'N', 'B', 'R', 'Q', 'K' };
                unsigned int letterSize = static_cast<unsigned int>(sq * 0.5f);
                if (letterSize < 12) letterSize = 12;
                sf::Text letter(font, std::string(1, letters[static_cast<int>(pt)]), letterSize);
                letter.setFillColor(sf::Color(240, 240, 240));
                auto lb = letter.getGlobalBounds();
                letter.setPosition({
                    cellPos.x + (sq - lb.size.x) / 2.f,
                    cellPos.y + (sq - lb.size.y) / 2.f
                });
                window.draw(letter);
            }
        }
    }

    float px = boardView_.panelX();

    sf::Text title(font, "Game in progress", 24);
    title.setFillColor(sf::Color(200, 200, 200));
    title.setPosition({px, 30.f});
    window.draw(title);

    sf::Text opponent(font, "vs " + opponentName_, 18);
    opponent.setFillColor(sf::Color(160, 160, 160));
    opponent.setPosition({px, 70.f});
    window.draw(opponent);

    const char* colorName = myColor_ == Color::White ? "White" : "Black";
    sf::Text colorText(font, std::string("Playing as ") + colorName, 18);
    colorText.setFillColor(myColor_ == Color::White
        ? sf::Color(240, 240, 240) : sf::Color(100, 100, 100));
    colorText.setPosition({px, 100.f});
    window.draw(colorText);

    const char* turnText = gameOver_ ? "Game over"
        : (myTurn_ ? "Your turn" : "Opponent's turn");
    sf::Text turn(font, turnText, 16);
    turn.setFillColor(myTurn_ ? sf::Color(76, 175, 80) : sf::Color(200, 200, 200));
    turn.setPosition({px, 130.f});
    window.draw(turn);

    if (!statusMsg_.empty()) {
        sf::Text err(font, statusMsg_, 16);
        err.setFillColor(sf::Color(244, 67, 54));
        err.setPosition({px, 155.f});
        window.draw(err);
    }

    drawButtons(window);
    drawChat(window);
}

} // namespace chess::client
