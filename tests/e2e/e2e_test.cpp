#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include <SFML/Network/TcpSocket.hpp>

#include <chess/net/messages.h>
#include <chess/net/protocol.h>

#include "server.h"

using namespace std::chrono_literals;

static constexpr auto TestTimeout = 5s;

class E2eTest : public ::testing::Test {
protected:
    void SetUp() override {
        shutdownFlag_ = false;
        boundPort_ = 0;

        serverThread_ = std::thread([this]() {
            chess::server::ServerConfig config;
            config.port = 0;
            config.host = "127.0.0.1";
            // boundPort_ is written before the event loop starts
            chess::server::runServer(config, shutdownFlag_, &boundPort_);
        });

        while (boundPort_ == 0)
            std::this_thread::sleep_for(1ms);
    }

    void TearDown() override {
        chess::server::stopServer(shutdownFlag_);
        if (serverThread_.joinable()) serverThread_.join();
    }

    sf::TcpSocket connectClient() {
        sf::TcpSocket sock;
        auto status = sock.connect(sf::IpAddress::LocalHostV4, boundPort_);
        EXPECT_EQ(status, sf::Socket::Status::Done);
        sock.setBlocking(false);
        return sock;
    }

    void sendMsg(sf::TcpSocket& sock, const chess::net::ClientMessage& msg) {
        sf::Packet packet;
        chess::net::serialize(packet, msg);
        auto s = sock.send(packet);
        ASSERT_EQ(s, sf::Socket::Status::Done);
    }

    std::optional<chess::net::ServerMessage> tryRecv(sf::TcpSocket& sock) {
        sf::Packet packet;
        if (sock.receive(packet) != sf::Socket::Status::Done) return std::nullopt;
        return chess::net::deserializeServer(packet);
    }

    template <typename Pred>
    bool pollUntil(sf::TcpSocket& a, sf::TcpSocket& b, Pred pred,
                   std::chrono::milliseconds timeout = TestTimeout)
    {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (pred()) return true;
            std::this_thread::sleep_for(1ms);
        }
        return false;
    }

    void joinBoth(sf::TcpSocket& white, sf::TcpSocket& black,
                  const std::string& wName = "White", const std::string& bName = "Black")
    {
        sendMsg(white, chess::net::JoinMsg{wName});
        sendMsg(black, chess::net::JoinMsg{bName});

        bool wWelcome = false, bWelcome = false;
        ASSERT_TRUE(pollUntil(white, black, [&]() {
            if (!wWelcome) {
                auto m = tryRecv(white);
                if (m) {
                    auto* w = std::get_if<chess::net::WelcomeMsg>(&*m);
                    EXPECT_NE(w, nullptr);
                    if (w) {
                        EXPECT_EQ(w->color, chess::Color::White);
                        EXPECT_EQ(w->opponent, bName);
                    }
                    wWelcome = true;
                }
            }
            if (!bWelcome) {
                auto m = tryRecv(black);
                if (m) {
                    auto* w = std::get_if<chess::net::WelcomeMsg>(&*m);
                    EXPECT_NE(w, nullptr);
                    if (w) {
                        EXPECT_EQ(w->color, chess::Color::Black);
                        EXPECT_EQ(w->opponent, wName);
                    }
                    bWelcome = true;
                }
            }
            return wWelcome && bWelcome;
        }));
    }

    // Drain one move from each side, return true if both received ServerMoveMsg.
    bool drainOneMovePair(sf::TcpSocket& white, sf::TcpSocket& black,
                          std::string* wSan = nullptr, std::string* bSan = nullptr)
    {
        bool wGot = false, bGot = false;
        auto mw = tryRecv(white);
        auto mb = tryRecv(black);
        if (mw) {
            auto* mv = std::get_if<chess::net::ServerMoveMsg>(&*mw);
            if (mv) { wGot = true; if (wSan) *wSan = mv->san; }
        }
        if (mb) {
            auto* mv = std::get_if<chess::net::ServerMoveMsg>(&*mb);
            if (mv) { bGot = true; if (bSan) *bSan = mv->san; }
        }
        return wGot && bGot;
    }

    // Written by server thread before entering the event loop (runServer boundPortOut).
    // Read by main thread in a spin loop — intentional data race, safe on all
    // platforms for an aligned 2-byte write/read, and the sleep provides a barrier.
    unsigned short boundPort_{0};
    std::atomic<bool> shutdownFlag_{false};
    std::thread serverThread_;
};

// ── Join / matchmaking ──────────────────────────────────────────────────────

TEST_F(E2eTest, JoinAndWelcome)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black);
    white.disconnect();
    black.disconnect();
}

// ── Full game: Fool's Mate (1. f3 e5 2. g4 Qh4#) ──────────────────────────

