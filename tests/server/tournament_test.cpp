// Unit tests for the 12.3 arena core: the pure pairing pass (closest
// scores meet, no immediate rematch), fixed-quota scoring (W=2/D=1/L=0),
// join validation (name uniqueness, capacity), the Ended reopen semantics
// and the survivor-scoring rule for games finished after a seat left.
#include <gtest/gtest.h>

#include "tournament.h"

using namespace chess::server;
using chess::net::GameResult;

namespace {

ArenaEntrant row(std::int64_t id, const std::string& name, int points,
                 std::int64_t lastOpponent = 0, bool inGame = false)
{
    ArenaEntrant r;
    r.id = id;
    r.name = name;
    // Convenience: encode points directly (wins = points/2 keeps parity
    // with arenaPoints, which reads wins/losses/draws).
    r.wins = points / 2;
    r.draws = points % 2;
    r.lastOpponentId = lastOpponent;
    r.inGame = inGame;
    return r;
}

} // namespace

// ── Scoring ─────────────────────────────────────────────────────────────────

TEST(ArenaPoints, WinDrawLossValues)
{
    ArenaEntrant r;
    r.wins = 3;
    EXPECT_EQ(arenaPoints(r), 6);
    r.draws = 2;
    EXPECT_EQ(arenaPoints(r), 8);
    r.losses = 5;
    EXPECT_EQ(arenaPoints(r), 8);
    EXPECT_EQ(arenaGames(r), 10);
}

// ── Join / leave ────────────────────────────────────────────────────────────

TEST(TournamentJoin, JoinAssignsFreshIds)
{
    Tournament t("5+0|", 3);
    std::string err;
    auto a = t.join("Alice", err);
    auto b = t.join("Bruno", err);
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_NE(*a, *b);
    EXPECT_EQ(t.findByName("Alice"), *a);
}

TEST(TournamentJoin, DuplicateNameRefused)
{
    Tournament t("", 2);
    std::string err;
    ASSERT_TRUE(t.join("Alice", err).has_value());
    auto dup = t.join("Alice", err);
    EXPECT_FALSE(dup.has_value());
    EXPECT_FALSE(err.empty());
    // The failed join consumed nothing.
    EXPECT_EQ(t.entrants().size(), 1u);
}

TEST(TournamentJoin, LeaveDrops)
{
    Tournament t("", 2);
    std::string err;
    auto id = *t.join("Alice", err);
    EXPECT_TRUE(t.leave(id));
    EXPECT_FALSE(t.has(id));
    // Free of the id, the name slots away from the departed row.
    EXPECT_TRUE(t.join("Alice", err).has_value());
}

TEST(TournamentJoin, LeaveUnknownIdNoop)
{
    Tournament t("", 2);
    EXPECT_FALSE(t.leave(999));
    EXPECT_FALSE(t.leave(0));
}

TEST(TournamentJoin, QuotaBelowOneClampsToOne)
{
    Tournament t("", 0);
    EXPECT_EQ(t.roundsTarget(), 1);
    Tournament neg("", -5);
    EXPECT_EQ(neg.roundsTarget(), 1);
}

TEST(TournamentJoin, EndedArenaReopensWithFreshRows)
{
    Tournament t("", 1);
    std::string err;
    auto first = *t.join("Alice", err);
    auto other = *t.join("Bruno", err);
    t.entrant(first)->wins = 1;
    ASSERT_TRUE(t.applyResult("Alice", "Bruno", GameResult::WhiteWins));
    EXPECT_TRUE(t.everyoneFinished());
    t.endArena();
    EXPECT_EQ(t.phase(), ArenaPhase::Ended);

    // A fresh join reopens: rows cleared, phase back open, quota fresh.
    auto rejoin = t.join("Alice", err);
    ASSERT_TRUE(rejoin.has_value());
    EXPECT_EQ(t.phase(), ArenaPhase::Open);
    EXPECT_EQ(t.entrants().size(), 1u);
    EXPECT_EQ(t.entrant(*rejoin)->wins, 0);
    // The generation's row count stayed clean of the old members.
    EXPECT_EQ(t.findByName("Bruno"), 0);
}

// ── applyResult ─────────────────────────────────────────────────────────────

