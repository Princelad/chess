#!/usr/bin/env python3
"""Minimal UCI engine for testing. Responds to basic UCI protocol."""
import sys
import time

def main():
    position_fen = "startpos"
    position_moves = []

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
            print("readyok")
            sys.stdout.flush()
        elif line.startswith("position"):
            parts = line.split()
            if "startpos" in parts:
                position_fen = "startpos"
                position_moves = []
                if "moves" in parts:
                    idx = parts.index("moves")
                    position_moves = parts[idx+1:]
            elif "fen" in parts:
                idx = parts.index("fen")
                fen_parts = []
                i = idx + 1
                while i < len(parts) and parts[i] != "moves":
                    fen_parts.append(parts[i])
                    i += 1
                position_fen = " ".join(fen_parts)
                position_moves = parts[i+1:] if i < len(parts) and parts[i] == "moves" else []
        elif line.startswith("go"):
            parts = line.split()
            depth = 1
            for i, p in enumerate(parts):
                if p == "depth" and i+1 < len(parts):
                    depth = int(parts[i+1])

            print(f"info depth {depth} score cp 10 pv e2e4")
            sys.stdout.flush()
            time.sleep(0.01)
            print("bestmove e2e4")
            sys.stdout.flush()
        elif line.startswith("setoption"):
            pass
        elif line == "stop":
            print("bestmove e2e4")
            sys.stdout.flush()
        elif line == "quit":
            break

if __name__ == "__main__":
    main()
