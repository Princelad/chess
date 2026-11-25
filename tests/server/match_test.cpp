#include <gtest/gtest.h>

#include <chrono>
#include <map>

#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>

#include <chess/board.h>
#include <chess/chess960.h>
#include <chess/fen.h>
#include <chess/movegen.h>
#include <chess/net/messages.h>
#include <chess/net/protocol.h>
#include <chess/san.h>

#include "client.h"
#include "match.h"

#include <memory>
#include <thread>

class MatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(listener_.listen(0), sf::Socket::Status::Done);
        port_ = listener_.getLocalPort();

        acceptThread_ = std::thread([this]() {
            whiteSock_ = std::make_unique<sf::TcpSocket>();
            [[maybe_unused]] auto s1 = listener_.accept(*whiteSock_);
            blackSock_ = std::make_unique<sf::TcpSocket>();
            [[maybe_unused]] auto s2 = listener_.accept(*blackSock_);
            listener_.close();
        });

        clientWhite_ = std::make_unique<sf::TcpSocket>();
        ASSERT_EQ(clientWhite_->connect(sf::IpAddress::LocalHostV4, port_), sf::Socket::Status::Done);
        clientBlack_ = std::make_unique<sf::TcpSocket>();
        ASSERT_EQ(clientBlack_->connect(sf::IpAddress::LocalHostV4, port_), sf::Socket::Status::Done);

        acceptThread_.join();

        white_.socket = std::move(whiteSock_);
        black_.socket = std::move(blackSock_);
        white_.name = "Alice";
        black_.name = "Bob";

        // Zero grace makes the 11.2 reconnect window deterministic in unit
        // tests: the seat is absent and the next pollReconnect expires it.
        match_ = std::make_unique<Match>(white_, black_, std::string(),
                                         std::string(), testGraceMs_);
    }

    void sendFromClient(sf::TcpSocket& sock, const chess::net::ClientMessage& msg) {
        sf::Packet packet;
        chess::net::serialize(packet, msg);
        [[maybe_unused]] auto s = sock.send(packet);
    }

    std::optional<chess::net::ServerMessage> recvOnClient(sf::TcpSocket& sock) {
        sock.setBlocking(true);
        sf::Packet packet;
        if (sock.receive(packet) != sf::Socket::Status::Done) return std::nullopt;
        return chess::net::deserializeServer(packet);
    }

    std::optional<chess::net::ServerMessage> tryRecvOnClient(sf::TcpSocket& sock) {
        sock.setBlocking(false);
        sf::Packet packet;
        if (sock.receive(packet) != sf::Socket::Status::Done) return std::nullopt;
        return chess::net::deserializeServer(packet);
    }

    sf::TcpListener listener_;
    unsigned short port_ = 0;
    std::unique_ptr<sf::TcpSocket> whiteSock_;
    std::unique_ptr<sf::TcpSocket> blackSock_;
    std::unique_ptr<sf::TcpSocket> clientWhite_;
    std::unique_ptr<sf::TcpSocket> clientBlack_;
    std::thread acceptThread_;
    Client white_;
    Client black_;
    std::unique_ptr<Match> match_;
    std::int64_t testGraceMs_ = 0;
};

TEST_F(MatchTest, ValidMove)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});

    auto bMsg = recvOnClient(*clientBlack_);
    ASSERT_TRUE(bMsg.has_value());
    auto* move = std::get_if<chess::net::ServerMoveMsg>(&*bMsg);
    ASSERT_NE(move, nullptr);
    EXPECT_EQ(move->san, "e4");
}

TEST_F(MatchTest, WrongTurn)
{
    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});

    auto msg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "Not your turn");
}

TEST_F(MatchTest, IllegalMove)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e5"});

    auto msg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "Illegal move");
}

TEST_F(MatchTest, Resign)
{
    match_->handleMessage(white_, chess::net::ResignMsg{});

    EXPECT_FALSE(match_->isActive());

    auto wMsg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(wMsg.has_value());
    auto* over = std::get_if<chess::net::GameOverMsg>(&*wMsg);
    ASSERT_NE(over, nullptr);
    EXPECT_EQ(over->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Resignation);

    auto bMsg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(bMsg.has_value());
    auto* bOver = std::get_if<chess::net::GameOverMsg>(&*bMsg);
    ASSERT_NE(bOver, nullptr);
    EXPECT_EQ(bOver->result, chess::net::GameResult::BlackWins);
}

TEST_F(MatchTest, DrawOfferAndAccept)
{
    match_->handleMessage(white_, chess::net::DrawOfferMsg{});

    auto bOffer = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(bOffer.has_value());
    EXPECT_TRUE(std::get_if<chess::net::ServerDrawOfferMsg>(&*bOffer));

    match_->handleMessage(black_, chess::net::DrawAcceptMsg{});
    EXPECT_FALSE(match_->isActive());

    auto wOver = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(wOver.has_value());
    auto* over = std::get_if<chess::net::GameOverMsg>(&*wOver);
    ASSERT_NE(over, nullptr);
    EXPECT_EQ(over->result, chess::net::GameResult::Draw);

    auto bOver = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(bOver.has_value());
    auto* bOverMsg = std::get_if<chess::net::GameOverMsg>(&*bOver);
    ASSERT_NE(bOverMsg, nullptr);
    EXPECT_EQ(bOverMsg->result, chess::net::GameResult::Draw);
}

