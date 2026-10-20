#include "match.h"

#include <chrono>
#include <random>
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
// MatchClock and shorten the reconnect grace into Match, only Match itself
// reads real time.
static std::int64_t steadyNowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Per-seat rejoin credential (11.2): short, hex, no collision worries at
// match concurrency; security by obscurity is fine for an in-memory,
// minutes-lived window and the token is masked in logs.
static std::string makeResumeToken()
{
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    static const char* hex = "0123456789abcdef";
    std::string token;
    token.reserve(24);
    for (int i = 0; i < 24; ++i)
        token += hex[rng() % 16];
    return token;
}

// Delivery to one seat of a match; absent seats receive nothing, never a
// game change (11.2). The present-pointer is read by Match members, which
// is where this helper is always called from.
template <typename T>
static void sendToPresent(Client* present, const T& msg)
{
    if (present)
        sendTo(*present, chess::net::ServerMessage{msg});
}

Match::Match(Client& white, Client& black, const std::string& timeControl,
             std::int64_t graceMs)
    : timeControl_(timeControl)
    , board_(chess::Board::fromStartPos())
    , seats_{}
    , graceMs_(graceMs)
{
    auto fill = [this](std::size_t idx, Client& client) {
        Seat& seat = seats_[idx];
        seat.present = &client;
        seat.name = client.name;
        seat.playerId = client.playerId;
        seat.isBot = client.isBot;
        seat.resumeToken = makeResumeToken();
    };
    fill(0, white);
    fill(1, black);

    // White's clock starts at game start per custom; unlimited games leave
    // the clock passive.
    clock_.start(chess::net::parseTimeControl(timeControl_),
                 chess::Color::White, steadyNowMs());
}

std::size_t Match::seatIndexOf(const Client& client) const
{
    for (std::size_t i = 0; i < 2; ++i)
        if (seats_[i].present == &client)
            return i;
    return 2; // not present
}

Match::SeatView Match::seat(chess::Color color) const
{
    const Seat& s = seats_[color == chess::Color::White ? 0 : 1];
    return SeatView{
        s.name, s.playerId, s.isBot, s.present != nullptr,
        s.rating, s.deviation};
}

const std::string& Match::resumeToken(chess::Color color) const
{
    return seats_[color == chess::Color::White ? 0 : 1].resumeToken;
}

void Match::seedRatings(const std::pair<double, double>& whiteRd,
                        const std::pair<double, double>& blackRd)
{
    seats_[0].rating = whiteRd.first;
    seats_[0].deviation = whiteRd.second;
    seats_[1].rating = blackRd.first;
    seats_[1].deviation = blackRd.second;
}

Client* Match::white() const { return seats_[0].present; }

Client* Match::black() const { return seats_[1].present; }

