#pragma once

#include "app.h"
#include "board_interaction.h"
#include "boardview.h"
#include "move_animator.h"
#include "puzzles.h"
#include "promo_state.h"
#include "widgets/button.h"

#include <chess/board.h>
#include <chess/move.h>
#include <chess/types.h>

#include <optional>
#include <string>
#include <vector>

namespace chess::client {

// Offline puzzle mode (12.2): mate-in-N positions from the bundled
// assets/puzzles.pgn. The solver plays; every correct move triggers the
// stored defender reply, a wrong move restarts the puzzle and zeroes the
// streak, and the best streak persists via config (puzzle.best_streak).
// Pure puzzle data/rules live in puzzles.{h,cpp}; this screen has no logic
// of its own worth testing.
class PuzzleScreen : public Screen {
public:
    explicit PuzzleScreen(App& app);

    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    const Puzzle* current() const;
    void loadPuzzle(int index);
    void resetPosition();

    void selectPiece(int file, int rank);
    void tryMove(int targetFile, int targetRank);
    void buildPromotion(int fromFile, int fromRank, int toFile, int toRank);
    void applyPromotionMove(chess::PieceType type);
    void cancelPromotion();
    void applySolverMove(const chess::Move& m);
    void startMovePresentation(const chess::Move& m);
    void scheduleReply();
    void fireReply();
    void onSolved();
    void onWrongMove();
    void hint();
    void revealSolution();
    void nextPuzzle();
    void backToMenu();

    bool solverTurn() const;
    bool solverPieceAt(int file, int rank) const;
    void deselect();
    void clearBoardInput();
    void syncViewHighlights();
    void setStatus(std::string text, float seconds = 999.f);
    void refreshStatus();
    PromoCell promoCell(int index) const;
    std::optional<chess::PieceType> promoTypeForKey(sf::Keyboard::Key key) const;
    void drawHelpOverlay(sf::RenderWindow& window);

    App& app_;
    std::vector<Puzzle> puzzles_;
    int index_ = -1;
    PuzzleStreak streak_;

    Board board_;
    int solutionPly_ = 0; // next solution ply the solver owes
    std::vector<std::string> playedSans_;
    bool solved_ = false;
    bool revealed_ = false;
    int hintLevel_ = 0;

    BoardView boardView_;
    HighlightState hl_;
    BoardAnnotations annotations_;
    DragTracker drag_;
    std::optional<std::pair<int, int>> dragFrom_;
    std::optional<std::pair<int, int>> rightPress_;
    std::optional<std::pair<int, int>> lastFrom_;
    std::optional<std::pair<int, int>> lastTo_;
    sf::Vector2f cursor_{0.f, 0.f};
    std::pair<int, int> keyCursor_{4, 4};
    bool showingHelp_ = false;

    MoveAnimator anim_;
    std::optional<chess::Move> pendingReply_;
    float replyTimer_ = 0.f;
    int promoHover_ = -1;
    std::optional<PromotionState> promo_;

    Button hintBtn_;
    Button solutionBtn_;
    Button nextBtn_;
    Button backBtn_;

    // Panel text widgets rebuilt per puzzle / per status change.
    std::string title_;
    std::string status_;
    float statusTimer_ = 0.f;
    std::string playedLine_;
};

} // namespace chess::client