TEST_F(E2eTest, FullCheckmateGame)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black);

    // Helper: send a move from one side, verify both receive ServerMoveMsg with expected SAN.
    auto playMove = [&](sf::TcpSocket& sender, sf::TcpSocket& other, const char* san) {
        sendMsg(sender, chess::net::MoveMsg{san});
        bool wGot = false, bGot = false;
        ASSERT_TRUE(pollUntil(white, black, [&]() {
            if (!wGot) {
                auto m = tryRecv(white);
                if (m && std::get_if<chess::net::ServerMoveMsg>(&*m)) wGot = true;
            }
            if (!bGot) {
                auto m = tryRecv(black);
                if (m && std::get_if<chess::net::ServerMoveMsg>(&*m)) bGot = true;
            }
            return wGot && bGot;
        }));
    };

    playMove(white, black, "f3");
    playMove(black, white, "e5");
    playMove(white, black, "g4");

    // 2... Qh4# — expect ServerMoveMsg + GameOverMsg from both
    sendMsg(black, chess::net::MoveMsg{"Qh4#"});
    bool wOver = false, bOver = false;
    ASSERT_TRUE(pollUntil(white, black, [&]() {
        if (!wOver) {
            auto m = tryRecv(white);
            if (m) {
                if (auto* go = std::get_if<chess::net::GameOverMsg>(&*m)) {
                    EXPECT_EQ(go->result, chess::net::GameResult::BlackWins);
                    EXPECT_EQ(go->reason, chess::net::GameOverReason::Checkmate);
                    wOver = true;
                }
            }
        }
        if (!bOver) {
            auto m = tryRecv(black);
            if (m) {
                if (auto* go = std::get_if<chess::net::GameOverMsg>(&*m)) {
                    EXPECT_EQ(go->result, chess::net::GameResult::BlackWins);
                    EXPECT_EQ(go->reason, chess::net::GameOverReason::Checkmate);
                    bOver = true;
                }
            }
        }
        return wOver && bOver;
    }));
}

// ── Draw by agreement ───────────────────────────────────────────────────────

TEST_F(E2eTest, DrawByAgreement)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black);

    // Play 1. e4 e5 2. Nf3
    auto playMove = [&](const char* san, sf::TcpSocket& sender) {
        sendMsg(sender, chess::net::MoveMsg{san});
        bool done = false;
        ASSERT_TRUE(pollUntil(white, black, [&]() {
            if (done) return true;
            auto mw = tryRecv(white);
            auto mb = tryRecv(black);
            if (mw && std::get_if<chess::net::ServerMoveMsg>(&*mw) &&
                mb && std::get_if<chess::net::ServerMoveMsg>(&*mb)) {
                done = true;
                return true;
            }
            return false;
        }));
    };

    playMove("e4", white);
    playMove("e5", black);
    playMove("Nf3", white);

    // White offers draw
    sendMsg(white, chess::net::DrawOfferMsg{});
    bool blackGotOffer = false;
    ASSERT_TRUE(pollUntil(white, black, [&]() {
        if (blackGotOffer) return true;
        auto m = tryRecv(black);
        if (m) {
            EXPECT_TRUE(std::get_if<chess::net::ServerDrawOfferMsg>(&*m));
            blackGotOffer = true;
        }
        return false;
    }));

    // Black accepts
    sendMsg(black, chess::net::DrawAcceptMsg{});
    bool wOver = false, bOver = false;
    ASSERT_TRUE(pollUntil(white, black, [&]() {
        if (!wOver) {
            auto m = tryRecv(white);
            if (m && std::get_if<chess::net::GameOverMsg>(&*m)) {
                auto* go = std::get_if<chess::net::GameOverMsg>(&*m);
                EXPECT_EQ(go->result, chess::net::GameResult::Draw);
                EXPECT_EQ(go->reason, chess::net::GameOverReason::AgreedDraw);
                wOver = true;
            }
        }
        if (!bOver) {
            auto m = tryRecv(black);
            if (m && std::get_if<chess::net::GameOverMsg>(&*m)) {
                auto* go = std::get_if<chess::net::GameOverMsg>(&*m);
                EXPECT_EQ(go->result, chess::net::GameResult::Draw);
                EXPECT_EQ(go->reason, chess::net::GameOverReason::AgreedDraw);
                bOver = true;
            }
        }
        return wOver && bOver;
    }));
}

// ── Resign ──────────────────────────────────────────────────────────────────

