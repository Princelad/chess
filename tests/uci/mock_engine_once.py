#!/usr/bin/env python3
"""Mock UCI engine that exits right after its first bestmove — exercises
the bot's Dead path (audit 1.3): a dead engine must not wedge the match."""
import sys

for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    if line == "uci":
        print("id name MockOnceEngine 1.0")
        print("uciok")
        sys.stdout.flush()
    elif line == "isready":
        print("readyok")
        sys.stdout.flush()
    elif line.startswith("go"):
        print("info depth 1 score cp 10 pv e7e5")
        sys.stdout.flush()
        print("bestmove e7e5")
        sys.stdout.flush()
        sys.exit(0)  # engine dies after answering once
    elif line == "quit":
        break
