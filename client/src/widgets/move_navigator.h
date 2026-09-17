#pragma once

#include "widgets/button.h"
#include <chess/board.h>
#include <chess/move.h>
#include <chess/types.h>

#include <SFML/Graphics.hpp>

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace chess::client {

class App;

// Captured-material bookkeeping for one side: how many of each opponent piece
// type that side has taken, plus a value-sorted list for sprite rendering.
struct CapturedMaterial {
    int value = 0;
    std::array<int, 5> counts{}; // indexed by PieceType 0..4 (P,N,B,R,Q)
    std::vector<Piece> pieces;   // sorted pawns-first for display
};

// Clickable two-column SAN move grid with navigation controls. Keeps the full
// move history, a "live" final board (position after all moves), and a replay
// board at the currently selected ply. Also tracks captured material.
class MoveNavigator {
public:
    MoveNavigator() = default;

    void setGame(Board initialBoard, std::vector<chess::Move> moves,
                 std::vector<std::string> sans);
    void appendMove(const chess::Move& m, const std::string& san);

    // Navigation
    int currentPly() const { return currentPly_; }
    int totalPlies() const { return static_cast<int>(moves_.size()); }
    bool atStart() const { return currentPly_ == 0; }
    bool atEnd() const { return currentPly_ == totalPlies(); }
    void goToPly(int ply);
    void goStart() { goToPly(0); }
    void goBack() { goToPly(currentPly_ - 1); }
    void goForward() { goToPly(currentPly_ + 1); }
    void goEnd() { goToPly(totalPlies()); }

    const Board& board() const { return board_; }            // position at currentPly_
    const Board& finalBoard() const { return finalBoard_; }  // position after all moves
    Board initialBoard() const { return initialBoard_; }

    const std::vector<chess::Move>& moves() const { return moves_; }
    const std::vector<std::string>& sans() const { return sans_; }

    std::optional<std::pair<int, int>> lastMoveFrom() const;
    std::optional<std::pair<int, int>> lastMoveTo() const;

    // capturedBy(White) = pieces White has captured (black pieces removed).
    const CapturedMaterial& capturedBy(Color color) const
    {
        return captured_[static_cast<int>(color)];
    }
    int materialAdvantage(Color color) const;

    // Layout / events / drawing
    void setLayout(sf::FloatRect gridRect, sf::FloatRect navRect);
    void handleScroll(float delta);
    // Returns true when the event was consumed by the navigator (nav keys,
    // grid click, or nav-button click).
    bool handleEvent(const sf::Event& event, const sf::Vector2f& local);
    void draw(sf::RenderWindow& window, const sf::Font& font, const App& app);

private:
    void rebuildFromMoves();
    void replayTo(int ply);
    void clampScroll();
    void updateNavButtons();
    void keepCurrentPlyVisible();
    int cellPlyAt(sf::Vector2f local) const;
    void updateHover(sf::Vector2f local);
    void drawGrid(sf::RenderWindow& window, const sf::Font& font);
    void drawNavButtons(sf::RenderWindow& window, const sf::Font& font);

    Board initialBoard_ = Board::fromStartPos();
    Board board_ = Board::fromStartPos();
    Board finalBoard_ = Board::fromStartPos();
    std::vector<chess::Move> moves_;
    std::vector<std::string> sans_;
    std::array<CapturedMaterial, 2> captured_{};
    int currentPly_ = 0;
    int moveScroll_ = 0;

    sf::FloatRect gridRect_;
    sf::FloatRect navRect_;
    std::array<Button, 4> navButtons_ = [] {
        std::array<Button, 4> btns;
        const char* labels[4] = { "|<", "<", ">", ">|" };
        for (int i = 0; i < 4; ++i) btns[i].setLabel(labels[i]);
        return btns;
    }();
    int hoveredCellPly_ = -1;
};

} // namespace chess::client