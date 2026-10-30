#include "game_screen.h"

#include "ui/theme.h"
#include "game_over_screen.h"
#include "connect_screen.h"
#include "keyboard.h"
#include "ui_helpers.h"
#include "widgets/layout.h"

#include <chess/movegen.h>
#include <chess/san.h>
#include <chess/net/messages.h>

#include <algorithm>
#include <ctime>

namespace chess::client {

namespace {
constexpr float BtnH = 30.f;
constexpr float InputH = 28.f;
// Reconnect loop timings (11.2): one retry per second, at most ~30 s of
// trying — the server side grace is as long, so an unresponsive server
// window naturally outlives the token's validity.
constexpr float ReconnectRetrySeconds = 1.f;
constexpr float ReconnectGiveUpSeconds = 30.f;
constexpr std::size_t MaxChatLog = 50;
constexpr std::size_t MaxChatInput = 200;
}

GameScreen::GameScreen(App& app, Color myColor, const std::string& opponentName,
                       std::optional<std::pair<double, double>> opponentRating,
                       bool rated, const std::string& timeControl,
                       const std::string& resumeToken,
                       const std::string& startFen,
                       bool arena, bool spectator)
    : app_(app)
    , myColor_(myColor)
    , opponentName_(opponentName)
    , opponentRating_(std::move(opponentRating))
    , rated_(rated)
    , resumeToken_(resumeToken)
    , startFen_(startFen)
    , arena_(arena)
    , spectator_(spectator)
    , boardView_(static_cast<float>(App::WindowWidth),
                 static_cast<float>(App::WindowHeight),
                 myColor,
                 app.boardTheme(),
                 app.showCoordinates())
    , hud_(app_, boardView_.panelX(),
           static_cast<float>(App::WindowWidth) - boardView_.panelX() - 8.f)
    , myTurn_(myColor == Color::White)
{
    // 12.1: a Chess960 welcome carries the Start Position the server rolled
    // at pairing; empty = standard start, junk = fall back (never crash).
    hud_.setGame(startBoard(), {}, {});
    hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
    refreshRatingsHud();

    // Prime the clocks from the agreed time control (11.1): the server
    // starts White's clock at match creation, so both displays begin at
    // base time; each ServerMoveMsg re-syncs real authority afterwards.
    timeControl_ = timeControl;
    const auto tc = chess::net::parseTimeControl(timeControl_);
    if (!tc.unlimited)
        hud_.syncClocks(Color::White, tc.baseSeconds * 60000);

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
    chatLabel_.setColor(ui::pal::textMuted);

    drawOfferLabel_.setText("Draw offered:");
    drawOfferLabel_.setFontSize(14);
    drawOfferLabel_.setColor(ui::pal::warn);

    resignBtn_.setLabel("Resign");
    resignBtn_.setIcon(ui::icons::IconKind::Flag);
    resignBtn_.setStyle(ui::buttonDanger());
    resignBtn_.setOnClick([this] {
        if (!gameOver_) app_.connection().send(chess::net::ResignMsg{});
    });

    offerDrawBtn_.setLabel("Draw");
    offerDrawBtn_.setIcon(ui::icons::IconKind::HandDraw);
    offerDrawBtn_.setStyle(ui::buttonPlum());
    offerDrawBtn_.setOnClick([this] {
        if (gameOver_) return;
        app_.connection().send(chess::net::DrawOfferMsg{});
        addSystemChat("Draw offer sent");
    });

    declineBtn_.setLabel("Decline");
    declineBtn_.setStyle(ui::buttonDanger());
    declineBtn_.setOnClick([this] {
        if (drawOfferPending_) {
            app_.connection().send(chess::net::DrawDeclineMsg{});
            drawOfferPending_ = false;
            addSystemChat("Draw declined");
        } else if (takebackOfferPending_) {
            app_.connection().send(chess::net::TakebackDeclineMsg{});
            takebackOfferPending_ = false;
            addSystemChat("Takeback declined");
        }
    });

    acceptBtn_.setLabel("Accept");
    acceptBtn_.setStyle(ui::buttonSuccessBright());
    acceptBtn_.setOnClick([this] {
        if (drawOfferPending_) {
            app_.connection().send(chess::net::DrawAcceptMsg{});
            drawOfferPending_ = false;
        } else if (takebackOfferPending_) {
            app_.connection().send(chess::net::TakebackAcceptMsg{});
            takebackOfferPending_ = false;
        }
    });

    takebackLabel_.setText("Takeback asked:");
    takebackLabel_.setFontSize(14);
    takebackLabel_.setColor(ui::pal::warn);

    takebackBtn_.setLabel("Takeback");
    takebackBtn_.setStyle(ui::buttonPlum());
    takebackBtn_.setOnClick([this] {
        if (gameOver_) return;
        app_.connection().send(chess::net::TakebackRequestMsg{});
        addSystemChat("Takeback requested");
    });

    // Spectator mode (12.4): the action buttons give way to one Leave
    // button; the chat input is disabled (watchers may not talk).
    leaveBtn_.setLabel("Leave");
    leaveBtn_.setIcon(ui::icons::IconKind::ArrowBack);
    leaveBtn_.setStyle(ui::buttonLeave());
    leaveBtn_.setOnClick([this] { leaveWatch(); });

    if (spectator_) {
        chatInput_.setEnabled(false);
        chatInput_.setPlaceholder("Spectators can't chat");
    }

    layoutPanel();
}

// The spectator view (12.4): a White-oriented read-only window on the live
// game. Delegation wires all the widgets; the snapshot then paints names,
// clocks and the SAN chain.
GameScreen::GameScreen(App& app, const chess::net::SpectateStateMsg& snapshot)
    : GameScreen(app, Color::White, snapshot.blackName,
                 std::make_pair(snapshot.blackRating, snapshot.blackDeviation),
                 false, snapshot.timeControl, {}, snapshot.startFen, false,
                 true)
{
    applySpectateState(snapshot);
}

void GameScreen::applySpectateState(const chess::net::SpectateStateMsg& msg)
{
    spectator_ = true;
    whiteName_ = msg.whiteName;
    spectatorCount_ = msg.spectators;
    opponentName_ = msg.blackName;
    opponentRating_ = std::make_pair(msg.blackRating, msg.blackDeviation);
    rated_ = false;
    timeControl_ = msg.timeControl;

    hud_.setSpectatorMode(true);
    hud_.setSpectatorNames(msg.whiteName, msg.blackName);
    hud_.setWatcherCount(msg.spectators);
    // localRating_ renders the WHITE card's suffix and opponentRating_ the
    // BLACK one — with the White orientation both land on the right seat,
    // so the player-flow refreshRatingsHud is deliberately NOT called.
    hud_.setRatings(std::make_pair(msg.whiteRating, msg.whiteDeviation),
                    std::make_pair(msg.blackRating, msg.blackDeviation));

    // The live chain: identical SAN replay to applyResume (12.4), from the
    // game's own Start Position rather than the snapshot FEN.
    Board board = startBoard();
    std::vector<chess::Move> moves;
    moves.reserve(msg.sans.size());
    for (const auto& san : msg.sans) {
        auto mv = chess::san::fromSan(board, san);
        if (!mv.has_value())
            break;
        moves.push_back(*mv);
        board.makeMove(*mv);
    }
    hud_.setGame(startBoard(), moves, msg.sans);
    hud_.navigator().goEnd(); // the live position, not ply zero

    // Any staged interaction state from construction is stale by definition.
    anim_.cancel();
    hl_.selectedSquare.reset();
    hl_.legalMoveTargets.clear();
    dragFrom_.reset();
    rightPress_.reset();
    promo_.reset();
    premove_.reset();
    keyCursor_ = {4, 4};
    annotations_.clear();
    gameOver_ = false;
    myTurn_ = false; // a watcher never has the move
    hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
    hud_.setGameOver(false);
    hud_.syncClocks(msg.whiteRemainingMs, msg.blackRemainingMs,
                    board.sideToMove());
    hud_.setStatus("Watching " + whiteName_ + " vs " + opponentName_, 4.f);
}

void GameScreen::leaveWatch()
{
    // Tell the server to release the watch (its count relays on the next
    // move), then back to the connect screen for another pick.
    if (app_.connection().state() == ConnectionState::Connected)
        app_.connection().spectateStop();
    app_.connection().disconnect();
    app_.switchScreen(std::make_unique<ConnectScreen>(app_));
}

void GameScreen::layoutPanel()
{
    float px = boardView_.panelX();

    // Three action buttons share the panel row (96px each with gaps); the
    // accept/decline prompt buttons keep the roomier two-button layout.
    resignBtn_.setRect(sf::FloatRect(sf::Vector2f(px, hud_.contentBottom()),
                                     sf::Vector2f(96.f, BtnH)));
    offerDrawBtn_.setRect(sf::FloatRect(sf::Vector2f(px + 102.f, hud_.contentBottom()),
                                        sf::Vector2f(96.f, BtnH)));
    takebackBtn_.setRect(sf::FloatRect(sf::Vector2f(px + 204.f, hud_.contentBottom()),
                                       sf::Vector2f(96.f, BtnH)));

    // The spectator leave button takes the first slot (12.4).
    leaveBtn_.setRect(sf::FloatRect(sf::Vector2f(px, hud_.contentBottom()),
                                    sf::Vector2f(96.f, BtnH)));

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
    chatLogBg_.setFill(ui::pal::bgDeep);
    chatLogBg_.setOutline(ui::pal::bgElevated);

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
    // Not our turn yet: the move is queued as a premove and auto-sends the
    // moment the echoed response makes it legal (11.4).
    if (!myTurn_) {
        queuePremove(hl_.selectedSquare->first, hl_.selectedSquare->second,
                     targetFile, targetRank);
        return;
    }

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

// Queues the opponent-waiting move (11.4). The latest attempt replaces any
// earlier premove — a single slot; right-click/Esc cancel it (see below).
void GameScreen::queuePremove(int fromFile, int fromRank, int toFile, int toRank)
{
    if (fromFile == toFile && fromRank == toRank)
        return;
    premove_ = Premove{fromFile, fromRank, toFile, toRank};
    deselect();
}

// Flushes the queued premove after the opponent's move has been applied.
// Resolution is pure legality: whatever legal move matches the queued
// from/to goes out as a plain MoveMsg (captured, castled, en passant),
// a promotion frame sends the queen pre-move. Anything else — an empty
// "capture" square, a blocked/pinned path, a landing on an own piece —
// is dropped with a status line. Both states lose the queue.
void GameScreen::flushPremove()
{
    if (!premove_)
        return;
    const Premove queued = *premove_;
    premove_.reset();
    if (!myTurn_ || gameOver_)
        return;

    auto resolved = resolvePremove(hud_.navigator().finalBoard(), queued);
    if (resolved.has_value()) {
        hud_.setStatus("Premove played", 1.5f);
        commitMove(*resolved);
    } else {
        hud_.setStatus("Premove illegal", 2.f);
        deselect();
    }
}

void GameScreen::syncViewHighlights()
{
    const Board& shown = hud_.navigator().board();
    hl_.lastMoveFrom = hud_.navigator().lastMoveFrom();
    hl_.lastMoveTo = hud_.navigator().lastMoveTo();
    hl_.cursorSquare = keyCursor_;
    Color stm = shown.sideToMove();
    hl_.checkSquare = chess::inCheck(shown, stm)
        ? findKingSquare(shown, stm)
        : std::optional<std::pair<int, int>>{};
    hl_.premoveFrom = premove_.has_value()
        ? std::optional<std::pair<int, int>>{ {
              premove_->fromFile, premove_->fromRank } }
        : std::nullopt;
    hl_.premoveTo = premove_.has_value()
        ? std::optional<std::pair<int, int>>{ {
              premove_->toFile, premove_->toRank } }
        : std::nullopt;
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

void GameScreen::addChatLine(const std::string& name, const std::string& text)
{
    chatLog_.push_back(ChatEntry{name, text, std::time(nullptr)});
    if (chatLog_.size() > MaxChatLog)
        chatLog_.erase(chatLog_.begin());
}

void GameScreen::addSystemChat(const std::string& text)
{
    addChatLine("", text);
}

void GameScreen::sendChat()
{
    if (spectator_) return; // watchers may not talk (12.4)
    if (chatInput_.text().empty()) return;
    const std::string text = chatInput_.text();
    chatInput_.setText("");

    // Slash commands (11.8) stay client-local: mute bookkeeping never
    // travels to the server. The name is the rest of the line (names may
    // contain spaces); the saved config re-persists on every toggle.
    if (text[0] == '/') {
        Config& config = app_.config();
        const std::size_t space = text.find(' ');
        const std::string cmd = space == std::string::npos
            ? text : text.substr(0, space);
        std::string name = space == std::string::npos
            ? "" : text.substr(space + 1);
        while (!name.empty() && (name.front() == ' ' || name.front() == '\t'))
            name.erase(name.begin());
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t'))
            name.pop_back();
        if ((cmd == "/mute" || cmd == "/unmute") && !name.empty()) {
            if (!chatMuteNamePersistable(name)) {
                addSystemChat("That name can't be muted safely");
                return;
            }
            const bool muted = chatToggleMute(config, name);
            config.save();
            addSystemChat(name + (muted ? " muted" : " unmuted"));
        } else {
            addSystemChat("Commands: /mute <name>, /unmute <name>");
        }
        return;
    }

    app_.connection().send(chess::net::ChatMsg{text});
    addChatLine(YouName(), text);
}

void GameScreen::handleEvent(const sf::Event& event)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (chatInput_.isFocused()) {
            if (kp->code == sf::Keyboard::Key::Escape) {
                chatInput_.setFocused(false);
                return;
            }
            if (chatInput_.handleEvent(*kp, {0.f, 0.f})) return;
            return;
        }

        if (kp->code == sf::Keyboard::Key::Escape) {
            if (showingHelp_) { showingHelp_ = false; return; }
            if (promo_) { cancelPromotion(); return; }
            if (spectator_) { leaveWatch(); return; } // leave the stands
            clearBoardInput();
            premove_.reset();
            deselect();
            return;
        }

        if (promo_) {
            if (kp->code == sf::Keyboard::Key::Space) { cancelPromotion(); return; }
            if (auto t = promoTypeForKey(kp->code)) {
                sendPromotionMove(*t);
                return;
            }
            return;
        }

        switch (keyToAction(kp->code, kp->shift)) {
            case KeyboardAction::Deselect:
                if (spectator_) return; // nothing selected to clear
                clearBoardInput();
                premove_.reset();
                deselect();
                return;
            case KeyboardAction::Flip:
                boardView_.toggleFlipped();
                return;
            case KeyboardAction::Help:
                showingHelp_ = !showingHelp_;
                return;
            case KeyboardAction::PrevMove:
                hud_.navigator().goBack();
                clearBoardInput();
                return;
            case KeyboardAction::NextMove:
                hud_.navigator().goForward();
                clearBoardInput();
                return;
            case KeyboardAction::MoveCursor: {
                // Keyboard play is seat-only (9.6): a watcher's arrows do
                // nothing — there is no cursor commit to give a move to.
                if (spectator_) return;
                auto [df, dr] = cursorDelta(kp->code);
                keyCursor_ = moveCursorStep(keyCursor_.first, keyCursor_.second, df, dr);
                return;
            }
            case KeyboardAction::Enter: {
                if (spectator_) {
                    // Reaching the live position is a watcher's only use
                    // for Enter (the navigator stepper above is shared).
                    if (!hud_.navigator().atEnd())
                        hud_.navigator().goEnd();
                    return;
                }
                if (!hud_.navigator().atEnd()) {
                    hud_.navigator().goEnd();
                    deselect();
                }
                if (gameOver_) return;
                if (hl_.selectedSquare) {
                    if (*hl_.selectedSquare == keyCursor_) {
                        deselect();
                    } else if (ownPieceAt(keyCursor_.first, keyCursor_.second)) {
                        selectPiece(keyCursor_.first, keyCursor_.second);
                    } else {
                        trySendMove(keyCursor_.first, keyCursor_.second);
                    }
                } else {
                    if (!myTurn_) return;
                    if (ownPieceAt(keyCursor_.first, keyCursor_.second))
                        selectPiece(keyCursor_.first, keyCursor_.second);
                }
                return;
            }
            case KeyboardAction::None:
            default:
                break;
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

    if (spectator_) {
        if (leaveBtn_.handleEvent(event, local)) return;
    } else if (!gameOver_) {
        if (drawOfferPending_ || takebackOfferPending_) {
            if (declineBtn_.handleEvent(event, local)) return;
            if (acceptBtn_.handleEvent(event, local)) return;
        } else {
            if (resignBtn_.handleEvent(event, local)) return;
            if (offerDrawBtn_.handleEvent(event, local)) return;
            if (takebackBtn_.handleEvent(event, local)) return;
        }
    }

    if (spectator_) return; // the board is read-only from here on
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
            if (gameOver_) return;

            if (!hud_.navigator().atEnd()) {
                hud_.navigator().goEnd();
                deselect();
            }

            auto square = boardView_.pixelToSquare(local);
            if (!square) return;

            keyCursor_ = *square;
            drag_.press(local.x, local.y);
            cursor_ = local;
            dragFrom_ = *square;

            if (ownPieceAt(square->first, square->second))
                selectPiece(square->first, square->second);
        } else if (mb->button == sf::Mouse::Button::Right) {
            if (promo_) cancelPromotion();
            clearBoardInput();
            premove_.reset();
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
    anim_.update(dtSec);

    if (gameOverTransition_.armed()) {
        if (gameOverTransition_.tick(dtSec))
            finishGameOver();
        return;
    }

    app_.connection().poll();

    while (app_.connection().hasMessages()) {
        auto msg = app_.connection().nextMessage();

        if (auto* move = std::get_if<chess::net::ServerMoveMsg>(&msg)) {
            auto parsed = chess::san::fromSan(hud_.navigator().finalBoard(), move->san);
            if (parsed) {
                const Board& before = hud_.navigator().finalBoard();
                const auto& cfg = app_.config();
                const float duration = static_cast<float>(
                    cfg.getFloat("animation.duration", 0.3));
                if (cfg.getBool("animation.enabled", true) && duration > 0.f) {
                    anim_.start(*parsed, before, duration);
                }
                app_.sounds().playMove(before, *parsed);
                hud_.navigator().appendMove(*parsed, move->san);
                // Re-sync the clock authority: remainingMs is the fresh
                // remaining of whoever is on the move after this move.
                hud_.syncClocks(hud_.navigator().finalBoard().sideToMove(),
                                move->remainingMs);
                hl_.selectedSquare.reset();
                hl_.legalMoveTargets.clear();
                if (spectator_) {
                    // The watcher count hops with the move relay (12.4).
                    if (move->spectators != spectatorCount_) {
                        spectatorCount_ = move->spectators;
                        hud_.setWatcherCount(spectatorCount_);
                        if (spectatorCount_ > 0)
                            hud_.setStatus(
                                std::to_string(spectatorCount_) + " watching",
                                3.f);
                    }
                } else {
                    myTurn_ = true;
                    hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
                    // Our premove (if any) auto-sends now, or dies trying
                    // (11.4).
                    flushPremove();
                }
            }
        }
        else if (auto* gameOver = std::get_if<chess::net::GameOverMsg>(&msg)) {
            gameOver_ = true;
            drawOfferPending_ = false;
            takebackOfferPending_ = false;
            premove_.reset();
            hud_.setGameOver(true);
            hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
            const auto& cfg = app_.config();
            const float animDur = static_cast<float>(
                cfg.getFloat("animation.duration", 0.3));
            const bool animOn = cfg.getBool("animation.enabled", true);
            gameOverTransition_.arm(gameOver->result, gameOver->reason,
                                    gameOverDelaySec(animOn, animDur));
        }
        else if (auto* drawOffer = std::get_if<chess::net::ServerDrawOfferMsg>(&msg)) {
            (void)drawOffer;
            drawOfferPending_ = true;
            hud_.setStatus("Opponent offers a draw", 3.0f);
            addSystemChat("Draw offer received");
        }
        else if (std::holds_alternative<chess::net::ServerDrawDeclineMsg>(msg)) {
            drawOfferPending_ = false;
            hud_.setStatus("Draw offer declined", 2.0f);
            addSystemChat("Draw offer declined");
        }
        else if (std::holds_alternative<chess::net::ServerTakebackRequestMsg>(msg)) {
            // The opponent asks to retract the last full move; the same
            // accept/decline prompt row answers it (draw prompt takes
            // precedence only while also showing — both stay pending).
            takebackOfferPending_ = true;
            hud_.setStatus("Opponent asks for a takeback", 3.0f);
            addSystemChat("Takeback requested by opponent");
        }
        else if (auto* takebackOk = std::get_if<chess::net::TakebackAcceptedMsg>(&msg)) {
            // Agreed (or bot-accepted) takeback: roll the board back to the
            // trimmed SAN list. The retracted moves are history, not action —
            // no animation, no move sound, clocks stay as they are.
            applyTakeback(takebackOk->sans);
        }
        else if (std::holds_alternative<chess::net::ServerTakebackDeclineMsg>(msg)) {
            hud_.setStatus("Takeback declined", 2.0f);
            addSystemChat("Takeback declined");
        }
        else if (auto* profileMsg = std::get_if<chess::net::ProfileMsg>(&msg)) {
            // Rated game just ended: the server re-sends our fresh profile.
            app_.setProfile(*profileMsg);
            refreshRatingsHud();
        }
        else if (auto* resumeState = std::get_if<chess::net::ResumeStateMsg>(&msg)) {
            // Token resume (11.2): the full remote state, replayed in place.
            reconnectPhase_ = ReconnectPhase::None;
            applyResume(*resumeState);
        }
        else if (auto* spectateState = std::get_if<chess::net::SpectateStateMsg>(&msg)) {
            // The live view has landed (12.4): repaint as a watcher.
            applySpectateState(*spectateState);
        }
        else if (auto* reconn = std::get_if<chess::net::OpponentReconnectingMsg>(&msg)) {
            hud_.setStatus("Opponent lost connection",
                           static_cast<float>(reconn->graceSeconds));
            addSystemChat("Opponent lost connection");
        }
        else if (std::holds_alternative<chess::net::OpponentResumedMsg>(msg)) {
            hud_.setStatus("Opponent reconnected", 3.f);
            addSystemChat("Opponent reconnected");
        }
        else if (auto* chat = std::get_if<chess::net::ServerChatMsg>(&msg)) {
            // Mute is client-local (11.8): the opposite seat never learns
            // who is muted; a muted line simply never joins this log.
            if (!chatIsMuted(chatMutedFromConfig(app_.config()), chat->name))
                addChatLine(chat->name, chat->text);
        }
        else if (auto* err = std::get_if<chess::net::ErrorMsg>(&msg)) {
            hud_.setStatus(err->message, 2.0f);
            if (err->message == "Illegal move" || err->message == "Not your turn") {
                myTurn_ = true;
                hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
            }
            if (reconnectPhase_ == ReconnectPhase::AwaitingResume) {
                // The token was rejected: give up at once instead of
                // retrying the same dead resume for the rest of the window.
                checkSelfResumeError_ = true;
            }
        }
    }

    // Reconnect engine (11.2): runs while the flow is active, and picks up
    // a fresh drop whenever the connection died outside the flow.
    if (reconnectPhase_ != ReconnectPhase::None
        || app_.connection().state() == ConnectionState::Disconnected)
        updateReconnect(dtSec);
}

// Token-based reconnect loop (11.2). Left alone, this self-heals within a
// second of the network coming back; rejected tokens exit the loop at once
// (see checkSelfResumeError_) and everything else falls back to the
// game-over screen after the give-up window.
void GameScreen::updateReconnect(float dtSec)
{
    // Defense: a game session with no token has nothing to resume — keep
    // the old instant "Connection lost" behavior.
    if (resumeToken_.empty())
        return giveUpReconnect();

    auto& conn = app_.connection();
    switch (reconnectPhase_) {
        case ReconnectPhase::None:
            // Fresh drop detected by the caller: arm the flow.
            reconnectElapsed_ = 0.f;
            reconnectWait_ = 0.f;
            reconnectPhase_ = ReconnectPhase::Backoff;
            hud_.setStatus("Connection lost — reconnecting...",
                           ReconnectGiveUpSeconds);
            break;

        case ReconnectPhase::Backoff: {
            reconnectWait_ += dtSec;
            reconnectElapsed_ += dtSec;
            if (reconnectElapsed_ >= ReconnectGiveUpSeconds)
                return giveUpReconnect();
            if (reconnectWait_ >= ReconnectRetrySeconds
                && conn.state() == ConnectionState::Disconnected) {
                // Use the credentials remembered at the last successful
                // connect; failed dials loop back to Backoff via state
                // checks below.
                try {
                    const auto port = static_cast<unsigned short>(
                        std::stoi(app_.lastPort()));
                    app_.connection().connect(app_.lastHost(), port);
                    reconnectPhase_ = ReconnectPhase::Connecting;
                } catch (...) {
                    // Unparsable remembered port: no credentials, no resume.
                    return giveUpReconnect();
                }
            }
            break;
        }

        case ReconnectPhase::Connecting: {
            reconnectElapsed_ += dtSec;
            if (reconnectElapsed_ >= ReconnectGiveUpSeconds)
                return giveUpReconnect();
            if (conn.state() == ConnectionState::Connected) {
                conn.resume(resumeToken_);
                reconnectPhase_ = ReconnectPhase::AwaitingResume;
            } else if (conn.state() == ConnectionState::Disconnected) {
                reconnectWait_ = 0.f;
                reconnectPhase_ = ReconnectPhase::Backoff;
            }
            break;
        }

        case ReconnectPhase::AwaitingResume: {
            if (checkSelfResumeError_)
                return giveUpReconnect();
            reconnectElapsed_ += dtSec;
            if (reconnectElapsed_ >= ReconnectGiveUpSeconds)
                return giveUpReconnect();
            // Another drop while awaiting the reply: retry from scratch.
            if (conn.state() == ConnectionState::Disconnected) {
                reconnectWait_ = 0.f;
                reconnectPhase_ = ReconnectPhase::Backoff;
            }
            break;
        }
    }
}

void GameScreen::giveUpReconnect()
{
    reconnectPhase_ = ReconnectPhase::None;
    checkSelfResumeError_ = false;
    gameOver_ = true;
    hud_.setGameOver(true);
    app_.switchScreen(std::make_unique<GameOverScreen>(
        app_, net::GameResult::Abort, net::GameOverReason::Disconnection,
        hud_.navigator().initialBoard(), hud_.navigator().moves(),
        hud_.navigator().sans()));
}

// The board every SAN-chain reconstruction starts from (12.1): the welcome's
// Start Position when the match is a Chess960 game, else the standard start.
// Mirrors the server's startBoardFrom fallback — a malformed FEN
// never crashes the screen, it degrades to the standard game.
Board GameScreen::startBoard() const
{
    if (startFen_.empty())
        return Board::fromStartPos();
    auto fromFen = Board::fromFen(startFen_);
    return fromFen.has_value() ? *fromFen : Board::fromStartPos();
}

// Rebuilds this screen from a full ResumeStateMsg (11.2): SAN replay for the
// board, refreshed identity/opponent/ratings, both clock figures. No move
// animation — the missing moves are history, not action.
void GameScreen::applyResume(const chess::net::ResumeStateMsg& msg)
{
    myColor_ = msg.color;
    opponentName_ = msg.opponent;
    rated_ = msg.rated;
    timeControl_ = msg.timeControl;
    opponentRating_ = std::make_pair(msg.rating, msg.deviation);

    // Replay the SAN chain over the game's own start board (12.1: Chess960
    // matches chain from the rolled SP — NOT the standard start, and
    // ResumeStateMsg.fen is the current snapshot, not the chain's origin).
    Board board = startBoard();
    std::vector<chess::Move> moves;
    moves.reserve(msg.sans.size());
    for (const auto& san : msg.sans) {
        auto mv = chess::san::fromSan(board, san);
        if (!mv.has_value())
            break;
        moves.push_back(*mv);
        board.makeMove(*mv);
    }
    hud_.setGame(startBoard(), moves, msg.sans);
    hud_.navigator().goEnd(); // show the live position, not ply zero

    // Drop leftover interaction state from before the drop.
    hl_.selectedSquare.reset();
    dragFrom_.reset();
    rightPress_.reset();
    promo_.reset();
    premove_.reset();
    annotations_.clear();
    drawOfferPending_ = false;
    takebackOfferPending_ = false;
    chatLog_.clear();
    chatInput_.setFocused(false);

    myTurn_ = board.sideToMove() == myColor_ && !gameOver_;
    hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
    hud_.setGameOver(gameOver_);
    refreshRatingsHud();
    hud_.syncClocks(msg.whiteRemainingMs, msg.blackRemainingMs,
                    board.sideToMove());
    hud_.setStatus("Reconnected", 2.f);
}

// Rebuilds the board from the takeback's trimmed SAN list (11.3): the same
// replay as a token resume, minus the identity/clock refresh — a takeback
// keeps both clocks exactly as they were, the retracted plies' time stays
// spent. The requester ends up on the move; no animation or move sound
// plays, the retracted moves were already aired once.
void GameScreen::applyTakeback(const std::vector<std::string>& sans)
{
    Board board = startBoard();
    std::vector<chess::Move> moves;
    moves.reserve(sans.size());
    for (const auto& san : sans) {
        auto mv = chess::san::fromSan(board, san);
        if (!mv.has_value())
            break;
        moves.push_back(*mv);
        board.makeMove(*mv);
    }
    hud_.setGame(startBoard(), moves, sans);
    hud_.navigator().goEnd();

    // Stale interaction state pointing at the retracted plies.
    anim_.cancel();
    hl_.selectedSquare.reset();
    hl_.legalMoveTargets.clear();
    dragFrom_.reset();
    rightPress_.reset();
    promo_.reset();
    premove_.reset();
    annotations_.clear();
    myTurn_ = board.sideToMove() == myColor_ && !gameOver_;
    hud_.setInfo(opponentName_, myColor_, myTurn_, gameOver_);
    hud_.setStatus("Takeback accepted", 2.f);
    addSystemChat("Takeback accepted");
}

void GameScreen::finishGameOver()
{
    if (spectator_) {
        // A watcher holds no seat: no result screen / rematch flow. The
        // transition delay above let the finale play out; drop back to
        // the connect screen.
        leaveWatch();
        return;
    }

    // The arena flag rides along so the result screen can hand us back to
    // the lobby (12.3). The variant is reconstructed from the Start
    // Position: a Chess960 welcome always carries a (rolled) FEN, the
    // standard game never does.
    const std::string variant = startFen_.empty()
        ? std::string() : std::string(chess::net::Chess960Variant);
    app_.switchScreen(std::make_unique<GameOverScreen>(
        app_, gameOverTransition_.result(), gameOverTransition_.reason(),
        hud_.navigator().initialBoard(), hud_.navigator().moves(),
        hud_.navigator().sans(), arena_, timeControl_, variant));
}

void GameScreen::refreshRatingsHud()
{
    std::optional<std::pair<double, double>> local;
    if (const auto& own = app_.profile(); own.has_value() && !own->isGuest)
        local = std::make_pair(own->rating, own->deviation);
    hud_.setRatings(local, opponentRating_);
}

void GameScreen::drawButtons(sf::RenderWindow& window)
{
    auto& font = app_.font();

    if (drawOfferPending_) {
        drawOfferLabel_.setPosition({boardView_.panelX(), hud_.contentBottom() - 20.f});
        drawOfferLabel_.draw(window, font);
        declineBtn_.draw(window, font);
        acceptBtn_.draw(window, font);
    } else if (takebackOfferPending_) {
        takebackLabel_.setPosition({boardView_.panelX(), hud_.contentBottom() - 20.f});
        takebackLabel_.draw(window, font);
        declineBtn_.draw(window, font);
        acceptBtn_.draw(window, font);
    } else if (!gameOver_) {
        resignBtn_.draw(window, font);
        offerDrawBtn_.draw(window, font);
        takebackBtn_.draw(window, font);
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
    sep.setFillColor(ui::pal::stroke);
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
        const ChatEntry& entry = chatLog_[i];

        // Three-part line (11.8): dim [HH:MM] tag, colored sender, gray
        // text; system lines skip the sender and render gray. Each part
        // truncates into the remaining panel width (~6 px per glyph, the
        // same estimator the pre-11.8 renderer used).
        const float left = px + 4.f;
        const float right = px + panelW - 4.f;
        const auto fits = [&](float x, const std::string& text) {
            constexpr float CharWidth = 6.f;
            return safeTruncate(text,
                static_cast<std::size_t>(std::max(0.f, right - x) / CharWidth));
        };

        sf::Text timeTag(font, "[" + chatTimestamp(entry.epochSec) + "] ", 13);
        timeTag.setFillColor(sf::Color(110, 110, 110)); // chat stub gray
        timeTag.setPosition({left, y});
        window.draw(timeTag);

        float x = left + timeTag.getLocalBounds().size.x;
        if (entry.name.empty()) {
            sf::Text line(font, fits(x, entry.text), 13);
            line.setFillColor(sf::Color(170, 170, 170)); // chat muted line
            line.setPosition({x, y});
            window.draw(line);
        } else {
            sf::Text namePart(font, entry.name + ": ", 13);
            namePart.setFillColor(entry.name == YouName()
                ? ui::pal::goldChatSelf : chatNameColor(entry.name));
            namePart.setPosition({x, y});
            window.draw(namePart);

            sf::Text line(font, fits(x + namePart.getLocalBounds().size.x,
                                     entry.text), 13);
            line.setFillColor(ui::pal::textSecondary);
            line.setPosition({x + namePart.getLocalBounds().size.x, y});
            window.draw(line);
        }
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
    boardView_.drawPieces(window, font, shown, app_, &anim_);

    // Premove ghost (11.4): the queued piece translucent on its putative
    // landing square. Live position only — a stepper is reading history.
    if (premove_ && !myTurn_ && !gameOver_ && hud_.navigator().atEnd()) {
        Piece ghost = hud_.navigator().finalBoard().pieceAt(
            squareOf(premove_->fromFile, premove_->fromRank));
        if (!ghost.isNone() && ghost.color == myColor_)
            boardView_.drawGhostPiece(window, font, ghost,
                                      premove_->toFile, premove_->toRank,
                                      app_);
    }

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
            cell.setFillColor(ui::overlay::cellFill);
            cell.setOutlineColor(ui::overlay::cellStroke);
            cell.setOutlineThickness(1.f);
            window.draw(cell);

            if (i == promoHover_) {
                sf::RectangleShape hover({c.size, c.size});
                hover.setPosition(c.pos);
                hover.setFillColor(ui::overlay::cellHover);
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
                letter.setFillColor(ui::overlay::cellGlyph);
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

    if (showingHelp_) drawHelpOverlay(window);
}

void GameScreen::drawHelpOverlay(sf::RenderWindow& window)
{
    auto& font = app_.font();

    sf::RectangleShape dim(
        {static_cast<float>(App::WindowWidth), static_cast<float>(App::WindowHeight)});
    dim.setFillColor(ui::overlay::dim);
    window.draw(dim);

    const char* lines[] = {
        "Shortcuts",
        "Arrows — move cursor",
        "Enter — select / move",
        "Space — cancel / deselect",
        "Esc — close help / deselect",
        "F — flip board",
        "[ / ] — previous / next move",
        "Home / End — go to start / end",
        "Q R B N or 1-4 — pick promotion",
        "Right-click — cancel premove / annotate",
        "? — toggle this help",
    };
    constexpr int Count = static_cast<int>(sizeof(lines) / sizeof(lines[0]));

    constexpr unsigned int HdrSize = 17;
    constexpr unsigned int RowSize = 14;
    constexpr float RowH = 24.f;
    constexpr float PadX = 22.f;
    constexpr float PadY = 18.f;

    float maxW = 0.f;
    for (int i = 0; i < Count; ++i) {
        sf::Text t(font, lines[i], (i == 0) ? HdrSize : RowSize);
        maxW = std::max(maxW, t.getLocalBounds().size.x);
    }

    const float panelW = maxW + 2.f * PadX;
    const float panelH = RowH * static_cast<float>(Count) + 2.f * PadY;
    const float px = (static_cast<float>(App::WindowWidth) - panelW) / 2.f;
    const float py = (static_cast<float>(App::WindowHeight) - panelH) / 2.f;

    sf::RectangleShape bg({panelW, panelH});
    bg.setPosition({px, py});
    bg.setFillColor(ui::overlay::panelFill);
    bg.setOutlineColor(ui::overlay::panelStroke);
    bg.setOutlineThickness(1.f);
    window.draw(bg);

    float y = py + PadY;
    for (int i = 0; i < Count; ++i) {
        sf::Text t(font, lines[i], (i == 0) ? HdrSize : RowSize);
        t.setFillColor(i == 0 ? ui::pal::white : ui::pal::textCell);
        auto lb = t.getLocalBounds();
        t.setPosition({
            px + (panelW - lb.size.x) / 2.f - lb.position.x,
            y - lb.position.y
        });
        window.draw(t);
        y += RowH;
    }
}

} // namespace chess::client