TEST_F(MatchTest, DrawAcceptNoOffer)
{
    match_->handleMessage(black_, chess::net::DrawAcceptMsg{});

    auto msg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "No draw offer pending");
}

// Audit 1.5: the offerer accepting their own offer must not end the game
// as a rated draw — that was a one-packet cheat for the losing side.
TEST_F(MatchTest, DrawOfferSelfAcceptRefused)
{
    match_->handleMessage(white_, chess::net::DrawOfferMsg{});
    match_->handleMessage(white_, chess::net::DrawAcceptMsg{});

    EXPECT_TRUE(match_->isActive());

    auto msg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "That is your own draw offer");

    // The offer survives the refused self-accept: the opponent can still
    // accept it for real.
    match_->handleMessage(black_, chess::net::DrawAcceptMsg{});
    EXPECT_FALSE(match_->isActive());
    auto wOver = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(wOver.has_value());
    auto* over = std::get_if<chess::net::GameOverMsg>(&*wOver);
    ASSERT_NE(over, nullptr);
    EXPECT_EQ(over->result, chess::net::GameResult::Draw);
}

// Audit 10.3: a decline with no offer pending must not notify the other
// seat (the client's stale prompt used to fire phantom declines).
TEST_F(MatchTest, DrawDeclineWithoutOfferRefused)
{
    match_->handleMessage(black_, chess::net::DrawDeclineMsg{});

    auto msg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "No draw offer pending");
    // The other seat hears nothing.
    EXPECT_FALSE(tryRecvOnClient(*clientWhite_).has_value());
}

// Audit 10.3: a cross-offer used to overwrite the offerer slot, so the
// second asker's Accept answered "That is your own draw offer". Both
// seats wanting a draw is an agreement.
TEST_F(MatchTest, DrawCrossOfferAgreesToDraw)
{
    match_->handleMessage(white_, chess::net::DrawOfferMsg{});
    auto offer = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(offer.has_value());
    EXPECT_TRUE(std::get_if<chess::net::ServerDrawOfferMsg>(&*offer));

    match_->handleMessage(black_, chess::net::DrawOfferMsg{});
    EXPECT_FALSE(match_->isActive());

    auto wOver = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(wOver.has_value());
    auto* w = std::get_if<chess::net::GameOverMsg>(&*wOver);
    ASSERT_NE(w, nullptr);
    EXPECT_EQ(w->result, chess::net::GameResult::Draw);
    EXPECT_EQ(w->reason, chess::net::GameOverReason::AgreedDraw);
}

// Audit 10.3: a duplicate offer from the same seat is a notice, not a
// second prompt, and it leaves the original offer intact.
TEST_F(MatchTest, DrawDuplicateOfferRefused)
{
    match_->handleMessage(white_, chess::net::DrawOfferMsg{});
    match_->handleMessage(white_, chess::net::DrawOfferMsg{});

    auto msg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "Your draw offer is already pending");

    // Only the first offer reached the opponent, and it still works.
    auto offer = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(offer.has_value());
    EXPECT_TRUE(std::get_if<chess::net::ServerDrawOfferMsg>(&*offer));
    EXPECT_FALSE(tryRecvOnClient(*clientBlack_).has_value());

    match_->handleMessage(black_, chess::net::DrawAcceptMsg{});
    EXPECT_FALSE(match_->isActive());
}

// Audit 10.3: the asked seat cannot answer an offer it never received —
// a mid-grace absence fails fast instead of storing a dead offer.
TEST_F(MatchTest, DrawOfferRefusedWhenOpponentAway)
{
    match_->handleDisconnect(black_);
    ASSERT_TRUE(match_->isActive());

    // The survivor is told about the grace window first.
    auto notice = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(notice.has_value());
    EXPECT_TRUE(std::get_if<chess::net::OpponentReconnectingMsg>(&*notice));

    match_->handleMessage(white_, chess::net::DrawOfferMsg{});

    auto msg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "The opponent is away");
    EXPECT_TRUE(match_->isActive());
}

// Audit 1.5: a sender who is not a seat never ends the game through the
// draw/resign verbs (the old resign fallthrough read a non-seat as
// "White resigned").
TEST_F(MatchTest, ResignFromNonSeatRefused)
{
    Client outsider;
    outsider.name = "Mallory";
    match_->handleMessage(outsider, chess::net::ResignMsg{});
    match_->handleMessage(outsider, chess::net::DrawAcceptMsg{});
    match_->handleMessage(outsider, chess::net::DrawDeclineMsg{});

    EXPECT_TRUE(match_->isActive());
    // Nothing was sent to the real seats either.
    EXPECT_FALSE(tryRecvOnClient(*clientWhite_).has_value());
    EXPECT_FALSE(tryRecvOnClient(*clientBlack_).has_value());
}

