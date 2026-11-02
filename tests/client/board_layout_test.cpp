#include "screens/board_layout.h"
#include "boardview.h"
#include "app.h"

#include <gtest/gtest.h>

#include <chess/types.h>

using namespace chess;
using namespace chess::client;
using namespace chess::client::layout;

namespace {

// Shared window shape (the app's fixed window); the expected values below
// are the pre-15.1 hand-drawn geometry, pinned so the consolidation stays
// pixel-identical.
constexpr float WinW = static_cast<float>(App::WindowWidth);
constexpr float WinH = static_cast<float>(App::WindowHeight);

struct BoardFixture {
    BoardFixture()
        : board(WinW, WinH, Color::White)
    {
    }
    BoardView board;
};

} // namespace

TEST(BoardLayout, PanelBandIsTheSingleWidthDefinition)
{
    BoardFixture f;
    BoardLayout l = composeBoardLayout(WinW, WinH, f.board, {});

    // White-oriented board: margin 20, board fills the height (960x640).
    EXPECT_FLOAT_EQ(f.board.panelX(), 640.f);
    EXPECT_FLOAT_EQ(l.panelX, 640.f);
    EXPECT_FLOAT_EQ(l.panelWidth, WinW - l.panelX - 8.f); // 312
    EXPECT_FLOAT_EQ(l.panelWidth, 312.f);
    // Full-bleed band (puzzle renderer): no right margin.
    EXPECT_FLOAT_EQ(l.panelEdgeWidth, 320.f);

    // The board rect rides along.
    EXPECT_FLOAT_EQ(l.squareSize, 75.f);
    EXPECT_FLOAT_EQ(l.boardSize, 600.f);
    EXPECT_FLOAT_EQ(l.boardOrigin.x, 20.f);
}

TEST(BoardLayout, CardStackMatchesTheFormerHudTable)
{
    BoardFixture f;
    BoardLayout l = composeBoardLayout(WinW, WinH, f.board, {true});

    EXPECT_FLOAT_EQ(l.opponentNameY, 20.f);
    EXPECT_FLOAT_EQ(l.opponentCapturedY, 42.f);
    EXPECT_FLOAT_EQ(l.localNameY, 66.f);
    EXPECT_FLOAT_EQ(l.localCapturedY, 88.f);
    EXPECT_FLOAT_EQ(l.statusY, 112.f);
    EXPECT_FLOAT_EQ(l.separatorY, 134.f);
    EXPECT_FLOAT_EQ(l.moveHeaderY, 144.f);

    EXPECT_FLOAT_EQ(l.moveList.position.x, 640.f);
    EXPECT_FLOAT_EQ(l.moveList.position.y, 162.f);
    EXPECT_FLOAT_EQ(l.moveList.size.x, 312.f);
    EXPECT_FLOAT_EQ(l.moveList.size.y, 236.f); // 398 - 162

    EXPECT_FLOAT_EQ(l.navRow.position.y, 402.f);
    EXPECT_FLOAT_EQ(l.navRow.size.y, 26.f);

    // Former Hud::contentBottom: nav row bottom + 4.
    EXPECT_FLOAT_EQ(l.actionRowY, 432.f);
    EXPECT_FLOAT_EQ(l.actionRowH, 30.f);
}

TEST(BoardLayout, ActionRowPlacesEqualPitchButtons)
{
    BoardFixture f;
    BoardLayout l = composeBoardLayout(WinW, WinH, f.board, {true});

    // GameScreen trio: 96-wide buttons on a 6px gap (former 0/102/204 pitch).
    auto b0 = l.actionRect(0, 96.f);
    auto b1 = l.actionRect(1, 96.f);
    auto b2 = l.actionRect(2, 96.f);
    EXPECT_FLOAT_EQ(b0.position.x, 640.f);
    EXPECT_FLOAT_EQ(b0.position.y, 432.f);
    EXPECT_FLOAT_EQ(b0.size.x, 96.f);
    EXPECT_FLOAT_EQ(b0.size.y, 30.f);
    EXPECT_FLOAT_EQ(b1.position.x, 742.f);
    EXPECT_FLOAT_EQ(b2.position.x, 844.f);

    // Accept/decline pair: 90-wide, 8px gap (former 0/98 pitch).
    EXPECT_FLOAT_EQ(l.actionRect(0, 90.f, 8.f).position.x, 640.f);
    EXPECT_FLOAT_EQ(l.actionRect(1, 90.f, 8.f).position.x, 738.f);

    // Local game duo: 140-wide, 8px gap (former pitch 148).
    EXPECT_FLOAT_EQ(l.actionRect(1, 140.f, 8.f).position.x, 788.f);
}

