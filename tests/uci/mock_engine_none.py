#!/usr/bin/env python3
"""Mock UCI engine that answers every search with "(none)" — the shape a
broken engine can emit; exercises the bot's Rejected path (audit 1.3)."""
import sys

for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    if line == "uci":
        print("id name MockNoneEngine 1.0")
        print("uciok")
        sys.stdout.flush()
    elif line == "isready":
        print("readyok")
        sys.stdout.flush()
    elif line.startswith("go"):
        print("info depth 1 score cp 0")
        sys.stdout.flush()
        print("bestmove (none)")
        sys.stdout.flush()
    elif line == "quit":
        break