// Audit 1.6: a draw offer does not survive the opponent's disconnect —
// the offer notice never reached the absent seat, so a blind accept on
// the resumed link must be refused.
TEST_F(MatchTest, DrawOfferDissolvesOnOpponentDisconnect)
{
    match_->handleMessage(white_, chess::net::DrawOfferMsg{});
    auto offer = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(offer.has_value());
    EXPECT_TRUE(std::get_if<chess::net::ServerDrawOfferMsg>(&*offer));

    match_->handleDisconnect(black_);
    EXPECT_TRUE(match_->isActive()); // grace window, zero ms in tests

    // The returning seat tries the blind accept — refused.
    match_->handleMessage(black_, chess::net::DrawAcceptMsg{});
    auto msg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "No draw offer pending");
    EXPECT_TRUE(match_->isActive());
}

TEST_F(MatchTest, ChatRelay)
{
    match_->handleMessage(white_, chess::net::ChatMsg{"hello"});

    auto msg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(msg.has_value());
    auto* chat = std::get_if<chess::net::ServerChatMsg>(&*msg);
    ASSERT_NE(chat, nullptr);
    EXPECT_EQ(chat->name, "Alice");
    EXPECT_EQ(chat->text, "hello");
}

TEST_F(MatchTest, ChatTooLong)
{
    std::string longMsg(501, 'x');
    match_->handleMessage(white_, chess::net::ChatMsg{longMsg});

    auto msg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "Message too long");
}

// 11.2: a disconnect no longer ends the game — the absent seat runs out
// its grace window (clock keeps ticking) while the opponent waits, and a
// token rejoin rebinds the seat. Only a lapsed window or a second
// departure is final.

TEST_F(MatchTest, DisconnectHoldsMatchInReconnectGrace)
{
    match_->handleDisconnect(white_);
    EXPECT_TRUE(match_->isActive());

    // The survivor is told about the window, not the game end.
    auto bMsg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(bMsg.has_value());
    auto* rc = std::get_if<chess::net::OpponentReconnectingMsg>(&*bMsg);
    ASSERT_NE(rc, nullptr);
    EXPECT_EQ(rc->graceSeconds, static_cast<std::int32_t>((testGraceMs_ + 999) / 1000));

    // The absent seat is gone, the seated one still here.
    EXPECT_FALSE(match_->seat(chess::Color::White).present);
    EXPECT_TRUE(match_->seat(chess::Color::Black).present);
    EXPECT_EQ(match_->white(), nullptr);
    EXPECT_EQ(match_->black(), &black_);
}

