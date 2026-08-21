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

std::string safeTruncate(const std::string& s, std::size_t maxBytes)
{
    if (s.size() <= maxBytes) return s;
    std::size_t n = maxBytes;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
    return s.substr(0, n > 0 ? n - 1 : 0) + "...";
}

constexpr float BtnH = 30.f;
constexpr float InputH = 28.f;
constexpr std::size_t MaxChatLog = 50;
constexpr std::size_t MaxChatInput = 200;

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
}

GameScreen::GameScreen(App& app, Color myColor, const std::string& opponentName)
    : app_(app)
    , board_(Board::fromStartPos())
    , myColor_(myColor)
    , opponentName_(opponentName)
    , boardView_(static_cast<float>(App::WindowWidth),
                 static_cast<float>(App::WindowHeight),
                 myColor)
    , hud_(boardView_.panelX(),
           static_cast<float>(App::WindowWidth) - boardView_.panelX(),
           static_cast<float>(App::WindowHeight))
    , myTurn_(myColor == Color::White)
{
    inCheck_ = chess::inCheck(board_, myColor_);
    hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
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
    app_.connection().send(chess::net::MoveMsg{san});
    myTurn_ = false;
    hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
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
            hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
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

GameScreen::PromoCell GameScreen::promoCell(int index) const
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
    float btnY = hud_.contentBottom();

    auto inRect = [mx, my](float x, float y, float w, float h) {
        return mx >= x && mx < x + w && my >= y && my < y + h;
    };

    if (drawOfferPending_) {
        if (inRect(px, btnY, 90.f, BtnH)) {
            app_.connection().send(chess::net::DrawDeclineMsg{});
            drawOfferPending_ = false;
            chatLog_.push_back("Draw declined");
            if (chatLog_.size() > MaxChatLog)
                chatLog_.erase(chatLog_.begin());
            return;
        }
        if (inRect(px + 98.f, btnY, 90.f, BtnH)) {
            app_.connection().send(chess::net::DrawAcceptMsg{});
            drawOfferPending_ = false;
            return;
        }
    } else {
        if (inRect(px, btnY, 140.f, BtnH)) {
            app_.connection().send(chess::net::ResignMsg{});
            return;
        }
        if (inRect(px + 148.f, btnY, 140.f, BtnH)) {
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
            } else if (ch >= 32 && ch < 127 && chatInput_.size() < MaxChatInput) {
                chatInput_ += static_cast<char>(ch);
            }
            return;
        }
    }

    if (const auto* we = event.getIf<sf::Event::MouseWheelScrolled>()) {
        float px = boardView_.panelX();
        if (we->position.x >= px) {
            hud_.handleScroll(-we->delta);
            return;
        }
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return;
        int mx = mb->position.x;
        int my = mb->position.y;

        float px = boardView_.panelX();
        float btnY = hud_.contentBottom();
        float chatSepY = btnY + BtnH + 8.f;
        float chatLogTop = chatSepY + 18.f;
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

            for (int i = 0; i < static_cast<int>(promo_->candidates.size()); ++i) {
                auto cell = promoCell(i);
                sf::FloatRect rect(cell.pos, {cell.size, cell.size});
                if (rect.contains(pos)) {
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
    hud_.update(dtSec);

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
                hud_.addMove(move->san);
                inCheck_ = chess::inCheck(board_, myColor_);
                hl_.checkSquare = inCheck_
                    ? findKingSquare(board_, myColor_)
                    : std::optional<std::pair<int,int>>{};
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
                app_, gameOver->result, gameOver->reason));
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
        else if (auto* chat = std::get_if<chess::net::ServerChatMsg>(&msg)) {
            chatLog_.push_back(chat->name + ": " + chat->text);
            if (chatLog_.size() > MaxChatLog)
                chatLog_.erase(chatLog_.begin());
        }
        else if (auto* err = std::get_if<chess::net::ErrorMsg>(&msg)) {
            hud_.setStatus(err->message, 2.0f);
            myTurn_ = true;
            hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
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
    float btnY = hud_.contentBottom();
    auto& font = app_.font();

    if (drawOfferPending_) {
        sf::Text offerText(font, "Draw offered:", 14);
        offerText.setFillColor(sf::Color(255, 200, 60));
        offerText.setPosition({px, btnY - 20.f});
        window.draw(offerText);

        drawBtn(window, px, btnY, 90.f, BtnH, sf::Color(180, 60, 60), font, "Decline");
        drawBtn(window, px + 98.f, btnY, 90.f, BtnH, sf::Color(60, 140, 60), font, "Accept");
    } else if (!gameOver_) {
        drawBtn(window, px, btnY, 140.f, BtnH, sf::Color(180, 60, 60), font, "Resign");
        drawBtn(window, px + 148.f, btnY, 140.f, BtnH, sf::Color(80, 80, 100), font, "Offer Draw");
    }
}

void GameScreen::drawChat(sf::RenderWindow& window)
{
    float px = boardView_.panelX();
    auto& font = app_.font();
    float panelW = 296.f;
    float btnY = hud_.contentBottom();
    float chatSepY = btnY + BtnH + 8.f;

    sf::RectangleShape sep({panelW, 1.f});
    sep.setPosition({px, chatSepY});
    sep.setFillColor(sf::Color(80, 80, 80));
    window.draw(sep);

    sf::Text chatLabel(font, "Chat:", 14);
    chatLabel.setFillColor(sf::Color(160, 160, 160));
    chatLabel.setPosition({px, chatSepY + 6.f});
    window.draw(chatLabel);

    float logTop = chatSepY + 24.f;
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
                    safeTruncate(chatLog_[i],
                        static_cast<std::size_t>((panelW - 8.f) / 6.f))));
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
    drawButtons(window);
    drawChat(window);
}

} // namespace chess::client