TEST(BoardLayout, ChatBundlePinsInputToTheWindowBottom)
{
    BoardFixture f;
    BoardLayout l = composeBoardLayout(WinW, WinH, f.board, {true, false, true});

    EXPECT_FLOAT_EQ(l.chatSeparatorY, 470.f); // 432 + 30 + 8
    EXPECT_FLOAT_EQ(l.chatLabelY, 476.f);

    // Log area: label + 18, down to 8 px above the input.
    EXPECT_FLOAT_EQ(l.chatPanel.position.y, 494.f);
    EXPECT_FLOAT_EQ(l.chatPanel.size.x, 312.f);
    EXPECT_FLOAT_EQ(l.chatInput.position.y, WinH - 20.f - 28.f); // 592
    EXPECT_FLOAT_EQ(l.chatInput.size.x, 312.f);
    EXPECT_FLOAT_EQ(l.chatInput.size.y, 28.f);
    EXPECT_FLOAT_EQ(l.chatPanel.position.y + l.chatPanel.size.y,
                    l.chatInput.position.y - 8.f);
}

TEST(BoardLayout, NonChatCardStackLeavesBundleZeroed)
{
    BoardFixture f;
    BoardLayout l = composeBoardLayout(WinW, WinH, f.board, {true});
    EXPECT_FLOAT_EQ(l.chatSeparatorY, 0.f);
    EXPECT_FLOAT_EQ(l.chatPanel.size.x, 0.f);
    EXPECT_FLOAT_EQ(l.chatInput.size.y, 0.f);
}

TEST(BoardLayout, AnalysisStackShiftsTheTableAndAddsEvalSlot)
{
    BoardFixture f;
    BoardLayout l = composeBoardLayout(WinW, WinH, f.board, {false, true});

    EXPECT_FLOAT_EQ(l.analysisTitleY, 30.f);
    EXPECT_FLOAT_EQ(l.analysisInfoY, 55.f);
    EXPECT_FLOAT_EQ(l.analysisPlyY, 80.f);

    EXPECT_FLOAT_EQ(l.moveList.position.y, 130.f);
    EXPECT_FLOAT_EQ(l.moveList.size.y, 270.f); // 400 - 130
    EXPECT_FLOAT_EQ(l.navRowA.position.y, 410.f);
    EXPECT_FLOAT_EQ(l.navRowA.size.y, 30.f); // taller than the Hud's 26

    EXPECT_FLOAT_EQ(l.evalTextY, 450.f);
    EXPECT_FLOAT_EQ(l.evalTurnY, 472.f);
    EXPECT_FLOAT_EQ(l.hintY, WinH - 40.f); // 600
}

TEST(BoardLayout, PuzzleStackBottomAnchorsTheActionColumn)
{
    BoardFixture f;
    BoardLayout l = composeBoardLayout(WinW, WinH, f.board,
                                       {false, false, false, true});

    EXPECT_FLOAT_EQ(l.puzzleTitleY, 18.f);
    EXPECT_FLOAT_EQ(l.puzzleStreakY, 44.f);
    EXPECT_FLOAT_EQ(l.puzzleStatusY, 68.f);
    EXPECT_FLOAT_EQ(l.puzzleSolutionY, 96.f);

    EXPECT_FLOAT_EQ(l.buttonInsetX, 14.f);
    EXPECT_FLOAT_EQ(l.buttonWidth, 150.f);
    EXPECT_FLOAT_EQ(l.buttonHeight, 30.f);
    EXPECT_FLOAT_EQ(l.buttonStep, 38.f);
    EXPECT_FLOAT_EQ(l.buttonBottomY, WinH - 56.f); // 584

    // Four rows counted from the bottom: Menu / Next / Solution / Hint.
    EXPECT_FLOAT_EQ(l.buttonY(0), 584.f);
    EXPECT_FLOAT_EQ(l.buttonY(1), 546.f);
    EXPECT_FLOAT_EQ(l.buttonY(2), 508.f);
    EXPECT_FLOAT_EQ(l.buttonY(3), 470.f);
}

TEST(BoardLayout, ComposeFormulaHoldsForOtherWindowShapes)
{
    // The existing clock/perft discipline: formulas, not magic screen
    // constants. A different window must re-derive every band.
    BoardView board(1280.f, 800.f, Color::White);
    BoardLayout l = composeBoardLayout(1280.f, 800.f, board, {true, false, true});

    const float boardSize = 800.f - 2.f * 20.f; // height-limited
    const float panelX = 20.f + boardSize + 20.f;
    EXPECT_FLOAT_EQ(l.panelX, panelX);
    EXPECT_FLOAT_EQ(l.panelWidth, 1280.f - panelX - 8.f);
    EXPECT_FLOAT_EQ(l.actionRowY, 432.f); // vertical table is window-independent
    EXPECT_FLOAT_EQ(l.chatInput.position.y, 800.f - 48.f);
}