TEST_F(MatchTest, GraceExpiryEndsGameWithOpponentWin)
{
    match_->handleDisconnect(white_);

    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    match_->pollClock(now);
    match_->pollReconnect(now);
    EXPECT_FALSE(match_->isActive());
    EXPECT_TRUE(match_->reconnecting() == false);

    const auto over = match_->lastGameOver();
    ASSERT_TRUE(over.has_value());
    EXPECT_EQ(over->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Disconnection);

    // The socket may still hold the reconnect notice; skip to the end.
    std::optional<chess::net::GameOverMsg> bOver;
    for (int i = 0; i < 8; ++i) {
        auto m = tryRecvOnClient(*clientBlack_);
        if (m) {
            if (auto* go = std::get_if<chess::net::GameOverMsg>(&*m)) {
                bOver = *go;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(bOver.has_value());
    EXPECT_EQ(bOver->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(bOver->reason, chess::net::GameOverReason::Disconnection);
}

TEST_F(MatchTest, GraceBudgetShrinksRepeatedDrops)
{
    // The re-arm budget: a match tolerates 2x graceMs_ of CUMULATIVE
    // absence. Two drops each burning ~250 ms of a 400 ms window leave
    // ~300 ms for the third drop — which expires at +350 ms where a full
    // re-armed window (400 ms) would still be running.
    constexpr std::int64_t graceMs = 400;
    Match m(white_, black_, std::string(), std::string(), graceMs);

    auto nowMs = [] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    };
    Client rejoiner; // fresh connection object, no socket needed
    rejoiner.name = "Ada";

    for (int drop = 0; drop < 2; ++drop) {
        // After the first resume the rejoiner owns seat 0 — white_'s
        // departure is a silent no-op from then on.
        m.handleDisconnect(drop == 0 ? white_ : rejoiner);
        ASSERT_TRUE(m.isActive());
        // The survivor sees the full window on an unconsumed budget.
        auto rc = tryRecvOnClient(*clientBlack_);
        ASSERT_TRUE(rc.has_value());
        auto* notice = std::get_if<chess::net::OpponentReconnectingMsg>(&*rc);
        ASSERT_NE(notice, nullptr);
        EXPECT_EQ(notice->graceSeconds,
                  static_cast<std::int32_t>((graceMs + 999) / 1000));

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        // Still inside the window (a full 400 ms runs; 250 ms elapsed).
        m.pollClock(nowMs());
        m.pollReconnect(nowMs());
        ASSERT_TRUE(m.isActive());

        const auto outcome = m.resume(m.resumeToken(chess::Color::White), rejoiner);
        ASSERT_EQ(outcome, Match::ResumeOutcome::Rebound);
        // Drain the resumed notice so the next drop's is next in line.
        while (tryRecvOnClient(*clientBlack_).has_value())
            ;
    }

    // Third drop: the budget has ~300 ms left, so the window shrinks below
    // graceMs and expires within 350 ms — a full re-arm would not.
    m.handleDisconnect(rejoiner);
    ASSERT_TRUE(m.isActive());
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    m.pollClock(nowMs());
    m.pollReconnect(nowMs());
    EXPECT_FALSE(m.isActive());

    const auto over = m.lastGameOver();
    ASSERT_TRUE(over.has_value());
    EXPECT_EQ(over->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Disconnection);
}

TEST_F(MatchTest, GraceBudgetForfeitsWhenSpent)
{
    // Zero grace keeps the legacy semantics (deadline = drop time; the
    // next poll decides) — the budget must not turn a zero-window drop
    // into a forfeit at drop time.
    {
        Match m(white_, black_, std::string(), std::string(), 0);
        m.handleDisconnect(white_);
        EXPECT_TRUE(m.isActive());
    }

    // A spent budget forfeits immediately: each absence books its real
    // span, so four short drop-resume cycles spend the whole 2x budget and
    // the fifth drop gets no window at all.
    constexpr std::int64_t graceMs = 100;
    Match m(white_, black_, std::string(), std::string(), graceMs);
    Client rejoiner;
    rejoiner.name = "Ada";

    auto nowMs = [] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    };

    for (int drop = 0; drop < 4; ++drop) {
        m.handleDisconnect(drop == 0 ? white_ : rejoiner);
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        m.pollClock(nowMs());
        m.pollReconnect(nowMs());
        // Each absence burns ~60 ms of the 200 ms budget; once the spent
        // total passes it, a poll (or the next drop) ends the game — both
        // converge on the assertions below, so only resume while alive.
        if (!m.isActive()) break;
        ASSERT_EQ(m.resume(m.resumeToken(chess::Color::White), rejoiner),
                  Match::ResumeOutcome::Rebound);
        while (tryRecvOnClient(*clientBlack_).has_value())
            ;
    }

    // The budget is spent: the next drop forfeits on the spot, with no
    // reconnect notice — only the game-over broadcast.
    if (m.isActive()) {
        m.handleDisconnect(white_);
        EXPECT_FALSE(m.isActive());
    }
    const auto over = m.lastGameOver();
    ASSERT_TRUE(over.has_value());
    EXPECT_EQ(over->result, chess::net::GameResult::BlackWins);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Disconnection);
}

TEST_F(MatchTest, BotEngineStartFailureResigns)
{
    // An engine-less host: the start failure used to leave the bot seat
    // permanently unable to move — an unlimited game hung forever (the 1.3
    // tri-state only covered mid-search deaths).
    match_->startBot(chess::Color::Black, 1, "/nonexistent/engine");
    EXPECT_FALSE(match_->isActive());

    const auto over = match_->lastGameOver();
    ASSERT_TRUE(over.has_value());
    EXPECT_EQ(over->result, chess::net::GameResult::WhiteWins);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Resignation);

    // The human seat learns the game is over.
    std::optional<chess::net::GameOverMsg> wOver;
    for (int i = 0; i < 8 && !wOver.has_value(); ++i) {
        if (auto m = tryRecvOnClient(*clientWhite_))
            if (auto* go = std::get_if<chess::net::GameOverMsg>(&*m))
                wOver = *go;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(wOver.has_value());
    EXPECT_EQ(wOver->result, chess::net::GameResult::WhiteWins);
}

// The record cap: sanHistory feeds ResumeState/TakebackAccepted/Spectate
// packets whose wire guard refuses counts past MaxResumeMoves — reaching
// ply 512 used to desync takeback and strand a rejoining seat with
// packets its own client rejects. The 513rd move attempt now ends the
// game as a draw instead of ever producing an oversize packet.
TEST_F(MatchTest, MoveRecordCapAutoDraws)
{
    // A quiet-ish long game: pawn advances and (late) captures reset the
    // fifty-move clock long before it reaches 100, material stays rich
    // (only warm-epoch captures happen, so InsufficientMaterial can never
    // fire), the chooser refuses any move that leaves the opponent
    // without a legal move (no stalemate/checkmate) and never plays a
    // third occurrence of a position (no threefold). That lets the game
    // legally run past the cap, where the record guard must end it as a
    // draw.
    auto start = chess::Board::fromFen(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1");
    ASSERT_TRUE(start.has_value());
    chess::Board b = *start;

    Match m(white_, black_, std::string(), std::string(), 0);

    // Position identity for the threefold count: placement, side,
    // castling and en-passant — the move counters never affect it.
    auto identity = [](const chess::Board& bd) {
        const std::string fen = bd.toFen();
        std::size_t end = 0;
        for (int field = 0; field < 4 && end != std::string::npos; ++field)
            end = fen.find(' ', end + 1);
        return end == std::string::npos ? fen : fen.substr(0, end);
    };

    std::map<std::string, int> seen;
    ++seen[identity(b)];

    for (int ply = 0; ply < 640 && m.isActive(); ++ply) {
        const auto moves = chess::generateLegalMoves(b);
        ASSERT_FALSE(moves.empty());

        const bool clockWarm = b.halfmoveClock() >= 50;
        const chess::Move* pick = nullptr;
        int pickScore = 0;
        for (const auto& mv : moves) {
            chess::Board scratch = b;
            scratch.makeMove(mv);
            if (chess::generateLegalMoves(scratch).empty())
                continue; // never stalemate or mate the opponent
            const auto [it, inserted] =
                seen.try_emplace(identity(scratch), 1);
            if (!inserted && it->second >= 2)
                continue; // never a third occurrence
            const bool resets =
                (b.pieceAt(mv.from).type == chess::PieceType::Pawn
                 && !mv.isCapture())
                || mv.isCapture(); // captures reset the clock too
            int score = it->second; // prefer fresh positions
            if (clockWarm) {
                if (!resets)
                    score += 10; // reset plies win the warm epoch
            } else if (mv.isCapture()) {
                score += 100; // never eat material while the clock is cold
            }
            if (!pick || score < pickScore) {
                pick = &mv;
                pickScore = score;
            }
        }
        if (!pick) FAIL() << "chooser cornered at ply " << ply;

        Client& mover =
            b.sideToMove() == chess::Color::White ? white_ : black_;
        const std::string san = chess::san::toSan(b, *pick);
        m.handleMessage(mover, chess::net::MoveMsg{san});
        if (m.isActive()) {
            b.makeMove(*pick);
            ++seen[identity(b)];
        }
    }

    // The game must have been ended BY THE CAP, with exactly the capped
    // number of plies recorded and no oversize packet ever possible.
    ASSERT_FALSE(m.isActive());
    EXPECT_EQ(m.sanHistory().size(),
              static_cast<std::size_t>(chess::net::MaxResumeMoves));
    const auto over = m.lastGameOver();
    ASSERT_TRUE(over.has_value());
    EXPECT_EQ(over->result, chess::net::GameResult::Draw);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::AgreedDraw);
}

TEST_F(MatchTest, ResumeRebindsSeatAndNotifiesOpponent)
{
    match_->handleDisconnect(white_);

    Client carol; // fresh connection, no socket needed for the match logic
    carol.name = "Carol";
    match_->resume(match_->resumeToken(chess::Color::White), carol);

    EXPECT_TRUE(match_->isActive());
    EXPECT_EQ(match_->white(), &carol);
    EXPECT_TRUE(match_->seat(chess::Color::White).present);
    EXPECT_FALSE(match_->reconnecting());

    // The survivor learns play may continue.
    auto bMsg = tryRecvOnClient(*clientBlack_);
    bool resumed = false;
    while (auto m = tryRecvOnClient(*clientBlack_))
        if (std::holds_alternative<chess::net::OpponentResumedMsg>(*m))
            resumed = true;
    EXPECT_TRUE(resumed);

    // The rebound seat is fully seated: they are the side to move and their
    // move goes through (no "Not your turn").
    match_->handleMessage(carol, chess::net::MoveMsg{"e4"});
    auto bMsg2 = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(bMsg2.has_value());
    auto* move = std::get_if<chess::net::ServerMoveMsg>(&*bMsg2);
    ASSERT_NE(move, nullptr);
    EXPECT_EQ(move->san, "e4");
}

TEST_F(MatchTest, ResumeWithWrongTokenDoesNothing)
{
    match_->handleDisconnect(white_);

    Client carol;
    carol.name = "Carol";
    const auto outcome = match_->resume("no-such-token", carol);
    EXPECT_EQ(outcome, Match::ResumeOutcome::NoSeat);

    // Still absent; grace still armed; the stranger was not seated.
    EXPECT_EQ(match_->white(), nullptr);
    EXPECT_FALSE(match_->seat(chess::Color::White).present);
}

TEST_F(MatchTest, ResumeAlreadyConnectedRefused)
{
    // A live seat's token played back on a second connection is refused.
    Client carol;
    carol.name = "Carol";
    const auto outcome =
        match_->resume(match_->resumeToken(chess::Color::White), carol);
    EXPECT_EQ(outcome, Match::ResumeOutcome::AlreadyConnected);
    EXPECT_EQ(match_->white(), &white_);
}

TEST_F(MatchTest, DoubleDisconnectEndsGameUnratedAbort)
{
    match_->handleDisconnect(white_);
    match_->handleDisconnect(black_);

    EXPECT_FALSE(match_->isActive());
    const auto over = match_->lastGameOver();
    ASSERT_TRUE(over.has_value());
    // Nobody stayed behind: nobody wins. Aborted games are never rated.
    EXPECT_EQ(over->result, chess::net::GameResult::Abort);
    EXPECT_EQ(over->reason, chess::net::GameOverReason::Disconnection);
}

TEST_F(MatchTest, FinishedGameMatchesTokenForLateResume)
{
    match_->handleMessage(white_, chess::net::ResignMsg{});
    EXPECT_FALSE(match_->isActive());

    const std::string loserToken = match_->resumeToken(chess::Color::White);
    Client carol;
    carol.name = "Carol";
    EXPECT_EQ(match_->resume(loserToken, carol),
              Match::ResumeOutcome::ReboundOver);
    EXPECT_TRUE(match_->lastGameOver().has_value());
}


TEST_F(MatchTest, PingPong)
{
    match_->handleMessage(white_, chess::net::PingMsg{});

    auto msg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(msg.has_value());
    EXPECT_TRUE(std::get_if<chess::net::PongMsg>(&*msg));
}

TEST_F(MatchTest, MessageAfterGameOver)
{
    match_->handleMessage(white_, chess::net::ResignMsg{});
    EXPECT_FALSE(match_->isActive());

    for (int i = 0; i < 10; ++i) {
        tryRecvOnClient(*clientWhite_);
        tryRecvOnClient(*clientBlack_);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});
    auto msg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "Game is over");
}

