#include "board_layout.h"

#include "boardview.h"

namespace chess::client::layout {

BoardLayout composeBoardLayout(float windowWidth, float windowHeight,
                               const BoardView& board,
                               BoardLayoutOptions options)
{
    BoardLayout l;

    // Board rect passthrough.
    l.boardOrigin = board.boardOrigin();
    l.boardSize = board.squareSize() * 8.f;
    l.squareSize = board.squareSize();

    // Panel band: the single definition of both width shapes.
    l.panelX = board.panelX();
    l.panelEdgeWidth = windowWidth - l.panelX;
    l.panelWidth = windowWidth - l.panelX - 8.f;

    // Card stack (Hud family): the fixed vertical table games have always
    // drawn — opponent band, status, separator, "Moves" header, move list,
    // nav buttons, shared action row.
    if (options.cards) {
        l.opponentNameY = 20.f;
        l.opponentCapturedY = 42.f;
        l.localNameY = 66.f;
        l.localCapturedY = 88.f;
        l.statusY = 112.f;
        l.separatorY = 134.f;
        l.moveHeaderY = 144.f;

        l.moveList = sf::FloatRect(
            sf::Vector2f(l.panelX, 162.f),
            sf::Vector2f(l.panelWidth, 398.f - 162.f));
        l.navRow = sf::FloatRect(
            sf::Vector2f(l.panelX, 402.f),
            sf::Vector2f(l.panelWidth, 26.f));
        // First action row sits just below the nav buttons.
        l.actionRowY = l.navRow.position.y + l.navRow.size.y + 4.f;
        l.actionRowH = 30.f;

        // Card bands frame the four card rows (name at +6, captured at
        // +28 inside each 44-px band).
        l.opponentCard = sf::FloatRect(
            sf::Vector2f(l.panelX, l.opponentNameY - 6.f),
            sf::Vector2f(l.panelWidth, 44.f));
        l.localCard = sf::FloatRect(
            sf::Vector2f(l.panelX, l.localNameY - 6.f),
            sf::Vector2f(l.panelWidth, 44.f));
    }

    // Chat bundle: pinned between the action row and the window bottom
    // (online GameScreen; spectators render it with the input disabled).
    if (options.chat) {
        l.chatSeparatorY = l.actionRowY + l.actionRowH + 8.f;
        l.chatLabelY = l.chatSeparatorY + 6.f;

        const float logTop = l.chatSeparatorY + 24.f;
        const float inputY = windowHeight - 20.f - 28.f;
        const float logBottom = inputY - 8.f;

        l.chatPanel = sf::FloatRect(
            sf::Vector2f(l.panelX, logTop),
            sf::Vector2f(l.panelWidth, logBottom - logTop));
        l.chatInput = sf::FloatRect(
            sf::Vector2f(l.panelX, inputY),
            sf::Vector2f(l.panelWidth, 28.f));
    }

    // Analysis stack: the label pile above a taller move list, an eval
    // text slot under the nav row, and the bottom-edge hint.
    if (options.analysis) {
        l.analysisTitleY = 30.f;
        l.analysisInfoY = 55.f;
        l.analysisPlyY = 80.f;

        l.moveListTop = 130.f;
        l.moveListBottom = 400.f;
        l.moveList = sf::FloatRect(
            sf::Vector2f(l.panelX, l.moveListTop),
            sf::Vector2f(l.panelWidth, l.moveListBottom - l.moveListTop));
        l.navRowA = sf::FloatRect(
            sf::Vector2f(l.panelX, 410.f),
            sf::Vector2f(l.panelWidth, 30.f));

        l.evalTextY = 450.f;
        l.evalTurnY = l.evalTextY + 22.f;
        l.hintY = windowHeight - 40.f;
    }

    // Puzzle stack: header rows and a bottom-anchored column of action
    // buttons; no cards, no move list.
    if (options.puzzle) {
        l.puzzleTitleY = 18.f;
        l.puzzleStreakY = 44.f;
        l.puzzleStatusY = 68.f;
        l.puzzleSolutionY = 96.f;

        l.buttonInsetX = 14.f;
        l.buttonWidth = 150.f;
        l.buttonHeight = 30.f;
        l.buttonStep = l.buttonHeight + 8.f; // button height + gap
        l.buttonBottomY = windowHeight - 56.f;
    }

    return l;
}

} // namespace chess::client::layout
