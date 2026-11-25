#!/usr/bin/env python3
"""Mock UCI engine that answers the handshake late (1 s): the caller's
timeout fires first, and the late uciok arrives while init() is tearing
down. Publishing it needs the engine mutex, so a quit() that still holds
that lock deadlocks both threads."""
import sys
import time

for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    if line == "uci":
        time.sleep(1.0)
        print("id name MockSlowEngine 1.0")
        print("uciok")
        sys.stdout.flush()
    elif line == "isready":
        print("readyok")
        sys.stdout.flush()
    elif line == "quit":
        break