// ── Takeback (11.3) ─────────────────────────────────────────────────────────

TEST_F(MatchTest, TakebackRequestRequiresTwoPlies)
{
    match_->handleMessage(white_, chess::net::TakebackRequestMsg{});
    auto msg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "No move to take back");
    EXPECT_FALSE(match_->takebackPending());
}

TEST_F(MatchTest, TakebackRequestRequiresOwnTurn)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});
    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});
    // Both on the table: black just moved, so white is on the move and
    // black's request sits in the wrong frame. Both sockets hold the two
    // echoes first.
    match_->handleMessage(black_, chess::net::TakebackRequestMsg{});
    for (auto sock : {clientWhite_.get(), clientBlack_.get()}) {
        tryRecvOnClient(*sock);
        tryRecvOnClient(*sock);
        auto msg = tryRecvOnClient(*sock);

        if (sock == clientBlack_.get()) {
            ASSERT_TRUE(msg.has_value());
            auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
            ASSERT_NE(err, nullptr);
            EXPECT_EQ(err->message, "Not your turn");
        } else {
            EXPECT_FALSE(msg.has_value());
        }
    }
    EXPECT_FALSE(match_->takebackPending());
}

TEST_F(MatchTest, TakebackRequestForwardedAndPending)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});
    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});
    tryRecvOnClient(*clientWhite_); // echo e4
    tryRecvOnClient(*clientBlack_); // echo e4
    tryRecvOnClient(*clientWhite_); // echo e5
    tryRecvOnClient(*clientBlack_); // echo e5

    match_->handleMessage(white_, chess::net::TakebackRequestMsg{});
    EXPECT_TRUE(match_->takebackPending());

    auto ask = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(ask.has_value());
    EXPECT_TRUE(std::get_if<chess::net::ServerTakebackRequestMsg>(&*ask));
    // The asker gets nothing but quiet; the pending flag carries the state.
    EXPECT_EQ(tryRecvOnClient(*clientWhite_), std::nullopt);
}