TEST(TournamentApplyResult, StandardScoring)
{
    Tournament t("", 2);
    std::string err;
    auto a = *t.join("Alice", err);
    auto b = *t.join("Bruno", err);
    t.entrant(a)->inGame = true;
    t.entrant(b)->inGame = true;

    ASSERT_TRUE(t.applyResult("Alice", "Bruno", GameResult::WhiteWins));
    EXPECT_EQ(t.entrant(a)->wins, 1);
    EXPECT_EQ(t.entrant(b)->losses, 1);
    EXPECT_EQ(arenaPoints(*t.entrant(a)), 2);
    EXPECT_FALSE(t.entrant(a)->inGame);
    EXPECT_FALSE(t.entrant(b)->inGame);

    t.entrant(a)->inGame = true;
    t.entrant(b)->inGame = true;
    t.applyResult("Alice", "Bruno", GameResult::Draw);
    EXPECT_EQ(arenaPoints(*t.entrant(a)), 3);
    EXPECT_EQ(arenaPoints(*t.entrant(b)), 1);
}

TEST(TournamentApplyResult, BlackSeatWinsScoreBlackRow)
{
    Tournament t("", 2);
    std::string err;
    t.join("Alice", err);
    t.join("Bruno", err);
    // First argument is the White seat's name: here Bruno is White,
    // Alice is Black, and the BLACK side wins → Alice collects the win.
    ASSERT_TRUE(t.applyResult("Bruno", "Alice", GameResult::BlackWins));
    EXPECT_EQ(t.entrant(t.findByName("Alice"))->wins, 1);
    EXPECT_EQ(t.entrant(t.findByName("Bruno"))->losses, 1);
}

TEST(TournamentApplyResult, AbortChangesNoScores)
{
    Tournament t("", 2);
    std::string err;
    auto a = *t.join("Alice", err);
    auto b = *t.join("Bruno", err);
    t.entrant(a)->inGame = true;
    t.entrant(b)->inGame = true;
    ASSERT_TRUE(t.applyResult("Alice", "Bruno", GameResult::Abort));
    EXPECT_EQ(t.entrant(a)->wins + t.entrant(a)->losses + t.entrant(a)->draws,
              0);
    EXPECT_EQ(t.entrant(b)->wins + t.entrant(b)->losses + t.entrant(b)->draws,
              0);
    // The pairing is over regardless — rows unseat for the next pass.
    EXPECT_FALSE(t.entrant(a)->inGame);
}

TEST(TournamentApplyResult, SurvivorScoresWhenOpponentRowReaped)
{
    // Disconnect reaps the leaver's row before the game ends; the
    // survivor still collects their result by name.
    Tournament t("", 2);
    std::string err;
    auto a = *t.join("Alice", err);
    t.join("Bruno", err);
    t.leave(t.findByName("Bruno")); // opponent's row is gone

    EXPECT_TRUE(t.applyResult("Alice", "Bruno", GameResult::WhiteWins));
    EXPECT_EQ(t.entrant(a)->wins, 1);
    EXPECT_EQ(arenaPoints(*t.entrant(a)), 2);
}

TEST(TournamentApplyResult, UnknownNamesChangeNothing)
{
    Tournament t("", 2);
    std::string err;
    t.join("Alice", err);
    EXPECT_FALSE(t.applyResult("Zed", "Yan", GameResult::Draw));
    EXPECT_EQ(t.entrants().size(), 1u);
}

// ── everyoneFinished / endArena ─────────────────────────────────────────────

TEST(TournamentQuota, EveryoneFinished)
{
    Tournament t("", 2);
    std::string err;
    auto a = *t.join("Alice", err);
    auto b = *t.join("Bruno", err);
    auto c = *t.join("Cara", err);
    EXPECT_FALSE(t.everyoneFinished());

    // One game for Alice: Bruno and Cara owe two.
    t.entrant(a)->wins = 1;
    EXPECT_FALSE(t.everyoneFinished());

    // Bruno catches up; Alice and Cara still owe one.
    t.entrant(b)->wins = 2;
    EXPECT_FALSE(t.everyoneFinished());
    t.entrant(c)->draws = 2;
    EXPECT_FALSE(t.everyoneFinished());

    // Cara is done now; Alice is the last straggler.
    t.entrant(a)->wins = 2;
    EXPECT_TRUE(t.everyoneFinished());
}

TEST(TournamentQuota, EndedPhaseFreezesUntilReopen)
{
    Tournament t("", 1);
    std::string err;
    t.join("Alice", err);
    t.join("Bruno", err);
    EXPECT_TRUE(t.applyResult("Alice", "Bruno", GameResult::WhiteWins));
    EXPECT_TRUE(t.everyoneFinished());
    t.endArena();
    EXPECT_EQ(t.phase(), ArenaPhase::Ended);
    t.endArena(); // idempotent
    EXPECT_EQ(t.phase(), ArenaPhase::Ended);
}

// ── arenaPairIdle ───────────────────────────────────────────────────────────

