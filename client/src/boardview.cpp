#include "boardview.h"
#include "app.h"

#include <algorithm>

namespace chess::client {

BoardView::BoardView(float windowWidth, float windowHeight, Color playerColor)
    : margin_(20.f)
    , flipped_(playerColor == Color::Black)
{
    float availW = windowWidth * 0.65f - margin_;
    float availH = windowHeight - 2.f * margin_;
    boardSize_ = std::min(availW, availH);
    squareSize_ = boardSize_ / 8.f;
    boardOrigin_ = { margin_, margin_ + (availH - boardSize_) / 2.f };
    panelX_ = boardOrigin_.x + boardSize_ + margin_;
}

sf::Vector2f BoardView::squareToPixel(int file, int rank) const
{
    int col = flipped_ ? (7 - file) : file;
    int row = flipped_ ? rank : (7 - rank);
    return {
        boardOrigin_.x + static_cast<float>(col) * squareSize_,
        boardOrigin_.y + static_cast<float>(row) * squareSize_
    };
}

std::pair<int, int> BoardView::toFileRank(int col, int row) const
{
    int file = flipped_ ? (7 - col) : col;
    int rank = flipped_ ? row : (7 - row);
    return { file, rank };
}

sf::FloatRect BoardView::squareRect(int file, int rank) const
{
    auto pos = squareToPixel(file, rank);
    return { pos, { squareSize_, squareSize_ } };
}

std::optional<std::pair<int, int>> BoardView::pixelToSquare(sf::Vector2f pixel) const
{
    float col = (pixel.x - boardOrigin_.x) / squareSize_;
    float row = (pixel.y - boardOrigin_.y) / squareSize_;

    if (col < 0.f || col >= 8.f || row < 0.f || row >= 8.f)
        return std::nullopt;

    int ic = static_cast<int>(col);
    int ir = static_cast<int>(row);
    return toFileRank(ic, ir);
}

void BoardView::drawSquares(sf::RenderWindow& window) const
{
    static const sf::Color LightSquare(240, 217, 181);
    static const sf::Color DarkSquare(181, 136, 99);

    sf::RectangleShape sq({ squareSize_, squareSize_ });

    for (int file = 0; file < 8; ++file) {
        for (int rank = 0; rank < 8; ++rank) {
            bool light = (file + rank) % 2 != 0;
            sq.setPosition(squareToPixel(file, rank));
            sq.setFillColor(light ? LightSquare : DarkSquare);
            window.draw(sq);
        }
    }
}

void BoardView::drawSquareTint(sf::RenderWindow& window, int file, int rank,
                                sf::Color color) const
{
    sf::RectangleShape sq({ squareSize_, squareSize_ });
    sq.setPosition(squareToPixel(file, rank));
    sq.setFillColor(color);
    window.draw(sq);
}

void BoardView::drawHighlights(sf::RenderWindow& window, const HighlightState& hl,
                                const Board& board) const
{
    if (hl.lastMoveFrom)
        drawSquareTint(window, hl.lastMoveFrom->first, hl.lastMoveFrom->second,
                       sf::Color(255, 255, 0, 80));
    if (hl.lastMoveTo)
        drawSquareTint(window, hl.lastMoveTo->first, hl.lastMoveTo->second,
                       sf::Color(255, 255, 0, 80));

    if (hl.selectedSquare)
        drawSquareTint(window, hl.selectedSquare->first, hl.selectedSquare->second,
                       sf::Color(0, 120, 215, 100));

    if (hl.checkSquare)
        drawSquareTint(window, hl.checkSquare->first, hl.checkSquare->second,
                       sf::Color(255, 0, 0, 100));

    float dotRadius = squareSize_ * 0.15f;
    float ringRadius = squareSize_ * 0.45f;
    float ringThickness = squareSize_ * 0.08f;

    for (auto& [file, rank] : hl.legalMoveTargets) {
        auto pos = squareToPixel(file, rank);
        float cx = pos.x + squareSize_ / 2.f;
        float cy = pos.y + squareSize_ / 2.f;

        Piece target = board.pieceAt(squareOf(file, rank));
        bool isCapture = !target.isNone();

        if (isCapture) {
            sf::CircleShape ring(ringRadius);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineColor(sf::Color(0, 0, 0, 60));
            ring.setOutlineThickness(ringThickness);
            ring.setOrigin({ ringRadius, ringRadius });
            ring.setPosition({ cx, cy });
            window.draw(ring);
        } else {
            sf::CircleShape dot(dotRadius);
            dot.setFillColor(sf::Color(0, 0, 0, 60));
            dot.setOrigin({ dotRadius, dotRadius });
            dot.setPosition({ cx, cy });
            window.draw(dot);
        }
    }
}

void BoardView::drawLabels(sf::RenderWindow& window, const sf::Font& font) const
{
    unsigned int fontSize = static_cast<unsigned int>(squareSize_ * 0.2f);
    if (fontSize < 10) fontSize = 10;

    static const sf::Color LightText(181, 136, 99);
    static const sf::Color DarkText(240, 217, 181);

    const char files[] = "abcdefgh";
    const char ranks[] = "12345678";

    for (int i = 0; i < 8; ++i) {
        int file = flipped_ ? (7 - i) : i;
        int rank = flipped_ ? (7 - i) : i;

        {
            bool light = (file + 0) % 2 != 0;
            auto pos = squareToPixel(file, 0);
            sf::Text text(font, std::string(1, files[file]), fontSize);
            text.setFillColor(light ? DarkText : LightText);
            text.setPosition({
                pos.x + squareSize_ - text.getGlobalBounds().size.x - 2.f,
                pos.y + squareSize_ - text.getGlobalBounds().size.y - 1.f
            });
            window.draw(text);
        }

        {
            bool light = (0 + rank) % 2 != 0;
            auto pos = squareToPixel(0, rank);
            sf::Text text(font, std::string(1, ranks[rank]), fontSize);
            text.setFillColor(light ? DarkText : LightText);
            text.setPosition({ pos.x + 2.f, pos.y + 1.f });
            window.draw(text);
        }
    }
}

void BoardView::drawPieces(sf::RenderWindow& window, const sf::Font& font,
                            const Board& board, const App& app) const
{
    float pieceSize = squareSize_ * 0.8f;
    float offset = (squareSize_ - pieceSize) / 2.f;

    unsigned int letterSize = static_cast<unsigned int>(squareSize_ * 0.5f);
    if (letterSize < 12) letterSize = 12;

    const char pieceLetters[] = { 'P', 'N', 'B', 'R', 'Q', 'K' };

    for (int file = 0; file < 8; ++file) {
        for (int rank = 0; rank < 8; ++rank) {
            Piece piece = board.pieceAt(squareOf(file, rank));
            if (piece.isNone()) continue;

            auto pos = squareToPixel(file, rank);

            if (app.piecesLoaded()) {
                const auto& tex = app.pieceTexture(piece.color, piece.type);
                sf::Sprite sprite(tex);
                float pieceScale = pieceSize / static_cast<float>(tex.getSize().x);
                sprite.setScale({ pieceScale, pieceScale });
                sprite.setPosition({ pos.x + offset, pos.y + offset });
                window.draw(sprite);
            } else {
                bool lightSquare = (file + rank) % 2 == 0;
                sf::Text letter(font, std::string(1, pieceLetters[static_cast<int>(piece.type)]), letterSize);
                letter.setFillColor(piece.color == Color::White
                    ? (lightSquare ? sf::Color(80, 80, 80) : sf::Color(240, 240, 240))
                    : (lightSquare ? sf::Color(40, 40, 40) : sf::Color(200, 200, 200)));
                auto lb = letter.getGlobalBounds();
                letter.setPosition({
                    pos.x + (squareSize_ - lb.size.x) / 2.f - lb.position.x,
                    pos.y + (squareSize_ - lb.size.y) / 2.f - lb.position.y
                });
                window.draw(letter);
            }
        }
    }
}

} // namespace chess::client