TEST_F(MatchTest, TakebackDuplicateRequestErrors)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});
    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});
    for (auto sock : {clientWhite_.get(), clientBlack_.get()}) {
        tryRecvOnClient(*sock);
        tryRecvOnClient(*sock);
    }

    match_->handleMessage(white_, chess::net::TakebackRequestMsg{});
    match_->handleMessage(white_, chess::net::TakebackRequestMsg{});
    auto msg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "A takeback is already pending");
    EXPECT_TRUE(match_->takebackPending());
}

TEST_F(MatchTest, TakebackAcceptedRollsTwoPliesBack)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});
    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});
    match_->handleMessage(white_, chess::net::MoveMsg{"Nf3"});
    match_->handleMessage(black_, chess::net::MoveMsg{"Nc6"});
    for (auto sock : {clientWhite_.get(), clientBlack_.get()}) {
        for (int i = 0; i < 4; ++i)
            tryRecvOnClient(*sock);
    }

    match_->handleMessage(white_, chess::net::TakebackRequestMsg{});
    tryRecvOnClient(*clientBlack_); // ServerTakebackRequestMsg
    match_->handleMessage(black_, chess::net::TakebackAcceptMsg{});

    for (auto& sock : {clientWhite_.get(), clientBlack_.get()}) {
        auto msg = tryRecvOnClient(*sock);
        ASSERT_TRUE(msg.has_value());
        auto* accepted = std::get_if<chess::net::TakebackAcceptedMsg>(&*msg);
        ASSERT_NE(accepted, nullptr);
        ASSERT_EQ(accepted->sans.size(), 2u);
        EXPECT_EQ(accepted->sans[0], "e4");
        EXPECT_EQ(accepted->sans[1], "e5");
    }
    // The rollback retracts Nf3 and Nc6: position after 1.e4 e5, white
    // (the asker) to move.
    EXPECT_EQ(match_->currentFen(),
              "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2");
    EXPECT_EQ(match_->sanHistory().size(), 2u);
    EXPECT_FALSE(match_->takebackPending());
    EXPECT_TRUE(match_->isActive());
}

