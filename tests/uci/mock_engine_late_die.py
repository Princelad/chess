#!/usr/bin/env python3
"""Mock UCI engine that answers the first go late (0.5 s) and then exits:
the answer is queued under the first search's generation and the process is
dead before the caller retries. waitBestMove() wakes on the death and must
not hand the stale answer to the retry's position."""
import sys
import time

answered = False
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    if line == "uci":
        print("id name MockLateDieEngine 1.0")
        print("uciok")
        sys.stdout.flush()
    elif line == "isready":
        print("readyok")
        sys.stdout.flush()
    elif line.startswith("go"):
        if not answered:
            answered = True
            time.sleep(0.5)
            print("bestmove e2e4")
            sys.stdout.flush()
            sys.exit(0)  # the engine dies after its one (late) answer
    elif line == "quit":
        break