Client* Match::opponent(const Client& client) const
{
    const std::size_t idx = seatIndexOf(client);
    if (idx >= 2)
        return nullptr;
    return seats_[1 - idx].present;
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
    const std::size_t moverIdx = seatIndexOf(sender);
    if (moverIdx >= 2
        || (board_.sideToMove() == chess::Color::White ? 0 : 1) != moverIdx) {
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
    const std::size_t moverIdx = seatIndexOf(sender);
    if ((whiteToMove && moverIdx != 0) || (!whiteToMove && moverIdx != 1)) {
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
    for (auto& seat : seats_)
        if (seat.present)
            sendTo(*seat.present, serverMove);

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
    const std::size_t idx = seatIndexOf(sender);
    if (idx >= 2)
        return;
    drawOfferPending_ = true;
    sendToPresent(seats_[1 - idx].present, chess::net::ServerDrawOfferMsg{});
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
    const std::size_t idx = seatIndexOf(sender);
    if (idx >= 2)
        return;
    drawOfferPending_ = false;
    sendToPresent(seats_[1 - idx].present, chess::net::ServerDrawDeclineMsg{});
}

void Match::handleResign(Client& sender)
{
    const std::size_t idx = seatIndexOf(sender);
    chess::net::GameResult result = (idx == 0)
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
    const std::size_t idx = seatIndexOf(sender);
    if (idx >= 2)
        return; // reads as a fire-and-forget line for a stale sender
    // An away opponent does not read chat while absent; the message drops
    // rather than queues (chat history is not restored on resume).
    sendToPresent(seats_[1 - idx].present,
                  chess::net::ServerChatMsg{sender.name, msg.text});
}

void Match::handlePing(Client& sender)
{
    sendTo(sender, chess::net::PongMsg{});
}

void Match::handleDisconnect(const Client& client)
{
    if (!active_)
        return;

    const std::size_t idx = seatIndexOf(client);
    if (idx >= 2)
        return;
    seats_[idx].present = nullptr;

    if (bothSeatsAbsent()) {
        // Nobody left behind: nobody wins (Abort results never rate).
        endGame(chess::net::GameResult::Abort,
                chess::net::GameOverReason::Disconnection);
        return;
    }

    // Solo drop: hold the game open for the rejoin window and tell the
    // survivor. The absent seat's clock keeps running meanwhile.
    graceRunning_ = true;
    graceDeadlineMs_ = steadyNowMs() + graceMs_;
    Seat& survivor = seats_[1 - idx];
    if (survivor.present)
        sendTo(*survivor.present, chess::net::OpponentReconnectingMsg{
            static_cast<std::int32_t>((graceMs_ + 999) / 1000)});
    LOG_INFO("Seat absent (" + seats_[idx].name + "), grace "
             + std::to_string(graceMs_ / 1000) + "s");
}

bool Match::bothSeatsAbsent() const
{
    return seats_[0].present == nullptr && seats_[1].present == nullptr;
}

bool Match::graceArmed() const
{
    return graceRunning_ && active_;
}

Match::ResumeOutcome Match::resume(const std::string& token, Client& newcomer)
{
    chess::Color rebound = chess::Color::White;
    return resume(token, newcomer, rebound);
}

Match::ResumeOutcome Match::resume(const std::string& token, Client& newcomer,
                                   chess::Color& reboundColorOut)
{
    for (std::size_t i = 0; i < 2; ++i) {
        Seat& seat = seats_[i];
        if (seat.resumeToken != token)
            continue;

        if (!active_) {
            // Detached when the game ended; the token still finds the match
            // in retention, replaying the final state + outcome. The seat
            // is not re-bound (nothing left to play).
            reboundColorOut = i == 0 ? chess::Color::White : chess::Color::Black;
            return ResumeOutcome::ReboundOver;
        }

        if (seat.present != nullptr)
            return ResumeOutcome::AlreadyConnected;

        seat.present = &newcomer;
        graceRunning_ = false;
        reboundColorOut = i == 0 ? chess::Color::White : chess::Color::Black;
        LOG_INFO("Seat rebound (" + seat.name + ")");
        // Only the survivor needed the grace notice; the rejoiner gets the
        // full ResumeStateMsg from the server's resume routing.
        sendToPresent(seats_[1 - i].present, chess::net::OpponentResumedMsg{});
        return ResumeOutcome::Rebound;
    }
    return ResumeOutcome::NoSeat;
}

std::optional<chess::net::GameOverMsg> Match::lastGameOver() const
{
    return lastGameOver_;
}

bool Match::retentionEnded(std::int64_t nowMs) const
{
    // Finished matches survive exactly the reconnect grace window, so a
    // late token still finds the final state; the server loop erases once
    // the window lapses.
    return !active_ && finishedAtMs_ != 0 && nowMs - finishedAtMs_ > graceMs_;
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

void Match::pollReconnect(std::int64_t nowMs)
{
    if (!graceArmed())
        return;

    for (std::size_t i = 0; i < 2; ++i) {
        if (seats_[i].present != nullptr)
            continue;
        if (nowMs >= graceDeadlineMs_) {
            // The grace outlived the absent player: decisive loss for the
            // departee, same as an immediate exit has always been.
            endGame(i == 0 ? chess::net::GameResult::BlackWins
                           : chess::net::GameResult::WhiteWins,
                    chess::net::GameOverReason::Disconnection);
        }
        return;
    }
}

bool Match::reconnecting() const
{
    return graceArmed();
}

std::pair<std::int32_t, std::int32_t> Match::clockSnapshotMs(std::int64_t nowMs) const
{
    if (!clock_.active())
        return {0, 0};
    return {
        static_cast<std::int32_t>(clock_.remainingMs(chess::Color::White, nowMs)),
        static_cast<std::int32_t>(clock_.remainingMs(chess::Color::Black, nowMs)),
    };
}

void Match::endGame(chess::net::GameResult result, chess::net::GameOverReason reason)
{
    active_ = false;
    graceRunning_ = false;
    lastGameOver_ = chess::net::GameOverMsg{result, reason};
    finishedAtMs_ = steadyNowMs();
    // Present seats learn the outcome now; a seat already away replays it
    // from lastGameOver() with the final state on a late token resume.
    for (auto& seat : seats_)
        sendToPresent(seat.present, *lastGameOver_);
    LOG_INFO("Game over: result=" + std::to_string(static_cast<int>(result))
             + " reason=" + std::to_string(static_cast<int>(reason)));
    if (gameOverCallback_)
        gameOverCallback_(*this, result, reason);
    // Detach last so the hook still finds present seats for profile re-sends
    // and archive lookups; hot clients head back to the lobby immediately.
    for (auto& seat : seats_) {
        if (seat.present) {
            seat.present->match = nullptr;
            seat.present->state = ClientState::Connected;
            seat.present = nullptr;
        }
    }
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

    Client* sender = (botColor_ == chess::Color::White) ? white() : black();
    if (sender)
        handleMoveSAN(*sender, san);
}

bool Match::isBotTurn() const
{
    return botEngine_ && botThinking_ && active_;
}

Client* Match::botClient() const
{
    if (!botEngine_) return nullptr;
    return (botColor_ == chess::Color::White) ? white() : black();
}