TEST_F(MatchTest, TakebackDeclinedNotifiesAskerOnly)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});
    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});
    for (auto sock : {clientWhite_.get(), clientBlack_.get()}) {
        tryRecvOnClient(*sock);
        tryRecvOnClient(*sock);
    }

    match_->handleMessage(white_, chess::net::TakebackRequestMsg{});
    tryRecvOnClient(*clientBlack_);
    match_->handleMessage(black_, chess::net::TakebackDeclineMsg{});

    auto msg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(msg.has_value());
    ASSERT_TRUE(std::get_if<chess::net::ServerTakebackDeclineMsg>(&*msg));
    // The history and the position never moved.
    EXPECT_EQ(match_->sanHistory().size(), 2u);
    EXPECT_FALSE(match_->takebackPending());
    // Black's decline answer stops at the white socket: nothing for black.
    EXPECT_EQ(tryRecvOnClient(*clientBlack_), std::nullopt);
}

TEST_F(MatchTest, TakebackAskerCannotSelfAccept)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});
    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});
    for (auto sock : {clientWhite_.get(), clientBlack_.get()}) {
        tryRecvOnClient(*sock);
        tryRecvOnClient(*sock);
    }

    match_->handleMessage(white_, chess::net::TakebackRequestMsg{});
    match_->handleMessage(white_, chess::net::TakebackAcceptMsg{});
    auto msg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "No takeback request pending");
    EXPECT_EQ(match_->sanHistory().size(), 2u);
}

TEST_F(MatchTest, TakebackAskerSelfCancelsViaDecline)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});
    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});
    for (auto sock : {clientWhite_.get(), clientBlack_.get()}) {
        tryRecvOnClient(*sock);
        tryRecvOnClient(*sock);
    }

    match_->handleMessage(white_, chess::net::TakebackRequestMsg{});
    tryRecvOnClient(*clientBlack_); // the forwarded request
    match_->handleMessage(white_, chess::net::TakebackDeclineMsg{});
    EXPECT_FALSE(match_->takebackPending());
    // The cancellation notice lands on the asker's own socket, read as a
    // plain "declined" line there.
    auto msg = tryRecvOnClient(*clientWhite_);
    ASSERT_TRUE(msg.has_value());
    ASSERT_TRUE(std::get_if<chess::net::ServerTakebackDeclineMsg>(&*msg));
    EXPECT_EQ(tryRecvOnClient(*clientBlack_), std::nullopt);
}

TEST_F(MatchTest, AcceptedMoveDissolvesPendingTakeback)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});
    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});
    for (auto sock : {clientWhite_.get(), clientBlack_.get()}) {
        tryRecvOnClient(*sock);
        tryRecvOnClient(*sock);
    }

    match_->handleMessage(white_, chess::net::TakebackRequestMsg{});
    EXPECT_TRUE(match_->takebackPending());
    // White changes their mind and simply plays on.
    match_->handleMessage(white_, chess::net::MoveMsg{"Nf3"});
    EXPECT_FALSE(match_->takebackPending());

    // Black's cached prompt is stale: accepting now errors politely.
    // Black's queue: the forwarded request, the Nf3 echo, then the error.
    match_->handleMessage(black_, chess::net::TakebackAcceptMsg{});
    auto fwd = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(fwd.has_value());
    EXPECT_TRUE(std::get_if<chess::net::ServerTakebackRequestMsg>(&*fwd));
    auto echo = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(echo.has_value());
    auto* echoMove = std::get_if<chess::net::ServerMoveMsg>(&*echo);
    ASSERT_NE(echoMove, nullptr);
    EXPECT_EQ(echoMove->san, "Nf3");
    auto msg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "No takeback request pending");
    tryRecvOnClient(*clientWhite_); // white's Nf3 echo, back to quiet
}

TEST_F(MatchTest, DisconnectDissolvesPendingTakeback)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});
    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});
    for (auto sock : {clientWhite_.get(), clientBlack_.get()}) {
        tryRecvOnClient(*sock);
        tryRecvOnClient(*sock);
    }

    match_->handleMessage(white_, chess::net::TakebackRequestMsg{});
    EXPECT_TRUE(match_->takebackPending());
    match_->handleDisconnect(black_);
    EXPECT_FALSE(match_->takebackPending());
}

