#!/usr/bin/env python3
"""Minimal UCI engine for testing. Responds to basic UCI protocol."""
import sys
import time

def bestmove_for_turn(is_white):
    return "e2e4" if is_white else "e7e5"

def main():
    is_white_turn = True

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue

        if line == "uci":
            print("id name MockEngine 1.0")
            print("id author Test")
            print("uciok")
            sys.stdout.flush()
        elif line == "isready":
            print("readyok")
            sys.stdout.flush()
        elif line == "ucinewgame":
            is_white_turn = True
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
                fen = " ".join(fen_parts)
                if len(fen_parts) >= 2:
                    is_white_turn = fen_parts[1] == "w"
            elif "startpos" in parts:
                is_white_turn = True
                if "moves" in parts:
                    idx = parts.index("moves")
                    move_count = len(parts[idx+1:])
                    is_white_turn = (move_count % 2 == 0)
        elif line.startswith("go"):
            parts = line.split()
            depth = 1
            for i, p in enumerate(parts):
                if p == "depth" and i+1 < len(parts):
                    depth = int(parts[i+1])

            bm = bestmove_for_turn(is_white_turn)
            print(f"info depth {depth} score cp 10 pv {bm}")
            sys.stdout.flush()
            time.sleep(0.01)
            print(f"bestmove {bm}")
            sys.stdout.flush()
        elif line.startswith("setoption"):
            pass
        elif line == "stop":
            bm = bestmove_for_turn(is_white_turn)
            print(f"bestmove {bm}")
            sys.stdout.flush()
        elif line == "quit":
            break

if __name__ == "__main__":
    main()
