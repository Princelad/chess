#!/usr/bin/env python3
"""Position-aware mock UCI engine: reads the FEN it is handed and picks
its move from the live board (a knight on b1 plays b1c3, otherwise the
e-pawn pushes). Used to pin that a Chess960 bot searches the SP it was
dealt, not the standard start (audit 1.4): under the old hardcoded
"startpos" search this engine answered b1c3 for every position, which is
illegal on any SP without a knight on b1 and wedged the bot."""
import sys

placement = "rnbqkbnr"  # standard start until a position line arrives
white_to_move = True

def rank_row(rank):  # rank 1..8 -> FEN row string
    return placement.split("/")[8 - rank]

def bestmove():
    if white_to_move:
        return "b1c3" if len(rank_row(1)) > 1 and rank_row(1)[1] == "N" else "e2e4"
    return "b8c6" if len(rank_row(8)) > 1 and rank_row(8)[1] == "n" else "e7e5"

for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    if line == "uci":
        print("id name MockFenEngine 1.0")
        print("uciok")
        sys.stdout.flush()
    elif line == "isready":
        print("readyok")
        sys.stdout.flush()
    elif line.startswith("position"):
        parts = line.split()
        if "fen" in parts:
            idx = parts.index("fen")
            fen_parts = []
            i = idx + 1
            while i < len(parts) and parts[i] != "moves":
                fen_parts.append(parts[i])
                i += 1
            placement = fen_parts[0]
            if len(fen_parts) >= 2:
                white_to_move = fen_parts[1] == "w"
        elif "startpos" in parts:
            placement = "rnbqkbnr"
            white_to_move = True
            if "moves" in parts:
                white_to_move = (len(parts[idx + 1:]) % 2 == 0) if "idx" in dir() else True
    elif line.startswith("go"):
        bm = bestmove()
        print(f"info depth 1 score cp 10 pv {bm}")
        sys.stdout.flush()
        print(f"bestmove {bm}")
        sys.stdout.flush()
    elif line == "quit":
        break