TEST_F(MatchTest, TakebackWhileOpponentAwayFailsFast)
{
    match_->handleMessage(white_, chess::net::MoveMsg{"e4"});
    match_->handleMessage(black_, chess::net::MoveMsg{"e5"});
    match_->handleMessage(white_, chess::net::MoveMsg{"Nf3"});
    match_->handleDisconnect(white_); // black is on the move, three plies deep
    for (auto sock : {clientWhite_.get(), clientBlack_.get()}) {
        for (int i = 0; i < 3; ++i)
            tryRecvOnClient(*sock);
    }
    tryRecvOnClient(*clientBlack_); // OpponentReconnecting notice

    // The frame checks pass (on the move, >= 2 plies); the absent seat is
    // the gate that fails fast.
    match_->handleMessage(black_, chess::net::TakebackRequestMsg{});
    auto msg = tryRecvOnClient(*clientBlack_);
    ASSERT_TRUE(msg.has_value());
    auto* err = std::get_if<chess::net::ErrorMsg>(&*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->message, "The opponent is away");
    EXPECT_FALSE(match_->takebackPending());
}

// ── 12.1: Chess960 start positions ──────────────────────────────────────────

namespace {

// First legal SAN in `match`'s current position (arbitrary but stable).
std::string firstLegalSan(const Match& match)
{
    auto board = chess::fromFen(match.currentFen());
    for (const auto& m : chess::generateLegalMoves(*board)) {
        std::string san = chess::san::toSan(*board, m);
        if (chess::san::fromSan(*board, san).has_value()) return san;
    }
    return {};
}

} // namespace

class Chess960MatchTest : public MatchTest {
protected:
    // SP 3 = "BQNNRKRB": inner rooks around a king already on the f-file —
    // exercise letters-castling geometry end to end through the match.
    static constexpr int kStartId = 3;

    void SetUp() override {
        MatchTest::SetUp();  // wires the sockets; replaces the standard match
        auto fen = chess::chess960StartFen(kStartId);
        ASSERT_TRUE(fen.has_value());
        startFen_ = *fen;
        match_ = std::make_unique<Match>(white_, black_, std::string(),
                                         startFen_, testGraceMs_);
    }

    std::string startFen_;
};

TEST_F(Chess960MatchTest, BoardStartsFromCarriedStartFen)
{
    EXPECT_EQ(match_->currentFen(), startFen_);
    EXPECT_EQ(match_->startFen(), startFen_);
    EXPECT_TRUE(match_->isActive());
}

TEST_F(Chess960MatchTest, LegalMovePlaysAndRecomputesPosition)
{
    const std::string san = firstLegalSan(*match_);
    ASSERT_FALSE(san.empty());

    match_->handleMessage(white_, chess::net::MoveMsg{san});

    auto bMsg = recvOnClient(*clientBlack_);
    ASSERT_TRUE(bMsg.has_value());
    auto* move = std::get_if<chess::net::ServerMoveMsg>(&*bMsg);
    ASSERT_NE(move, nullptr);
    EXPECT_EQ(move->san, san);

    // The server's board equals the same SAN replayed on a scratch FRC
    // seed board.
    auto scratch = chess::fromFen(startFen_);
    auto moveObj = chess::san::fromSan(*scratch, san);
    ASSERT_TRUE(moveObj.has_value());
    scratch->makeMove(*moveObj);
    EXPECT_EQ(match_->currentFen(), scratch->toFen());
}

TEST_F(Chess960MatchTest, TakebackRollbackRechainsFromFrcStart)
{
    const std::string whiteSan = firstLegalSan(*match_);
    ASSERT_FALSE(whiteSan.empty());
    match_->handleMessage(white_, chess::net::MoveMsg{whiteSan});
    const std::string blackSan = firstLegalSan(*match_);
    ASSERT_FALSE(blackSan.empty());
    match_->handleMessage(black_, chess::net::MoveMsg{blackSan});
    for (auto sock : {clientWhite_.get(), clientBlack_.get()}) {
        tryRecvOnClient(*sock);
        tryRecvOnClient(*sock);
    }
    ASSERT_EQ(match_->sanHistory().size(), 2u);

    match_->handleMessage(white_, chess::net::TakebackRequestMsg{});
    tryRecvOnClient(*clientBlack_); // ServerTakebackRequestMsg
    match_->handleMessage(black_, chess::net::TakebackAcceptMsg{});

    for (auto& sock : {clientWhite_.get(), clientBlack_.get()}) {
        auto msg = tryRecvOnClient(*sock);
        ASSERT_TRUE(msg.has_value());
        auto* accepted = std::get_if<chess::net::TakebackAcceptedMsg>(&*msg);
        ASSERT_NE(accepted, nullptr);
        ASSERT_EQ(accepted->sans.size(), 0u);
    }
    // The two-ply retraction rebuilt the board from the FRC SP — a replay
    // seeded from the standard start position would diverge (and desync).
    EXPECT_EQ(match_->currentFen(), startFen_);
    EXPECT_TRUE(match_->isActive());
}

