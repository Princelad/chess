#include "match.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <string>
#include <type_traits>

#include <chess/fen.h>
#include <chess/movegen.h>
#include <chess/san.h>
#include <chess/uci/engine.h>

#include "client.h"
#include "log.h"
#include "send.h"

static constexpr std::size_t MaxChatLength = 500;
static constexpr std::size_t MaxSanLength = 10;

// The match's starting board (12.1): standard position unless a Chess960
// SP FEN was carried in. The server generates its own start positions, so
// a parse failure here is unreachable — but a misbehaving startFen must
// never take the pairing sequence down, so fall back to the standard board.
static chess::Board startBoardFrom(const std::string& startFen)
{
    if (startFen.empty())
        return chess::Board::fromStartPos();
    auto board = chess::fromFen(startFen);
    if (board.has_value())
        return *board;
    LOG_ERROR("Unparseable start FEN — falling back to the standard position");
    return chess::Board::fromStartPos();
}

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
             const std::string& startFen, std::int64_t graceMs)
    : timeControl_(timeControl)
    , startFen_(startFen)
    , board_(startBoardFrom(startFen))
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

void Match::setArenaKey(std::string key)
{
    arenaKey_ = std::move(key);
}

bool Match::addSpectator(Client& spectator)
{
    if (spectators_.size() >= MaxSpectators)
        return false;
    if (std::find(spectators_.begin(), spectators_.end(), &spectator)
        != spectators_.end())
        return true; // already watching — idempotent re-bind
    spectators_.push_back(&spectator);
    return true;
}

bool Match::removeSpectator(Client& spectator)
{
    auto it = std::find(spectators_.begin(), spectators_.end(), &spectator);
    if (it == spectators_.end())
        return false;
    spectators_.erase(it);
    return true;
}

bool Match::isSpectator(const Client& spectator) const
{
    return std::find(spectators_.begin(), spectators_.end(), &spectator)
        != spectators_.end();
}