namespace {

std::vector<std::pair<std::size_t, std::size_t>>
sortedPairs(std::vector<ArenaEntrant> idle)
{
    auto pairs = arenaPairIdle(idle);
    for (auto& [l, r] : pairs)
        if (l > r) std::swap(l, r);
    return pairs;
}

} // namespace

TEST(ArenaPairIdle, PairsClosestScores)
{
    // ids 1..4 at 0/1/2/4 points: the two lowest scores meet, then the
    // two remaining.
    std::vector<ArenaEntrant> idle = {row(1, "Zero", 0), row(2, "One", 1),
                                      row(3, "Two", 2), row(4, "Four", 4)};
    auto pairs = sortedPairs(idle);
    ASSERT_EQ(pairs.size(), 2u);
    EXPECT_EQ(idle[pairs[0].first].id + idle[pairs[0].second].id, 3)
        << "0 + 1 pts pair first";
    EXPECT_EQ(idle[pairs[1].first].id + idle[pairs[1].second].id, 7)
        << "then 2 + 4 pts";
}

TEST(ArenaPairIdle, TieBreaksToSmallestId)
{
    // Three candidates tie at 2 pts for the 0-pts row: the smallest id
    // wins the pairing.
    std::vector<ArenaEntrant> idle = {row(1, "Zero", 0), row(2, "TwoA", 2),
                                      row(3, "TwoB", 2), row(4, "TwoC", 2)};
    auto pairs = sortedPairs(idle);
    ASSERT_EQ(pairs.size(), 2u);
    EXPECT_EQ(idle[pairs[0].first].id + idle[pairs[0].second].id, 3)
        << "Zero meets TwoA (id 2), not the later ties";
}

TEST(ArenaPairIdle, AvoidsImmediateRematchWhenAlternativesExist)
{
    // Three rows at 0 points; 1 and 2 already played each other
    // (lastOpponentId set). The pass must pair 1-3 and 2-...: with an odd
    // count one row rests — 1 and 2 must NOT meet again while 3 exists.
    std::vector<ArenaEntrant> idle = {row(1, "A", 0, 2), row(2, "B", 0, 1),
                                      row(3, "C", 0)};
    auto pairs = sortedPairs(idle);
    for (const auto& [l, r] : pairs) {
        const auto& a = idle[l];
        const auto& b = idle[r];
        EXPECT_FALSE(a.id + b.id == 3)
            << "1+2 re-match forbidden while C idles";
    }
    // Four-entrant even case where the rematch is fully avoidable: 1 and
    // 2 played before (mutual lastOpponentId) but id 4 sits at the same
    // score, so the pass routes 1→4 and 2→3.
    std::vector<ArenaEntrant> four = {row(1, "A", 0, 2), row(2, "B", 0, 1),
                                      row(3, "C", 2, 4), row(4, "D", 0)};
    auto evenPairs = sortedPairs(four);
    ASSERT_EQ(evenPairs.size(), 2u);
    EXPECT_EQ(four[evenPairs[0].first].id + four[evenPairs[0].second].id, 5)
        << "A meets D, not the rematch with B";
    EXPECT_EQ(four[evenPairs[1].first].id + four[evenPairs[1].second].id, 5)
        << "2 meets 3";
}

TEST(ArenaPairIdle, RematchAllowedWhenNoAlternative)
{
    // Two rows only: the previous-opponent guard cannot apply, they must
    // pair (or the arena stalls forever).
    std::vector<ArenaEntrant> idle = {row(1, "A", 0, 2), row(2, "B", 0, 1)};
    auto pairs = sortedPairs(idle);
    ASSERT_EQ(pairs.size(), 1u);
    EXPECT_EQ(idle[pairs[0].first].id + idle[pairs[0].second].id, 3);
}

TEST(ArenaPairIdle, OddCountLeavesOneOut)
{
    std::vector<ArenaEntrant> idle = {row(1, "A", 0), row(2, "B", 0),
                                      row(3, "C", 0)};
    auto pairs = sortedPairs(idle);
    EXPECT_EQ(pairs.size(), 1u);
}

TEST(ArenaPairIdle, EmptyAndSingleton)
{
    EXPECT_TRUE(arenaPairIdle({}).empty());
    EXPECT_TRUE(arenaPairIdle({row(1, "A", 0)}).empty());
}

TEST(ArenaPairIdle, Deterministic)
{
    std::vector<ArenaEntrant> idle = {row(1, "A", 0), row(2, "B", 2),
                                      row(3, "C", 2), row(4, "D", 3)};
    auto first = sortedPairs(idle);
    auto second = sortedPairs(idle);
    EXPECT_EQ(first, second);
}
