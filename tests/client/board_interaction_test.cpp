#include "board_interaction.h"
#include "boardview.h"

#include <chess/move.h>
#include <chess/types.h>

#include <gtest/gtest.h>

namespace chess::client {
namespace {

TEST(BoardAnnotations, AddArrowDeduplicates)
{
    BoardAnnotations ann;
    ann.addArrow(0, 0, 1, 1);
    ann.addArrow(0, 0, 1, 1);
    EXPECT_EQ(ann.arrows().size(), 1u);
    EXPECT_FALSE(ann.empty());

    ann.addArrow(2, 2, 3, 3);
    EXPECT_EQ(ann.arrows().size(), 2u);
}

TEST(BoardAnnotations, ToggleCircleAddsThenRemoves)
{
    BoardAnnotations ann;
    ann.toggleCircle(4, 4);
    ASSERT_EQ(ann.circles().size(), 1u);
    EXPECT_EQ(ann.circles()[0], std::make_pair(4, 4));
    EXPECT_FALSE(ann.empty());

    ann.toggleCircle(4, 4);
    EXPECT_TRUE(ann.circles().empty());
    EXPECT_TRUE(ann.empty());
}

TEST(BoardAnnotations, ClearRemovesEverything)
{
    BoardAnnotations ann;
    ann.addArrow(0, 0, 7, 7);
    ann.toggleCircle(3, 3);
    ann.clear();
    EXPECT_TRUE(ann.empty());
    EXPECT_TRUE(ann.arrows().empty());
    EXPECT_TRUE(ann.circles().empty());
}

TEST(DragTracker, ThresholdTransitions)
{
    DragTracker drag(8.f);
    EXPECT_EQ(drag.phase(), DragPhase::Idle);

    drag.press(100.f, 100.f);
    EXPECT_EQ(drag.phase(), DragPhase::Tracking);

    drag.move(104.f, 102.f); // displacement ~4.5 < 8
    EXPECT_EQ(drag.phase(), DragPhase::Tracking);

    drag.move(110.f, 100.f); // displacement 10 >= 8
    EXPECT_EQ(drag.phase(), DragPhase::Dragging);
    EXPECT_TRUE(drag.isDragging());

    EXPECT_TRUE(drag.release());
    EXPECT_EQ(drag.phase(), DragPhase::Idle);
}

TEST(DragTracker, SimpleClickIsNotDrag)
{
    DragTracker drag(8.f);
    drag.press(50.f, 50.f);
    EXPECT_FALSE(drag.release());
    EXPECT_EQ(drag.phase(), DragPhase::Idle);
}

TEST(DragTracker, CancelResets)
{
    DragTracker drag(8.f);
    drag.press(0.f, 0.f);
    drag.move(30.f, 0.f);
    EXPECT_TRUE(drag.isDragging());
    drag.cancel();
    EXPECT_EQ(drag.phase(), DragPhase::Idle);
    EXPECT_FALSE(drag.isActive());
}

TEST(BoardView, PixelToSquareRoundTrip)
{
    BoardView unflipped(960.f, 640.f, Color::White);
    BoardView flipped(960.f, 640.f, Color::Black);

    for (int file = 0; file < 8; ++file) {
        for (int rank = 0; rank < 8; ++rank) {
            auto expected = std::make_pair(file, rank);
            EXPECT_EQ(unflipped.pixelToSquare(unflipped.squareCenter(file, rank)),
                      expected);
            EXPECT_EQ(flipped.pixelToSquare(flipped.squareCenter(file, rank)),
                      expected);
        }
    }
}

TEST(BoardView, TopLeftCornerMapsToA8Unflipped)
{
    BoardView unflipped(960.f, 640.f, Color::White);
    EXPECT_EQ(unflipped.pixelToSquare(unflipped.boardOrigin()),
              std::make_pair(0, 7));

    BoardView flipped(960.f, 640.f, Color::Black);
    EXPECT_EQ(flipped.pixelToSquare(flipped.boardOrigin()),
              std::make_pair(7, 0));
}

TEST(AutoQueenMove, ResolvesQueenCandidate)
{
    chess::Move mq(chess::squareOf(4, 6), chess::squareOf(4, 7),
                   chess::MoveFlag::Promotion);
    mq.promotion = PieceType::Queen;
    chess::Move mr(chess::squareOf(4, 6), chess::squareOf(4, 7),
                   chess::MoveFlag::Promotion);
    mr.promotion = PieceType::Rook;

    auto qm = autoQueenMove({ mr, mq });
    ASSERT_TRUE(qm.has_value());
    EXPECT_EQ(qm->promotion, PieceType::Queen);
}

TEST(AutoQueenMove, EmptyOrNoQueenReturnsNullopt)
{
    EXPECT_FALSE(autoQueenMove({}).has_value());

    chess::Move mr(chess::squareOf(4, 6), chess::squareOf(4, 7),
                   chess::MoveFlag::Promotion);
    mr.promotion = PieceType::Rook;
    EXPECT_FALSE(autoQueenMove({ mr }).has_value());
}

} // namespace
} // namespace chess::client