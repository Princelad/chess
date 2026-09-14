#include "sfx.h"

#include <chess/san.h>

#include <gtest/gtest.h>

#include <cstdint>

namespace chess::client {
namespace {

Board startPos() { return Board::fromStartPos(); }

std::optional<chess::Move> play(Board& board, const std::string& san)
{
    auto m = chess::san::fromSan(board, san);
    if (m) board.makeMove(*m);
    return m;
}

TEST(ClassifyMove, QuietMoveIsMove)
{
    Board b = startPos();
    auto m = play(b, "e4");
    ASSERT_TRUE(m.has_value());
    Board before = startPos();
    EXPECT_EQ(classifyMove(before, *m), Sfx::Move);
}

TEST(ClassifyMove, CaptureIsCapture)
{
    // White pawn on e5 captures the pawn on f6 en passant.
    Board b = *Board::fromFen("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3");
    auto m = play(b, "exf6");
    ASSERT_TRUE(m.has_value());
    Board before = *Board::fromFen(
        "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3");
    EXPECT_EQ(classifyMove(before, *m), Sfx::Capture);
}

TEST(ClassifyMove, OrdinaryCaptureIsCapture)
{
    Board b = *Board::fromFen("k7/p7/1P6/8/8/8/8/K7 w - - 0 1");
    auto m = play(b, "bxa7");
    ASSERT_TRUE(m.has_value());
    Board before = *Board::fromFen("k7/p7/1P6/8/8/8/8/K7 w - - 0 1");
    EXPECT_EQ(classifyMove(before, *m), Sfx::Capture);
}

TEST(ClassifyMove, CheckIsCheck)
{
    // Rh8+ from the start rank delivers check to the lone king on e8.
    Board b = *Board::fromFen("4k3/8/8/8/8/8/8/7R w - - 0 1");
    auto m = play(b, "Rh8+");
    ASSERT_TRUE(m.has_value());
    Board before = *Board::fromFen("4k3/8/8/8/8/8/8/7R w - - 0 1");
    EXPECT_EQ(classifyMove(before, *m), Sfx::Check);
}

TEST(ClassifyMove, FoolMateIsCheckmate)
{
    Board b = startPos();
    play(b, "f3");
    play(b, "e5");
    play(b, "g4");
    auto mate = play(b, "Qh4#");
    ASSERT_TRUE(mate.has_value());
    Board before = *Board::fromFen("rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 1 3");
    EXPECT_EQ(classifyMove(before, *mate), Sfx::Checkmate);
}

TEST(ClassifyMove, CapturingMatingMoveIsCheckmateNotCapture)
{
    // Scholar's mate: Qh5 dominates e5, Qxf7# captures f7 and mates.
    Board b = startPos();
    play(b, "e4");
    play(b, "e5");
    play(b, "Bc4");
    play(b, "Nc6");
    play(b, "Qh5");
    play(b, "Nf6");
    auto mate = play(b, "Qxf7#");
    ASSERT_TRUE(mate.has_value());
    Board before = *Board::fromFen("r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4");
    EXPECT_EQ(classifyMove(before, *mate), Sfx::Checkmate);
}

TEST(SoundSynth, ToneHasExpectedSampleCount)
{
    auto samples = SoundManager::synthTone(330.f, 0.10f, 15.f);
    EXPECT_EQ(samples.size(), static_cast<std::size_t>(0.10f * SoundManager::sampleRate()));
}

TEST(SoundSynth, ToneIsNotSilent)
{
    const auto samples = SoundManager::synthTone(330.f, 0.10f, 15.f);
    bool nonzero = false;
    for (std::int16_t s : samples) {
        if (s != 0) { nonzero = true; break; }
    }
    EXPECT_TRUE(nonzero);
}

TEST(SoundSynth, MoveAndMateBuffersDiffer)
{
    auto moveBuf = SoundManager::makeToneBuffer(330.f, 0.09f, 15.f);
    // Checkmate is a three-note descending sequence built by the player; the
    // standalone buffer differs from the move tone already in duration.
    auto otherBuf = SoundManager::makeToneBuffer(262.f, 0.20f, 18.f);
    EXPECT_NE(moveBuf.getSampleCount(), otherBuf.getSampleCount());
    EXPECT_NE(moveBuf.getChannelCount(), 0u);
    EXPECT_EQ(moveBuf.getSampleRate(), SoundManager::sampleRate());
}

TEST(SoundManager, DefaultsAndGating)
{
    SoundManager sm;
    EXPECT_TRUE(sm.enabled());
    EXPECT_EQ(sm.volume(), 100);
    EXPECT_FALSE(sm.ready()); // lazy: nothing built until play()

    sm.setEnabled(false);
    sm.play(Sfx::Move); // must be a no-op
    EXPECT_FALSE(sm.ready());

    sm.setEnabled(true);
    sm.setVolume(40);
    EXPECT_EQ(sm.volume(), 40);

    sm.play(Sfx::Check);
    EXPECT_TRUE(sm.ready()); // buffers built lazily on first play

    sm.setVolume(0);
    sm.play(Sfx::Check); // still gated by enabled() only; volume 0 is allowed
}

} // namespace
} // namespace chess::client