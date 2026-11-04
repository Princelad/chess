#pragma once

#include <SFML/Graphics.hpp>

namespace chess::client {

class BoardView;

// 15.1: the shared board composer — the ONE writer of the right-panel
// rect math. Every board-bearing screen queries this struct for its
// geometry (player cards, status, move list, action rows, chat bundle,
// eval slot, puzzle header); screens keep their event handling and own
// no rect arithmetic of their own.
//
// Row values reproduce the pre-15.1 hand-drawn geometry of each screen
// verbatim — this is a consolidation refactor, not a restyle. New panel
// shapes are built by changing THIS module, never re-deriving floats on
// a call site.
namespace layout {

// Per-screen parameterization (exactly one stack flag per screen; chat
// accompanies the card stack on the online GameScreen — spectator mode
// renders the same bundle with the input disabled by the screen).
struct BoardLayoutOptions {
    bool cards = false;    // player-card bands + status + "Moves" header stack
    bool analysis = false; // analysis label stack + eval text slot
    bool chat = false;     // chat bundle pinned under the action row
    bool puzzle = false;   // puzzle header rows + stacked action column
};

struct BoardLayout {
    // Panel band. `panelWidth` leaves the 8 px right margin the Hud
    // family widgets use; `panelEdgeWidth` is the full-bleed band the
    // puzzle (and any edge-to-edge) renderer paints.
    float panelX = 0.f;
    float panelWidth = 0.f;
    float panelEdgeWidth = 0.f;

    // Card-stack rows (opts.cards): the Hud family's fixed vertical
    // table — opponent band, status, separator, "Moves" header, move
    // list, nav buttons, then the shared action row.
    float opponentNameY = 0.f;
    float opponentCapturedY = 0.f;
    float localNameY = 0.f;
    float localCapturedY = 0.f;
    float statusY = 0.f;
    float separatorY = 0.f;
    float moveHeaderY = 0.f;
    sf::FloatRect moveList{};
    sf::FloatRect navRow{};
    float actionRowY = 0.f; // first action row top (former contentBottom)
    float actionRowH = 30.f;

    // Player card bands (opts.cards): a full card backdrop spanning both
    //rows of one seat (name row on top, captured glyphs below). Name /
    // captured rows keep their exact pre-15.1 Y positions — the bands
    // frame them without shifting anything.
    sf::FloatRect opponentCard{};
    sf::FloatRect localCard{};

    // Quick-message chips (15.4, opts.chat): one row of canned greetings
    // pinned between the log and the input; the log yields room for it.
    sf::FloatRect chipRow{};

    // Action slot in the card stack's action row: a left-to-right run of
    // equal-pitch buttons starting at the panel edge (the writer of the
    // 96/6 and 140/8 shapes the screens quoted before the composer).
    sf::FloatRect actionRect(int index, float width, float gap = 6.f) const
    {
        return sf::FloatRect(
            sf::Vector2f(panelX + static_cast<float>(index) * (width + gap),
                         actionRowY),
            sf::Vector2f(width, actionRowH));
    }

    // Chat bundle (opts.chat), online GameScreen only.
    float chatSeparatorY = 0.f;
    float chatLabelY = 0.f;
    sf::FloatRect chatPanel{}; // log area under the label
    sf::FloatRect chatInput{};

    // Analysis rows (opts.analysis).
    float analysisTitleY = 0.f;
    float analysisInfoY = 0.f;
    float analysisPlyY = 0.f;
    float moveListTop = 0.f;   // analysis move list spans moveListTop..-Bottom
    float moveListBottom = 0.f;
    sf::FloatRect navRowA{};   // analysis nav row (taller than the Hud's)
    float evalTextY = 0.f;
    float evalTurnY = 0.f;
    float hintY = 0.f;

    // Puzzle rows (opts.puzzle).
    float puzzleTitleY = 0.f;
    float puzzleStreakY = 0.f;
    float puzzleStatusY = 0.f;
    float puzzleSolutionY = 0.f;
    float buttonInsetX = 0.f;
    float buttonWidth = 0.f;
    float buttonHeight = 0.f;
    float buttonStep = 0.f;   // vertical pitch of the action column
    float buttonBottomY = 0.f; // bottom row (the "Menu" button)
    // Row for the k-th button counted from the bottom (0 = Menu).
    float buttonY(int fromBottom) const
    {
        return buttonBottomY - static_cast<float>(fromBottom) * buttonStep;
    }

    // Board rect passthrough read from the BoardView — consumers get the
    // full picture from one struct.
    sf::Vector2f boardOrigin{};
    float boardSize = 0.f;
    float squareSize = 0.f;
};

BoardLayout composeBoardLayout(float windowWidth, float windowHeight,
                               const BoardView& board,
                               BoardLayoutOptions options);

} // namespace layout
} // namespace chess::client
