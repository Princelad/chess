#include "match.h"

#include <chrono>
#include <string>
#include <type_traits>

#include <chess/movegen.h>
#include <chess/san.h>
#include <chess/uci/engine.h>

#include "client.h"
#include "log.h"
#include "send.h"

static constexpr std::size_t MaxChatLength = 500;
static constexpr std::size_t MaxSanLength = 10;

// The one server-side monotonic time source for clocks. Matches replace the
// steady wall by it; e2e/unit tests inject their own timestamps into
// MatchClock, only Match itself reads real time.
static std::int64_t steadyNowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

Match::Match(Client& white, Client& black, const std::string& timeControl)
    : timeControl_(timeControl)
    , board_(chess::Board::fromStartPos())
    , white_(&white)
    , black_(&black)
{
    // White's clock starts at game start per custom; unlimited games leave
    // the clock passive.
    clock_.start(chess::net::parseTimeControl(timeControl_),
                 chess::Color::White, steadyNowMs());
}

void Match::handleMessage(Client& sender, const chess::net::ClientMessage& msg)
{
    if (std::get_if<chess::net::PingMsg>(&msg)) {
        handlePing(sender);
        return;
    }

    if (!active_) {
        sendTo(sender, chess::net::ErrorMsg{"Game is over"});
        return;
    }

    std::visit([this, &sender](auto&& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, chess::net::MoveMsg>)
            handleMove(sender, m);
        else if constexpr (std::is_same_v<T, chess::net::DrawOfferMsg>)
            handleDrawOffer(sender);
        else if constexpr (std::is_same_v<T, chess::net::DrawAcceptMsg>)
            handleDrawAccept(sender);
        else if constexpr (std::is_same_v<T, chess::net::DrawDeclineMsg>)
            handleDrawDecline(sender);
        else if constexpr (std::is_same_v<T, chess::net::ResignMsg>)
            handleResign(sender);
        else if constexpr (std::is_same_v<T, chess::net::ChatMsg>)
            handleChat(sender, m);
    }, msg);
}

void Match::handleMove(Client& sender, const chess::net::MoveMsg& msg)
{
    if (&sender != (board_.sideToMove() == chess::Color::White ? white_ : black_)) {
        sendTo(sender, chess::net::ErrorMsg{"Not your turn"});
        return;
    }

    if (msg.san.size() > MaxSanLength) {
        sendTo(sender, chess::net::ErrorMsg{"Illegal move"});
        return;
    }

    handleMoveSAN(sender, msg.san);
}

bool Match::handleMoveSAN(Client& sender, const std::string& san)
{
    // Turn authority: the sender must be the side to move. Without this, a
    // client (or two racing packets drained out of socket order by the
    // select loop) could apply a move for either side out of turn.
    const bool whiteToMove = board_.sideToMove() == chess::Color::White;
    const bool senderIsWhite = white_ == &sender;
    const bool senderIsBlack = black_ == &sender;
    if ((whiteToMove && !senderIsWhite) || (!whiteToMove && !senderIsBlack)) {
        sendTo(sender, chess::net::ErrorMsg{"Not your turn"});
        return false;
    }

    auto move = chess::san::fromSan(board_, san);
    if (!move.has_value() || !chess::isLegalMove(board_, *move)) {
        sendTo(sender, chess::net::ErrorMsg{"Illegal move"});
        return false;
    }

    std::string canonical = chess::san::toSan(board_, *move);
    board_.makeMove(*move);
    sanHistory_.push_back(canonical);

    // Charge the mover's clock for the elapsed turn and hand the turn to
    // the opponent: the broadcast carries the on-move player's fresh
    // remaining time (0 = unlimited).
    chess::net::ServerMoveMsg serverMove{canonical, 0};
    if (clock_.active()) {
        std::int64_t moverRemaining = 0;
        clock_.completedMove(whiteToMove ? chess::Color::White : chess::Color::Black,
                             steadyNowMs(), moverRemaining);
        serverMove.remainingMs = static_cast<std::int32_t>(moverRemaining);
    }
    sendTo(*white_, serverMove);
    sendTo(*black_, serverMove);

    drawOfferPending_ = false;

    auto state = chess::evaluateGameState(board_);
    if (state != chess::GameState::Ongoing) {
        auto [result, reason] = classifyState(state);
        endGame(result, reason);
        return true;
    }

    if (botEngine_ && board_.sideToMove() == botColor_ && active_) {
        botThinking_ = true;
        botEngine_->position(board_.toFen());
        botEngine_->go(botDepth_);
    }

    return true;
}

void Match::handleDrawOffer(Client& sender)
{
    drawOfferPending_ = true;
    sendTo(*opponent(sender), chess::net::ServerDrawOfferMsg{});
}

void Match::handleDrawAccept(Client& sender)
{
    if (!drawOfferPending_) {
        sendTo(sender, chess::net::ErrorMsg{"No draw offer pending"});
        return;
    }
    // Agreed draw — both players accepted.
    endGame(chess::net::GameResult::Draw, chess::net::GameOverReason::AgreedDraw);
}

void Match::handleDrawDecline(Client& sender)
{
    drawOfferPending_ = false;
    sendTo(*opponent(sender), chess::net::ServerDrawDeclineMsg{});
}