// The SpectateStateMsg this match would hand a fresh watcher now (12.4):
// names/ratings from the seat stamps — the same figures the seats see —
// the SAN chain, both clocks sampled now, the watcher count. Ratings come
// from the pairing-time stamps, not a live PlayerStore lookup.
chess::net::SpectateStateMsg Match::spectateSnapshot() const
{
    chess::net::SpectateStateMsg snapshot;
    const Seat& white = seats_[0];
    const Seat& black = seats_[1];
    snapshot.whiteName = white.name;
    snapshot.blackName = black.name;
    snapshot.whiteRating = white.rating;
    snapshot.whiteDeviation = white.deviation;
    snapshot.blackRating = black.rating;
    snapshot.blackDeviation = black.deviation;
    snapshot.timeControl = timeControl_;
    snapshot.startFen = startFen_;
    snapshot.sans = sanHistory_;
    const auto clocks = clockSnapshotMs(steadyNowMs());
    snapshot.whiteRemainingMs = clocks.first;
    snapshot.blackRemainingMs = clocks.second;
    snapshot.spectators = static_cast<std::int32_t>(spectators_.size());
    return snapshot;
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

    // Arena games (12.3) never rematch: the lobby's next pairing IS the
    // "again" channel, so all three rematch verbs answer with the same
    // refusal whether the game is live or already finished.
    if (isArena()) {
        if (std::get_if<chess::net::RematchRequestMsg>(&msg)
            || std::get_if<chess::net::RematchAcceptMsg>(&msg)
            || std::get_if<chess::net::RematchDeclineMsg>(&msg)) {
            sendTo(sender, chess::net::ErrorMsg{
                "Arena games can't be rematched — the lobby pairs you next"});
            return;
        }
    }

    // Rematch (11.7) is heard on finished matches (that is its home) but
    // answers mid-game attempts with a clear error instead of silence.
    if (std::get_if<chess::net::RematchRequestMsg>(&msg)) {
        handleRematchRequest(sender);
        return;
    }
    if (std::get_if<chess::net::RematchAcceptMsg>(&msg)) {
        handleRematchAccept(sender);
        return;
    }
    if (std::get_if<chess::net::RematchDeclineMsg>(&msg)) {
        handleRematchDecline(sender);
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
        else if constexpr (std::is_same_v<T, chess::net::TakebackRequestMsg>)
            handleTakebackRequest(sender);
        else if constexpr (std::is_same_v<T, chess::net::TakebackAcceptMsg>)
            handleTakebackAccept(sender);
        else if constexpr (std::is_same_v<T, chess::net::TakebackDeclineMsg>)
            handleTakebackDecline(sender);
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
    // The watcher count rides the broadcast (12.4): seats and spectators
    // refresh it exactly when the position changes.
    serverMove.spectators = static_cast<std::int32_t>(spectators_.size());
    for (auto& seat : seats_)
        if (seat.present)
            sendTo(*seat.present, serverMove);
    for (Client* spectator : spectators_)
        if (spectator)
            sendTo(*spectator, serverMove);

    // An accepted move resolves whatever was on the table: the draw offer
    // was for the old position, and the takeback (if one was out and the
    // mover is its asker) issues from a frame that has moved on.
    drawOfferPending_ = false;
    takebackPending_.reset();

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

void Match::handleTakebackRequest(Client& sender)
{
    const std::size_t idx = seatIndexOf(sender);
    if (idx >= 2)
        return;

    if (takebackPending_.has_value()) {
        sendTo(sender, chess::net::ErrorMsg{"A takeback is already pending"});
        return;
    }
    const std::size_t onMoveIdx =
        board_.sideToMove() == chess::Color::White ? 0 : 1;
    if (onMoveIdx != idx) {
        sendTo(sender, chess::net::ErrorMsg{"Not your turn"});
        return;
    }
    // Classic takeback = retract the last full move: the asker's own ply
    // plus the opponent's response, which alone puts the asker back on the
    // move. Earlier than that (fewer than two plies, or the asker moving
    // first) has no two-ply frame to roll back.
    if (sanHistory_.size() < 2) {
        sendTo(sender, chess::net::ErrorMsg{"No move to take back"});
        return;
    }
    // The asked seat answers, so a mid-grace absence (11.2) fails fast
    // instead of queuing behind the reconnect.
    if (seats_[1 - idx].present == nullptr) {
        sendTo(sender, chess::net::ErrorMsg{"The opponent is away"});
        return;
    }

    if (seats_[1 - idx].isBot) {
        // Server bots always agree — keeps solo games flowing, and there is
        // no round trip to surface a UI for anyway. No in-flight engine
        // hazard: the bot only thinks on its own turn, which is exactly the
        // frame a valid request (before the rollback) never is.
        LOG_INFO("Takeback accepted by bot (" + seats_[idx].name + ")");
        applyTakeback(idx);
        return;
    }

    takebackPending_ = idx;
    LOG_INFO("Takeback requested by " + seats_[idx].name);
    sendToPresent(seats_[1 - idx].present,
                  chess::net::ServerTakebackRequestMsg{});
}

void Match::handleTakebackAccept(Client& sender)
{
    const std::size_t idx = seatIndexOf(sender);
    if (idx >= 2)
        return;
    // Only the asked seat can agree; the asker cannot accept own request.
    if (!takebackPending_.has_value() || *takebackPending_ == idx) {
        sendTo(sender, chess::net::ErrorMsg{"No takeback request pending"});
        return;
    }
    applyTakeback(*takebackPending_);
}

void Match::handleTakebackDecline(Client& sender)
{
    const std::size_t idx = seatIndexOf(sender);
    if (idx >= 2)
        return;
    if (!takebackPending_.has_value()) {
        sendTo(sender, chess::net::ErrorMsg{"No takeback request pending"});
        return;
    }
    // Either seat dissolves the offer: the asked seat declines outright,
    // while the asker's own decline message reads as a self-cancel (the
    // requester is the seat notified below, so the notice lands right
    // either way).
    const std::size_t requesterIdx = *takebackPending_;
    takebackPending_.reset();
    LOG_INFO("Takeback declined (seat of " + seats_[idx].name + ")");
    sendToPresent(seats_[requesterIdx].present,
                  chess::net::ServerTakebackDeclineMsg{});
}

// Rolls the live position two plies back: the asker's last move and the
// opponent's reply disappear, leaving the asker on the move in the prior
// position. The rebuild chains san::fromSan over the truncated history from
// the start position — the reconstruction already exercised by the 11.2
// resume and the archive replay — rather than adapting Board::undoMove's
// internal state restoration to match-level scope.
void Match::applyTakeback(std::size_t requesterIdx)
{
    if (sanHistory_.size() < 2)
        return; // unreachable: every accept path validated the frame first

    // Audit hardening: replay the truncated chain on a scratch board BEFORE
    // touching match state — the old form truncated sanHistory_ and reset
    // board_ first, so a mid-chain replay failure (corrupt entry in an
    // in-memory list) would have left the match two plies short with no
    // TakebackAcceptedMsg sent: a silent desync. Rebuild locally, commit all
    // three only once the whole replay applies.
    std::vector<std::string> trimmed(sanHistory_.begin(),
                                     sanHistory_.end() - 2);
    // Rebuild from THIS game's starting position (12.1): a Chess960 match
    // must never re-chain its SAN history from the standard board.
    chess::Board rebuilt = startBoardFrom(startFen_);
    for (const auto& san : trimmed) {
        auto move = chess::san::fromSan(rebuilt, san);
        if (!move.has_value()) {
            // Unreachable: every entry was produced by handleMoveSAN's own
            // legality checks. Bailing keeps the corrupt board out entirely.
            LOG_ERROR("Takeback replay failed at " + san);
            return;
        }
        rebuilt.makeMove(*move);
    }

    sanHistory_ = std::move(trimmed);
    takebackPending_.reset();
    board_ = std::move(rebuilt);

    // Both present seats rebuild from the same list; an absent seat gets
    // nothing and catches up through its resume replay of sanHistory_.
    chess::net::TakebackAcceptedMsg accepted{sanHistory_};
    for (auto& seat : seats_)
        sendToPresent(seat.present, accepted);
    // Spectators (12.4) have no takeback verb of their own and the two
    // retracted plies never air as Move broadcasts — re-send the whole
    // snapshot so their replayed history shrinks with the game's.
    for (Client* spectator : spectators_)
        if (spectator)
            sendTo(*spectator, spectateSnapshot());
    LOG_INFO("Takeback for " + seats_[requesterIdx].name
             + " (" + std::to_string(sanHistory_.size()) + " plies remain)");

    // Nothing to trigger for the bot: after a valid request the asker —
    // never a bot — is on the move in the rebuilt position.
}

bool Match::retentionLapsed() const
{
    return !active_ && retentionEnded(steadyNowMs());
}

void Match::handleRematchRequest(Client& sender)
{
    const std::size_t idx = seatIndexOf(sender);
    if (idx >= 2)
        return;

    // Rematch is a post-game creature: mid-game requests are unambiguous
    // noise and get a clear answer.
    if (active_) {
        sendTo(sender, chess::net::ErrorMsg{"Game not over yet"});
        return;
    }
    if (retentionLapsed()) {
        // The sweep had not run since the window lapsed; this stale frame
        // answers as the lobby would (the sweep dissolves pending offers
        // without notices).
        rematchPending_.reset();
        sendTo(sender, chess::net::ErrorMsg{
            "No game to rematch — session expired"});
        return;
    }
    if (rematchPending_.has_value()) {
        // A double click from the asker is harmless (idempotent); anything
        // else tries to queue a second offer.
        if (*rematchPending_ != idx)
            sendTo(sender, chess::net::ErrorMsg{"A rematch is already pending"});
        return;
    }
    // The asked seat answers, so their absence fails fast instead of
    // queueing behind an unknown window length (same frame as takeback).
    if (seats_[1 - idx].present == nullptr) {
        sendTo(sender, chess::net::ErrorMsg{"The opponent is away"});
        return;
    }

    if (seats_[1 - idx].isBot) {
        // Server bots always agree — keeps solo games flowing, and there is
        // no round trip to surface a UI for anyway. The fresh match is the
        // server's callback problem: colors swap there, and the bot is
        // restarted against its new seat.
        LOG_INFO("Rematch accepted by bot (" + seats_[idx].name + ")");
        rematchPending_ = idx;
        if (rematchCallback_)
            rematchCallback_(*this);
        return;
    }

    rematchPending_ = idx;
    LOG_INFO("Rematch requested by " + seats_[idx].name);
    sendToPresent(seats_[1 - idx].present,
                  chess::net::ServerRematchRequestMsg{});
}

void Match::handleRematchAccept(Client& sender)
{
    const std::size_t idx = seatIndexOf(sender);
    if (idx >= 2)
        return;
    if (active_ || !rematchPending_.has_value() || *rematchPending_ == idx
        || retentionLapsed()) {
        sendTo(sender, chess::net::ErrorMsg{"No rematch request pending"});
        return;
    }
    rematchPending_.reset();
    // The server (via callback) tears this match's seats down and pairs the
    // same clients into a fresh one with swapped colors; both seats learn
    // about it through the fresh WelcomeMsg, which is the acceptance signal.
    if (rematchCallback_) {
        LOG_INFO("Rematch accepted (seat of " + seats_[idx].name + ")");
        rematchCallback_(*this);
    }
}

void Match::handleRematchDecline(Client& sender)
{
    const std::size_t idx = seatIndexOf(sender);
    if (idx >= 2)
        return;
    if (active_ || !rematchPending_.has_value() || retentionLapsed()) {
        sendTo(sender, chess::net::ErrorMsg{"No rematch request pending"});
        return;
    }
    // Either seat dissolves the offer — the asker can decline their own
    // (self-cancel). The requester's client needs the notice either way to
    // clear its waiting state. Mirrors takeback's decline semantics.
    const bool selfCancel = *rematchPending_ == idx;
    dissolveRematch(selfCancel);
}

void Match::dissolveRematch(bool withdrawn)
{
    if (!rematchPending_.has_value())
        return;
    const std::size_t requesterIdx = *rematchPending_;
    rematchPending_.reset();
    LOG_INFO(withdrawn ? "Rematch offer withdrawn"
                       : "Rematch declined");
    sendToPresent(seats_[requesterIdx].present,
                  chess::net::ServerRematchDeclineMsg{withdrawn});
}

// Voluntary departure from a finished match's game-over seat: the seat is
// released, a pending rematch dissolves with the requester notified (the
// asker leaving reads as withdrawn, the asked seat leaving as a decline).
void Match::leave(const Client& client)
{
    const std::size_t idx = seatIndexOf(client);
    if (idx >= 2)
        return;
    seats_[idx].present = nullptr;
    if (rematchPending_.has_value())
        dissolveRematch(*rematchPending_ == idx);
}

void Match::detachAllSeats()
{
    for (auto& seat : seats_)
        seat.present = nullptr;
    rematchPending_.reset();
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
    const std::size_t idx = seatIndexOf(client);
    if (idx >= 2)
        return;

    if (!active_) {
        // Game-over seat (11.7): the socket died while the seated player
        // sat on the result screen. Release the seat quietly; a pending
        // rematch offer dissolves (the asker's departure reads as
        // withdrawn, the asked seat's as a plain decline).
        seats_[idx].present = nullptr;
        if (rematchPending_.has_value())
            dissolveRematch(*rematchPending_ == idx);
        return;
    }

    seats_[idx].present = nullptr;

    // A pending takeback is a two-seat conversation; a seat leaving ends
    // it. A resumed asker can simply ask again once the table is whole.
    takebackPending_.reset();

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
    drawOfferPending_ = false;
    takebackPending_.reset();
    lastGameOver_ = chess::net::GameOverMsg{result, reason};
    finishedAtMs_ = steadyNowMs();
    // Present seats learn the outcome now; a seat already away replays it
    // from lastGameOver() with the final state on a late token resume.
    for (auto& seat : seats_)
        sendToPresent(seat.present, *lastGameOver_);
    // Spectators (12.4) see the result too, then their watch ends with the
    // game. Release the bindings here so no stale spectating pointer can
    // outlive this match once retention sweeps it away.
    for (Client* spectator : spectators_)
        if (spectator)
            sendTo(*spectator, *lastGameOver_);
    for (Client* spectator : spectators_)
        if (spectator)
            spectator->spectating = nullptr;
    spectators_.clear();
    LOG_INFO("Game over: result=" + std::to_string(static_cast<int>(result))
             + " reason=" + std::to_string(static_cast<int>(reason)));
    if (gameOverCallback_)
        gameOverCallback_(*this, result, reason);
    // Seats stay attached through the retention window (11.7): the rematch
    // offer routes through the match, so both game-over clients keep their
    // seat membership until they leave, drop, or retention lapses (the
    // server loop returns lingering clients to the Connected state then).
    for (auto& seat : seats_)
        if (seat.present)
            seat.present->state = ClientState::GameOver;
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