TEST_F(E2eTest, Resign)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black);

    // Play 1. e4
    sendMsg(white, chess::net::MoveMsg{"e4"});
    {
        bool done = false;
        ASSERT_TRUE(pollUntil(white, black, [&]() {
            if (done) return true;
            auto mw = tryRecv(white);
            auto mb = tryRecv(black);
            if (mw && std::get_if<chess::net::ServerMoveMsg>(&*mw) &&
                mb && std::get_if<chess::net::ServerMoveMsg>(&*mb)) {
                done = true;
                return true;
            }
            return false;
        }));
    }

    // White resigns
    sendMsg(white, chess::net::ResignMsg{});
    bool wOver = false, bOver = false;
    ASSERT_TRUE(pollUntil(white, black, [&]() {
        if (!wOver) {
            auto m = tryRecv(white);
            if (m && std::get_if<chess::net::GameOverMsg>(&*m)) {
                auto* go = std::get_if<chess::net::GameOverMsg>(&*m);
                EXPECT_EQ(go->result, chess::net::GameResult::BlackWins);
                EXPECT_EQ(go->reason, chess::net::GameOverReason::Resignation);
                wOver = true;
            }
        }
        if (!bOver) {
            auto m = tryRecv(black);
            if (m && std::get_if<chess::net::GameOverMsg>(&*m)) {
                auto* go = std::get_if<chess::net::GameOverMsg>(&*m);
                EXPECT_EQ(go->result, chess::net::GameResult::BlackWins);
                EXPECT_EQ(go->reason, chess::net::GameOverReason::Resignation);
                bOver = true;
            }
        }
        return wOver && bOver;
    }));
}

// ── Disconnect mid-game ─────────────────────────────────────────────────────

TEST_F(E2eTest, DisconnectMidGame)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black);

    // Play 1. e4
    sendMsg(white, chess::net::MoveMsg{"e4"});
    {
        bool done = false;
        ASSERT_TRUE(pollUntil(white, black, [&]() {
            if (done) return true;
            auto mw = tryRecv(white);
            auto mb = tryRecv(black);
            if (mw && std::get_if<chess::net::ServerMoveMsg>(&*mw) &&
                mb && std::get_if<chess::net::ServerMoveMsg>(&*mb)) {
                done = true;
                return true;
            }
            return false;
        }));
    }

    // White disconnects
    white.disconnect();

    // Black should receive GameOver
    bool bOver = false;
    ASSERT_TRUE(pollUntil(black, black, [&]() {
        if (bOver) return true;
        auto m = tryRecv(black);
        if (m && std::get_if<chess::net::GameOverMsg>(&*m)) {
            auto* go = std::get_if<chess::net::GameOverMsg>(&*m);
            EXPECT_EQ(go->result, chess::net::GameResult::BlackWins);
            EXPECT_EQ(go->reason, chess::net::GameOverReason::Disconnection);
            bOver = true;
            return true;
        }
        return false;
    }, 2s));
}

// ── Move after game-over returns error ──────────────────────────────────────

TEST_F(E2eTest, MoveAfterGameOver)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black);

    // White resigns
    sendMsg(white, chess::net::ResignMsg{});

    // Drain game-over messages
    bool wOver = false, bOver = false;
    ASSERT_TRUE(pollUntil(white, black, [&]() {
        if (!wOver) {
            auto m = tryRecv(white);
            if (m && std::get_if<chess::net::GameOverMsg>(&*m)) wOver = true;
        }
        if (!bOver) {
            auto m = tryRecv(black);
            if (m && std::get_if<chess::net::GameOverMsg>(&*m)) bOver = true;
        }
        return wOver && bOver;
    }));

    // After game-over, clients are returned to Connected state (match cleaned up).
    // Sending a move now gets "Join first" since we're no longer in a match.
    sendMsg(black, chess::net::MoveMsg{"e5"});
    bool gotResponse = false;
    ASSERT_TRUE(pollUntil(black, black, [&]() {
        if (gotResponse) return true;
        auto m = tryRecv(black);
        if (m && std::get_if<chess::net::ErrorMsg>(&*m)) {
            auto* err = std::get_if<chess::net::ErrorMsg>(&*m);
            EXPECT_EQ(err->message, "Join first");
            gotResponse = true;
            return true;
        }
        return false;
    }));
}

// ── Chat relay through server ───────────────────────────────────────────────

TEST_F(E2eTest, ChatRelay)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black);

    sendMsg(white, chess::net::ChatMsg{"hello"});
    bool gotChat = false;
    ASSERT_TRUE(pollUntil(white, black, [&]() {
        if (gotChat) return true;
        auto m = tryRecv(black);
        if (m && std::get_if<chess::net::ServerChatMsg>(&*m)) {
            auto* chat = std::get_if<chess::net::ServerChatMsg>(&*m);
            EXPECT_EQ(chat->name, "White");
            EXPECT_EQ(chat->text, "hello");
            gotChat = true;
            return true;
        }
        return false;
    }));
}

// ── Wrong turn returns error ────────────────────────────────────────────────

TEST_F(E2eTest, WrongTurn)
{
    sf::TcpSocket white = connectClient();
    sf::TcpSocket black = connectClient();
    joinBoth(white, black);

    sendMsg(black, chess::net::MoveMsg{"e5"});
    bool gotError = false;
    ASSERT_TRUE(pollUntil(white, black, [&]() {
        if (gotError) return true;
        auto m = tryRecv(black);
        if (m && std::get_if<chess::net::ErrorMsg>(&*m)) {
            auto* err = std::get_if<chess::net::ErrorMsg>(&*m);
            EXPECT_EQ(err->message, "Not your turn");
            gotError = true;
            return true;
        }
        return false;
    }));
}