void Match::handleResign(Client& sender)
{
    chess::net::GameResult result = (&sender == white_)
        ? chess::net::GameResult::BlackWins
        : chess::net::GameResult::WhiteWins;
    endGame(result, chess::net::GameOverReason::Resignation);
}

void Match::handleChat(Client& sender, const chess::net::ChatMsg& msg)
{
    if (msg.text.size() > MaxChatLength) {
        sendTo(sender, chess::net::ErrorMsg{"Message too long"});
        return;
    }
    sendTo(*opponent(sender), chess::net::ServerChatMsg{sender.name, msg.text});
}

void Match::handlePing(Client& sender)
{
    sendTo(sender, chess::net::PongMsg{});
}

void Match::handleDisconnect(const Client& client)
{
    if (!active_)
        return;

    Client* survivor = opponent(client);
    if (survivor) {
        chess::net::GameResult result = (&client == white_)
            ? chess::net::GameResult::BlackWins
            : chess::net::GameResult::WhiteWins;
        endGame(result, chess::net::GameOverReason::Disconnection);
    } else {
        endGame(chess::net::GameResult::Abort, chess::net::GameOverReason::Abort);
    }
}

bool Match::isActive() const { return active_; }

void Match::pollClock(std::int64_t nowMs)
{
    if (!active_ || !clock_.active())
        return;

    auto flagged = clock_.checkFlag(nowMs);
    if (!flagged)
        return;

    endGame(*flagged == chess::Color::White
                ? chess::net::GameResult::BlackWins
                : chess::net::GameResult::WhiteWins,
            chess::net::GameOverReason::Timeout);
}

Client* Match::white() const { return white_; }

Client* Match::black() const { return black_; }

Client* Match::opponent(const Client& client) const
{
    if (&client == white_) return black_;
    if (&client == black_) return white_;
    return nullptr;
}

void Match::endGame(chess::net::GameResult result, chess::net::GameOverReason reason)
{
    active_ = false;
    chess::net::GameOverMsg gameOver{result, reason};
    [[maybe_unused]] bool w = sendTo(*white_, gameOver);
    [[maybe_unused]] bool b = sendTo(*black_, gameOver);
    LOG_INFO("Game over: result=" + std::to_string(static_cast<int>(result))
             + " reason=" + std::to_string(static_cast<int>(reason)));
    if (gameOverCallback_)
        gameOverCallback_(*this, result, reason);
}

std::pair<chess::net::GameResult, chess::net::GameOverReason>
Match::classifyState(chess::GameState state) const
{
    switch (state) {
        case chess::GameState::Checkmate: {
            chess::Color loser = board_.sideToMove();
            return {
                loser == chess::Color::White ? chess::net::GameResult::BlackWins
                                             : chess::net::GameResult::WhiteWins,
                chess::net::GameOverReason::Checkmate
            };
        }
        case chess::GameState::Stalemate:
            return {chess::net::GameResult::Draw, chess::net::GameOverReason::Stalemate};
        case chess::GameState::Draw:
            if (chess::fiftyMoveRule(board_))
                return {chess::net::GameResult::Draw, chess::net::GameOverReason::FiftyMove};
            if (chess::threefoldRepetition(board_))
                return {chess::net::GameResult::Draw, chess::net::GameOverReason::Repetition};
            if (chess::insufficientMaterial(board_))
                return {chess::net::GameResult::Draw, chess::net::GameOverReason::InsufficientMaterial};
            return {chess::net::GameResult::Draw, chess::net::GameOverReason::Abort};
        default:
            return {chess::net::GameResult::Abort, chess::net::GameOverReason::Abort};
    }
}

void Match::startBot(chess::Color color, int depth, std::string enginePath)
{
    botColor_ = color;
    botDepth_ = depth;
    botEngine_ = std::make_unique<chess::uci::UciEngine>(std::move(enginePath));
    auto info = botEngine_->init(std::chrono::seconds(2));
    if (info.name.empty() && !botEngine_->isRunning()) {
        LOG_ERROR("Bot engine failed to start");
        botEngine_.reset();
        return;
    }

    botEngine_->setOption("Threads", "1");
    botEngine_->setOption("Hash", "64");
    botEngine_->newGame();
    botEngine_->position("startpos");

    if (board_.sideToMove() == botColor_) {
        botThinking_ = true;
        botEngine_->go(botDepth_);
    }
}

void Match::stopBot()
{
    if (botEngine_) {
        botEngine_->quit();
        botEngine_.reset();
    }
    botThinking_ = false;
}

void Match::pollBotMove()
{
    if (!botEngine_ || !botThinking_ || !active_)
        return;

    auto move = botEngine_->tryGetBestMove(board_);
    if (!move)
        return;

    std::string san = chess::san::toSan(board_, *move);
    botThinking_ = false;

    Client* sender = (botColor_ == chess::Color::White) ? white_ : black_;
    handleMoveSAN(*sender, san);
}

bool Match::isBotTurn() const
{
    return botEngine_ && botThinking_ && active_;
}

Client* Match::botClient() const
{
    if (!botEngine_) return nullptr;
    return (botColor_ == chess::Color::White) ? white_ : black_;
}
