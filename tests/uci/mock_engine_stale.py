#!/usr/bin/env python3
"""Mock UCI engine for the stale-bestmove tests (audit 7.3):

- sends an UNSOLICITED bestmove right after uciok (before any go), and
- answers the FIRST go late (0.5 s), so a short-timeout consumer gives up
  while that answer is still in flight.

An engine layer with generation stamping must never surface either stale
reply as the answer of a search it does not belong to: the pre-go reply is
stamped with generation 0 (no search started), the late reply is stamped
with the first search's generation and must not satisfy the retry's wait."""
import sys
import time

first_go = True
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    if line == "uci":
        print("id name MockStaleEngine 1.0")
        print("uciok")
        sys.stdout.flush()
        # Unsolicited, before any go — stamped generation 0.
        print("bestmove b1c3")
        sys.stdout.flush()
    elif line == "isready":
        print("readyok")
        sys.stdout.flush()
    elif line.startswith("go"):
        if first_go:
            first_go = False
            time.sleep(0.5)
            print("bestmove e2e4")
            sys.stdout.flush()
        else:
            print("bestmove a2a3")
            sys.stdout.flush()
    elif line == "quit":
        break
