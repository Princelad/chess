#pragma once

#include "board_interaction.h"
#include "themes.h"

#include <chess/board.h>
#include <chess/types.h>

#include <SFML/Graphics.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace chess::client {

class App;
class MoveAnimator;

struct HighlightState {
    std::optional<std::pair<int, int>> selectedSquare;
    std::vector<std::pair<int, int>> legalMoveTargets;
    std::optional<std::pair<int, int>> lastMoveFrom;
    std::optional<std::pair<int, int>> lastMoveTo;
    std::optional<std::pair<int, int>> checkSquare;
    std::optional<std::pair<int, int>> cursorSquare;
};

class BoardView {
public:
    BoardView(float windowWidth, float windowHeight, Color playerColor,
              BoardTheme theme = BoardTheme::Classic,
              bool showCoordinates = true);

    void drawSquares(sf::RenderWindow& window) const;
    void drawHighlights(sf::RenderWindow& window, const HighlightState& hl,
                        const Board& board) const;
    void drawLabels(sf::RenderWindow& window, const sf::Font& font) const;
    void drawPieces(sf::RenderWindow& window, const sf::Font& font,
                    const Board& board, const App& app,
                    const MoveAnimator* anim = nullptr) const;
    void drawAnnotations(sf::RenderWindow& window,
                         const std::vector<AnnotatedArrow>& arrows,
                         const std::vector<std::pair<int, int>>& circles) const;
    void drawDraggedPiece(sf::RenderWindow& window, const sf::Font& font,
                          chess::Piece piece, sf::Vector2f cursor,
                          const App& app) const;

    std::optional<std::pair<int, int>> pixelToSquare(sf::Vector2f pixel) const;
    sf::Vector2f squareCenter(int file, int rank) const;

    float panelX() const { return panelX_; }
    float squareSize() const { return squareSize_; }
    sf::Vector2f boardOrigin() const { return boardOrigin_; }
    bool isFlipped() const { return flipped_; }

    void setFlipped(bool flipped) { flipped_ = flipped; }
    void toggleFlipped() { flipped_ = !flipped_; }

private:
    sf::Vector2f squareToPixel(int file, int rank) const;
    std::pair<int, int> toFileRank(int col, int row) const;
    void drawSquareTint(sf::RenderWindow& window, int file, int rank,
                        sf::Color color) const;
    void drawPieceAtCenter(sf::RenderWindow& window, const sf::Font& font,
                           chess::Piece piece, sf::Vector2f center,
                           const App& app) const;

    float margin_;
    float boardSize_;
    float squareSize_;
    sf::Vector2f boardOrigin_;
    float panelX_;
    bool flipped_;
    BoardColors colors_;
    bool showCoordinates_;
};

} // namespace chess::client
