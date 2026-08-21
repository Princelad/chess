#pragma once

#include <chess/board.h>
#include <chess/types.h>

#include <SFML/Graphics.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace chess::client {

class App;

struct HighlightState {
    std::optional<std::pair<int, int>> selectedSquare;
    std::vector<std::pair<int, int>> legalMoveTargets;
    std::optional<std::pair<int, int>> lastMoveFrom;
    std::optional<std::pair<int, int>> lastMoveTo;
    std::optional<std::pair<int, int>> checkSquare;
};

class BoardView {
public:
    BoardView(float windowWidth, float windowHeight, Color playerColor);

    void drawSquares(sf::RenderWindow& window) const;
    void drawHighlights(sf::RenderWindow& window, const HighlightState& hl,
                        const Board& board) const;
    void drawLabels(sf::RenderWindow& window, const sf::Font& font) const;
    void drawPieces(sf::RenderWindow& window, const sf::Font& font,
                    const Board& board, const App& app) const;

    sf::FloatRect squareRect(int file, int rank) const;
    std::optional<std::pair<int, int>> pixelToSquare(sf::Vector2f pixel) const;

    float panelX() const { return panelX_; }
    float boardSize() const { return boardSize_; }
    float squareSize() const { return squareSize_; }
    sf::Vector2f boardOrigin() const { return boardOrigin_; }
    bool isFlipped() const { return flipped_; }

private:
    sf::Vector2f squareToPixel(int file, int rank) const;
    std::pair<int, int> toFileRank(int col, int row) const;
    void drawSquareTint(sf::RenderWindow& window, int file, int rank,
                        sf::Color color) const;

    float margin_;
    float boardSize_;
    float squareSize_;
    sf::Vector2f boardOrigin_;
    float panelX_;
    bool flipped_;
};

} // namespace chess::client
